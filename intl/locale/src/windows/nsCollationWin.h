
/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef nsCollationWin_h__
#define nsCollationWin_h__


#include "nsICollation.h"
#include "nsCollation.h"  // static library
#include "plstr.h"



class nsCollationWin MOZ_FINAL : public nsICollation {

protected:
  nsCollation   *mCollation;  // XP collation class
  uint32_t      mLCID;        // Windows platform locale ID

public: 
  nsCollationWin();
  ~nsCollationWin(); 

  // nsISupports interface
  NS_DECL_ISUPPORTS

  // nsICollation interface
  NS_DECL_NSICOLLATION

};

#endif  /* nsCollationWin_h__ */
