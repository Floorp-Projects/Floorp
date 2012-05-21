/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsUCConstructors.h"
#include "nsUnicodeToISO88596.h"
#include "nsUnicodeToISO88596E.h"

//----------------------------------------------------------------------
// Global functions and data [declaration]


nsresult
nsUnicodeToISO88596EConstructor(nsISupports *aOuter, REFNSIID aIID,
                                void **aResult) 
{
  return nsUnicodeToISO88596Constructor(aOuter, aIID, aResult);
}

