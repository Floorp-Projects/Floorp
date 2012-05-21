/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


#ifndef nsTIS620ToUnicode_h___
#define nsTIS620ToUnicode_h___

#include "nsCP874ToUnicode.h"

// Just make it an alias to CP874 decoder. (bug 127755)
nsresult
nsTIS620ToUnicodeConstructor(nsISupports *aOuter, REFNSIID aIID,
                                void **aResult);

#endif /* nsTIS620ToUnicode_h___ */

