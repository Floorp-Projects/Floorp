/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_mscom_PassthruProxy_h
#define mozilla_mscom_PassthruProxy_h

#include "mozilla/Atomics.h"
#include "mozilla/mscom/ProxyStream.h"
#include "mozilla/mscom/Ptr.h"
#include "mozilla/NotNull.h"
#if defined(MOZ_SANDBOX)
#  include "mozilla/SandboxSettings.h"
#endif  // defined(MOZ_SANDBOX)

#include <objbase.h>

namespace mozilla {
namespace mscom {
namespace detail {

template <typename Iface>
struct VTableSizer;

template <>
struct VTableSizer<IDispatch> {
  enum { Size = 7 };
};

}  // namespace detail

class PassthruProxy final : public IMarshal, public IClientSecurity {
 public:
  template <typename Iface>
  static RefPtr<Iface> Wrap(NotNull<Iface*> aIn) {
    static_assert(detail::VTableSizer<Iface>::Size >= 3, "VTable too small");

#if defined(MOZ_SANDBOX)
    if (mozilla::GetEffectiveContentSandboxLevel() < 3) {
      // The sandbox isn't strong enough to be a problem; no wrapping required
      return aIn.get();
    }

    typename detail::EnvironmentSelector<Iface>::Type env;

    RefPtr<PassthruProxy> passthru(new PassthruProxy(
        &env, __uuidof(Iface), detail::VTableSizer<Iface>::Size, aIn));

    RefPtr<Iface> result;
    if (FAILED(passthru->QueryProxyInterface(getter_AddRefs(result)))) {
      return nullptr;
    }

    return result;
#else
    // No wrapping required
    return aIn.get();
#endif  // defined(MOZ_SANDBOX)
  }

  static HRESULT Register();

  PassthruProxy();

  // IUnknown
  STDMETHODIMP QueryInterface(REFIID riid, void** ppv) override;
  STDMETHODIMP_(ULONG) AddRef() override;
  STDMETHODIMP_(ULONG) Release() override;

  // IMarshal
  STDMETHODIMP GetUnmarshalClass(REFIID riid, void* pv, DWORD dwDestContext,
                                 void* pvDestContext, DWORD mshlflags,
                                 CLSID* pCid) override;
  STDMETHODIMP GetMarshalSizeMax(REFIID riid, void* pv, DWORD dwDestContext,
                                 void* pvDestContext, DWORD mshlflags,
                                 DWORD* pSize) override;
  STDMETHODIMP MarshalInterface(IStream* pStm, REFIID riid, void* pv,
                                DWORD dwDestContext, void* pvDestContext,
                                DWORD mshlflags) override;
  STDMETHODIMP UnmarshalInterface(IStream* pStm, REFIID riid,
                                  void** ppv) override;
  STDMETHODIMP ReleaseMarshalData(IStream* pStm) override;
  STDMETHODIMP DisconnectObject(DWORD dwReserved) override;

  // IClientSecurity - we don't actually implement this interface, but its
  // presence signals to mscom::IsProxy() that we are a proxy.
  STDMETHODIMP QueryBlanket(IUnknown* aProxy, DWORD* aAuthnSvc,
                            DWORD* aAuthzSvc, OLECHAR** aSrvPrincName,
                            DWORD* aAuthnLevel, DWORD* aImpLevel,
                            void** aAuthInfo, DWORD* aCapabilities) override {
    return E_NOTIMPL;
  }

  STDMETHODIMP SetBlanket(IUnknown* aProxy, DWORD aAuthnSvc, DWORD aAuthzSvc,
                          OLECHAR* aSrvPrincName, DWORD aAuthnLevel,
                          DWORD aImpLevel, void* aAuthInfo,
                          DWORD aCapabilities) override {
    return E_NOTIMPL;
  }

  STDMETHODIMP CopyProxy(IUnknown* aProxy, IUnknown** aOutCopy) override {
    return E_NOTIMPL;
  }

 private:
  PassthruProxy(ProxyStream::Environment* aEnv, REFIID aIidToWrap,
                uint32_t aVTableSize, NotNull<IUnknown*> aObjToWrap);
  ~PassthruProxy();

  bool IsInitialMarshal() const { return !mStream; }
  HRESULT QueryProxyInterface(void** aOutInterface);

  Atomic<ULONG> mRefCnt;
  IID mWrappedIid;
  PreservedStreamPtr mPreservedStream;
  RefPtr<IStream> mStream;
  uint32_t mVTableSize;
  IUnknown* mVTable;
  bool mForgetPreservedStream;
};

}  // namespace mscom
}  // namespace mozilla

#endif  // mozilla_mscom_PassthruProxy_h
