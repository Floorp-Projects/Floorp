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

// {9a907000-7829-47f1-80eb-f67a26f47b34}
DEFINE_GUID(IID_IMainThreadHandoff, 0x9a907000, 0x7829, 0x47f1, 0x80, 0xeb,
            0xf6, 0x7a, 0x26, 0xf4, 0x7b, 0x34);

struct IMainThreadHandoff : public IInterceptorSink {
  virtual STDMETHODIMP GetHandlerProvider(IHandlerProvider** aProvider) = 0;
};

struct ArrayData;

class MainThreadHandoff final : public IMainThreadHandoff,
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

  // IMainThreadHandoff
  STDMETHODIMP GetHandlerProvider(IHandlerProvider** aProvider) override {
    RefPtr<IHandlerProvider> provider = mHandlerProvider;
    provider.forget(aProvider);
    return mHandlerProvider ? S_OK : S_FALSE;
  }

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
