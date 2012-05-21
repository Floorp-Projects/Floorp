/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsUnicodeToHKSCS.h"
#include "nsUCvTWDll.h"
#include "nsUCConstructors.h"

//----------------------------------------------------------------------
// Global functions and data [declaration]



static const PRUint16 *g_Big5HKSCSMappingTable[] = {
  g_ufBig5HKSCSMapping
};

static const uScanClassID g_Big5HKSCSScanClassIDs[] =  {
  u2BytesCharset
};

//----------------------------------------------------------------------
// Class nsUnicodeToHKSCS [implementation]

nsresult
nsUnicodeToHKSCSConstructor(nsISupports *aOuter, REFNSIID aIID,
                                void **aResult)
{
  return CreateMultiTableEncoder(1,
                                 (uScanClassID*) &g_Big5HKSCSScanClassIDs,
                                 (uMappingTable**) &g_Big5HKSCSMappingTable,
                                 2 /* max length = src * 2 */,
                                 aOuter, aIID, aResult);
}

