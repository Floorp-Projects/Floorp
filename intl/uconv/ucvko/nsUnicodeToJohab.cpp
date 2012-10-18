/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsUnicodeToJohab.h"
#include "nsUCvKODll.h"
#include "nsUCConstructors.h"

//----------------------------------------------------------------------
// Global functions and data [declaration]

static const uint16_t *g_JohabMappingTable[4] = {
  g_ASCIIMappingTable,
  g_HangulNullMapping,
  g_ufJohabJamoMapping,
  g_ufKSC5601Mapping
};

static const uScanClassID g_JohabScanClassTable[4] =  {
  u1ByteCharset,
  uJohabHangulCharset,
  u2BytesCharset,
  uJohabSymbolCharset
};

//----------------------------------------------------------------------
// Class nsUnicodeToJohab [implementation]

nsresult
nsUnicodeToJohabConstructor(nsISupports *aOuter, REFNSIID aIID,
                            void **aResult) {

  return CreateMultiTableEncoder(sizeof(g_JohabScanClassTable) / sizeof(g_JohabScanClassTable[0]),
                                 (uScanClassID*) g_JohabScanClassTable, 
                                 (uMappingTable**) g_JohabMappingTable,
                                 2 /* max length = src * 2*/,
                                 aOuter, aIID, aResult);
}
