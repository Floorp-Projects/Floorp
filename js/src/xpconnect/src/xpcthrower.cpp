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

/* Code for throwing errors into JavaScript. */

#include "xpcprivate.h"

XPCJSErrorFormatString XPCJSThrower::default_formats[] = {
#define MSG_DEF(name, number, count, exception, format) \
    { format/*, count*/ } ,
#include "xpc.msg"
#undef MSG_DEF
    { NULL/*, 0*/ }
};

XPCJSThrower::XPCJSThrower(JSBool Verbose /*= JS_FALSE*/)
    : mFormats(default_formats), mVerbose(Verbose) {}

XPCJSThrower::~XPCJSThrower() {}


char*
XPCJSThrower::BuildCallerString(JSContext* cx)
{
    JSStackFrame* iter = NULL;
    JSStackFrame* fp;

    while(NULL != (fp = JS_FrameIterator(cx, &iter)) )
    {
        JSScript* script = JS_GetFrameScript(cx, fp);
        jsbytecode* pc = JS_GetFramePC(cx, fp);

        if(!script || !pc || JS_IsNativeFrame(cx, fp))
            continue;

        const char* filename = JS_GetScriptFilename(cx, script);
        return JS_smprintf("{file: %s, line: %d}",
                           filename ? filename : "<unknown>",
                           JS_PCToLineNumber(cx, script, pc));
    }
    return NULL;
}

void
XPCJSThrower::Verbosify(JSContext* cx,
                        nsXPCWrappedNativeClass* clazz,
                        const XPCNativeMemberDescriptor* desc,
                        char** psz, PRBool own)
{
    char* sz = NULL;
    char* caller_string = BuildCallerString(cx);

    if(clazz && desc)
        sz = JS_smprintf("%s [%s.%s, %s]",
                         *psz,
                         clazz->GetInterfaceName(),
                         clazz->GetMemberName(desc),
                         caller_string ? caller_string : "");
    if(caller_string)
        JS_smprintf_free(caller_string);
    if(sz)
    {
        if(own)
            JS_smprintf_free(*psz);
        *psz = sz;
    }
}

void
XPCJSThrower::ThrowBadResultException(JSContext* cx,
                                      nsXPCWrappedNativeClass* clazz,
                                      const XPCNativeMemberDescriptor* desc,
                                      nsresult result)
{
    char* sz;
    const char* format;
    JSString* str = NULL;

    format = mFormats[XPCJSError::NATIVE_RETURNED_FAILURE].format;

    sz = JS_smprintf("%s %x", format, result);

    if(sz && mVerbose)
        Verbosify(cx, clazz, desc, &sz, PR_TRUE);

    if(sz)
    {
        str = JS_NewStringCopyZ(cx, sz);
        JS_smprintf_free(sz);
    }

    if(str)
        JS_SetPendingException(cx, STRING_TO_JSVAL(str));
    else
        JS_ReportOutOfMemory(cx);
}

void
XPCJSThrower::ThrowBadParamException(uintN errNum,
                            JSContext* cx,
                            nsXPCWrappedNativeClass* clazz,
                            const XPCNativeMemberDescriptor* desc,
                            uintN paramNum)
{
    char* sz;
    const char* format;
    JSString* str = NULL;

    format = mFormats[errNum].format;

    sz = JS_smprintf("%s arg %d", format, paramNum);

    if(sz && mVerbose)
        Verbosify(cx, clazz, desc, &sz, PR_TRUE);

    if(sz)
    {
        str = JS_NewStringCopyZ(cx, sz);
        JS_smprintf_free(sz);
    }

    if(str)
        JS_SetPendingException(cx, STRING_TO_JSVAL(str));
    else
        JS_ReportOutOfMemory(cx);
}

void
XPCJSThrower::ThrowException(uintN errNum,
                    JSContext* cx,
                    nsXPCWrappedNativeClass* clazz,
                    const XPCNativeMemberDescriptor* desc)
{
    char* sz;
    const char* format;
    JSString* str = NULL;

    format = mFormats[errNum].format;
    sz = (char*) format;

    if(sz && mVerbose)
        Verbosify(cx, clazz, desc, &sz, PR_FALSE);

    if(sz)
    {
        str = JS_NewStringCopyZ(cx, sz);
        if(sz != format)
            JS_smprintf_free(sz);
    }

    if(str)
        JS_SetPendingException(cx, STRING_TO_JSVAL(str));
    else
        JS_ReportOutOfMemory(cx);
}
