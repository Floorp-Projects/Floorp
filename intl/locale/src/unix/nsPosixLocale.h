/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */
#ifndef nsPosixLocale_h__
#define nsPosixLocale_h__


#include "nsISupports.h"
#include "nscore.h"
#include "nsString.h"
#include "nsIPosixLocale.h"



class nsPosixLocale : public nsIPosixLocale {

  NS_DECL_ISUPPORTS

public:
  
  nsPosixLocale();
  ~nsPosixLocale();

  NS_IMETHOD GetPlatformLocale(const nsString* locale,char* posixLocale,
                               size_t length);
  NS_IMETHOD GetXPLocale(const char* posixLocale, nsString* locale);

protected:
  inline PRBool ParseLocaleString(const char* locale_string, char* language, char* country, char* region, char separator);

};


#endif
