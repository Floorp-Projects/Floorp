/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsEUCKRToUnicode.h"
#include "nsUCvKODll.h"
#include "nsUCConstructors.h"

nsresult
nsEUCKRToUnicodeConstructor(nsISupports *aOuter, REFNSIID aIID,
                            void **aResult)
{
  return nsCP949ToUnicodeConstructor(aOuter, aIID, aResult);
}
