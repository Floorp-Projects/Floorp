/*
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See the
 * License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is the Mozilla OS/2 libraries.
 *
 * The Initial Developer of the Original Code is John Fairhurst,
 * <john_fairhurst@iname.com>.  Portions created by John Fairhurst are
 * Copyright (C) 1999 John Fairhurst. All Rights Reserved.
 *
 * Contributor(s): 
 */
#ifndef _nslocaleos2_h_
#define _nslocaleos2_h_


#include "nsISupports.h"
#include "nscore.h"
#include "nsString.h"
#include "nsIOS2Locale.h"


class nsOS2Locale : public nsIOS2Locale {

  NS_DECL_ISUPPORTS

public:

  nsOS2Locale();
  virtual ~nsOS2Locale();
   
  NS_IMETHOD GetPlatformLocale(PRUnichar* os2Locale,
                                size_t length);
  NS_IMETHOD GetXPLocale(const char* os2Locale, nsString* locale);

protected:
  inline PRBool ParseLocaleString(const char* locale_string, char* language, char* country, char* extra, char separator);

};

#endif
