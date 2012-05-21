/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsUnicodeToEUCKR.h"
#include "nsUCvKODll.h"
#include "nsUCConstructors.h"

//----------------------------------------------------------------------
// Global functions and data [declaration]


static const PRUint16 *g_EUCKRMappingTable[3] = {
  g_ucvko_AsciiMapping,
  g_ufKSC5601Mapping,
  g_HangulNullMapping
};

static const uScanClassID g_EUCKRScanCellIDTable[3] =  {
  u1ByteCharset,
  u2BytesGRCharset,
  uDecomposedHangulCharset
};

nsresult
nsUnicodeToEUCKRConstructor(nsISupports *aOuter, REFNSIID aIID,
                            void **aResult)
{
  return CreateMultiTableEncoder(3,
                                 (uScanClassID*) g_EUCKRScanCellIDTable, 
                                 (uMappingTable**) g_EUCKRMappingTable,
                                 // change from 2 to 8 because of composed jamo
                                 8 /* max length = src * 8 */,
                                 aOuter, aIID, aResult);
}

