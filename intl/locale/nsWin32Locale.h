/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef nsWin32Locale_h__
#define nsWin32Locale_h__

#include "nscore.h"
#include "nsString.h"
#include <windows.h>


class nsWin32Locale final {
public:
  static nsresult    GetPlatformLocale(const nsAString& locale, LCID* winLCID);
  static void        GetXPLocale(LCID winLCID, nsAString& locale);

private:
  // Static class - Don't allow instantiation.
  nsWin32Locale(void) {}
};

#endif
