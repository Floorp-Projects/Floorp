/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _NSTHEBESFONTENUMERATOR_H_
#define _NSTHEBESFONTENUMERATOR_H_

#include "mozilla/Attributes.h"         // for MOZ_FINAL
#include "nsIFontEnumerator.h"          // for NS_DECL_NSIFONTENUMERATOR, etc
#include "nsISupports.h"                // for NS_DECL_ISUPPORTS

class nsThebesFontEnumerator MOZ_FINAL : public nsIFontEnumerator
{
    ~nsThebesFontEnumerator() {}
public:
    nsThebesFontEnumerator();

    NS_DECL_ISUPPORTS

    NS_DECL_NSIFONTENUMERATOR
};

#endif /* _NSTHEBESFONTENUMERATOR_H_ */
