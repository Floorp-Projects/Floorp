/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsISO88599ToUnicode_h___
#define nsISO88599ToUnicode_h___

#include "nsCP1254ToUnicode.h"

// Just make it an alias to windows-1254 decoder. (bug 712876)
nsresult
nsISO88599ToUnicodeConstructor(nsISupports *aOuter, REFNSIID aIID,
                               void **aResult);

#endif /* nsISO88599ToUnicode_h___ */
