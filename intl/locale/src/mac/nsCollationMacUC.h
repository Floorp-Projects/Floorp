/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsCollationMacUC_h_
#define nsCollationMacUC_h_

#include "nsICollation.h"
#include "nsCollation.h"
#include <Carbon/Carbon.h>

// Maximum number of characters for a buffer to remember 
// the generated collation key.
const PRUint32 kCacheSize = 128;
// According to the documentation, the length of the key should typically be
// at least 5 * textLength, but 6* would be safer.
const PRUint32 kCollationValueSizeFactor = 6;

class nsCollationMacUC : public nsICollation {

public: 
  nsCollationMacUC();
  ~nsCollationMacUC(); 

  // nsISupports interface
  NS_DECL_ISUPPORTS

  // nsICollation interface
  NS_DECL_NSICOLLATION

protected:
  nsresult ConvertLocale(nsILocale* aNSLocale, LocaleRef* aMacLocale);
  nsresult StrengthToOptions(const PRInt32 aStrength,
                             UCCollateOptions* aOptions);
  nsresult EnsureCollator(const PRInt32 newStrength);

private:
  bool mInit;
  bool mHasCollator;
  LocaleRef mLocale;
  PRInt32 mLastStrength;
  CollatorRef mCollator;
  void *mBuffer; // temporary buffer to generate collation keys
  PRUint32 mBufferLen; // byte length of buffer
};

#endif  /* nsCollationMacUC_h_ */
