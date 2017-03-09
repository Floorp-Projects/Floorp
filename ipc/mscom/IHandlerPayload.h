/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_mscom_IHandlerPayload_h
#define mozilla_mscom_IHandlerPayload_h

#include "mozilla/mscom/Ptr.h"

#include <objidl.h>

namespace mozilla {
namespace mscom {

struct HandlerPayload
{
  virtual STDMETHODIMP GetHandler(CLSID* aHandlerClsid) = 0;
  virtual STDMETHODIMP GetHandlerPayloadSize(REFIID aIid,
                                             InterceptorTargetPtr<IUnknown> aTarget,
                                             DWORD* aOutPayloadSize) = 0;
  virtual STDMETHODIMP WriteHandlerPayload(IStream* aStream, REFIID aIid,
                                           InterceptorTargetPtr<IUnknown> aTarget) = 0;
  virtual REFIID MarshalAs(REFIID aIid) = 0;
};

struct IHandlerPayload : public IUnknown
                       , public HandlerPayload
{
  virtual STDMETHODIMP Clone(IHandlerPayload** aOutNewPayload) = 0;
};

} // namespace mscom
} // namespace mozilla

#endif // mozilla_mscom_IHandlerPayload_h
