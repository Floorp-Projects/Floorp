/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */



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
                        uScanClassID * aScanClassArray,
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
                                   aScanClassArray, aMappingTable,
                                   aMaxLengthFactor);
  if (!decoder)
    return NS_ERROR_OUT_OF_MEMORY;

  return StabilizedQueryInterface(decoder, aIID, aResult);
}

NS_METHOD
CreateMultiTableEncoder(PRInt32 aTableCount,
                        uScanClassID * aScanClassArray,
                        uShiftOutTable ** aShiftOutTable,
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
                                   aScanClassArray,
                                   aShiftOutTable,
                                   aMappingTable,
                                   aMaxLengthFactor);
  if (!encoder)
    return NS_ERROR_OUT_OF_MEMORY;

  return StabilizedQueryInterface(encoder, aIID, aResult);
}

NS_METHOD
CreateMultiTableEncoder(PRInt32 aTableCount,
                        uScanClassID * aScanClassArray,
                        uMappingTable ** aMappingTable,
                        PRUint32 aMaxLengthFactor,
                        nsISupports* aOuter,
                        REFNSIID aIID,
                        void** aResult)
{
  return CreateMultiTableEncoder(aTableCount, aScanClassArray,
                                 nullptr,
                                 aMappingTable, aMaxLengthFactor,
                                 aOuter, aIID, aResult);
}

NS_METHOD
CreateTableEncoder(uScanClassID aScanClass,
                   uShiftOutTable * aShiftOutTable,
                   uMappingTable  * aMappingTable,
                   PRUint32 aMaxLengthFactor,
                   nsISupports* aOuter,
                   REFNSIID aIID,
                   void** aResult)
{
  if (aOuter)
    return NS_ERROR_NO_AGGREGATION;
  
  nsTableEncoderSupport* encoder =
      new nsTableEncoderSupport(aScanClass,
                                aShiftOutTable,  aMappingTable,
                                aMaxLengthFactor);
  if (!encoder)
    return NS_ERROR_OUT_OF_MEMORY;

  return StabilizedQueryInterface(encoder, aIID, aResult);
}

NS_METHOD
CreateTableEncoder(uScanClassID aScanClass,
                   uMappingTable  * aMappingTable,
                   PRUint32 aMaxLengthFactor,
                   nsISupports* aOuter,
                   REFNSIID aIID,
                   void** aResult)
{
    return CreateTableEncoder(aScanClass, nullptr,
                              aMappingTable, aMaxLengthFactor,
                              aOuter, aIID, aResult);
}

NS_METHOD
CreateTableDecoder(uScanClassID aScanClass,
                   uShiftInTable * aShiftInTable,
                   uMappingTable  * aMappingTable,
                   PRUint32 aMaxLengthFactor,
                   nsISupports* aOuter,
                   REFNSIID aIID,
                   void** aResult)
{
  if (aOuter)
    return NS_ERROR_NO_AGGREGATION;
  
  nsTableDecoderSupport* decoder =
      new nsTableDecoderSupport(aScanClass, aShiftInTable, aMappingTable,
                                aMaxLengthFactor);
  if (!decoder)
    return NS_ERROR_OUT_OF_MEMORY;

  return StabilizedQueryInterface(decoder, aIID, aResult);
}

NS_METHOD
CreateOneByteDecoder(uMappingTable * aMappingTable,
                     
                     nsISupports* aOuter,
                     REFNSIID aIID,
                     void** aResult)
{
    if (aOuter) return NS_ERROR_NO_AGGREGATION;
    
    nsOneByteDecoderSupport* decoder =
        new nsOneByteDecoderSupport(aMappingTable);

    if (!decoder)
        return NS_ERROR_OUT_OF_MEMORY;
    
    return StabilizedQueryInterface(decoder, aIID, aResult);
}
