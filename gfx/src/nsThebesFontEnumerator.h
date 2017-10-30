/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _NSTHEBESFONTENUMERATOR_H_
#define _NSTHEBESFONTENUMERATOR_H_

#include "mozilla/Attributes.h"         // for final
#include "nsIFontEnumerator.h"          // for NS_DECL_NSIFONTENUMERATOR, etc
#include "nsISupports.h"                // for NS_DECL_ISUPPORTS

class nsThebesFontEnumerator final : public nsIFontEnumerator
{
    ~nsThebesFontEnumerator() {}
public:
    nsThebesFontEnumerator();

    NS_DECL_ISUPPORTS

    NS_DECL_NSIFONTENUMERATOR
};

#endif /* _NSTHEBESFONTENUMERATOR_H_ */
