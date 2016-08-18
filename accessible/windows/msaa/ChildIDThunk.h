/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_a11y_ChildIDThunk_h
#define mozilla_a11y_ChildIDThunk_h

#include "mozilla/mscom/Interceptor.h"
#include "mozilla/Maybe.h"
#include "mozilla/RefPtr.h"

#include <oleacc.h>
#include <callobj.h>

namespace mozilla {
namespace a11y {

class ChildIDThunk : public mozilla::mscom::IInterceptorSink
{
public:
  ChildIDThunk();

  // IUnknown
  STDMETHODIMP QueryInterface(REFIID riid, void** ppv) override;
  STDMETHODIMP_(ULONG) AddRef() override;
  STDMETHODIMP_(ULONG) Release() override;

  // ICallFrameEvents
  STDMETHODIMP OnCall(ICallFrame* aFrame) override;

  // IInterceptorSink
  STDMETHODIMP SetInterceptor(mozilla::mscom::IWeakReference* aInterceptor) override;

private:
  HRESULT ResolveTargetInterface(REFIID aIid, IUnknown** aOut);

  static mozilla::Maybe<ULONG> IsMethodInteresting(const ULONG aMethodIdx);
  static bool IsChildIDSelf(ICallFrame* aFrame, const ULONG aChildIdIdx,
                            LONG& aOutChildID);
  static HRESULT SetChildIDParam(ICallFrame* aFrame, const ULONG aParamIdx,
                                 const LONG aChildID);

private:
  ULONG mRefCnt;
  RefPtr<mozilla::mscom::IWeakReference> mInterceptor;
};

} // namespace a11y
} // namespace mozilla

#endif // mozilla_a11y_ChildIDThunk_h

