/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsCollationMacUC_h_
#define nsCollationMacUC_h_

#include "nsICollation.h"
#include "nsCollation.h"
#include "mozilla/Attributes.h"

#include "unicode/ucol.h"

class nsCollationMacUC final : public nsICollation {

public:
  nsCollationMacUC();

  // nsISupports interface
  NS_DECL_ISUPPORTS

  // nsICollation interface
  NS_DECL_NSICOLLATION

protected:
  ~nsCollationMacUC();

  nsresult ConvertLocaleICU(nsILocale* aNSLocale, char** aICULocale);
  nsresult ConvertStrength(const int32_t aStrength,
                           UCollationStrength* aStrengthOut,
                           UColAttributeValue* aCaseLevelOut);
  nsresult EnsureCollator(const int32_t newStrength);
  nsresult CleanUpCollator(void);

private:
  bool mInit;
  bool mHasCollator;
  char* mLocaleICU;
  int32_t mLastStrength;
  UCollator* mCollatorICU;
};

#endif  /* nsCollationMacUC_h_ */
