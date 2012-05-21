/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "assert.h"
#include "ANPBase.h"
#include <android/log.h>

#define LOG(args...)  __android_log_print(ANDROID_LOG_INFO, "GeckoPlugins" , ## args)
#define ASSIGN(obj, name)   (obj)->name = anp_log_##name

void
anp_log_log(ANPLogType type, const char format[], ...) {

  va_list argp;
  va_start(argp,format);
  __android_log_vprint(type == kError_ANPLogType ? ANDROID_LOG_ERROR : type == kWarning_ANPLogType ?
                       ANDROID_LOG_WARN : ANDROID_LOG_INFO, "GeckoPluginLog", format, argp);
  va_end(argp);
}

void InitLogInterface(ANPLogInterfaceV0 *i) {
      _assert(i->inSize == sizeof(*i));
      ASSIGN(i, log);
}
