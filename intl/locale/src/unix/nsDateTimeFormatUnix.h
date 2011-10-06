
/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
#ifndef nsDateTimeFormatUnix_h__
#define nsDateTimeFormatUnix_h__


#include "nsICharsetConverterManager.h"
#include "nsCOMPtr.h"
#include "nsIDateTimeFormat.h"

#define kPlatformLocaleLength 64

class nsDateTimeFormatUnix : public nsIDateTimeFormat {

public: 
  NS_DECL_ISUPPORTS 

  // performs a locale sensitive date formatting operation on the time_t parameter
  NS_IMETHOD FormatTime(nsILocale* locale, 
                        const nsDateFormatSelector  dateFormatSelector, 
                        const nsTimeFormatSelector timeFormatSelector, 
                        const time_t  timetTime, 
                        nsAString& stringOut); 

  // performs a locale sensitive date formatting operation on the struct tm parameter
  NS_IMETHOD FormatTMTime(nsILocale* locale, 
                        const nsDateFormatSelector  dateFormatSelector, 
                        const nsTimeFormatSelector timeFormatSelector, 
                        const struct tm*  tmTime, 
                        nsAString& stringOut); 

  // performs a locale sensitive date formatting operation on the PRTime parameter
  NS_IMETHOD FormatPRTime(nsILocale* locale, 
                          const nsDateFormatSelector  dateFormatSelector, 
                          const nsTimeFormatSelector timeFormatSelector, 
                          const PRTime  prTime, 
                          nsAString& stringOut);

  // performs a locale sensitive date formatting operation on the PRExplodedTime parameter
  NS_IMETHOD FormatPRExplodedTime(nsILocale* locale, 
                                  const nsDateFormatSelector  dateFormatSelector, 
                                  const nsTimeFormatSelector timeFormatSelector, 
                                  const PRExplodedTime*  explodedTime, 
                                  nsAString& stringOut); 


  nsDateTimeFormatUnix() {mLocale.Truncate();mAppLocale.Truncate();}

  virtual ~nsDateTimeFormatUnix() {}

private:
  // init this interface to a specified locale
  NS_IMETHOD Initialize(nsILocale* locale);

  void LocalePreferred24hour();

  nsString    mLocale;
  nsString    mAppLocale;
  nsCString   mCharset;        // in order to convert API result to unicode
  nsCString   mPlatformLocale; // for setlocale
  bool        mLocalePreferred24hour;                       // true if 24 hour format is preferred by current locale
  bool        mLocaleAMPMfirst;                             // true if AM/PM string is preferred before the time
  nsCOMPtr <nsIUnicodeDecoder>   mDecoder;
};

#endif  /* nsDateTimeFormatUnix_h__ */
