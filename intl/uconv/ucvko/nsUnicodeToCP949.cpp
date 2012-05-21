/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


#include "nsUnicodeToCP949.h"
#include "nsUCvKODll.h"
#include "nsUCConstructors.h"

//----------------------------------------------------------------------
// Global functions and data [declaration]


// Unicode Hangul syllables (not enumerated in KS X 1001) to CP949 : 8822 of them
static const PRUint16 g_ufCP949NoKSCHangulMapping[] = {
#include "u20cp949hangul.uf"
};



static const PRUint16 *g_CP949MappingTable[3] = {
  g_ucvko_AsciiMapping,
  g_ufKSC5601Mapping,
  g_ufCP949NoKSCHangulMapping
};

static const uScanClassID g_CP949ScanClassTable[3] =  {
  u1ByteCharset,
  u2BytesGRCharset,
  u2BytesCharset
};

nsresult
nsUnicodeToCP949Constructor(nsISupports *aOuter, REFNSIID aIID,
                            void **aResult)
{
  return CreateMultiTableEncoder(3,
                                 (uScanClassID*) g_CP949ScanClassTable, 
                                 (uMappingTable**) g_CP949MappingTable,
                                 2 /* max len = src * 2 */,
                                 aOuter, aIID, aResult);
}

