/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsCollation_h_
#define nsCollation_h_

#include "mozilla/Attributes.h"
#include "nsICollation.h"
#include "nsCollationFactory.h"
#include "nsString.h"

#ifdef ENABLE_INTL_API
#include "unicode/ucol.h"
#endif

class nsCollation final : public nsICollation {

public:
  nsCollation();

  // nsISupports interface
  NS_DECL_ISUPPORTS

  // nsICollation interface
  NS_DECL_NSICOLLATION

protected:
  ~nsCollation();

#ifdef ENABLE_INTL_API
  nsresult ConvertStrength(const int32_t aStrength,
                           UCollationStrength* aStrengthOut,
                           UColAttributeValue* aCaseLevelOut);
  nsresult EnsureCollator(const int32_t newStrength);
  nsresult CleanUpCollator(void);

private:
  bool mInit;
  bool mHasCollator;
  nsCString mLocale;
  int32_t mLastStrength;
  UCollator* mCollatorICU;
#endif
};

#endif  /* nsCollation_h_ */
