/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsUCConstructors.h"
#include "nsISO88598ToUnicode.h"
#include "nsISO88598EToUnicode.h"

//----------------------------------------------------------------------
// Global functions and data [declaration]

nsresult
nsISO88598EToUnicodeConstructor(nsISupports *aOuter, REFNSIID aIID,
                                void **aResult) 
{
  return nsISO88598ToUnicodeConstructor(aOuter, aIID, aResult);
}

