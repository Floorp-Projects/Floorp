/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsSemanticUnitScanner_h__
#define nsSemanticUnitScanner_h__

#include "nsSampleWordBreaker.h"
#include "nsISemanticUnitScanner.h"


class nsSemanticUnitScanner : public nsISemanticUnitScanner
                            , public nsSampleWordBreaker
{
public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSISEMANTICUNITSCANNER

  nsSemanticUnitScanner();

private:
  virtual ~nsSemanticUnitScanner();
  /* additional members */
};

#endif
