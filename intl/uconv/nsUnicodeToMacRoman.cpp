/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsUCConstructors.h"
#include "nsUnicodeToMacRoman.h"

//----------------------------------------------------------------------
// Global functions and data [declaration]

nsresult
nsUnicodeToMacRomanConstructor(nsISupports *aOuter, REFNSIID aIID,
                               void **aResult)
{
  static const uint16_t g_MacRomanMappingTable[] = {
#include "macroman.uf"
  };

  return CreateTableEncoder(u1ByteCharset,
                            (uMappingTable*) &g_MacRomanMappingTable, 1,
                            aOuter, aIID, aResult);
}

