/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsCollationMacUC_h_
#define nsCollationMacUC_h_

#include "nsICollation.h"
#include "nsCollation.h"
#include "mozilla/Attributes.h"
#include <Carbon/Carbon.h>

// Maximum number of characters for a buffer to remember 
// the generated collation key.
const uint32_t kCacheSize = 128;
// According to the documentation, the length of the key should typically be
// at least 5 * textLength, but 6* would be safer.
const uint32_t kCollationValueSizeFactor = 6;

class nsCollationMacUC MOZ_FINAL : public nsICollation {

public: 
  nsCollationMacUC();

  // nsISupports interface
  NS_DECL_ISUPPORTS

  // nsICollation interface
  NS_DECL_NSICOLLATION

protected:
  ~nsCollationMacUC(); 

  nsresult ConvertLocale(nsILocale* aNSLocale, LocaleRef* aMacLocale);
  nsresult StrengthToOptions(const int32_t aStrength,
                             UCCollateOptions* aOptions);
  nsresult EnsureCollator(const int32_t newStrength);

private:
  bool mInit;
  bool mHasCollator;
  LocaleRef mLocale;
  int32_t mLastStrength;
  CollatorRef mCollator;
  void *mBuffer; // temporary buffer to generate collation keys
  uint32_t mBufferLen; // byte length of buffer
};

#endif  /* nsCollationMacUC_h_ */
