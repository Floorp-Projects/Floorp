/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsUnicodeToBIG5HKSCS_h___
#define nsUnicodeToBIG5HKSCS_h___

#include "nsISupports.h"

/**
 * A character set converter from Unicode to BIG5-HKSCS.
 *
 * @created         02/Jul/2000
 * @author  Gavin Ho, Hong Kong Professional Services, Compaq Computer (Hong Kong) Ltd.
 */
nsresult
nsUnicodeToBIG5HKSCSConstructor(nsISupports *aOuter, REFNSIID aIID,
                                void **aResult);

#endif /* nsUnicodeToBIG5HKSCS_h___ */
