
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
#ifndef nsCollation_h__
#define nsCollation_h__


#include "nsICollation.h"
#include "nsICaseConversion.h"
#include "nsICharsetConverterManager.h"
#include "nsCOMPtr.h"


// Create a collation interface for an input locale.
// 
class nsCollationFactory: public nsICollationFactory {

public: 
  NS_DECL_ISUPPORTS 

  NS_IMETHOD CreateCollation(nsILocale* locale, nsICollation** instancePtr);

  nsCollationFactory() {};
};


struct nsCollation {

public: 

  nsCollation();
  
  ~nsCollation();

  // normalize string before collation key generation
  nsresult NormalizeString(const nsAString& stringIn, nsAString& stringOut);

  // charset conversion util, C string buffer is allocate by PR_Malloc, caller should call PR_Free
  nsresult SetCharset(const char* aCharset);
  nsresult UnicodeToChar(const nsAString& aSrc, char** dst);

protected:
  nsCOMPtr <nsICaseConversion>            mCaseConversion;
  nsCOMPtr <nsIUnicodeEncoder>            mEncoder;
};

#endif  /* nsCollation_h__ */
