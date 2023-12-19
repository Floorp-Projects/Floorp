/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <stdio.h>
#include <string.h>
#include <stdarg.h>

#include "CSFLog.h"
#include "MainThreadUtils.h"

#include "prthread.h"

#include "mozilla/Logging.h"
#include "mozilla/Sprintf.h"

mozilla::LazyLogModule gSignalingLog("signaling");

void CSFLogV(CSFLogLevel priority, const char* sourceFile, int sourceLine,
             const char* tag, const char* format, va_list args) {
#ifdef STDOUT_LOGGING
  printf("%s\n:", tag);
  vprintf(format, args);
#else

  mozilla::LogLevel level =
      static_cast<mozilla::LogLevel>(static_cast<unsigned int>(priority));

  // Skip doing any of this work if we're not logging the indicated level...
  if (!MOZ_LOG_TEST(gSignalingLog, level)) {
    return;
  }

  // Trim the path component from the filename
  const char* lastSlash = sourceFile;
  while (*sourceFile) {
    if (*sourceFile == '/' || *sourceFile == '\\') {
      lastSlash = sourceFile;
    }
    sourceFile++;
  }
  sourceFile = lastSlash;
  if (*sourceFile == '/' || *sourceFile == '\\') {
    sourceFile++;
  }

#  define MAX_MESSAGE_LENGTH 1024
  char message[MAX_MESSAGE_LENGTH];

  const char* threadName = NULL;

  // Check if we're the main thread...
  if (NS_IsMainThread()) {
    threadName = "main";
  } else {
    threadName = PR_GetThreadName(PR_GetCurrentThread());
  }

  // If we can't find it anywhere, use a blank string
  if (!threadName) {
    threadName = "";
  }

  VsprintfLiteral(message, format, args);
  MOZ_LOG(
      gSignalingLog, level,
      ("[%s|%s] %s:%d: %s", threadName, tag, sourceFile, sourceLine, message));
#endif
}

void CSFLog(CSFLogLevel priority, const char* sourceFile, int sourceLine,
            const char* tag, const char* format, ...) {
  va_list ap;
  va_start(ap, format);

  CSFLogV(priority, sourceFile, sourceLine, tag, format, ap);
  va_end(ap);
}

int CSFLogTestLevel(CSFLogLevel priority) {
  return MOZ_LOG_TEST(gSignalingLog, static_cast<mozilla::LogLevel>(
                                         static_cast<unsigned int>(priority)));
}
