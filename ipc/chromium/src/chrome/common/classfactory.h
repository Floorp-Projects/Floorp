// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// ClassFactory<class>
// is a simple class factory object for the parameterized class.
//
#ifndef _CLASSFACTORY_H_
#define _CLASSFACTORY_H_

#include <unknwn.h>

// GenericClassFactory
// provides the basic COM plumbing to implement IClassFactory, and
// maintains a static count on the number of these objects in existence.
// It remains for subclasses to implement CreateInstance.
class GenericClassFactory : public IClassFactory {
public:
  GenericClassFactory();
  ~GenericClassFactory();

  //IUnknown methods
  STDMETHOD(QueryInterface)(REFIID iid, LPVOID* ppvObject);
  STDMETHOD_(ULONG, AddRef)();
  STDMETHOD_(ULONG, Release)();

  //IClassFactory methods
  STDMETHOD(CreateInstance)(LPUNKNOWN pUnkOuter, REFIID riid,
                            LPVOID* ppvObject) = 0;
  STDMETHOD(LockServer)(BOOL fLock);

  // generally handy for DllUnloadNow -- count of existing descendant objects
  static LONG GetObjectCount() { return object_count_; }

protected:
  LONG reference_count_; // mind the reference counting for this object
  static LONG object_count_; // count of all these objects
};


// OneClassFactory<T>
// Knows how to be a factory for T's
template <class T>
class OneClassFactory : public GenericClassFactory
{
public:
  //IClassFactory methods
  STDMETHOD(CreateInstance)(LPUNKNOWN pUnkOuter, REFIID riid, void** ppvObject);
};


template <class T>
STDMETHODIMP OneClassFactory<T>::CreateInstance(LPUNKNOWN pUnkOuter,
                                                REFIID riid, void** result) {
  *result = NULL;

  if(pUnkOuter != NULL)
    return CLASS_E_NOAGGREGATION;

  T* const obj = new T();
  if(!obj)
    return E_OUTOFMEMORY;

  obj->AddRef();
  HRESULT const hr = obj->QueryInterface(riid, result);
  obj->Release();

  return hr;
}

#endif
