/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
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
  virtual ~nsPosixLocale();

  NS_IMETHOD GetPlatformLocale(const nsString* locale,char* posixLocale,
                               size_t length);
  NS_IMETHOD GetXPLocale(const char* posixLocale, nsString* locale);

protected:
  inline PRBool ParseLocaleString(const char* locale_string, char* language, char* country, char* extra, char separator);

};


#endif
