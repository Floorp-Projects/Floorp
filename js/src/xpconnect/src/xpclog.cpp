/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

/* Debug Logging support. */

#include <stdarg.h>
#include <string.h>
#include "prprf.h"
#include "xpclog.h"

#ifdef DEBUG

#define SPACE_COUNT     200
#define LINE_LEN        200
#define INDENT_FACTOR   2

#define CAN_RUN (g_InitState == 1 || (g_InitState == 0 && Init()))

static char*    g_Spaces;
static int      g_InitState = 0;
static int      g_Indent = 0;
static PRLogModuleInfo* g_LogMod = NULL;

static PRBool Init()
{
    g_LogMod = PR_NewLogModule("xpclog");
    g_Spaces = new char[SPACE_COUNT+1];
    memset(g_Spaces, ' ', SPACE_COUNT);
    g_Spaces[SPACE_COUNT] = 0;
    if(!g_LogMod || !g_Spaces || !PR_LOG_TEST(g_LogMod,1))
    {
        g_InitState = -1;
        return PR_FALSE;
    }
    g_InitState = 1;
    return PR_TRUE;
}

XPC_PUBLIC_API(void)
XPC_Log_print(const char *fmt, ...)
{
    va_list ap;
    char line[LINE_LEN];

    va_start(ap, fmt);
    PR_vsnprintf(line, sizeof(line)-1, fmt, ap);
    va_end(ap);
    if(g_Indent)
        PR_LogPrint("%s%s",g_Spaces+SPACE_COUNT-(INDENT_FACTOR*g_Indent),line);
    else
        PR_LogPrint("%s",line);
}

XPC_PUBLIC_API(PRBool)
XPC_Log_Check(int i)
{
    return CAN_RUN && PR_LOG_TEST(g_LogMod,1);
}

XPC_PUBLIC_API(void)
XPC_Log_Indent()
{
    if(INDENT_FACTOR*(++g_Indent) > SPACE_COUNT)
        g_Indent-- ;
}

XPC_PUBLIC_API(void)
XPC_Log_Outdent()
{
    if(--g_Indent < 0)
        g_Indent++;
}

XPC_PUBLIC_API(void)
XPC_Log_Clear_Indent()
{
    g_Indent = 0;
}

#endif
