/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsUnicodeToCP949_h___
#define nsUnicodeToCP949_h___

#include "nsISupports.h"

/**
 * A character set converter from Unicode to CP949.
 *
 * @created         14/May/2001  (patterned after Unicode to EUCKR converter
 * @author  Jungshik Shin 
 */
nsresult
nsUnicodeToCP949Constructor(nsISupports *aOuter, REFNSIID aIID,
                            void **aResult);

#endif /* nsUnicodeToCP949_h___ */
