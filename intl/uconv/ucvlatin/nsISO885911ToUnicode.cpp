/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsUCConstructors.h"
#include "nsISO885911ToUnicode.h"

nsresult
nsISO885911ToUnicodeConstructor(nsISupports *aOuter, REFNSIID aIID,
                            void **aResult)
{
  // Just make it an alias to CP874 decoder. (bug 127755)
  return nsCP874ToUnicodeConstructor(aOuter, aIID, aResult);
}

