/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsUCConstructors.h"
#include "nsUnicodeToSJIS.h"

//----------------------------------------------------------------------
// Global functions and data [declaration]

static const PRUint16 g_SJISMappingTable[] = {
#include "sjis.uf"
};

static const PRInt16 g_SJISShiftOutTable[] =  {
  4,
  ShiftOutCell(u1ByteChar,   1, 0x00, 0x00, 0x00, 0x7F),
  ShiftOutCell(u1ByteChar,   1, 0x00, 0xA1, 0x00, 0xDF),
  ShiftOutCell(u2BytesChar,  2, 0x81, 0x40, 0x9F, 0xFC),
  ShiftOutCell(u2BytesChar,  2, 0xE0, 0x40, 0xFC, 0xFC)
};

nsresult
nsUnicodeToSJISConstructor(nsISupports *aOuter, REFNSIID aIID,
                            void **aResult)
{
  return CreateTableEncoder(uMultibytesCharset,
                            (uShiftOutTable*) &g_SJISShiftOutTable, 
                            (uMappingTable*) &g_SJISMappingTable,
                            2 /* max length = src * 2 */,
                            aOuter, aIID, aResult);
}

