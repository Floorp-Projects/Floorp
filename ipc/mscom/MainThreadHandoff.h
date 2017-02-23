/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_mscom_MainThreadHandoff_h
#define mozilla_mscom_MainThreadHandoff_h

#include "mozilla/Assertions.h"
#include "mozilla/Move.h"
#include "mozilla/mscom/Interceptor.h"
#include "mozilla/mscom/MainThreadInvoker.h"
#include "mozilla/mscom/Utils.h"
#include "mozilla/Mutex.h"
#include "nsTArray.h"

namespace mozilla {
namespace mscom {

struct ArrayData;

class MainThreadHandoff final : public IInterceptorSink
                              , public ICallFrameWalker
{
public:
  static HRESULT Create(IInterceptorSink** aOutput);

  template <typename Interface>
  static HRESULT WrapInterface(STAUniquePtr<Interface> aTargetInterface,
                               Interface** aOutInterface)
  {
    MOZ_ASSERT(!IsProxy(aTargetInterface.get()));
    RefPtr<IInterceptorSink> handoff;
    HRESULT hr = MainThreadHandoff::Create(getter_AddRefs(handoff));
    if (FAILED(hr)) {
      return hr;
    }
    return CreateInterceptor(Move(aTargetInterface), handoff, aOutInterface);
  }

  // IUnknown
  STDMETHODIMP QueryInterface(REFIID riid, void** ppv) override;
  STDMETHODIMP_(ULONG) AddRef() override;
  STDMETHODIMP_(ULONG) Release() override;

  // ICallFrameEvents
  STDMETHODIMP OnCall(ICallFrame* aFrame) override;

  // IInterceptorSink
  STDMETHODIMP SetInterceptor(IWeakReference* aInterceptor) override;

  // ICallFrameWalker
  STDMETHODIMP OnWalkInterface(REFIID aIid, PVOID* aInterface, BOOL aIsInParam,
                               BOOL aIsOutParam) override;

private:
  MainThreadHandoff();
  ~MainThreadHandoff();
  HRESULT FixArrayElements(ICallFrame* aFrame,
                           const ArrayData& aArrayData);

private:
  ULONG                   mRefCnt;
  RefPtr<IWeakReference>  mInterceptor;
};

} // namespace mscom
} // namespace mozilla

#endif // mozilla_mscom_MainThreadHandoff_h
