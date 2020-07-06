/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/JSActorManager.h"
#include "mozilla/dom/JSActorService.h"
#include "mozJSComponentLoader.h"
#include "jsapi.h"

namespace mozilla {
namespace dom {

already_AddRefed<JSActor> JSActorManager::GetActor(const nsACString& aName,
                                                   ErrorResult& aRv) {
  MOZ_ASSERT(nsContentUtils::IsSafeToRunScript());

  // If our connection has been closed, return an error.
  mozilla::ipc::IProtocol* nativeActor = AsNativeActor();
  if (!nativeActor->CanSend()) {
    aRv.ThrowInvalidStateError(nsPrintfCString(
        "Cannot get actor '%s'. Native '%s' actor is destroyed.",
        PromiseFlatCString(aName).get(), nativeActor->GetProtocolName()));
    return nullptr;
  }

  // Check if this actor has already been created, and return it if it has.
  if (RefPtr<JSActor> actor = mJSActors.Get(aName)) {
    return actor.forget();
  }

  RefPtr<JSActorService> actorSvc = JSActorService::GetSingleton();
  if (!actorSvc) {
    aRv.ThrowInvalidStateError("JSActorService hasn't been initialized");
    return nullptr;
  }

  // Check if this actor satisfies the requirements of the protocol
  // corresponding to `aName`, and get the module which implements it.
  RefPtr<JSActorProtocol> protocol =
      MatchingJSActorProtocol(actorSvc, aName, aRv);
  if (!protocol) {
    return nullptr;
  }

  bool isParent = nativeActor->GetSide() == mozilla::ipc::ParentSide;
  auto& side = isParent ? protocol->Parent() : protocol->Child();

  // Constructing an actor requires a running script, so push an AutoEntryScript
  // onto the stack.
  AutoEntryScript aes(xpc::PrivilegedJunkScope(), "JSActor construction");
  JSContext* cx = aes.cx();

  // Load the module using mozJSComponentLoader.
  RefPtr<mozJSComponentLoader> loader = mozJSComponentLoader::Get();
  MOZ_ASSERT(loader);

  // If a module URI was provided, use it to construct an instance of the actor.
  JS::RootedObject actorObj(cx);
  if (side.mModuleURI) {
    JS::RootedObject global(cx);
    JS::RootedObject exports(cx);
    aRv = loader->Import(cx, side.mModuleURI.ref(), &global, &exports);
    if (aRv.Failed()) {
      return nullptr;
    }
    MOZ_ASSERT(exports, "null exports!");

    // Load the specific property from our module.
    JS::RootedValue ctor(cx);
    nsAutoCString ctorName(aName);
    ctorName.Append(isParent ? "Parent"_ns : "Child"_ns);
    if (!JS_GetProperty(cx, exports, ctorName.get(), &ctor)) {
      aRv.NoteJSContextException(cx);
      return nullptr;
    }

    if (NS_WARN_IF(!ctor.isObject())) {
      aRv.ThrowNotFoundError(
          nsPrintfCString("Could not find actor constructor '%s'",
                          PromiseFlatCString(ctorName).get()));
      return nullptr;
    }

    // Invoke the constructor loaded from the module.
    if (!JS::Construct(cx, ctor, JS::HandleValueArray::empty(), &actorObj)) {
      aRv.NoteJSContextException(cx);
      return nullptr;
    }
  }

  // Initialize our newly-constructed actor, and return it.
  RefPtr<JSActor> actor = InitJSActor(actorObj, aName, aRv);
  if (aRv.Failed()) {
    return nullptr;
  }
  mJSActors.Put(aName, RefPtr{actor});
  return actor.forget();
}

void JSActorManager::JSActorWillDestroy() {
  MOZ_ASSERT(nsContentUtils::IsSafeToRunScript());

  // Make a copy so that we can avoid potential iterator invalidation when
  // calling the user-provided Destroy() methods.
  nsTArray<RefPtr<JSActor>> actors(mJSActors.Count());
  for (auto& entry : mJSActors) {
    actors.AppendElement(entry.GetData());
  }
  for (auto& actor : actors) {
    actor->StartDestroy();
  }
}

void JSActorManager::JSActorDidDestroy() {
  MOZ_ASSERT(nsContentUtils::IsSafeToRunScript());

  // Swap the table with `mJSActors` so that we don't invalidate it while
  // iterating.
  nsRefPtrHashtable<nsCStringHashKey, JSActor> actors;
  mJSActors.SwapElements(actors);
  for (auto& entry : actors) {
    entry.GetData()->AfterDestroy();
  }
}

}  // namespace dom
}  // namespace mozilla
