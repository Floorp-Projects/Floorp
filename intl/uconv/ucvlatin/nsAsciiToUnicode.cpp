/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsUCConstructors.h"
#include "nsAsciiToUnicode.h"

//----------------------------------------------------------------------
// Global functions and data [declaration]

// use iso-8859-1 decoder to interpret us-ascii. Some websites are mistakenly 
// labeled as us-ascii for iso-8859-1. Be generous here should be fine. 

static const PRUint16 g_utMappingTable[] = {
#include "cp1252.ut"
};

nsresult
nsAsciiToUnicodeConstructor(nsISupports *aOuter, REFNSIID aIID,
                            void **aResult) 
{
  return CreateOneByteDecoder((uMappingTable*) &g_utMappingTable,
                              aOuter, aIID, aResult);
}
