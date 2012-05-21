/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Debug Logging support. */

#include "xpcprivate.h"

// this all only works for DEBUG...
#ifdef DEBUG

#define SPACE_COUNT     200
#define LINE_LEN        200
#define INDENT_FACTOR   2

#define CAN_RUN (g_InitState == 1 || (g_InitState == 0 && Init()))

static char*    g_Spaces;
static int      g_InitState = 0;
static int      g_Indent = 0;
static PRLogModuleInfo* g_LogMod = nsnull;

static bool Init()
{
    g_LogMod = PR_NewLogModule("xpclog");
    g_Spaces = new char[SPACE_COUNT+1];
    if (!g_LogMod || !g_Spaces || !PR_LOG_TEST(g_LogMod,1)) {
        g_InitState = 1;
        XPC_Log_Finish();
        return false;
    }
    memset(g_Spaces, ' ', SPACE_COUNT);
    g_Spaces[SPACE_COUNT] = 0;
    g_InitState = 1;
    return true;
}

void
XPC_Log_Finish()
{
    if (g_InitState == 1) {
        delete [] g_Spaces;
        // we'd like to properly cleanup the LogModule, but nspr owns that
        g_LogMod = nsnull;
    }
    g_InitState = -1;
}

void
XPC_Log_print(const char *fmt, ...)
{
    va_list ap;
    char line[LINE_LEN];

    va_start(ap, fmt);
    PR_vsnprintf(line, sizeof(line)-1, fmt, ap);
    va_end(ap);
    if (g_Indent)
        PR_LogPrint("%s%s",g_Spaces+SPACE_COUNT-(INDENT_FACTOR*g_Indent),line);
    else
        PR_LogPrint("%s",line);
}

bool
XPC_Log_Check(int i)
{
    return CAN_RUN && PR_LOG_TEST(g_LogMod,1);
}

void
XPC_Log_Indent()
{
    if (INDENT_FACTOR*(++g_Indent) > SPACE_COUNT)
        g_Indent-- ;
}

void
XPC_Log_Outdent()
{
    if (--g_Indent < 0)
        g_Indent++;
}

void
XPC_Log_Clear_Indent()
{
    g_Indent = 0;
}

#endif

#ifdef DEBUG_slimwrappers
void
LogSlimWrapperWillMorph(JSContext *cx, JSObject *obj, const char *propname,
                        const char *functionName)
{
    if (obj && IS_SLIM_WRAPPER(obj)) {
        XPCNativeScriptableInfo *si =
            GetSlimWrapperProto(obj)->GetScriptableInfo();
        printf("***** morphing %s from %s", si->GetJSClass()->name,
               functionName);
        if (propname)
            printf(" for %s", propname);
        printf(" (%p, %p)\n", obj,
               static_cast<nsISupports*>(xpc_GetJSPrivate(obj)));
        xpc_DumpJSStack(cx, false, false, false);
    }
}

void
LogSlimWrapperNotCreated(JSContext *cx, nsISupports *obj, const char *reason)
{
    char* className = nsnull;
    nsCOMPtr<nsIClassInfo> ci = do_QueryInterface(obj);
    if (ci)
        ci->GetClassDescription(&className);
    printf("***** refusing to create slim wrapper%s%s, reason: %s (%p)\n",
           className ? " for " : "", className ? className : "", reason, obj);
    if (className)
        PR_Free(className);
    JSAutoRequest autoRequest(cx);
    xpc_DumpJSStack(cx, false, false, false);
}
#endif
