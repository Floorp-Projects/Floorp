/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __nsUCConstructors_h
#define __nsUCConstructors_h

#include <stdint.h>
#include "nscore.h"
#include "nsID.h"
#include "uconvutil.h"

class nsISupports;

// all the useful constructors
nsresult
CreateMultiTableDecoder(int32_t aTableCount,
                        const uRange * aRangeArray,
                        uScanClassID * aScanClassArray,
                        uMappingTable ** aMappingTable,
                        uint32_t aMaxLengthFactor,
                        nsISupports* aOuter,
                        REFNSIID aIID,
                        void** aResult);

nsresult
CreateMultiTableEncoder(int32_t aTableCount,
                        uScanClassID * aScanClassArray,
                        uShiftOutTable ** aShiftOutTable,
                        uMappingTable  ** aMappingTable,
                        uint32_t aMaxLengthFactor,
                        nsISupports* aOuter,
                        REFNSIID aIID,
                        void** aResult);

nsresult
CreateTableEncoder(uScanClassID aScanClass,
                   uShiftOutTable * aShiftOutTable,
                   uMappingTable  * aMappingTable,
                   uint32_t aMaxLengthFactor,
                   nsISupports* aOuter,
                   REFNSIID aIID,
                   void** aResult);

nsresult
CreateMultiTableEncoder(int32_t aTableCount,
                        uScanClassID * aScanClassArray,
                        uMappingTable  ** aMappingTable,
                        uint32_t aMaxLengthFactor,
                        nsISupports* aOuter,
                        REFNSIID aIID,
                        void** aResult);

nsresult
CreateTableEncoder(uScanClassID aScanClass,
                   uMappingTable  * aMappingTable,
                   uint32_t aMaxLengthFactor,
                   nsISupports* aOuter,
                   REFNSIID aIID,
                   void** aResult);

nsresult
CreateOneByteDecoder(uMappingTable * aMappingTable,
                     nsISupports* aOuter,
                     REFNSIID aIID,
                     void** aResult);

                   
#endif
