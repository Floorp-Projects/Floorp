/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef InternetCiter_h
#define InternetCiter_h

#include "nscore.h"
#include "nsStringFwd.h"

namespace mozilla {

/**
 * Mail citations using standard Internet style.
 */
class InternetCiter final
{
public:
  static nsresult GetCiteString(const nsAString& aInString,
                                nsAString& aOutString);

  static nsresult Rewrap(const nsAString& aInString,
                         uint32_t aWrapCol, uint32_t aFirstLineOffset,
                         bool aRespectNewlines,
                         nsAString& aOutString);
};

} // namespace mozilla

#endif // #ifndef InternetCiter_h
