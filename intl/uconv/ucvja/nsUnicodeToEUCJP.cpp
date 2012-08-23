/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsUnicodeToEUCJP.h"
#include "nsUCVJADll.h"
#include "nsUCConstructors.h"

//----------------------------------------------------------------------
// Global functions and data [declaration]

// Shift Table
static const int16_t g0201ShiftOutTable[] =  {
        2,
        ShiftOutCell(u1ByteChar,         1, 0x00, 0x00, 0x00, 0x7F),
        ShiftOutCell(u1BytePrefix8EChar, 2, 0x00, 0xA1, 0x00, 0xDF)
};

#define SIZE_OF_TABLES 3
static const uScanClassID gScanClassIDs[SIZE_OF_TABLES] = {
  u2BytesGRCharset,
  u2BytesGRCharset,
  uMultibytesCharset
};

static const int16_t *gShiftTables[SIZE_OF_TABLES] =  {
    0,
    0,
    g0201ShiftOutTable
};

static const uint16_t *gMappingTables[SIZE_OF_TABLES] = {
    g_uf0208Mapping,
    g_uf0208extMapping,
    g_uf0201Mapping
};

nsresult
nsUnicodeToEUCJPConstructor(nsISupports *aOuter, REFNSIID aIID,
                            void **aResult)
{
    return CreateMultiTableEncoder(SIZE_OF_TABLES,
                                   (uScanClassID*) gScanClassIDs,
                                   (uShiftOutTable**) gShiftTables, 
                                   (uMappingTable**) gMappingTables,
                                   3 /* max length = src * 3 */,
                                   aOuter, aIID, aResult);
}

