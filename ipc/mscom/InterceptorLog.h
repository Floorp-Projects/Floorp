/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_mscom_InterceptorLog_h
#define mozilla_mscom_InterceptorLog_h

struct ICallFrame;
struct IUnknown;

namespace mozilla {
namespace mscom {

class InterceptorLog
{
public:
  static bool Init();
  static void QI(HRESULT aResult, IUnknown* aTarget, REFIID aIid, IUnknown* aInterface);
  static void Event(ICallFrame* aCallFrame, IUnknown* aTarget);
};

} // namespace mscom
} // namespace mozilla

#endif // mozilla_mscom_InterceptorLog_h

