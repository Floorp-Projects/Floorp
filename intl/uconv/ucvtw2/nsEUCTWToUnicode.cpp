/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsEUCTWToUnicode.h"
#include "nsUCvTW2Dll.h"
#include "nsUCConstructors.h"

//----------------------------------------------------------------------
// Global functions and data [declaration]
static const uScanClassID g_EUCTWScanClassIDs [] = {
  u1ByteCharset,
  u2BytesGRCharset,
  u2BytesGRPrefix8EA2Charset,
  u2BytesGRPrefix8EA3Charset,
  u2BytesGRPrefix8EA4Charset,
  u2BytesGRPrefix8EA5Charset,
  u2BytesGRPrefix8EA6Charset,
  u2BytesGRPrefix8EA7Charset
};

static const uint16_t *g_EUCTWMappingTableSet [] ={
  g_ASCIIMappingTable,
  g_utCNS1MappingTable,
  g_utCNS2MappingTable,
  g_utCNS3MappingTable,
  g_utCNS4MappingTable,
  g_utCNS5MappingTable,
  g_utCNS6MappingTable,
  g_utCNS7MappingTable
};

static const uRange g_EUCTWRanges[] = {
  { 0x00, 0x7E },
  { 0xA1, 0xFE },
  { 0x8E, 0x8E },
  { 0x8E, 0x8E },
  { 0x8E, 0x8E },
  { 0x8E, 0x8E },
  { 0x8E, 0x8E },
  { 0x8E, 0x8E }
};


//----------------------------------------------------------------------
// Class nsEUCTWToUnicode [implementation]

nsresult
nsEUCTWToUnicodeConstructor(nsISupports *aOuter, REFNSIID aIID,
                                void **aResult)
{
  return CreateMultiTableDecoder(8, 
                                 (const uRange*) &g_EUCTWRanges,
                                 (uScanClassID*) &g_EUCTWScanClassIDs,
                                 (uMappingTable**) &g_EUCTWMappingTableSet,
                                 1, aOuter, aIID, aResult);
}
