/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is mozilla.org code.
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
#ifndef nsUnicodeEncodeHelper_h__
#define nsUnicodeEncodeHelper_h__

#include "nsIUnicodeEncoder.h"
#include "nsIUnicodeEncodeHelper.h"

//----------------------------------------------------------------------
// Class nsUnicodeEncodeHelper [declaration]

/**
 * The actual implementation of the nsIUnicodeEncodeHelper interface.
 *
 * @created         22/Nov/1998
 * @author  Catalin Rotaru [CATA]
 */
class nsUnicodeEncodeHelper : public nsIUnicodeEncodeHelper
{
  NS_DECL_ISUPPORTS

public:

  /**
   * Class constructor.
   */
  nsUnicodeEncodeHelper();

  /**
   * Class destructor.
   */
  virtual ~nsUnicodeEncodeHelper();

  //--------------------------------------------------------------------
  // Interface nsIUnicodeEncodeHelper [declaration]

  NS_IMETHOD ConvertByTable(const PRUnichar * aSrc, PRInt32 * aSrcLength, 
      char * aDest, PRInt32 * aDestLength, uShiftTable * aShiftTable, 
      uMappingTable  * aMappingTable);

  NS_IMETHOD ConvertByMultiTable(const PRUnichar * aSrc, PRInt32 * aSrcLength,
      char * aDest, PRInt32 * aDestLength, PRInt32 aTableCount, 
      uShiftTable ** aShiftTable, uMappingTable  ** aMappingTable);

  NS_IMETHOD CreateCache(nsMappingCacheType aType, nsIMappingCache* aResult);

  NS_IMETHOD DestroyCache(nsIMappingCache aCache);
 
  NS_IMETHOD FillInfo(PRUint32* aInfo, uMappingTable  * aMappingTable);
  NS_IMETHOD FillInfo(PRUint32* aInfo, PRInt32 aTableCount, uMappingTable  ** aMappingTable);
};

#endif // nsUnicodeEncodeHelper_h__


