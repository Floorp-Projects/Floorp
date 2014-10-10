//
// Copyright (c) 2002-2010 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// debug.cpp: Debugging utilities.

#include "common/debug.h"
#include "common/platform.h"
#include "common/angleutils.h"

#include <stdarg.h>
#include <vector>
#include <fstream>
#include <cstdio>

namespace gl
{
#if defined(ANGLE_ENABLE_PERF)
typedef void (WINAPI *PerfOutputFunction)(D3DCOLOR, LPCWSTR);
#else
typedef void (*PerfOutputFunction)(unsigned int, const wchar_t*);
#endif

static void output(bool traceFileDebugOnly, PerfOutputFunction perfFunc, const char *format, va_list vararg)
{
#if defined(ANGLE_ENABLE_PERF) || defined(ANGLE_ENABLE_TRACE)
    std::string formattedMessage = FormatString(format, vararg);
#endif

#if defined(ANGLE_ENABLE_PERF)
    if (perfActive())
    {
        // The perf function only accepts wide strings, widen the ascii message
        static std::wstring wideMessage;
        if (wideMessage.capacity() < formattedMessage.length())
        {
            wideMessage.reserve(formattedMessage.size());
        }

        wideMessage.assign(formattedMessage.begin(), formattedMessage.end());

        perfFunc(0, wideMessage.c_str());
    }
#endif // ANGLE_ENABLE_PERF

#if defined(ANGLE_ENABLE_TRACE)
#if defined(NDEBUG)
    if (traceFileDebugOnly)
    {
        return;
    }
#endif // NDEBUG

    static std::ofstream file(TRACE_OUTPUT_FILE, std::ofstream::app);
    if (file)
    {
        file.write(formattedMessage.c_str(), formattedMessage.length());
        file.flush();
    }

#endif // ANGLE_ENABLE_TRACE
}

void trace(bool traceFileDebugOnly, const char *format, ...)
{
    va_list vararg;
    va_start(vararg, format);
#if defined(ANGLE_ENABLE_PERF)
    output(traceFileDebugOnly, D3DPERF_SetMarker, format, vararg);
#else
    output(traceFileDebugOnly, NULL, format, vararg);
#endif
    va_end(vararg);
}

bool perfActive()
{
#if defined(ANGLE_ENABLE_PERF)
    static bool active = D3DPERF_GetStatus() != 0;
    return active;
#else
    return false;
#endif
}

ScopedPerfEventHelper::ScopedPerfEventHelper(const char* format, ...)
{
#if !defined(ANGLE_ENABLE_TRACE)
    if (!perfActive())
    {
        return;
    }
#endif // !ANGLE_ENABLE_TRACE
    va_list vararg;
    va_start(vararg, format);
#if defined(ANGLE_ENABLE_PERF)
    output(true, reinterpret_cast<PerfOutputFunction>(D3DPERF_BeginEvent), format, vararg);
#else
    output(true, NULL, format, vararg);
#endif // ANGLE_ENABLE_PERF
    va_end(vararg);
}

ScopedPerfEventHelper::~ScopedPerfEventHelper()
{
#if defined(ANGLE_ENABLE_PERF)
    if (perfActive())
    {
        D3DPERF_EndEvent();
    }
#endif
}
}
