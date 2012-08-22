/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsUCConstructors.h"
#include "nsT61ToUnicode.h"

//----------------------------------------------------------------------
// Global functions and data [declaration]

static const uint16_t g_T61MappingTable[] = {
#include "t61.ut"
};


static const int16_t g_T61ShiftInTable[] =  {
    3,  
    ShiftInCell(u1ByteChar,   1, 0x00, 0xBF),
    ShiftInCell(u1ByteChar,   1, 0xD0, 0xFF),
    ShiftInCell(u2BytesChar,  2, 0xC0, 0xCF)
};

nsresult
nsT61ToUnicodeConstructor(nsISupports *aOuter, REFNSIID aIID,
                          void **aResult) 
{
  return CreateTableDecoder(uMultibytesCharset,
                            (uShiftInTable*) &g_T61ShiftInTable, 
                            (uMappingTable*) &g_T61MappingTable, 1,
                            aOuter, aIID, aResult);
}

