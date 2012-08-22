/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsUCConstructors.h"
#include "nsMacTurkishToUnicode.h"

//----------------------------------------------------------------------
// Global functions and data [declaration]

static const uint16_t g_MacTurkishMappingTable[] = {
#include "macturki.ut"
};

nsresult
nsMacTurkishToUnicodeConstructor(nsISupports *aOuter, REFNSIID aIID,
                                 void **aResult) 
{
  return CreateOneByteDecoder((uMappingTable*) &g_MacTurkishMappingTable,
                            aOuter, aIID, aResult);
}
