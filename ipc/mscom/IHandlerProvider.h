/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_mscom_IHandlerProvider_h
#define mozilla_mscom_IHandlerProvider_h

#include "mozilla/NotNull.h"
#include "mozilla/mscom/Ptr.h"

#include <objidl.h>

namespace mozilla {
namespace mscom {

struct IInterceptor;

struct HandlerProvider {
  virtual STDMETHODIMP GetHandler(NotNull<CLSID*> aHandlerClsid) = 0;
  virtual STDMETHODIMP GetHandlerPayloadSize(
      NotNull<IInterceptor*> aInterceptor, NotNull<DWORD*> aOutPayloadSize) = 0;
  virtual STDMETHODIMP WriteHandlerPayload(NotNull<IInterceptor*> aInterceptor,
                                           NotNull<IStream*> aStream) = 0;
  virtual STDMETHODIMP_(REFIID) MarshalAs(REFIID aIid) = 0;
  virtual STDMETHODIMP DisconnectHandlerRemotes() = 0;

  /**
   * Determine whether this interface might be supported by objects using
   * this HandlerProvider.
   * This is used to avoid unnecessary cross-thread QueryInterface calls for
   * interfaces known to be unsupported.
   * Return S_OK if the interface might be supported, E_NOINTERFACE if it
   * definitely isn't supported.
   */
  virtual STDMETHODIMP IsInterfaceMaybeSupported(REFIID aIid) { return S_OK; }
};

struct IHandlerProvider : public IUnknown, public HandlerProvider {
  virtual STDMETHODIMP_(REFIID)
      GetEffectiveOutParamIid(REFIID aCallIid, ULONG aCallMethod) = 0;
  virtual STDMETHODIMP NewInstance(
      REFIID aIid, InterceptorTargetPtr<IUnknown> aTarget,
      NotNull<IHandlerProvider**> aOutNewPayload) = 0;
};

}  // namespace mscom
}  // namespace mozilla

#endif  // mozilla_mscom_IHandlerProvider_h
