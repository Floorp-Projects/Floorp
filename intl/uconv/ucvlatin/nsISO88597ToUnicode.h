/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsISO88597ToUnicode_h___
#define nsISO88597ToUnicode_h___

#include "nsISupports.h"

/**
 * A character set converter from ISO-8859-7 to Unicode.
 *
 * @created         23/Mar/1998
 * @author  Catalin Rotaru [CATA]
 */
nsresult
nsISO88597ToUnicodeConstructor(nsISupports *aOuter, REFNSIID aIID,
                               void **aResult);

#endif /* nsISO88597ToUnicode_h___ */
