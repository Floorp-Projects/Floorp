/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsInternetCiter_h__
#define nsInternetCiter_h__

#include "nsString.h"

/** Mail citations using standard Internet style.
  */
class nsInternetCiter
{
public:
  static nsresult GetCiteString(const nsAString & aInString, nsAString & aOutString);

  static nsresult StripCites(const nsAString & aInString, nsAString & aOutString);

  static nsresult Rewrap(const nsAString & aInString,
                         PRUint32 aWrapCol, PRUint32 aFirstLineOffset,
                         bool aRespectNewlines,
                         nsAString & aOutString);

protected:
  static nsresult StripCitesAndLinebreaks(const nsAString& aInString, nsAString& aOutString,
                                          bool aLinebreaksToo, PRInt32* aCiteLevel);
};

#endif //nsInternetCiter_h__

