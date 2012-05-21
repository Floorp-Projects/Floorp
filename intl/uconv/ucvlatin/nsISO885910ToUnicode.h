/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsISO885910ToUnicode_h___
#define nsISO885910ToUnicode_h___

#include "nsISupports.h"

//----------------------------------------------------------------------
// Class nsISO885910ToUnicode [declaration]

/**
 * A character set converter from ISO885910 to Unicode.
 *
 * @created         23/Nov/1998
 * @author  Catalin Rotaru [CATA]
 */
nsresult
nsISO885910ToUnicodeConstructor(nsISupports *aOuter, REFNSIID aIID,
                                void **aResult);

#endif /* nsISO885910ToUnicode_h___ */
