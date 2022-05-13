/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_SpiderMonkeyInterface_h
#define mozilla_dom_SpiderMonkeyInterface_h

#include "jsapi.h"
#include "js/RootingAPI.h"
#include "js/TracingAPI.h"

namespace mozilla::dom {

/*
 * Class that just handles the JSObject storage and tracing for spidermonkey
 * interfaces
 */
struct SpiderMonkeyInterfaceObjectStorage {
 protected:
  JSObject* mImplObj;
  JSObject* mWrappedObj;

  SpiderMonkeyInterfaceObjectStorage()
      : mImplObj(nullptr), mWrappedObj(nullptr) {}

  SpiderMonkeyInterfaceObjectStorage(
      SpiderMonkeyInterfaceObjectStorage&& aOther)
      : mImplObj(aOther.mImplObj), mWrappedObj(aOther.mWrappedObj) {
    aOther.mImplObj = nullptr;
    aOther.mWrappedObj = nullptr;
  }

 public:
  inline void TraceSelf(JSTracer* trc) {
    JS::TraceRoot(trc, &mImplObj,
                  "SpiderMonkeyInterfaceObjectStorage.mImplObj");
    JS::TraceRoot(trc, &mWrappedObj,
                  "SpiderMonkeyInterfaceObjectStorage.mWrappedObj");
  }

  inline bool inited() const { return !!mImplObj; }

  inline bool WrapIntoNewCompartment(JSContext* cx) {
    return JS_WrapObject(
        cx, JS::MutableHandle<JSObject*>::fromMarkedLocation(&mWrappedObj));
  }

  inline JSObject* Obj() const {
    MOZ_ASSERT(inited());
    return mWrappedObj;
  }

 private:
  SpiderMonkeyInterfaceObjectStorage(
      const SpiderMonkeyInterfaceObjectStorage&) = delete;
};

// A class for rooting an existing SpiderMonkey Interface struct
template <typename InterfaceType>
class MOZ_RAII SpiderMonkeyInterfaceRooter : private JS::CustomAutoRooter {
 public:
  template <typename CX>
  SpiderMonkeyInterfaceRooter(const CX& cx, InterfaceType* aInterface)
      : JS::CustomAutoRooter(cx), mInterface(aInterface) {}

  virtual void trace(JSTracer* trc) override { mInterface->TraceSelf(trc); }

 private:
  SpiderMonkeyInterfaceObjectStorage* const mInterface;
};

// And a specialization for dealing with nullable SpiderMonkey interfaces
template <typename Inner>
struct Nullable;
template <typename InterfaceType>
class MOZ_RAII SpiderMonkeyInterfaceRooter<Nullable<InterfaceType>>
    : private JS::CustomAutoRooter {
 public:
  template <typename CX>
  SpiderMonkeyInterfaceRooter(const CX& cx, Nullable<InterfaceType>* aInterface)
      : JS::CustomAutoRooter(cx), mInterface(aInterface) {}

  virtual void trace(JSTracer* trc) override {
    if (!mInterface->IsNull()) {
      mInterface->Value().TraceSelf(trc);
    }
  }

 private:
  Nullable<InterfaceType>* const mInterface;
};

// Class for easily setting up a rooted SpiderMonkey interface object on the
// stack
template <typename InterfaceType>
class MOZ_RAII RootedSpiderMonkeyInterface final
    : public InterfaceType,
      private SpiderMonkeyInterfaceRooter<InterfaceType> {
 public:
  template <typename CX>
  explicit RootedSpiderMonkeyInterface(const CX& cx)
      : InterfaceType(), SpiderMonkeyInterfaceRooter<InterfaceType>(cx, this) {}

  template <typename CX>
  RootedSpiderMonkeyInterface(const CX& cx, JSObject* obj)
      : InterfaceType(obj),
        SpiderMonkeyInterfaceRooter<InterfaceType>(cx, this) {}
};

}  // namespace mozilla::dom

#endif /* mozilla_dom_SpiderMonkeyInterface_h */
