/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Debug Logging support. */

#include "XPCLog.h"
#include "mozilla/Logging.h"
#include "mozilla/Sprintf.h"
#include "mozilla/mozalloc.h"
#include <string.h>
#include <stdarg.h>

// this all only works for DEBUG...
#ifdef DEBUG

#  define SPACE_COUNT 200
#  define LINE_LEN 200
#  define INDENT_FACTOR 2

#  define CAN_RUN (g_InitState == 1 || (g_InitState == 0 && Init()))

static char* g_Spaces;
static int g_InitState = 0;
static int g_Indent = 0;
static mozilla::LazyLogModule g_LogMod("xpclog");

static bool Init() {
  g_Spaces = new char[SPACE_COUNT + 1];
  if (!g_Spaces || !MOZ_LOG_TEST(g_LogMod, LogLevel::Error)) {
    g_InitState = 1;
    XPC_Log_Finish();
    return false;
  }
  memset(g_Spaces, ' ', SPACE_COUNT);
  g_Spaces[SPACE_COUNT] = 0;
  g_InitState = 1;
  return true;
}

void XPC_Log_Finish() {
  if (g_InitState == 1) {
    delete[] g_Spaces;
  }
  g_InitState = -1;
}

void XPC_Log_print(const char* fmt, ...) {
  va_list ap;
  char line[LINE_LEN];

  va_start(ap, fmt);
  VsprintfLiteral(line, fmt, ap);
  va_end(ap);
  if (g_Indent) {
    PR_LogPrint("%s%s", g_Spaces + SPACE_COUNT - (INDENT_FACTOR * g_Indent),
                line);
  } else {
    PR_LogPrint("%s", line);
  }
}

bool XPC_Log_Check(int i) {
  return CAN_RUN && MOZ_LOG_TEST(g_LogMod, LogLevel::Error);
}

void XPC_Log_Indent() {
  if (INDENT_FACTOR * (++g_Indent) > SPACE_COUNT) {
    g_Indent--;
  }
}

void XPC_Log_Outdent() {
  if (--g_Indent < 0) {
    g_Indent++;
  }
}

void XPC_Log_Clear_Indent() { g_Indent = 0; }

#endif
