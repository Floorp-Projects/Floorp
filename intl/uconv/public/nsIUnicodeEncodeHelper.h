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

#ifndef nsIUnicodeEncodeHelper_h___
#define nsIUnicodeEncodeHelper_h___

#include "nscore.h"
#include "uconvutil.h"
#include "nsISupports.h"

// Interface ID for our Unicode Encode Helper interface
// {D8E6B700-CA9D-11d2-8AA9-00600811A836}
NS_DECLARE_ID(kIUnicodeEncodeHelperIID,
  0xd8e6b700, 0xca9d, 0x11d2, 0x8a, 0xa9, 0x0, 0x60, 0x8, 0x11, 0xa8, 0x36);

// Class ID for our UnicodeEncoderHelper implementation
// {1767FC50-CAA4-11d2-8AA9-00600811A836}
NS_DECLARE_ID(kUnicodeEncodeHelperCID, 
  0x1767fc50, 0xcaa4, 0x11d2, 0x8a, 0xa9, 0x0, 0x60, 0x8, 0x11, 0xa8, 0x36);

#define NS_ERROR_UENC_NOHELPER  \
  NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_UCONV, 0x31)

//----------------------------------------------------------------------
// Class nsIUnicodeEncodeHelper [declaration]

/**
 * Interface for a Unicode Encode Helper object.
 * 
 * @created         22/Feb/1999
 * @author  Catalin Rotaru [CATA]
 */
class nsIUnicodeEncodeHelper : public nsISupports
{
public:

  /**
   * Converts data using a lookup table.
   */
  NS_IMETHOD ConvertByTable(const PRUnichar * aSrc, PRInt32 * aSrcLength, 
      char * aDest, PRInt32 * aDestLength, uShiftTable * aShiftTable, 
      uMappingTable  * aMappingTable) = 0;

  /**
   * Converts data using a set of lookup tables.
   */
  NS_IMETHOD ConvertByMultiTable(const PRUnichar * aSrc, PRInt32 * aSrcLength,
      char * aDest, PRInt32 * aDestLength, PRInt32 aTableCount, 
      uShiftTable ** aShiftTable, uMappingTable  ** aMappingTable) = 0;
};



#endif /* nsIUnicodeEncodeHelper_h___ */
