 /* Usage:
 *   rbusWebpaConsumer                        # runs the default SetRequestedState + InstallDU calls
 *   rbusWebpaConsumer <methodName> <argName> <argValue>
 *
 * Example:
 *   rbusWebpaConsumer Device.SoftwareModules.ExecutionUnit.1.SetRequestedState() RequestedState Active
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <rbus.h>

#define METHOD_INSTALL_DU   "Device.SoftwareModules.InstallDU()"
#define METHOD_SET_STATE    "Device.SoftwareModules.ExecutionUnit.1.SetRequestedState()"

/*
 * Dump outParams exactly like WebPA does: iterate each property and print its
 * name, rbus type and string value. This surfaces the case where a property
 * name decodes correctly while its string value comes back empty.
 */
#define WEBPA_OUTPARAMS_DUMP_FILE   "/tmp/rbusWebpaConsumer_outParams.txt"

static void dumpOutParams(const char* methodName, rbusObject_t outParams)
{
    FILE* fp = NULL;

    if(outParams == NULL)
    {
        printf("consumer: %s outParams is NULL\n", methodName);
        return;
    }

    /* Dump the raw object to a file (like WebPA does to
     * /tmp/webpa_method_outParams.txt) as well as to stdout. */
    fp = fopen(WEBPA_OUTPARAMS_DUMP_FILE, "a");
    if(fp != NULL)
    {
        fprintf(fp, "method: %s\n", methodName);
        rbusObject_fwrite(outParams, 1, fp);
    }
    else
    {
        printf("consumer: failed to open %s for writing outParams\n", WEBPA_OUTPARAMS_DUMP_FILE);
    }

    printf("consumer: %s outParams (rbusObject_fwrite):\n", methodName);
    rbusObject_fwrite(outParams, 1, stdout);

    rbusProperty_t prop = rbusObject_GetProperties(outParams);
    if(prop == NULL)
    {
        printf("consumer: %s outParams carries no properties\n", methodName);
        if(fp != NULL)
        {
            fprintf(fp, "consumer: %s outParams carries no properties\n", methodName);
            fclose(fp);
        }
        return;
    }

    while(prop != NULL)
    {
        const char* name = rbusProperty_GetName(prop);
        rbusValue_t val = rbusProperty_GetValue(prop);
        int len = 0;
        const char* str = (val != NULL) ? rbusValue_GetString(val, &len) : NULL;
        printf("consumer: outParams[%s] rbusType=%d len=%d value='%s'\n",
            name ? name : "<null>",
            val ? (int)rbusValue_GetType(val) : -1,
            len,
            str ? str : "<null-or-non-string>");
        if(fp != NULL)
        {
            fprintf(fp, "consumer: outParams[%s] rbusType=%d len=%d value='%s'\n",
                name ? name : "<null>",
                val ? (int)rbusValue_GetType(val) : -1,
                len,
                str ? str : "<null-or-non-string>");
        }
        prop = rbusProperty_GetNext(prop);
    }

    if(fp != NULL)
    {
        fclose(fp);
        printf("consumer: dumped %s outParams to %s\n", methodName, WEBPA_OUTPARAMS_DUMP_FILE);
    }
}

/*
 * Build inParams with a single RBUS_STRING argument (WebPA style) and invoke
 * the method synchronously, then dump the response.
 */
static int invokeStringMethod(rbusHandle_t handle, const char* methodName,
        const char* argName, const char* argValue)
{
    rbusObject_t inParams = NULL;
    rbusObject_t outParams = NULL;
    rbusValue_t value = NULL;
    int rc;

    rbusObject_Init(&inParams, NULL);

    if(argName != NULL && argValue != NULL)
    {
        rbusValue_Init(&value);
        rbusValue_SetString(value, argValue);   /* WebPA scalars are strings */
        rbusObject_SetValue(inParams, argName, value);
        rbusValue_Release(value);
        printf("consumer: invoking %s with %s=\"%s\"\n", methodName, argName, argValue);
    }
    else
    {
        printf("consumer: invoking %s with no inParams\n", methodName);
    }

    rc = rbusMethod_Invoke(handle, methodName, inParams, &outParams);
    rbusObject_Release(inParams);

    printf("consumer: rbusMethod_Invoke(%s) rc=%d (%s)\n",
        methodName, rc, rc == RBUS_ERROR_SUCCESS ? "success" : "fail");

    dumpOutParams(methodName, outParams);

    if(outParams != NULL)
        rbusObject_Release(outParams);

    return rc;
}

int main(int argc, char* argv[])
{
    rbusHandle_t handle;
    int rc = RBUS_ERROR_SUCCESS;

    printf("consumer: start\n");
    rbus_setLogLevel(RBUS_LOG_DEBUG);
    rc = rbus_open(&handle, "WebpaConsumer");
    if(rc != RBUS_ERROR_SUCCESS)
    {
        printf("consumer: rbus_open failed: %d\n", rc);
        return rc;
    }

    if(argc >= 4)
    {
        /* User supplied: <methodName> <argName> <argValue> */
        invokeStringMethod(handle, argv[1], argv[2], argv[3]);
    }
    else if(argc == 2)
    {
        /* User supplied just a method name, no args. */
        invokeStringMethod(handle, argv[1], NULL, NULL);
    }
    else
    {
        /* Default: exercise the DSM-like methods the same way WebPA would. */
        printf("\n=== SetRequestedState (sync) ===\n");
        invokeStringMethod(handle, METHOD_SET_STATE, "RequestedState", "Active");

        printf("\n=== InstallDU (provider replies async; Invoke blocks) ===\n");
        invokeStringMethod(handle, METHOD_INSTALL_DU, "URL", "http://10.0.0.206/pkg.tar");
    }

    rbus_close(handle);
    printf("consumer: exit\n");
    return rc;
}

