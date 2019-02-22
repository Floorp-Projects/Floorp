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

/**
 * Helper for calling a named method on a JS Window Actor object with a single
 * parameter.
 *
 * It will do the following:
 *  1. Enter the actor object's compartment.
 *  2. Convert the given parameter into a JS parameter with ToJSValue.
 *  3. Call the named method, passing the single parameter.
 *  4. Place the return value in aRetVal.
 *
 * If an error occurs during this process, this method clears any pending
 * exceptions, and returns a nsresult.
 */
template <typename T>
nsresult CallJSActorMethod(nsWrapperCache* aActor, const char* aName,
                           T& aNativeArg, JS::MutableHandleValue aRetVal) {
  // FIXME(nika): We should avoid atomizing and interning the |aName| strings
  // every time we do this call. Given the limited set of possible IDs, it would
  // be better to cache the `jsid` values.

  aRetVal.setUndefined();

  // Get the wrapper for our actor. If we don't have a wrapper, the target
  // method won't be defined on it. so there's no reason to continue.
  JS::Rooted<JSObject*> actor(RootingCx(), aActor->GetWrapper());
  if (NS_WARN_IF(!actor)) {
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  // Enter the realm of our actor object to begin running script.
  AutoEntryScript aes(actor, "CallJSActorMethod");
  JSContext* cx = aes.cx();
  JSAutoRealm ar(cx, actor);

  // Get the method we want to call, and produce NS_ERROR_NOT_IMPLEMENTED if
  // it is not present.
  JS::Rooted<JS::Value> func(cx);
  if (NS_WARN_IF(!JS_GetProperty(cx, actor, aName, &func) ||
                 func.isPrimitive())) {
    JS_ClearPendingException(cx);
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  // Convert the native argument to a JS value.
  JS::Rooted<JS::Value> argv(cx);
  if (NS_WARN_IF(!ToJSValue(cx, aNativeArg, &argv))) {
    JS_ClearPendingException(cx);
    return NS_ERROR_FAILURE;
  }

  // Call our method.
  if (NS_WARN_IF(!JS_CallFunctionValue(cx, actor, func,
                                       JS::HandleValueArray(argv), aRetVal))) {
    JS_ClearPendingException(cx);
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

/**
 * Object corresponding to a single actor protocol. This object acts as an
 * Event listener for the actor which is called for events which would
 * trigger actor creation.
 *
 * This object also can act as a carrier for methods and other state related to
 * a single protocol managed by the JSWindowActorService.
 */
class JSWindowActorProtocol final : public nsISupports {
 public:
  NS_DECL_ISUPPORTS

  static already_AddRefed<JSWindowActorProtocol> FromIPC(
      const JSWindowActorInfo& aInfo);
  JSWindowActorInfo ToIPC();

  static already_AddRefed<JSWindowActorProtocol> FromWebIDLOptions(
      const nsAString& aName, const WindowActorOptions& aOptions,
      ErrorResult& aRv);

  struct Sided {
    nsCString mModuleURI;
  };

  struct ParentSide : public Sided {};

  struct ChildSide : public Sided {};

  const nsAString& Name() const { return mName; }
  const ParentSide& Parent() const { return mParent; }
  const ChildSide& Child() const { return mChild; }

  void RegisterListenersFor(EventTarget* aRoot);
  void UnregisterListenersFor(EventTarget* aRoot);

 private:
  explicit JSWindowActorProtocol(const nsAString& aName) : mName(aName) {}

  ~JSWindowActorProtocol() = default;

  nsString mName;
  ParentSide mParent;
  ChildSide mChild;
};

NS_IMPL_ISUPPORTS0(JSWindowActorProtocol);

/* static */ already_AddRefed<JSWindowActorProtocol>
JSWindowActorProtocol::FromIPC(const JSWindowActorInfo& aInfo) {
  MOZ_DIAGNOSTIC_ASSERT(XRE_IsContentProcess());

  RefPtr<JSWindowActorProtocol> proto = new JSWindowActorProtocol(aInfo.name());
  proto->mChild.mModuleURI.Assign(aInfo.url());

  return proto.forget();
}

JSWindowActorInfo JSWindowActorProtocol::ToIPC() {
  MOZ_DIAGNOSTIC_ASSERT(XRE_IsParentProcess());

  JSWindowActorInfo info;
  info.name() = mName;
  info.url() = mChild.mModuleURI;

  return info;
}

already_AddRefed<JSWindowActorProtocol>
JSWindowActorProtocol::FromWebIDLOptions(const nsAString& aName,
                                         const WindowActorOptions& aOptions,
                                         ErrorResult& aRv) {
  MOZ_DIAGNOSTIC_ASSERT(XRE_IsParentProcess());

  RefPtr<JSWindowActorProtocol> proto = new JSWindowActorProtocol(aName);
  proto->mParent.mModuleURI = aOptions.mParent.mModuleURI;
  proto->mChild.mModuleURI = aOptions.mChild.mModuleURI;

  return proto.forget();
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

  // Insert a new entry for the protocol.
  RefPtr<JSWindowActorProtocol> proto =
      JSWindowActorProtocol::FromWebIDLOptions(aName, aOptions, aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return;
  }

  entry.OrInsert([&] { return proto; });

  // Send information about the newly added entry to every existing content
  // process.
  AutoTArray<JSWindowActorInfo, 1> ipcInfos{proto->ToIPC()};
  for (auto* cp : ContentParent::AllProcesses(ContentParent::eLive)) {
    Unused << cp->SendInitJSWindowActorInfos(ipcInfos);
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
    // Create our JSWindowActorProtocol, register it in mDescriptors.
    RefPtr<JSWindowActorProtocol> proto =
        JSWindowActorProtocol::FromIPC(aInfos[i]);
    mDescriptors.Put(aInfos[i].name(), proto);
  }
}

void JSWindowActorService::GetJSWindowActorInfos(
    nsTArray<JSWindowActorInfo>& aInfos) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(XRE_IsParentProcess());

  for (auto iter = mDescriptors.ConstIter(); !iter.Done(); iter.Next()) {
    aInfos.AppendElement(iter.Data()->ToIPC());
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
  RefPtr<JSWindowActorProtocol> proto = mDescriptors.Get(aName);
  if (!proto) {
    aRv.Throw(NS_ERROR_DOM_NOT_SUPPORTED_ERR);
    return;
  }

  const JSWindowActorProtocol::Sided* side;
  if (aParentSide) {
    side = &proto->Parent();
  } else {
    side = &proto->Child();
  }

  // Load the module using mozJSComponentLoader.
  RefPtr<mozJSComponentLoader> loader = mozJSComponentLoader::Get();
  MOZ_ASSERT(loader);

  JS::RootedObject global(cx);
  JS::RootedObject exports(cx);
  aRv = loader->Import(cx, side->mModuleURI, &global, &exports);
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