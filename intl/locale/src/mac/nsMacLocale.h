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
#ifndef nsMacLocale_h__
#define nsMacLocale_h__


#include "nsISupports.h"
#include "nscore.h"
#include "nsString.h"
#include "nsIMacLocale.h"



class nsMacLocale : public nsIMacLocale {

  NS_DECL_ISUPPORTS

public:
  
  nsMacLocale();
  virtual ~nsMacLocale();

  NS_IMETHOD GetPlatformLocale(const nsString* locale,short* scriptCode, short* langCode, short* regionCode);
  NS_IMETHOD GetXPLocale(short scriptCode, short langCode, short regionCode, nsString* locale);
  
protected:
  inline PRBool ParseLocaleString(const char* locale_string, char* language, char* country, char* region, char separator);


};


#endif
