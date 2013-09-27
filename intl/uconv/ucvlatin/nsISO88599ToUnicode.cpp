/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsISO88599ToUnicode.h"
#include "nsCP1254ToUnicode.h"

nsresult
nsISO88599ToUnicodeConstructor(nsISupports *aOuter, REFNSIID aIID,
                               void **aResult) 
{
  // Just make it an alias to windows-1254 decoder. (bug 712876)
  return nsCP1254ToUnicodeConstructor(aOuter, aIID, aResult);
}
