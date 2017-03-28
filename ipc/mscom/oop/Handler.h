/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_mscom_Handler_h
#define mozilla_mscom_Handler_h

#if defined(MOZILLA_INTERNAL_API)
#error This code is NOT for internal Gecko use!
#endif // defined(MOZILLA_INTERNAL_API)

#include <objidl.h>

#include "mozilla/mscom/Aggregation.h"
#include "mozilla/RefPtr.h"

/* WARNING! The code in this file may be loaded into the address spaces of other
   processes! It MUST NOT link against xul.dll or other Gecko binaries! Only
   inline code may be included! */

namespace mozilla {
namespace mscom {

class Handler : public IMarshal
{
public:
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

  /**
   * This method allows the handler to return its own interfaces that override
   * those interfaces that are exposed by the underlying COM proxy.
   * @param aProxyUnknown is the IUnknown of the underlying COM proxy. This is
   *                      provided to give the handler implementation an
   *                      opportunity to acquire interfaces to the underlying
   *                      remote object, if needed.
   * @param aIid Interface requested, similar to IUnknown::QueryInterface
   * @param aOutInterface Outparam for the resulting interface to return to the
   *                      client.
   * @return The usual HRESULT codes similarly to IUnknown::QueryInterface
   */
  virtual HRESULT QueryHandlerInterface(IUnknown* aProxyUnknown, REFIID aIid,
                                        void** aOutInterface) = 0;
  /**
   * Called when the implementer should deserialize data in aStream.
   * @return S_OK on success;
   *         S_FALSE if the deserialization was successful but there was no data;
   *         HRESULT error code otherwise.
   */
  virtual HRESULT ReadHandlerPayload(IStream* aStream, REFIID aIid)
  { return S_FALSE; }

  /**
   * Unfortunately when COM marshals a proxy, it doesn't implicitly marshal
   * the payload that was originally sent with the proxy. We must implement
   * that code in the handler in order to make this happen.
   */

  /**
   * This function allows the implementer to substitute a different interface
   * for marshaling than the one that COM is intending to marshal. For example,
   * the implementer might want to marshal a proxy for an interface that is
   * derived from the requested interface.
   *
   * The default implementation is the identity function.
   */
  virtual REFIID MarshalAs(REFIID aRequestedIid) { return aRequestedIid; }

  /**
   * Called when the implementer must provide the size of the payload.
   */
  virtual HRESULT GetHandlerPayloadSize(REFIID aIid, DWORD* aOutPayloadSize)
  {
    if (!aOutPayloadSize) {
      return E_INVALIDARG;
    }
    *aOutPayloadSize = 0;
    return S_OK;
  }

  /**
   * Called when the implementer should serialize the payload data into aStream.
   */
  virtual HRESULT WriteHandlerPayload(IStream* aStream, REFIID aIid)
  {
    return S_OK;
  }

  IUnknown* GetProxy() const { return mInnerUnk; }

  static HRESULT Register(REFCLSID aClsid);
  static HRESULT Unregister(REFCLSID aClsid);

protected:
  Handler(IUnknown* aOuter, HRESULT* aResult);
  virtual ~Handler() {}
  bool HasPayload() const { return mHasPayload; }
  IUnknown* GetOuter() const { return mOuter; }

private:
  ULONG             mRefCnt;
  IUnknown*         mOuter;
  RefPtr<IUnknown>  mInnerUnk;
  IMarshal*         mUnmarshal; // WEAK
  bool              mHasPayload;
  DECLARE_AGGREGATABLE(Handler);
};

} // namespace mscom
} // namespace mozilla

#endif // mozilla_mscom_Handler_h

