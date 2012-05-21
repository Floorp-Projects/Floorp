/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef nsIPosixLocale_h__
#define nsIPosixLocale_h__


#include "nscore.h"
#include "nsString.h"

#define MAX_LANGUAGE_CODE_LEN 3
#define MAX_COUNTRY_CODE_LEN  3
#define MAX_LOCALE_LEN        128
#define MAX_EXTRA_LEN         65

class nsPosixLocale {

public:
  static nsresult GetPlatformLocale(const nsAString& locale, nsACString& posixLocale);
  static nsresult GetXPLocale(const char* posixLocale, nsAString& locale);
};

#endif
