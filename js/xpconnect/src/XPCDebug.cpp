/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set ts=8 sts=4 et sw=4 tw=99: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "xpcprivate.h"
#include "jsprf.h"
#include "nsThreadUtils.h"
#include "nsContentUtils.h"

#include "mozilla/Sprintf.h"

#ifdef XP_WIN
#include <windows.h>
#endif

static void DebugDump(const char* fmt, ...)
{
  char buffer[2048];
  va_list ap;
  va_start(ap, fmt);
#ifdef XPWIN
  _vsnprintf(buffer, sizeof(buffer), fmt, ap);
  buffer[sizeof(buffer)-1] = '\0';
#else
  VsprintfLiteral(buffer, fmt, ap);
#endif
  va_end(ap);
#ifdef XP_WIN
  if (IsDebuggerPresent()) {
    OutputDebugStringA(buffer);
  }
#endif
  printf("%s", buffer);
}

bool
xpc_DumpJSStack(bool showArgs, bool showLocals, bool showThisProps)
{
    JSContext* cx = nsContentUtils::GetCurrentJSContextForThread();
    if (!cx) {
        printf("there is no JSContext on the stack!\n");
    } else if (JS::UniqueChars buf = xpc_PrintJSStack(cx, showArgs, showLocals, showThisProps)) {
        DebugDump("%s\n", buf.get());
    }
    return true;
}

JS::UniqueChars
xpc_PrintJSStack(JSContext* cx, bool showArgs, bool showLocals,
                 bool showThisProps)
{
    JS::AutoSaveExceptionState state(cx);

    JS::UniqueChars buf = JS::FormatStackDump(cx, nullptr, showArgs, showLocals, showThisProps);
    if (!buf)
        DebugDump("%s", "Failed to format JavaScript stack for dump\n");

    state.restore();
    return buf;
}
