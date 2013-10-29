/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsUnicodeToBIG5HKSCS.h"
#include "nsUCvTWDll.h"
#include "nsUCConstructors.h"

//----------------------------------------------------------------------
// Global functions and data [declaration]

nsresult
nsUnicodeToBIG5HKSCSConstructor(nsISupports *aOuter, REFNSIID aIID,
                                void **aResult)
{
    static const uint16_t *g_Big5HKSCSMappingTable[] = {
      g_ASCIIMappingTable,
      g_ufBig5Mapping,
      g_ufBig5HKSCSMapping
    };

    static const uScanClassID g_Big5HKSCSScanClassIDs[] =  {
      u1ByteCharset,
      u2BytesCharset,
      u2BytesCharset
    };

    return CreateMultiTableEncoder(3,
                                   (uScanClassID*) &g_Big5HKSCSScanClassIDs,
                                   (uMappingTable**) &g_Big5HKSCSMappingTable,
                                   2 /* max length = src * 2 */,
                                   aOuter, aIID, aResult);
}


