/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef nsUnicodeEncodeHelper_h__
#define nsUnicodeEncodeHelper_h__

#include "nsIUnicodeEncoder.h"
#include "uconvutil.h"
//----------------------------------------------------------------------
// Class nsUnicodeEncodeHelper [declaration]

/**
 *
 * @created         22/Nov/1998
 * @author  Catalin Rotaru [CATA]
 */
class nsUnicodeEncodeHelper
{

public:
  //--------------------------------------------------------------------

  /**
   * Converts data using a lookup table and optional shift table.
   */
  static nsresult ConvertByTable(const PRUnichar * aSrc, PRInt32 * aSrcLength, 
      char * aDest, PRInt32 * aDestLength, uScanClassID aScanClass,
      uShiftOutTable * aShiftOutTable, uMappingTable  * aMappingTable);

  /**
   * Converts data using a set of lookup tables and optional shift tables.
   */
  static nsresult ConvertByMultiTable(const PRUnichar * aSrc, PRInt32 * aSrcLength,
      char * aDest, PRInt32 * aDestLength, PRInt32 aTableCount, 
      uScanClassID * aScanClassArray, 
      uShiftOutTable ** aShiftOutTable, uMappingTable  ** aMappingTable);
};

#endif // nsUnicodeEncodeHelper_h__


