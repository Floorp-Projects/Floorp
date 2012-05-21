/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsUnicodeToAdobeEuro_h___
#define nsUnicodeToAdobeEuro_h___ 1

#include "nsISupports.h"

/**
 * A character set converter from Unicode to Adobe Euro 
 * (see http://bugzilla.mozilla.org/show_bug.cgi?id=158129 for details).
 *
 */
nsresult
nsUnicodeToAdobeEuroConstructor(nsISupports *aOuter, REFNSIID aIID,
                                void **aResult);

#endif /* !nsUnicodeToAdobeEuro_h___ */
