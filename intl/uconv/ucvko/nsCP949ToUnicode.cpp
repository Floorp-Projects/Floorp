/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsCP949ToUnicode.h"
#include "nsUCvKODll.h"
#include "nsUCConstructors.h"

//----------------------------------------------------------------------
// Global functions and data [declaration]

static const uScanClassID g_CP949ScanClassIDs[] = {
  u1ByteCharset,
// This is necessary to decode 8byte sequence representation of Hangul
// syllables. This representation is uniq to EUC-KR and is not used
// in CP949. However, this converter is for both EUC-KR and CP949
// so that this class ID is put here. See bug 131388. 
  uDecomposedHangulCharset,  
  u2BytesGRCharset,       // EUC_KR
  u2BytesGR128Charset,    // CP949 High
  u2BytesCharset          // CP949 Low
};

// CP949(non-EUC-KR portion) to Unicode
static const PRUint16 g_utCP949NoKSCHangulMapping[] = {
#include "u20cp949hangul.ut"
};

static const uRange g_CP949Ranges[] = {
  { 0x00, 0x7E },
  { 0xA4, 0xA4 },   // 8byte seq. for Hangul syllables not available
                    // in pre-composed form in KS X 1001
  { 0xA1, 0xFE },
  { 0xA1, 0xC6 },   // CP949 extension B. ends at 0xC6.
  { 0x80, 0xA0 }
};

static const PRUint16 *g_CP949MappingTableSet [] ={
  g_ucvko_AsciiMapping,
  g_HangulNullMapping,
  g_utKSC5601Mapping,
  g_utCP949NoKSCHangulMapping,
  g_utCP949NoKSCHangulMapping
//g_CP949HighMapping,
//g_CP949LowMapping
};


nsresult
nsCP949ToUnicodeConstructor(nsISupports *aOuter, REFNSIID aIID,
                            void **aResult)
{
  return CreateMultiTableDecoder(sizeof(g_CP949Ranges) / sizeof(g_CP949Ranges[0]),
                                 (const uRange*) &g_CP949Ranges,
                                 (uScanClassID*) &g_CP949ScanClassIDs,
                                 (uMappingTable**) &g_CP949MappingTableSet, 1,
                                 aOuter, aIID, aResult);
}

