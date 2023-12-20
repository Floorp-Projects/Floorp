/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_mscom_AgileReference_h
#define mozilla_mscom_AgileReference_h

#include "mozilla/Attributes.h"
#include "mozilla/RefPtr.h"
#include "nsISupportsImpl.h"

#include <objidl.h>

namespace mozilla::mscom {

/**
 * This class encapsulates an "agile reference." These are references that
 * allow you to pass COM interfaces between apartments. When you have an
 * interface that you would like to pass between apartments, you wrap that
 * interface in an AgileReference and pass the agile reference instead. Then
 * you unwrap the interface by calling AgileReference::Resolve.
 *
 * Sample usage:
 *
 * // In the multithreaded apartment, foo is an IFoo*
 * auto myAgileRef = AgileReference(IID_IFoo, foo);
 *
 * // myAgileRef is passed to our main thread, which runs in a single-threaded
 * // apartment:
 *
 * RefPtr<IFoo> foo;
 * HRESULT hr = myAgileRef.Resolve(IID_IFoo, getter_AddRefs(foo));
 * // Now foo may be called from the main thread
 */
class AgileReference final {
 public:
  AgileReference();

  template <typename InterfaceT>
  explicit AgileReference(RefPtr<InterfaceT>& aObject)
      : AgileReference(__uuidof(InterfaceT), aObject) {}

  AgileReference(REFIID aIid, IUnknown* aObject);

  AgileReference(const AgileReference& aOther) = default;
  AgileReference(AgileReference&& aOther) noexcept;

  ~AgileReference();

  explicit operator bool() const { return !!mAgileRef; }

  HRESULT GetHResult() const { return mHResult; }

  template <typename T>
  void Assign(const RefPtr<T>& aOther) {
    Assign(__uuidof(T), aOther);
  }

  // Raw version, and implementation, of Resolve(). Can be used directly if
  // necessary, but in general, prefer one of the templated versions below
  // (depending on whether or not you need the HRESULT).
  HRESULT ResolveRaw(REFIID aIid, void** aOutInterface) const;

  template <typename Interface>
  HRESULT Resolve(RefPtr<Interface>& aOutInterface) const {
    return this->ResolveRaw(__uuidof(Interface), getter_AddRefs(aOutInterface));
  }

  template <typename T>
  RefPtr<T> Resolve() {
    RefPtr<T> p;
    Resolve<T>(p);
    return p;
  }

  AgileReference& operator=(const AgileReference& aOther);
  AgileReference& operator=(AgileReference&& aOther) noexcept;

  AgileReference& operator=(decltype(nullptr)) {
    Clear();
    return *this;
  }

  void Clear();

 private:
  void Assign(REFIID aIid, IUnknown* aObject);
  void AssignInternal(IUnknown* aObject);

 private:
  // The interface ID with which this reference was constructed.
  IID mIid;
  RefPtr<IAgileReference> mAgileRef;
  // The result associated with this reference's construction. May be modified
  // when mAgileRef changes, but is explicitly not touched by `Resolve`.
  HRESULT mHResult;
};

}  // namespace mozilla::mscom

#endif  // mozilla_mscom_AgileReference_h
