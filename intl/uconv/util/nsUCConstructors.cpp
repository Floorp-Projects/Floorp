/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
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
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */



#include "nsUCSupport.h"
#include "nsUCConstructors.h"

template<class T>
inline NS_METHOD StabilizedQueryInterface(T* aNewObject,
                                         REFNSIID aIID,
                                         void **aResult)
{
    NS_ADDREF(aNewObject);
    nsresult rv = aNewObject->QueryInterface(aIID, aResult);
    NS_RELEASE(aNewObject);
    return rv;
}

NS_METHOD
CreateMultiTableDecoder(PRInt32 aTableCount, const uRange * aRangeArray, 
                        uShiftTable ** aShiftTable,
                        uMappingTable ** aMappingTable,
                        PRUint32 aMaxLengthFactor,
                        nsISupports* aOuter,
                        REFNSIID aIID,
                        void** aResult)
{

  if (aOuter)
    return NS_ERROR_NO_AGGREGATION;
  
  nsMultiTableDecoderSupport* decoder =
    new nsMultiTableDecoderSupport(aTableCount, aRangeArray,
                                   aShiftTable, aMappingTable,
                                   aMaxLengthFactor);
  if (!decoder)
    return NS_ERROR_OUT_OF_MEMORY;

  return StabilizedQueryInterface(decoder, aIID, aResult);
}

NS_METHOD
CreateMultiTableEncoder(PRInt32 aTableCount,
                        uShiftTable ** aShiftTable,
                        uMappingTable ** aMappingTable,
                        PRUint32 aMaxLengthFactor,
                        nsISupports* aOuter,
                        REFNSIID aIID,
                        void** aResult)
{

  if (aOuter)
    return NS_ERROR_NO_AGGREGATION;
  
  nsMultiTableEncoderSupport* encoder =
    new nsMultiTableEncoderSupport(aTableCount,
                                   aShiftTable, aMappingTable,
                                   aMaxLengthFactor);
  if (!encoder)
    return NS_ERROR_OUT_OF_MEMORY;

  return StabilizedQueryInterface(encoder, aIID, aResult);
}

NS_METHOD
CreateTableEncoder(uShiftTable * aShiftTable, 
                   uMappingTable  * aMappingTable,
                   PRUint32 aMaxLengthFactor,
                   nsISupports* aOuter,
                   REFNSIID aIID,
                   void** aResult)
{
  if (aOuter)
    return NS_ERROR_NO_AGGREGATION;
  
  nsTableEncoderSupport* encoder =
      new nsTableEncoderSupport(aShiftTable, aMappingTable,
                                aMaxLengthFactor);
  if (!encoder)
    return NS_ERROR_OUT_OF_MEMORY;

  return StabilizedQueryInterface(encoder, aIID, aResult);
}

NS_METHOD
CreateTableDecoder(uShiftTable * aShiftTable, 
                   uMappingTable  * aMappingTable,
                   PRUint32 aMaxLengthFactor,
                   nsISupports* aOuter,
                   REFNSIID aIID,
                   void** aResult)
{
  if (aOuter)
    return NS_ERROR_NO_AGGREGATION;
  
  nsTableDecoderSupport* decoder =
      new nsTableDecoderSupport(aShiftTable, aMappingTable,
                                aMaxLengthFactor);
  if (!decoder)
    return NS_ERROR_OUT_OF_MEMORY;

  return StabilizedQueryInterface(decoder, aIID, aResult);
}

NS_METHOD
CreateOneByteDecoder(uShiftTable * aShiftTable, 
                     uMappingTable * aMappingTable,
                     
                     nsISupports* aOuter,
                     REFNSIID aIID,
                     void** aResult)
{
    if (aOuter) return NS_ERROR_NO_AGGREGATION;
    
    nsOneByteDecoderSupport* decoder =
        new nsOneByteDecoderSupport(aShiftTable, aMappingTable);

    if (!decoder)
        return NS_ERROR_OUT_OF_MEMORY;
    
    return StabilizedQueryInterface(decoder, aIID, aResult);
}
