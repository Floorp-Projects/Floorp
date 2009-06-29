//
// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "classfactory.h"


GenericClassFactory::GenericClassFactory() :
  reference_count_(1)
{
  InterlockedIncrement(&object_count_);
}


GenericClassFactory::~GenericClassFactory() {
  InterlockedDecrement(&object_count_);
}


LONG GenericClassFactory::object_count_ = 0;


STDMETHODIMP GenericClassFactory::QueryInterface(REFIID riid,
                                                 LPVOID* ppobject) {
  *ppobject = NULL;

  if (IsEqualIID(riid, IID_IUnknown) ||
      IsEqualIID(riid, IID_IClassFactory))
    *ppobject = static_cast<IClassFactory*>(this);
  else
    return E_NOINTERFACE;

  this->AddRef();
  return S_OK;
}


STDMETHODIMP_(ULONG) GenericClassFactory::AddRef() {
  return InterlockedIncrement(&reference_count_);
}


STDMETHODIMP_(ULONG) GenericClassFactory::Release() {
  if(0 == InterlockedDecrement(&reference_count_)) {
    delete this;
    return 0;
  }
  return reference_count_;
}


STDMETHODIMP GenericClassFactory::LockServer(BOOL) {
  return E_NOTIMPL;
}
