/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef _nscollationos2_h_
#define _nscollationos2_h_


#include "nsICollation.h"
#include "nsCollation.h"  // static library
#include "nsCRT.h"



class nsCollationOS2 : public nsICollation {

protected:
  nsCollation   *mCollation;
  nsString      mLocale;
  nsString      mSavedLocale;

public:
  nsCollationOS2();
  ~nsCollationOS2();

  // nsISupports interface
  NS_DECL_ISUPPORTS

  // nsICollation interface
  NS_DECL_NSICOLLATION

};

#endif /* nsCollationOS2_h__ */ 
