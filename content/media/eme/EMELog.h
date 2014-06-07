/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "prlog.h"

namespace mozilla {

#ifdef PR_LOGGING

#ifndef EME_LOG
PRLogModuleInfo* GetEMELog();
#define EME_LOG(...) PR_LOG(GetEMELog(), PR_LOG_DEBUG, (__VA_ARGS__))
#endif

#ifndef EME_VERBOSE_LOG
PRLogModuleInfo* GetEMEVerboseLog();
#define EME_VERBOSE_LOG(...) PR_LOG(GetEMEVerboseLog(), PR_LOG_DEBUG, (__VA_ARGS__))

#else

#ifndef EME_LOG
#define EME_LOG(...)
#endif

#ifndef EME_VERBOSE_LOG
#define EME_VERBOSE_LOG(...)
#endif

#endif

#endif // PR_LOGGING
} // namespace mozilla
