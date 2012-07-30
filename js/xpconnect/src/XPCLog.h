/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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

JS_BEGIN_EXTERN_C

void   XPC_Log_print(const char *fmt, ...);
bool XPC_Log_Check(int i);
void   XPC_Log_Indent();
void   XPC_Log_Outdent();
void   XPC_Log_Clear_Indent();
void   XPC_Log_Finish();

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
#define XPC_LOG_FINISH()       ((void)0)
#endif


#ifdef DEBUG_peterv
#define DEBUG_slimwrappers 1
#endif

#ifdef DEBUG_slimwrappers
extern void LogSlimWrapperWillMorph(JSContext *cx, JSObject *obj,
                                    const char *propname,
                                    const char *functionName);
extern void LogSlimWrapperNotCreated(JSContext *cx, nsISupports *obj,
                                     const char *reason);

#define SLIM_LOG_WILL_MORPH_FOR_PROP(cx, obj, prop)                           \
    PR_BEGIN_MACRO                                                            \
        LogSlimWrapperWillMorph(cx, obj, (const char*)prop, __FUNCTION__);    \
    PR_END_MACRO
#define SLIM_LOG_WILL_MORPH_FOR_ID(cx, obj, id)                               \
    PR_BEGIN_MACRO                                                            \
        JSString* strId = ::JS_ValueToString(cx, id);                         \
        if (strId) {                                                          \
          NS_ConvertUTF16toUTF8 name((PRUnichar*)::JS_GetStringChars(strId),  \
                                     ::JS_GetStringLength(strId));            \
          LOG_WILL_MORPH_FOR_PROP(cx, obj, name.get());                       \
        }                                                                     \
        else                                                                  \
        {                                                                     \
          LOG_WILL_MORPH_FOR_PROP(cx, obj, nullptr);                           \
        }                                                                     \
    PR_END_MACRO
#define SLIM_LOG_NOT_CREATED(cx, obj, reason)                                 \
    PR_BEGIN_MACRO                                                            \
        LogSlimWrapperNotCreated(cx, obj, reason);                            \
    PR_END_MACRO
#define SLIM_LOG(_args)                                                       \
    PR_BEGIN_MACRO                                                            \
        printf _args;                                                         \
    PR_END_MACRO
#else
#define SLIM_LOG_WILL_MORPH_FOR_PROP(cx, obj, prop)                           \
    PR_BEGIN_MACRO                                                            \
    PR_END_MACRO
#define SLIM_LOG_WILL_MORPH_FOR_ID(cx, obj)                                   \
    PR_BEGIN_MACRO                                                            \
    PR_END_MACRO
#define SLIM_LOG_NOT_CREATED(cx, obj, reason)                                 \
    PR_BEGIN_MACRO                                                            \
    PR_END_MACRO
#define SLIM_LOG(_args)                                                       \
    PR_BEGIN_MACRO                                                            \
    PR_END_MACRO
#endif
#define SLIM_LOG_WILL_MORPH(cx, obj)                                         \
    SLIM_LOG_WILL_MORPH_FOR_PROP(cx, obj, nullptr)

#endif /* xpclog_h___ */
