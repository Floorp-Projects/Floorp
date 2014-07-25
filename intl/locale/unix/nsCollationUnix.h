
/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef nsCollationUnix_h__
#define nsCollationUnix_h__


#include "nsICollation.h"
#include "nsCollation.h"  // static library
#include "plstr.h"
#include "mozilla/Attributes.h"
#include "nsString.h"



class nsCollationUnix MOZ_FINAL : public nsICollation {

protected:
  nsCollation   *mCollation;
  nsCString     mLocale;
  nsCString     mSavedLocale;
  bool          mUseCodePointOrder;

  void DoSetLocale();
  void DoRestoreLocale();

  ~nsCollationUnix(); 

public:
  nsCollationUnix();

  // nsISupports interface
  NS_DECL_ISUPPORTS

  // nsICollation interface
  NS_DECL_NSICOLLATION

};

#endif  /* nsCollationUnix_h__ */
