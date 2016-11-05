/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_mscom_WeakRef_h
#define mozilla_mscom_WeakRef_h

#include <guiddef.h>
#include <unknwn.h>

#include "mozilla/Mutex.h"
#include "nsTArray.h"

/**
 * Thread-safe weak references for COM that works pre-Windows 8 and do not
 * require WinRT.
 */

namespace mozilla {
namespace mscom {

// {F841AEFA-064C-49A4-B73D-EBD14A90F012}
DEFINE_GUID(IID_IWeakReference,
0xf841aefa, 0x64c, 0x49a4, 0xb7, 0x3d, 0xeb, 0xd1, 0x4a, 0x90, 0xf0, 0x12);

struct IWeakReference : public IUnknown
{
  virtual STDMETHODIMP Resolve(REFIID aIid, void** aOutStringReference) = 0;
};

// {87611F0C-9BBB-4F78-9D43-CAC5AD432CA1}
DEFINE_GUID(IID_IWeakReferenceSource,
0x87611f0c, 0x9bbb, 0x4f78, 0x9d, 0x43, 0xca, 0xc5, 0xad, 0x43, 0x2c, 0xa1);

struct IWeakReferenceSource : public IUnknown
{
  virtual STDMETHODIMP GetWeakReference(IWeakReference** aOutWeakRef) = 0;
};

class WeakRef;

class WeakReferenceSupport : public IWeakReferenceSource
{
public:
  enum class Flags
  {
    eNone = 0,
    eDestroyOnMainThread = 1
  };

  // IUnknown
  STDMETHODIMP QueryInterface(REFIID riid, void** ppv) override;
  STDMETHODIMP_(ULONG) AddRef() override;
  STDMETHODIMP_(ULONG) Release() override;

  // IWeakReferenceSource
  STDMETHODIMP GetWeakReference(IWeakReference** aOutWeakRef) override;

protected:
  explicit WeakReferenceSupport(Flags aFlags);
  virtual ~WeakReferenceSupport();

  virtual HRESULT ThreadSafeQueryInterface(REFIID aIid,
                                           IUnknown** aOutInterface) = 0;

private:
  void ClearWeakRefs();

private:
  // Using a raw CRITICAL_SECTION here because it can be reentered
  CRITICAL_SECTION    mCS;
  ULONG               mRefCnt;
  nsTArray<WeakRef*>  mWeakRefs;
  Flags               mFlags;
};

class WeakRef : public IWeakReference
{
public:
  // IUnknown
  STDMETHODIMP QueryInterface(REFIID riid, void** ppv) override;
  STDMETHODIMP_(ULONG) AddRef() override;
  STDMETHODIMP_(ULONG) Release() override;

  // IWeakReference
  STDMETHODIMP Resolve(REFIID aIid, void** aOutStrongReference) override;

  explicit WeakRef(WeakReferenceSupport* aSupport);

  void Clear();

private:
  ULONG                 mRefCnt;
  mozilla::Mutex        mMutex; // Protects mSupport
  WeakReferenceSupport* mSupport;
};

} // namespace mscom
} // namespace mozilla

#endif // mozilla_mscom_WeakRef_h

