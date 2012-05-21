/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * This Original Code has been modified by IBM Corporation.
 * Modifications made by IBM described herein are
 * Copyright (c) International Business Machines
 * Corporation, 1999
 *
 * Modifications to Mozilla code or documentation
 * identified per MPL Section 3.3
 *
 * Date         Modified by     Description of modification
 * 12/09/1999   IBM Corp.       Support for IBM codepages - 864i
 *
 */

#ifndef nsUnicodeToCP864i_h___
#define nsUnicodeToCP864i_h___

#include "nsISupports.h"

/**
 * A character set converter from Unicode to CP864i.
 */
nsresult
nsUnicodeToCP864iConstructor(nsISupports *aOuter, REFNSIID aIID,
                             void **aResult);

#endif /* nsUnicodeToCP864i_h___ */
