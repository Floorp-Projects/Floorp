/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef nsIUnicodeDecodeHelper_h___
#define nsIUnicodeDecodeHelper_h___

#include "nscore.h"
#include "uconvutil.h"
#include "nsISupports.h"
#include "nsIMappingCache.h"

// Interface ID for our Unicode Decode Helper interface
// {9CC39FF0-DD5D-11d2-8AAC-00600811A836}
#define NS_IUNICODEDECODEHELPER_IID \
{ 0x9cc39ff0, 0xdd5d, 0x11d2, {0x8a, 0xac, 0x0, 0x60, 0x8, 0x11, 0xa8, 0x36}}

// Class ID for our UnicodeDecoderHelper implementation
// {9CC39FF1-DD5D-11d2-8AAC-00600811A836}
#define NS_UNICODEDECODEHELPER_CID \
{  0x9cc39ff1, 0xdd5d, 0x11d2, {0x8a, 0xac, 0x0, 0x60, 0x8, 0x11, 0xa8, 0x36}}

#define NS_UNICODEDECODEHELPER_CONTRACTID "@mozilla.org/intl/unicode/decodehelper;1"

#define NS_ERROR_UDEC_NOHELPER  \
  NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_UCONV, 0x41)

//----------------------------------------------------------------------
// Class nsIUnicodeDecodeHelper [declaration]

/**
 * Interface for a Unicode Decode Helper object.
 * 
 * @created         22/Feb/1999
 * @author  Catalin Rotaru [CATA]
 */
class nsIUnicodeDecodeHelper : public nsISupports
{
public:
   NS_DEFINE_STATIC_IID_ACCESSOR(NS_IUNICODEDECODEHELPER_IID)

  /**
   * Converts data using a lookup table.
   */
  NS_IMETHOD ConvertByTable(const char * aSrc, PRInt32 * aSrcLength, 
      PRUnichar * aDest, PRInt32 * aDestLength, uShiftTable * aShiftTable, 
      uMappingTable * aMappingTable) = 0;

  /**
   * Converts data using a set of lookup tables.
   */
  NS_IMETHOD ConvertByMultiTable(const char * aSrc, PRInt32 * aSrcLength, 
      PRUnichar * aDest, PRInt32 * aDestLength, PRInt32 aTableCount, 
      uRange * aRangeArray, uShiftTable ** aShiftTable, 
      uMappingTable ** aMappingTable) = 0;

  /**
   * Converts data using a fast lookup table.
   */
  NS_IMETHOD ConvertByFastTable(const char * aSrc, PRInt32 * aSrcLength, 
      PRUnichar * aDest, PRInt32 * aDestLength, PRUnichar * aFastTable, 
      PRInt32 aTableSize) = 0;

  /**
   * Create a Mapping Cache
   */

  NS_IMETHOD CreateCache(nsMappingCacheType aType, nsIMappingCache* aResult) = 0;
  /**
   * Destroy a Mapping Cache
   */

  NS_IMETHOD DestroyCache(nsIMappingCache aCache) = 0;


  /**
   * Create a cache-like fast lookup table from a normal one.
   */
  NS_IMETHOD CreateFastTable( uShiftTable * aShiftTable, 
      uMappingTable * aMappingTable, PRUnichar * aFastTable, 
      PRInt32 aTableSize) = 0;
};

#endif /* nsIUnicodeDecodeHelper_h___ */
