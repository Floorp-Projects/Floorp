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

#ifndef xpclog_h___
#define xpclog_h___

#include "nsIXPConnect.h"
#include "prtypes.h"
#include "prlog.h"

/*
 * This uses prlog.h See prlog.h for environment settings for output.
 * The module name used here is 'xpclog'. These environment settings
 * should work...
 *
 * SET NSPR_LOG_MODULES=xpclog:5
 * SET NSPR_LOG_FILE=logfile.txt
 *
 * usage:
 *   XPC_LOG_ERROR(("my comment number %d", 5))   // note the double parens
 *
 */

#ifdef DEBUG
#define XPC_LOG_INTERNAL(number,_args)  \
    do{if(XPC_Log_Check(number)){XPC_Log_print _args;}}while(0)

#define XPC_LOG_ALWAYS(_args)   XPC_LOG_INTERNAL(1,_args)
#define XPC_LOG_ERROR(_args)    XPC_LOG_INTERNAL(2,_args)
#define XPC_LOG_WARNING(_args)  XPC_LOG_INTERNAL(3,_args)
#define XPC_LOG_DEBUG(_args)    XPC_LOG_INTERNAL(4,_args)
#define XPC_LOG_FLUSH()         PR_LogFlush()
#define XPC_LOG_INDENT()        XPC_Log_Indent()
#define XPC_LOG_OUTDENT()       XPC_Log_Outdent()
#define XPC_LOG_CLEAR_INDENT()  XPC_Log_Clear_Indent()

JS_BEGIN_EXTERN_C

XPC_PUBLIC_API(void)   XPC_Log_print(const char *fmt, ...);
XPC_PUBLIC_API(PRBool) XPC_Log_Check(int i);
XPC_PUBLIC_API(void)   XPC_Log_Indent();
XPC_PUBLIC_API(void)   XPC_Log_Outdent();
XPC_PUBLIC_API(void)   XPC_Log_Clear_Indent();

JS_END_EXTERN_C

#else

#define XPC_LOG_ALWAYS(_args)  ((void)0)
#define XPC_LOG_ERROR(_args)   ((void)0)
#define XPC_LOG_WARNING(_args) ((void)0)
#define XPC_LOG_DEBUG(_args)   ((void)0)
#define XPC_LOG_FLUSH()        ((void)0)
#define XPC_LOG_INDENT()       ((void)0)
#define XPC_LOG_OUTDENT()      ((void)0)
#define XPC_LOG_CLEAR_INDENT() ((void)0)
#endif

#endif /* xpclog_h___ */
