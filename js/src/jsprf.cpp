/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Portable safe sprintf code.
 *
 * Author: Kipp E.B. Hickman
 */

#include "jsprf.h"

#include "mozilla/Printf.h"

#include "jsalloc.h"

using namespace js;

typedef mozilla::SmprintfPolicyPointer<js::SystemAllocPolicy> JSSmprintfPointer;

JS_PUBLIC_API(char*) JS_smprintf(const char* fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    JSSmprintfPointer result = mozilla::Vsmprintf<js::SystemAllocPolicy>(fmt, ap);
    va_end(ap);
    return result.release();
}

JS_PUBLIC_API(void) JS_smprintf_free(char* mem)
{
    mozilla::SmprintfFree<js::SystemAllocPolicy>(mem);
}

JS_PUBLIC_API(char*) JS_sprintf_append(char* last, const char* fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    JSSmprintfPointer result =
        mozilla::VsmprintfAppend<js::SystemAllocPolicy>(JSSmprintfPointer(last), fmt, ap);
    va_end(ap);
    return result.release();
}

JS_PUBLIC_API(char*) JS_vsmprintf(const char* fmt, va_list ap)
{
    return mozilla::Vsmprintf<js::SystemAllocPolicy>(fmt, ap).release();
}

JS_PUBLIC_API(char*) JS_vsprintf_append(char* last, const char* fmt, va_list ap)
{
    return mozilla::VsmprintfAppend<js::SystemAllocPolicy>(JSSmprintfPointer(last),
                                                           fmt, ap).release();
}
