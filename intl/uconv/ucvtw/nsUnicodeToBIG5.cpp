/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsUnicodeToBIG5.h"
#include "nsUCvTWDll.h"
#include "nsUCConstructors.h"

//----------------------------------------------------------------------
// Global functions and data [declaration]


static const uint16_t *g_Big5MappingTable[2] = {
  g_ASCIIMappingTable,
  g_ufBig5Mapping
};

static const uScanClassID g_Big5ScanClassIDs[2] =  {
  u1ByteCharset,
  u2BytesCharset
};

//----------------------------------------------------------------------
// Class nsUnicodeToBIG5 [implementation]

nsresult
nsUnicodeToBIG5Constructor(nsISupports *aOuter, REFNSIID aIID,
                                void **aResult)
{

  return CreateMultiTableEncoder(2,
                                 (uScanClassID*) &g_Big5ScanClassIDs,
                                 (uMappingTable**) &g_Big5MappingTable,
                                 2 /* max length = src * 2 */,
                                 aOuter, aIID, aResult);
}

