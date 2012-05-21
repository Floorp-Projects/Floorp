/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsCharsetAlias_h___
#define nsCharsetAlias_h___

#include "nscore.h"
#include "nsStringGlue.h"

class nsCharsetAlias
{
public:
   static nsresult GetPreferred(const nsACString& aAlias, nsACString& aResult);
   static nsresult Equals(const nsACString& aCharset1, const nsACString& aCharset2, bool* aResult);
};

#endif /* nsCharsetAlias_h___ */
