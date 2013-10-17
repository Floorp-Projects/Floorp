/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsUCConstructors.h"
#include "nsCP1131ToUnicode.h"

//----------------------------------------------------------------------
// Global functions and data [declaration]

//----------------------------------------------------------------------
// Class nsCP1131ToUnicode [implementation]

nsresult
nsCP1131ToUnicodeConstructor(nsISupports* aOuter, REFNSIID aIID,
                            void **aResult) 
{
  static const uint16_t g_utMappingTable[] = {
#include "cp1131.ut"
  };

  return CreateOneByteDecoder((uMappingTable*) &g_utMappingTable,
                              aOuter, aIID, aResult);
}
