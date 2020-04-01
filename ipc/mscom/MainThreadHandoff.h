/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_mscom_MainThreadHandoff_h
#define mozilla_mscom_MainThreadHandoff_h

#include <utility>

#include "mozilla/Assertions.h"
#include "mozilla/Mutex.h"
#include "mozilla/mscom/Interceptor.h"
#include "mozilla/mscom/MainThreadInvoker.h"
#include "mozilla/mscom/Utils.h"
#include "nsTArray.h"

namespace mozilla {
namespace mscom {

struct ArrayData;

class MainThreadHandoff final : public IInterceptorSink,
                                public ICallFrameWalker {
 public:
  static HRESULT Create(IHandlerProvider* aHandlerProvider,
                        IInterceptorSink** aOutput);

  template <typename Interface>
  static HRESULT WrapInterface(STAUniquePtr<Interface> aTargetInterface,
                               Interface** aOutInterface) {
    return WrapInterface<Interface>(std::move(aTargetInterface), nullptr,
                                    aOutInterface);
  }

  template <typename Interface>
  static HRESULT WrapInterface(STAUniquePtr<Interface> aTargetInterface,
                               IHandlerProvider* aHandlerProvider,
                               Interface** aOutInterface) {
    MOZ_ASSERT(!IsProxy(aTargetInterface.get()));
    RefPtr<IInterceptorSink> handoff;
    HRESULT hr =
        MainThreadHandoff::Create(aHandlerProvider, getter_AddRefs(handoff));
    if (FAILED(hr)) {
      return hr;
    }
    return CreateInterceptor(std::move(aTargetInterface), handoff,
                             aOutInterface);
  }

  // IUnknown
  STDMETHODIMP QueryInterface(REFIID riid, void** ppv) override;
  STDMETHODIMP_(ULONG) AddRef() override;
  STDMETHODIMP_(ULONG) Release() override;

  // ICallFrameEvents
  STDMETHODIMP OnCall(ICallFrame* aFrame) override;

  // IInterceptorSink
  STDMETHODIMP SetInterceptor(IWeakReference* aInterceptor) override;
  STDMETHODIMP GetHandler(NotNull<CLSID*> aHandlerClsid) override;
  STDMETHODIMP GetHandlerPayloadSize(NotNull<IInterceptor*> aInterceptor,
                                     NotNull<DWORD*> aOutPayloadSize) override;
  STDMETHODIMP WriteHandlerPayload(NotNull<IInterceptor*> aInterceptor,
                                   NotNull<IStream*> aStream) override;
  STDMETHODIMP_(REFIID) MarshalAs(REFIID aIid) override;
  STDMETHODIMP DisconnectHandlerRemotes() override;
  STDMETHODIMP IsInterfaceMaybeSupported(REFIID aIid) override;

  // ICallFrameWalker
  STDMETHODIMP OnWalkInterface(REFIID aIid, PVOID* aInterface, BOOL aIsInParam,
                               BOOL aIsOutParam) override;

 private:
  explicit MainThreadHandoff(IHandlerProvider* aHandlerProvider);
  ~MainThreadHandoff();
  HRESULT FixArrayElements(ICallFrame* aFrame, const ArrayData& aArrayData);
  HRESULT FixIServiceProvider(ICallFrame* aFrame);

 private:
  ULONG mRefCnt;
  RefPtr<IWeakReference> mInterceptor;
  RefPtr<IHandlerProvider> mHandlerProvider;
};

}  // namespace mscom
}  // namespace mozilla

#endif  // mozilla_mscom_MainThreadHandoff_h
