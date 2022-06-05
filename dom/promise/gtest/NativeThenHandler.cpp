/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"

#include "js/TypeDecls.h"
#include "js/Value.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/Promise-inl.h"
#include "xpcpublic.h"

using namespace mozilla;
using namespace mozilla::dom;

struct TraceCounts {
  int32_t mValue = 0;
  int32_t mId = 0;
  int32_t mObject = 0;
  int32_t mWrapperCache = 0;
  int32_t mTenuredHeapObject = 0;
  int32_t mString = 0;
  int32_t mScript = 0;
  int32_t mFunction = 0;
};

struct DummyCallbacks final : public TraceCallbacks {
  void Trace(JS::Heap<JS::Value>*, const char*, void* aClosure) const override {
    static_cast<TraceCounts*>(aClosure)->mValue++;
  }

  void Trace(JS::Heap<jsid>*, const char*, void* aClosure) const override {
    static_cast<TraceCounts*>(aClosure)->mId++;
  }

  void Trace(JS::Heap<JSObject*>*, const char*, void* aClosure) const override {
    static_cast<TraceCounts*>(aClosure)->mObject++;
  }

  void Trace(nsWrapperCache*, const char* aName,
             void* aClosure) const override {
    static_cast<TraceCounts*>(aClosure)->mWrapperCache++;
  }

  void Trace(JS::TenuredHeap<JSObject*>*, const char*,
             void* aClosure) const override {
    static_cast<TraceCounts*>(aClosure)->mTenuredHeapObject++;
  }

  void Trace(JS::Heap<JSString*>*, const char*, void* aClosure) const override {
    static_cast<TraceCounts*>(aClosure)->mString++;
  }

  void Trace(JS::Heap<JSScript*>*, const char*, void* aClosure) const override {
    static_cast<TraceCounts*>(aClosure)->mScript++;
  }

  void Trace(JS::Heap<JSFunction*>*, const char*,
             void* aClosure) const override {
    static_cast<TraceCounts*>(aClosure)->mFunction++;
  }
};

TEST(NativeThenHandler, TraceValue)
{
  auto onResolve = [](JSContext*, JS::Handle<JS::Value>, ErrorResult&,
                      JS::Handle<JS::Value>) -> already_AddRefed<Promise> {
    return nullptr;
  };
  auto onReject = [](JSContext*, JS::Handle<JS::Value>, ErrorResult&,
                     JS::Handle<JS::Value>) -> already_AddRefed<Promise> {
    return nullptr;
  };

  // Explicit type for backward compatibility with clang<7 / gcc<8
  using HandlerType =
      NativeThenHandler<decltype(onResolve), decltype(onReject), std::tuple<>,
                        std::tuple<JS::HandleValue>>;
  RefPtr<HandlerType> handler = new HandlerType(
      nullptr, Some(onResolve), Some(onReject), std::make_tuple(),
      std::make_tuple(JS::UndefinedHandleValue));

  TraceCounts counts;
  NS_CYCLE_COLLECTION_PARTICIPANT(HandlerType)
      ->Trace(handler.get(), DummyCallbacks(), &counts);

  EXPECT_EQ(counts.mValue, 1);
}

TEST(NativeThenHandler, TraceObject)
{
  auto onResolve = [](JSContext*, JS::Handle<JS::Value>, ErrorResult&,
                      JS::Handle<JSObject*>) -> already_AddRefed<Promise> {
    return nullptr;
  };
  auto onReject = [](JSContext*, JS::Handle<JS::Value>, ErrorResult&,
                     JS::Handle<JSObject*>) -> already_AddRefed<Promise> {
    return nullptr;
  };

  AutoJSAPI jsapi;
  MOZ_ALWAYS_TRUE(jsapi.Init(xpc::PrivilegedJunkScope()));
  JSContext* cx = jsapi.cx();
  JS::Rooted<JSObject*> obj(cx, JS_NewPlainObject(cx));

  // Explicit type for backward compatibility with clang<7 / gcc<8
  using HandlerType =
      NativeThenHandler<decltype(onResolve), decltype(onReject), std::tuple<>,
                        std::tuple<JS::HandleObject>>;
  RefPtr<HandlerType> handler = new HandlerType(
      nullptr, Some(onResolve), Some(onReject), std::make_tuple(),
      std::make_tuple(JS::HandleObject(obj)));

  TraceCounts counts;
  NS_CYCLE_COLLECTION_PARTICIPANT(HandlerType)
      ->Trace(handler.get(), DummyCallbacks(), &counts);

  EXPECT_EQ(counts.mObject, 1);
}

TEST(NativeThenHandler, TraceMixed)
{
  auto onResolve = [](JSContext*, JS::Handle<JS::Value>, ErrorResult&,
                      nsIGlobalObject*, Promise*, JS::Handle<JS::Value>,
                      JS::Handle<JSObject*>) -> already_AddRefed<Promise> {
    return nullptr;
  };
  auto onReject = [](JSContext*, JS::Handle<JS::Value>, ErrorResult&,
                     nsIGlobalObject*, Promise*, JS::Handle<JS::Value>,
                     JS::Handle<JSObject*>) -> already_AddRefed<Promise> {
    return nullptr;
  };

  AutoJSAPI jsapi;
  MOZ_ALWAYS_TRUE(jsapi.Init(xpc::PrivilegedJunkScope()));
  JSContext* cx = jsapi.cx();
  nsCOMPtr<nsIGlobalObject> global = xpc::CurrentNativeGlobal(cx);
  JS::Rooted<JSObject*> obj(cx, JS_NewPlainObject(cx));

  RefPtr<Promise> promise = Promise::Create(global, IgnoreErrors());

  // Explicit type for backward compatibility with clang<7 / gcc<8
  using HandlerType =
      NativeThenHandler<decltype(onResolve), decltype(onReject),
                        std::tuple<nsCOMPtr<nsIGlobalObject>, RefPtr<Promise>>,
                        std::tuple<JS::HandleValue, JS::HandleObject>>;
  RefPtr<HandlerType> handler = new HandlerType(
      nullptr, Some(onResolve), Some(onReject),
      std::make_tuple(global, promise),
      std::make_tuple(JS::UndefinedHandleValue, JS::HandleObject(obj)));

  TraceCounts counts;
  NS_CYCLE_COLLECTION_PARTICIPANT(HandlerType)
      ->Trace(handler.get(), DummyCallbacks(), &counts);

  EXPECT_EQ(counts.mValue, 1);
  EXPECT_EQ(counts.mObject, 1);
}
