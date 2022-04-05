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
      [](JSContext*, JS::HandleValue, ErrorResult&) { return nullptr; },
      std::make_tuple(), std::make_tuple());
}

TEST(ThenWithCycleCollectedArgsJS, nsCOMPtr)
{
  nsCOMPtr<nsIGlobalObject> global =
      xpc::NativeGlobal(xpc::PrivilegedJunkScope());

  RefPtr<Promise> promise = Promise::Create(global, IgnoreErrors());
  auto result = promise->ThenWithCycleCollectedArgsJS(
      [](JSContext*, JS::HandleValue, ErrorResult&, nsIGlobalObject*) {
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
      [](JSContext*, JS::HandleValue, ErrorResult&, Promise*) {
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
      [](JSContext*, JS::HandleValue v, ErrorResult&, Promise*,
         JS::HandleValue) { return nullptr; },
      std::make_tuple(promise), std::make_tuple(JS::UndefinedHandleValue));
}

TEST(ThenWithCycleCollectedArgsJS, Mixed)
{
  AutoJSAPI jsapi;
  MOZ_ALWAYS_TRUE(jsapi.Init(xpc::PrivilegedJunkScope()));
  JSContext* cx = jsapi.cx();
  nsCOMPtr<nsIGlobalObject> global = xpc::CurrentNativeGlobal(cx);
  JS::RootedObject obj(cx, JS_NewPlainObject(cx));

  RefPtr<Promise> promise = Promise::Create(global, IgnoreErrors());
  auto result = promise->ThenWithCycleCollectedArgsJS(
      [](JSContext*, JS::HandleValue, ErrorResult&, nsIGlobalObject*, Promise*,
         JS::HandleValue, JS::HandleObject) { return nullptr; },
      std::make_tuple(global, promise),
      std::make_tuple(JS::UndefinedHandleValue, JS::HandleObject(obj)));
}
