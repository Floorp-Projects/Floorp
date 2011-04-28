/*
 * Copyright (C) 2003, 2006, 2007 Apple Inc.  All rights reserved.
 * Copyright (C) 2007-2009 Torch Mobile, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "Assertions.h"

#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#if WTF_COMPILER_MSVC
#ifndef WINVER
#define WINVER 0x0500
#endif
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0500
#endif
#include "jswin.h"
#include <crtdbg.h>
#endif

extern "C" {

WTF_ATTRIBUTE_PRINTF(1, 0)
static void vprintf_stderr_common(const char* format, va_list args)
{
    vfprintf(stderr, format, args);
}

WTF_ATTRIBUTE_PRINTF(1, 2)
static void printf_stderr_common(const char* format, ...)
{
    va_list args;
    va_start(args, format);
    vprintf_stderr_common(format, args);
    va_end(args);
}

static void printCallSite(const char* file, int line, const char* function)
{
#if WTF_COMPILER_MSVC && defined _DEBUG
    _CrtDbgReport(_CRT_WARN, file, line, NULL, "%s\n", function);
#else
    printf_stderr_common("(%s:%d %s)\n", file, line, function);
#endif
}

void WTFReportAssertionFailure(const char* file, int line, const char* function, const char* assertion)
{
    if (assertion)
        printf_stderr_common("Assertion failure: %s\n", assertion);
    else
        printf_stderr_common("Should never be reached\n");
    printCallSite(file, line, function);
}

void WTFReportAssertionFailureWithMessage(const char* file, int line, const char* function, const char* assertion, const char* format, ...)
{
    printf_stderr_common("Assertion failure: ");
    va_list args;
    va_start(args, format);
    vprintf_stderr_common(format, args);
    va_end(args);
    printf_stderr_common("\n%s\n", assertion);
    printCallSite(file, line, function);
}

void WTFReportArgumentAssertionFailure(const char* file, int line, const char* function, const char* argName, const char* assertion)
{
    printf_stderr_common("Argument bad: %s, %s\n", argName, assertion);
    printCallSite(file, line, function);
}

void WTFReportFatalError(const char* file, int line, const char* function, const char* format, ...)
{
    printf_stderr_common("Fatal error: ");
    va_list args;
    va_start(args, format);
    vprintf_stderr_common(format, args);
    va_end(args);
    printf_stderr_common("\n");
    printCallSite(file, line, function);
}

void WTFReportError(const char* file, int line, const char* function, const char* format, ...)
{
    printf_stderr_common("Error: ");
    va_list args;
    va_start(args, format);
    vprintf_stderr_common(format, args);
    va_end(args);
    printf_stderr_common("\n");
    printCallSite(file, line, function);
}

void WTFLog(WTFLogChannel* channel, const char* format, ...)
{
    if (channel->state != WTFLogChannelOn)
        return;

    va_list args;
    va_start(args, format);
    vprintf_stderr_common(format, args);
    va_end(args);
    if (format[strlen(format) - 1] != '\n')
        printf_stderr_common("\n");
}

void WTFLogVerbose(const char* file, int line, const char* function, WTFLogChannel* channel, const char* format, ...)
{
    if (channel->state != WTFLogChannelOn)
        return;

    va_list args;
    va_start(args, format);
    vprintf_stderr_common(format, args);
    va_end(args);
    if (format[strlen(format) - 1] != '\n')
        printf_stderr_common("\n");
    printCallSite(file, line, function);
}

} // extern "C"
