/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ----- BEGIN LICENSE BLOCK -----
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications Corporation.
 * Portions created by Netscape Communications Corporation are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the LGPL or the GPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ----- END LICENSE BLOCK ----- */

#ifndef nsIUnicodeEncodeHelper_h___
#define nsIUnicodeEncodeHelper_h___

#include "nscore.h"
#include "uconvutil.h"
#include "nsISupports.h"
#include "nsIMappingCache.h"

// Interface ID for our Unicode Encode Helper interface
// {D8E6B700-CA9D-11d2-8AA9-00600811A836}
#define NS_IUNICODEENCODEHELPER_IID \
{ 0xd8e6b700, 0xca9d, 0x11d2, {0x8a, 0xa9, 0x0, 0x60, 0x8, 0x11, 0xa8, 0x36}}

// Class ID for our UnicodeEncoderHelper implementation
// {1767FC50-CAA4-11d2-8AA9-00600811A836}
#define NS_UNICODEENCODEHELPER_CID \
{ 0x1767fc50, 0xcaa4, 0x11d2, {0x8a, 0xa9, 0x0, 0x60, 0x8, 0x11, 0xa8, 0x36}}


#define NS_UNICODEENCODEHELPER_CONTRACTID "@mozilla.org/intl/unicode/encodehelper;1"

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

  NS_DEFINE_STATIC_IID_ACCESSOR(NS_IUNICODEENCODEHELPER_IID)

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

  /**
   * Create a Mapping Cache
   */
  NS_IMETHOD CreateCache(nsMappingCacheType aType, nsIMappingCache* aResult) = 0;

  /**
   * Destroy a Mapping Cache
   */
  NS_IMETHOD DestroyCache(nsIMappingCache aCache) = 0;
  
  /**
   * Create Char Representable Info
   */
  NS_IMETHOD FillInfo(PRUint32* aInfo, uMappingTable  * aMappingTable) = 0;
  NS_IMETHOD FillInfo(PRUint32* aInfo, PRInt32 aTableCount, uMappingTable  ** aMappingTable) = 0;
};



#endif /* nsIUnicodeEncodeHelper_h___ */
