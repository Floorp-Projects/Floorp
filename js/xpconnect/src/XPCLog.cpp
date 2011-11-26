/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   John Bandhauer <jband@netscape.com> (original author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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
