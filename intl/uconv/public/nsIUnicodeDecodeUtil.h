/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are Copyright (C) 1998
 * Netscape Communications Corporation.  All Rights Reserved.
 */
#ifndef nsIUnicodeDecodeUtil_h__
#define nsIUnicodeDecodeUtil_h__


#include "nscore.h"
#include "nsISupports.h"
#include "uconvutil.h"

NS_DECLARE_ID ( kIUnicodeDecodeUtilIID,
  0x8a2f2851, 0xa98d, 0x11d2, 0xb3, 0xae, 0x0, 0x80, 0x5f, 0x8a, 0x66, 0x70 ) ;


class nsIUnicodeDecodeUtil : public nsISupports {

public:
    NS_IMETHOD ConvertBy1Table(
      PRUnichar   *aDest,
      PRInt32    aDestOffset,
      PRInt32    *aDestLength,
      const char  *aSrc,
      PRInt32    aSrcOffset,
      PRInt32    *aSrcLength,
      uShiftTable *aShiftTable,
      uMappingTable   *aMappingTable
   ) = 0;

   NS_IMETHOD ConvertByNTable(
      PRUnichar  *aDest,
      PRInt32   aDestOffset,
      PRInt32   *aDestLength,
      const char *aSrc,
      PRInt32   aSrcOffset,
      PRInt32   *aSrcLength,
      PRUint16   numOfTable,
      uRange     *aRangeArray,
      uShiftTable *aShiftTableArray,
      uMappingTable   *aMappingTableArray
   ) = 0;
};
#endif
