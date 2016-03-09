/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsIDateTimeFormat.h"
#include "mozilla/RefPtr.h"

#if defined(XP_MACOSX)
#define USE_MAC_LOCALE
#elif defined(XP_UNIX)
#define USE_UNIX_LOCALE
#endif

#ifdef XP_WIN
#include "windows/nsDateTimeFormatWin.h"
#endif
#ifdef USE_UNIX_LOCALE
#include "unix/nsDateTimeFormatUnix.h"
#endif
#ifdef USE_MAC_LOCALE
#include "mac/nsDateTimeFormatMac.h"
#endif

using mozilla::MakeAndAddRef;

/*static*/ already_AddRefed<nsIDateTimeFormat>
nsIDateTimeFormat::Create()
{
#ifdef XP_WIN
  return MakeAndAddRef<nsDateTimeFormatWin>();
#elif defined(USE_UNIX_LOCALE)
  return MakeAndAddRef<nsDateTimeFormatUnix>();
#elif defined(USE_MAC_LOCALE)
  return MakeAndAddRef<nsDateTimeFormatMac>();
#else
  return nullptr;
#endif
}
