/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DDLogMessage.h"

#include "DDLifetimes.h"

namespace mozilla {

nsCString
DDLogMessage::Print() const
{
  nsCString str;
  str.AppendPrintf("%" PRImi " | %f | %s[%p] | %s | %s | ",
                   mIndex.Value(),
                   ToSeconds(mTimeStamp),
                   mObject.TypeName(),
                   mObject.Pointer(),
                   ToShortString(mClass),
                   mLabel);
  AppendToString(mValue, str);
  return str;
}

nsCString
DDLogMessage::Print(const DDLifetimes& aLifetimes) const
{
  nsCString str;
  const DDLifetime* lifetime = aLifetimes.FindLifetime(mObject, mIndex);
  str.AppendPrintf("%" PRImi " | %f | ", mIndex.Value(), ToSeconds(mTimeStamp));
  lifetime->AppendPrintf(str);
  str.AppendPrintf(" | %s | %s | ", ToShortString(mClass), mLabel);
  if (!mValue.is<DDLogObject>()) {
    AppendToString(mValue, str);
  } else {
    const DDLifetime* lifetime2 =
      aLifetimes.FindLifetime(mValue.as<DDLogObject>(), mIndex);
    if (lifetime2) {
      lifetime2->AppendPrintf(str);
    } else {
      AppendToString(mValue, str);
    }
  }
  return str;
}

} // namespace mozilla
