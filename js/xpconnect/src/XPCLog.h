/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set ts=8 sts=4 et sw=4 tw=99: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Debug Logging support. */

#ifndef xpclog_h___
#define xpclog_h___

#include "mozilla/Logging.h"

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
    do{if (XPC_Log_Check(number)){XPC_Log_print _args;}}while (0)

#define XPC_LOG_ALWAYS(_args)   XPC_LOG_INTERNAL(1,_args)
#define XPC_LOG_ERROR(_args)    XPC_LOG_INTERNAL(2,_args)
#define XPC_LOG_WARNING(_args)  XPC_LOG_INTERNAL(3,_args)
#define XPC_LOG_DEBUG(_args)    XPC_LOG_INTERNAL(4,_args)
#define XPC_LOG_FLUSH()         PR_LogFlush()
#define XPC_LOG_INDENT()        XPC_Log_Indent()
#define XPC_LOG_OUTDENT()       XPC_Log_Outdent()
#define XPC_LOG_CLEAR_INDENT()  XPC_Log_Clear_Indent()
#define XPC_LOG_FINISH()        XPC_Log_Finish()

extern "C" {

void   XPC_Log_print(const char* fmt, ...);
bool   XPC_Log_Check(int i);
void   XPC_Log_Indent();
void   XPC_Log_Outdent();
void   XPC_Log_Clear_Indent();
void   XPC_Log_Finish();

} // extern "C"

#else

#define XPC_LOG_ALWAYS(_args)  ((void)0)
#define XPC_LOG_ERROR(_args)   ((void)0)
#define XPC_LOG_WARNING(_args) ((void)0)
#define XPC_LOG_DEBUG(_args)   ((void)0)
#define XPC_LOG_FLUSH()        ((void)0)
#define XPC_LOG_INDENT()       ((void)0)
#define XPC_LOG_OUTDENT()      ((void)0)
#define XPC_LOG_CLEAR_INDENT() ((void)0)
#define XPC_LOG_FINISH()       ((void)0)
#endif

#endif /* xpclog_h___ */
