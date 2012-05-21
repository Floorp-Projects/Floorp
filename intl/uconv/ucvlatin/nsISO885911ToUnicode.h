/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


#ifndef nsISO885911ToUnicode_h___
#define nsISO885911ToUnicode_h___

#include "nsCP874ToUnicode.h"

// Just make it an alias to CP874 decoder. (bug 127755)
nsresult
nsISO885911ToUnicodeConstructor(nsISupports *aOuter, REFNSIID aIID,
                                void **aResult);

#endif /* nsISO885911ToUnicode_h___ */

