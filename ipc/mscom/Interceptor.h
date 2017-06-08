/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_mscom_Interceptor_h
#define mozilla_mscom_Interceptor_h

#include "mozilla/Move.h"
#include "mozilla/Mutex.h"
#include "nsTArray.h"
#include "mozilla/mscom/IHandlerProvider.h"
#include "mozilla/mscom/Ptr.h"
#include "mozilla/mscom/WeakRef.h"
#include "mozilla/RefPtr.h"

#include <objidl.h>
#include <callobj.h>

namespace mozilla {
namespace mscom {

// {8831EB53-A937-42BC-9921-B3E1121FDF86}
DEFINE_GUID(IID_IInterceptorSink,
0x8831eb53, 0xa937, 0x42bc, 0x99, 0x21, 0xb3, 0xe1, 0x12, 0x1f, 0xdf, 0x86);

struct IInterceptorSink : public ICallFrameEvents
                        , public HandlerProvider
{
  virtual STDMETHODIMP SetInterceptor(IWeakReference* aInterceptor) = 0;
};

// {3710799B-ECA2-4165-B9B0-3FA1E4A9B230}
DEFINE_GUID(IID_IInterceptor,
0x3710799b, 0xeca2, 0x4165, 0xb9, 0xb0, 0x3f, 0xa1, 0xe4, 0xa9, 0xb2, 0x30);

struct IInterceptor : public IUnknown
{
  virtual STDMETHODIMP GetTargetForIID(REFIID aIid,
                                       InterceptorTargetPtr<IUnknown>& aTarget) = 0;
  virtual STDMETHODIMP GetInterceptorForIID(REFIID aIid,
                                            void** aOutInterceptor) = 0;
};

/**
 * The COM interceptor is the core functionality in mscom that allows us to
 * redirect method calls to different threads. It emulates the vtable of a
 * target interface. When a call is made on this emulated vtable, the call is
 * packaged up into an instance of the ICallFrame interface which may be passed
 * to other contexts for execution.
 *
 * In order to accomplish this, COM itself provides the CoGetInterceptor
 * function, which instantiates an ICallInterceptor. Note, however, that
 * ICallInterceptor only works on a single interface; we need to be able to
 * interpose QueryInterface calls so that we can instantiate a new
 * ICallInterceptor for each new interface that is requested.
 *
 * We accomplish this by using COM aggregation, which means that the
 * ICallInterceptor delegates its IUnknown implementation to its outer object
 * (the mscom::Interceptor we implement and control).
 */
class Interceptor final : public WeakReferenceSupport
                        , public IStdMarshalInfo
                        , public IMarshal
                        , public IInterceptor
{
public:
  static HRESULT Create(STAUniquePtr<IUnknown> aTarget, IInterceptorSink* aSink,
                        REFIID aInitialIid, void** aOutInterface);

  // IUnknown
  STDMETHODIMP QueryInterface(REFIID riid, void** ppv) override;
  STDMETHODIMP_(ULONG) AddRef() override;
  STDMETHODIMP_(ULONG) Release() override;

  // IStdMarshalInfo
  STDMETHODIMP GetClassForHandler(DWORD aDestContext, void* aDestContextPtr,
                                  CLSID* aHandlerClsid) override;

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

  // IInterceptor
  STDMETHODIMP GetTargetForIID(REFIID aIid,
                               InterceptorTargetPtr<IUnknown>& aTarget) override;
  STDMETHODIMP GetInterceptorForIID(REFIID aIid, void** aOutInterceptor) override;

private:
  struct MapEntry
  {
    MapEntry(REFIID aIid, IUnknown* aInterceptor, IUnknown* aTargetInterface)
      : mIID(aIid)
      , mInterceptor(aInterceptor)
      , mTargetInterface(aTargetInterface)
    {}

    IID               mIID;
    RefPtr<IUnknown>  mInterceptor;
    IUnknown*         mTargetInterface;
  };

private:
  explicit Interceptor(IInterceptorSink* aSink);
  ~Interceptor();
  HRESULT GetInitialInterceptorForIID(REFIID aTargetIid,
                                      STAUniquePtr<IUnknown> aTarget,
                                      void** aOutInterface);
  MapEntry* Lookup(REFIID aIid);
  HRESULT QueryInterfaceTarget(REFIID aIid, void** aOutput);
  HRESULT ThreadSafeQueryInterface(REFIID aIid,
                                   IUnknown** aOutInterface) override;
  HRESULT CreateInterceptor(REFIID aIid, IUnknown* aOuter, IUnknown** aOutput);

private:
  InterceptorTargetPtr<IUnknown>  mTarget;
  RefPtr<IInterceptorSink>  mEventSink;
  mozilla::Mutex            mMutex; // Guards mInterceptorMap
  // Using a nsTArray since the # of interfaces is not going to be very high
  nsTArray<MapEntry>        mInterceptorMap;
  RefPtr<IUnknown>          mStdMarshalUnk;
  IMarshal*                 mStdMarshal; // WEAK
};

template <typename InterfaceT>
inline HRESULT
CreateInterceptor(STAUniquePtr<InterfaceT> aTargetInterface,
                  IInterceptorSink* aEventSink,
                  InterfaceT** aOutInterface)
{
  if (!aTargetInterface || !aEventSink) {
    return E_INVALIDARG;
  }

  REFIID iidTarget = __uuidof(aTargetInterface);

  STAUniquePtr<IUnknown> targetUnknown(aTargetInterface.release());
  return Interceptor::Create(Move(targetUnknown), aEventSink, iidTarget,
                             (void**)aOutInterface);
}

} // namespace mscom
} // namespace mozilla

#endif // mozilla_mscom_Interceptor_h
