/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_mscom_InterceptorLog_h
#define mozilla_mscom_InterceptorLog_h

#include "mozilla/TimeStamp.h"

struct ICallFrame;
struct IUnknown;

namespace mozilla {
namespace mscom {

class InterceptorLog
{
public:
  static bool Init();
  static void QI(HRESULT aResult, IUnknown* aTarget, REFIID aIid,
                 IUnknown* aInterface,
                 const TimeDuration* aOverheadDuration = nullptr,
                 const TimeDuration* aGeckoDuration = nullptr);
  static void Event(ICallFrame* aCallFrame, IUnknown* aTarget,
                    const TimeDuration& aOverheadDuration,
                    const TimeDuration& aGeckoDuration);
};

} // namespace mscom
} // namespace mozilla

#endif // mozilla_mscom_InterceptorLog_h

