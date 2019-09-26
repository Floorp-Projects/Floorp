/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_MEDIA_GMPLOG_H_
#define DOM_MEDIA_GMPLOG_H_

#include "mozilla/Logging.h"

namespace mozilla {

extern LogModule* GetGMPLog();

#define GMP_LOG(msg, ...) \
  MOZ_LOG(GetGMPLog(), LogLevel::Debug, (msg, ##__VA_ARGS__))

}  // namespace mozilla

#endif  // DOM_MEDIA_GMPLOG_H_
