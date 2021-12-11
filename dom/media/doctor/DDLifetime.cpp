/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DDLifetime.h"

namespace mozilla {

void DDLifetime::AppendPrintf(nsCString& aString) const {
  if (!mDerivedObject.Pointer()) {
    mObject.AppendPrintf(aString);
    aString.AppendPrintf("#%" PRIi32, mTag);
  } else {
    mDerivedObject.AppendPrintf(aString);
    aString.AppendPrintf("#%" PRIi32 " (as ", mTag);
    if (mObject.Pointer() == mDerivedObject.Pointer()) {
      aString.Append(mObject.TypeName());
    } else {
      mObject.AppendPrintf(aString);
    }
    aString.Append(")");
  }
}

nsCString DDLifetime::Printf() const {
  nsCString s;
  AppendPrintf(s);
  return s;
}

}  // namespace mozilla
