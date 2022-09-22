/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"

#include "mozilla/dom/Promise.h"
#include "mozilla/dom/Promise-inl.h"
#include "xpcpublic.h"

using namespace mozilla;
using namespace mozilla::dom;

TEST(ThenWithCycleCollectedArgsJS, Empty)
{
  nsCOMPtr<nsIGlobalObject> global =
      xpc::NativeGlobal(xpc::PrivilegedJunkScope());

  RefPtr<Promise> promise = Promise::Create(global, IgnoreErrors());
  auto result = promise->ThenWithCycleCollectedArgsJS(
      [](JSContext*, JS::Handle<JS::Value>, ErrorResult&) { return nullptr; },
      std::make_tuple(), std::make_tuple());
}

TEST(ThenWithCycleCollectedArgsJS, nsCOMPtr)
{
  nsCOMPtr<nsIGlobalObject> global =
      xpc::NativeGlobal(xpc::PrivilegedJunkScope());

  RefPtr<Promise> promise = Promise::Create(global, IgnoreErrors());
  auto result = promise->ThenWithCycleCollectedArgsJS(
      [](JSContext*, JS::Handle<JS::Value>, ErrorResult&, nsIGlobalObject*) {
        return nullptr;
      },
      std::make_tuple(global), std::make_tuple());
}

TEST(ThenWithCycleCollectedArgsJS, RefPtr)
{
  nsCOMPtr<nsIGlobalObject> global =
      xpc::NativeGlobal(xpc::PrivilegedJunkScope());

  RefPtr<Promise> promise = Promise::Create(global, IgnoreErrors());
  auto result = promise->ThenWithCycleCollectedArgsJS(
      [](JSContext*, JS::Handle<JS::Value>, ErrorResult&, Promise*) {
        return nullptr;
      },
      std::make_tuple(promise), std::make_tuple());
}

TEST(ThenWithCycleCollectedArgsJS, RefPtrAndJSHandle)
{
  nsCOMPtr<nsIGlobalObject> global =
      xpc::NativeGlobal(xpc::PrivilegedJunkScope());

  RefPtr<Promise> promise = Promise::Create(global, IgnoreErrors());
  auto result = promise->ThenWithCycleCollectedArgsJS(
      [](JSContext*, JS::Handle<JS::Value> v, ErrorResult&, Promise*,
         JS::Handle<JS::Value>) { return nullptr; },
      std::make_tuple(promise), std::make_tuple(JS::UndefinedHandleValue));
}

TEST(ThenWithCycleCollectedArgsJS, Mixed)
{
  AutoJSAPI jsapi;
  MOZ_ALWAYS_TRUE(jsapi.Init(xpc::PrivilegedJunkScope()));
  JSContext* cx = jsapi.cx();
  nsCOMPtr<nsIGlobalObject> global = xpc::CurrentNativeGlobal(cx);
  JS::Rooted<JSObject*> obj(cx, JS_NewPlainObject(cx));

  RefPtr<Promise> promise = Promise::Create(global, IgnoreErrors());
  auto result = promise->ThenWithCycleCollectedArgsJS(
      [](JSContext*, JS::Handle<JS::Value>, ErrorResult&, nsIGlobalObject*,
         Promise*, JS::Handle<JS::Value>,
         JS::Handle<JSObject*>) { return nullptr; },
      std::make_tuple(global, promise),
      std::make_tuple(JS::UndefinedHandleValue, JS::HandleObject(obj)));
}

TEST(ThenCatchWithCycleCollectedArgsJS, Empty)
{
  nsCOMPtr<nsIGlobalObject> global =
      xpc::NativeGlobal(xpc::PrivilegedJunkScope());

  RefPtr<Promise> promise = Promise::Create(global, IgnoreErrors());
  auto result = promise->ThenCatchWithCycleCollectedArgsJS(
      [](JSContext*, JS::Handle<JS::Value>, ErrorResult&) { return nullptr; },
      [](JSContext*, JS::Handle<JS::Value>, ErrorResult&) { return nullptr; },
      std::make_tuple(), std::make_tuple());
}

TEST(ThenCatchWithCycleCollectedArgsJS, nsCOMPtr)
{
  nsCOMPtr<nsIGlobalObject> global =
      xpc::NativeGlobal(xpc::PrivilegedJunkScope());

  RefPtr<Promise> promise = Promise::Create(global, IgnoreErrors());
  auto result = promise->ThenCatchWithCycleCollectedArgsJS(
      [](JSContext*, JS::Handle<JS::Value>, ErrorResult&, nsIGlobalObject*) {
        return nullptr;
      },
      [](JSContext*, JS::Handle<JS::Value>, ErrorResult&, nsIGlobalObject*) {
        return nullptr;
      },
      std::make_tuple(global), std::make_tuple());
}

TEST(ThenCatchWithCycleCollectedArgsJS, RefPtr)
{
  nsCOMPtr<nsIGlobalObject> global =
      xpc::NativeGlobal(xpc::PrivilegedJunkScope());

  RefPtr<Promise> promise = Promise::Create(global, IgnoreErrors());
  auto result = promise->ThenCatchWithCycleCollectedArgsJS(
      [](JSContext*, JS::Handle<JS::Value>, ErrorResult&, Promise*) {
        return nullptr;
      },
      [](JSContext*, JS::Handle<JS::Value>, ErrorResult&, Promise*) {
        return nullptr;
      },
      std::make_tuple(promise), std::make_tuple());
}

TEST(ThenCatchWithCycleCollectedArgsJS, RefPtrAndJSHandle)
{
  nsCOMPtr<nsIGlobalObject> global =
      xpc::NativeGlobal(xpc::PrivilegedJunkScope());

  RefPtr<Promise> promise = Promise::Create(global, IgnoreErrors());
  auto result = promise->ThenCatchWithCycleCollectedArgsJS(
      [](JSContext*, JS::Handle<JS::Value> v, ErrorResult&, Promise*,
         JS::Handle<JS::Value>) { return nullptr; },
      [](JSContext*, JS::Handle<JS::Value> v, ErrorResult&, Promise*,
         JS::Handle<JS::Value>) { return nullptr; },
      std::make_tuple(promise), std::make_tuple(JS::UndefinedHandleValue));
}

TEST(ThenCatchWithCycleCollectedArgsJS, Mixed)
{
  AutoJSAPI jsapi;
  MOZ_ALWAYS_TRUE(jsapi.Init(xpc::PrivilegedJunkScope()));
  JSContext* cx = jsapi.cx();
  nsCOMPtr<nsIGlobalObject> global = xpc::CurrentNativeGlobal(cx);
  JS::Rooted<JSObject*> obj(cx, JS_NewPlainObject(cx));

  RefPtr<Promise> promise = Promise::Create(global, IgnoreErrors());
  auto result = promise->ThenCatchWithCycleCollectedArgsJS(
      [](JSContext*, JS::Handle<JS::Value>, ErrorResult&, nsIGlobalObject*,
         Promise*, JS::Handle<JS::Value>,
         JS::Handle<JSObject*>) { return nullptr; },
      [](JSContext*, JS::Handle<JS::Value>, ErrorResult&, nsIGlobalObject*,
         Promise*, JS::Handle<JS::Value>,
         JS::Handle<JSObject*>) { return nullptr; },
      std::make_tuple(global, promise),
      std::make_tuple(JS::UndefinedHandleValue, JS::HandleObject(obj)));
}
