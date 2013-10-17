/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsUCConstructors.h"
#include "nsUnicodeToT61.h"

//----------------------------------------------------------------------
// Global functions and data [declaration]

nsresult
nsUnicodeToT61Constructor(nsISupports *aOuter, REFNSIID aIID,
                          void **aResult) 
{
  static const uint16_t g_T61MappingTable[] = {
#include "t61.uf"
  };

  static const int16_t g_T61ShiftOutTable[] =  {
      3,  
      ShiftOutCell(u1ByteChar,   1, 0x00, 0x00, 0x00, 0xBF),
      ShiftOutCell(u1ByteChar,   1, 0x00, 0xD0, 0x00, 0xFF),
      ShiftOutCell(u2BytesChar,  2, 0xC0, 0x41, 0xCF, 0x7A)
  };

  return CreateTableEncoder(uMultibytesCharset,
                            (uShiftOutTable*) &g_T61ShiftOutTable, 
                            (uMappingTable*) &g_T61MappingTable, 2,
                            aOuter, aIID, aResult);
}
