
/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef nsCollation_h__
#define nsCollation_h__


#include "nsICollation.h"
#include "nsICharsetConverterManager.h"
#include "nsCOMPtr.h"
#include "mozilla/Attributes.h"


// Create a collation interface for an input locale.
// 
class nsCollationFactory MOZ_FINAL : public nsICollationFactory {

public: 
  NS_DECL_ISUPPORTS 

  NS_IMETHOD CreateCollation(nsILocale* locale, nsICollation** instancePtr);

  nsCollationFactory() {}
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
  nsCOMPtr <nsIUnicodeEncoder>            mEncoder;
};

#endif  /* nsCollation_h__ */
