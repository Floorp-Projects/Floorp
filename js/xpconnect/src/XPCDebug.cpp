/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set ts=8 sts=4 et sw=4 tw=99: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "xpcprivate.h"
#include "jsprf.h"
#include "js/OldDebugAPI.h"

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
#else
  vsnprintf(buffer, sizeof(buffer), fmt, ap);
#endif
  buffer[sizeof(buffer)-1] = '\0';
  va_end(ap);
#ifdef XP_WIN
  if (IsDebuggerPresent()) {
    OutputDebugStringA(buffer);
  }
#endif
  printf("%s", buffer);
}

bool
xpc_DumpJSStack(JSContext* cx, bool showArgs, bool showLocals, bool showThisProps)
{
    if (char* buf = xpc_PrintJSStack(cx, showArgs, showLocals, showThisProps)) {
        DebugDump("%s\n", buf);
        JS_smprintf_free(buf);
    }
    return true;
}

char*
xpc_PrintJSStack(JSContext* cx, bool showArgs, bool showLocals,
                 bool showThisProps)
{
    JS::AutoSaveExceptionState state(cx);

    char *buf = JS::FormatStackDump(cx, nullptr, showArgs, showLocals, showThisProps);
    if (!buf)
        DebugDump("%s", "Failed to format JavaScript stack for dump\n");

    state.restore();
    return buf;
}
