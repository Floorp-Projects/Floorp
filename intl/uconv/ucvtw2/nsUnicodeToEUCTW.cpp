/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsUnicodeToEUCTW.h"
#include "nsUCvTW2Dll.h"
#include "nsUCConstructors.h"

//----------------------------------------------------------------------
// Global functions and data [declaration]

static const uScanClassID g_EUCTWScanClassSet [] = {
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
  g_ufCNS1MappingTable,
  g_ufCNS2MappingTable,
  g_ufCNS3MappingTable,
  g_ufCNS4MappingTable,
  g_ufCNS5MappingTable,
  g_ufCNS6MappingTable,
  g_ufCNS7MappingTable
};

//----------------------------------------------------------------------
// Class nsUnicodeToEUCTW [implementation]

nsresult
nsUnicodeToEUCTWConstructor(nsISupports *aOuter, REFNSIID aIID,
                            void **aResult)
{
  return CreateMultiTableEncoder(8,
                                 (uScanClassID*) &g_EUCTWScanClassSet,
                                 (uMappingTable**) &g_EUCTWMappingTableSet,
                                 4 /* max length = src * 4 */,
                                 aOuter, aIID, aResult);
}

