/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __nsUCConstructors_h
#define __nsUCConstructors_h

#include "nsIUnicodeEncoder.h"
#include "nsIUnicodeDecoder.h"
#include "uconvutil.h"

// all the useful constructors
NS_METHOD
CreateMultiTableDecoder(PRInt32 aTableCount,
                        const uRange * aRangeArray, 
                        uScanClassID * aScanClassArray,
                        uMappingTable ** aMappingTable,
                        PRUint32 aMaxLengthFactor,
                        
                        nsISupports* aOuter,
                        REFNSIID aIID,
                        void** aResult);

NS_METHOD
CreateMultiTableEncoder(PRInt32 aTableCount,
                        uScanClassID * aScanClassArray,
                        uShiftOutTable ** aShiftOutTable,
                        uMappingTable  ** aMappingTable,
                        PRUint32 aMaxLengthFactor,
                        nsISupports* aOuter,
                        REFNSIID aIID,
                        void** aResult);

NS_METHOD
CreateTableEncoder(uScanClassID aScanClass,
                   uShiftOutTable * aShiftOutTable,
                   uMappingTable  * aMappingTable,
                   PRUint32 aMaxLengthFactor,
                   nsISupports* aOuter,
                   REFNSIID aIID,
                   void** aResult);

NS_METHOD
CreateMultiTableEncoder(PRInt32 aTableCount,
                        uScanClassID * aScanClassArray,
                        uMappingTable  ** aMappingTable,
                        PRUint32 aMaxLengthFactor,
                        nsISupports* aOuter,
                        REFNSIID aIID,
                        void** aResult);

NS_METHOD
CreateTableEncoder(uScanClassID aScanClass,
                   uMappingTable  * aMappingTable,
                   PRUint32 aMaxLengthFactor,
                   nsISupports* aOuter,
                   REFNSIID aIID,
                   void** aResult);

NS_METHOD
CreateTableDecoder(uScanClassID aScanClass,
                   uShiftInTable * aShiftInTable,
                   uMappingTable * aMappingTable,
                   PRUint32 aMaxLengthFactor,
                   nsISupports* aOuter,
                   REFNSIID aIID,
                   void** aResult);

NS_METHOD
CreateOneByteDecoder(uMappingTable * aMappingTable,
                     nsISupports* aOuter,
                     REFNSIID aIID,
                     void** aResult);

                   
#endif
