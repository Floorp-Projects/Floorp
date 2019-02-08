/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/JSWindowActorService.h"
#include "mozilla/dom/ChromeUtilsBinding.h"
#include "mozilla/dom/PContent.h"
#include "mozilla/StaticPtr.h"
#include "mozJSComponentLoader.h"

namespace mozilla {
namespace dom {
namespace {
StaticRefPtr<JSWindowActorService> gJSWindowActorService;
}

JSWindowActorService::JSWindowActorService() { MOZ_ASSERT(NS_IsMainThread()); }

JSWindowActorService::~JSWindowActorService() { MOZ_ASSERT(NS_IsMainThread()); }

/* static */ already_AddRefed<JSWindowActorService>
JSWindowActorService::GetSingleton() {
  MOZ_ASSERT(NS_IsMainThread());
  if (!gJSWindowActorService) {
    gJSWindowActorService = new JSWindowActorService();
    ClearOnShutdown(&gJSWindowActorService);
  }

  RefPtr<JSWindowActorService> service = gJSWindowActorService.get();
  return service.forget();
}

void JSWindowActorService::RegisterWindowActor(
    const nsAString& aName, const WindowActorOptions& aOptions,
    ErrorResult& aRv) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(XRE_IsParentProcess());

  auto entry = mDescriptors.LookupForAdd(aName);
  if (entry) {
    aRv.Throw(NS_ERROR_DOM_NOT_SUPPORTED_ERR);
    return;
  }

  entry.OrInsert([&] { return new WindowActorOptions(aOptions); });

  // Send child's WindowActorOptions to any existing content processes,
  // because parent's WindowActorOptions can never be accessed in content.
  for (auto* cp : ContentParent::AllProcesses(ContentParent::eLive)) {
    nsTArray<JSWindowActorInfo> infos;
    infos.AppendElement(
        JSWindowActorInfo(nsString(aName), aOptions.mChild.mModuleURI));
    Unused << cp->SendInitJSWindowActorInfos(infos);
  }
}

void JSWindowActorService::UnregisterWindowActor(const nsAString& aName) {
  mDescriptors.Remove(aName);
}

void JSWindowActorService::LoadJSWindowActorInfos(
    nsTArray<JSWindowActorInfo>& aInfos) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(XRE_IsContentProcess());

  for (uint32_t i = 0, len = aInfos.Length(); i < len; i++) {
    auto entry = mDescriptors.LookupForAdd(aInfos[i].name());

    entry.OrInsert([&] {
      WindowActorOptions* option = new WindowActorOptions();
      option->mChild.mModuleURI.Assign(aInfos[i].url());
      return option;
    });
  }
}

void JSWindowActorService::GetJSWindowActorInfos(
    nsTArray<JSWindowActorInfo>& aInfos) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(XRE_IsParentProcess());

  for (auto iter = mDescriptors.ConstIter(); !iter.Done(); iter.Next()) {
    aInfos.AppendElement(JSWindowActorInfo(nsString(iter.Key()),
                                           iter.Data()->mChild.mModuleURI));
  }
}

void JSWindowActorService::ConstructActor(const nsAString& aName,
                                          bool aParentSide,
                                          JS::MutableHandleObject aActor,
                                          ErrorResult& aRv) {
  MOZ_ASSERT_IF(aParentSide, XRE_IsParentProcess());

  // Constructing an actor requires a running script, so push an AutoEntryScript
  // onto the stack.
  AutoEntryScript aes(xpc::PrivilegedJunkScope(), "JSWindowActor construction");
  JSContext* cx = aes.cx();

  // Load our descriptor
  const WindowActorOptions* descriptor = mDescriptors.Get(aName);
  if (!descriptor) {
    MOZ_ASSERT(false, "WindowActorOptions must be found in mDescriptors");
    aRv.Throw(NS_ERROR_DOM_NOT_SUPPORTED_ERR);
    return;
  }

  const WindowActorSidedOptions& side =
      aParentSide ? descriptor->mParent : descriptor->mChild;

  // Load the module using mozJSComponentLoader.
  RefPtr<mozJSComponentLoader> loader = mozJSComponentLoader::Get();
  MOZ_ASSERT(loader);

  JS::RootedObject global(cx);
  JS::RootedObject exports(cx);
  aRv = loader->Import(cx, side.mModuleURI, &global, &exports);
  if (aRv.Failed()) {
    return;
  }
  MOZ_ASSERT(exports, "null exports!");

  // Load the specific property from our module.
  JS::RootedValue ctor(cx);
  nsAutoString ctorName(aName);
  ctorName.Append(aParentSide ? NS_LITERAL_STRING("Parent")
                              : NS_LITERAL_STRING("Child"));
  if (!JS_GetUCProperty(cx, exports, ctorName.get(), ctorName.Length(),
                        &ctor)) {
    aRv.NoteJSContextException(cx);
    return;
  }

  // Invoke the constructor loaded from the module.
  if (!JS::Construct(cx, ctor, JS::HandleValueArray::empty(), aActor)) {
    aRv.NoteJSContextException(cx);
    return;
  }
}

void JSWindowActorService::ReceiveMessage(JS::RootedObject& aObj,
                                          const nsString& aMessageName,
                                          ipc::StructuredCloneData& aData) {
  IgnoredErrorResult error;
  AutoEntryScript aes(js::CheckedUnwrap(aObj),
                      "WindowGlobalChild Message Handler");
  JSContext* cx = aes.cx();

  // We passed the unwrapped object to AutoEntryScript so we now need to
  // enter the realm of the global object that represents the realm of our
  // callback.
  JSAutoRealm ar(cx, aObj);
  JS::RootedValue json(cx, JS::NullValue());

  // Deserialize our data into a JS object in the correct compartment.
  aData.Read(aes.cx(), &json, error);
  if (NS_WARN_IF(error.Failed())) {
    JS_ClearPendingException(cx);
    return;
  }

  RootedDictionary<ReceiveMessageArgument> argument(cx);
  argument.mObjects = JS_NewPlainObject(cx);
  argument.mName = aMessageName;
  argument.mData = json;
  argument.mJson = json;
  JS::RootedValue argv(cx);
  if (NS_WARN_IF(!ToJSValue(cx, argument, &argv))) {
    return;
  }

  // Now that we have finished, call the recvAsyncMessage callback.
  JS::RootedValue dummy(cx);
  if (NS_WARN_IF(!JS_CallFunctionName(cx, aObj, "recvAsyncMessage",
                                      JS::HandleValueArray(argv), &dummy))) {
    JS_ClearPendingException(cx);
    return;
  }
}

}  // namespace dom
}  // namespace mozilla