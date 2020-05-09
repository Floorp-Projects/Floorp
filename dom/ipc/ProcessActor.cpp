/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/ProcessActor.h"

#include "nsContentUtils.h"
#include "mozJSComponentLoader.h"
#include "mozilla/ContentBlockingAllowList.h"
#include "mozilla/Logging.h"
#include "mozilla/dom/JSActorService.h"
#include "mozilla/dom/JSProcessActorParent.h"
#include "mozilla/dom/JSProcessActorChild.h"

namespace mozilla {
namespace dom {

void ProcessActor::ConstructActor(const nsACString& aName,
                                  JS::MutableHandleObject aActor,
                                  ErrorResult& aRv) {
  MOZ_ASSERT(nsContentUtils::IsSafeToRunScript());

  JSActor::Type actorType = GetSide();
  MOZ_ASSERT_IF(actorType == JSActor::Type::Parent, XRE_IsParentProcess());

  // Constructing an actor requires a running script, so push an AutoEntryScript
  // onto the stack.
  AutoEntryScript aes(xpc::PrivilegedJunkScope(), "ProcessActor construction");
  JSContext* cx = aes.cx();

  RefPtr<JSActorService> actorSvc = JSActorService::GetSingleton();
  if (!actorSvc) {
    aRv.ThrowInvalidStateError(
        "While constructing JSProcessActor, could not acquire JSActorService.");
    return;
  }

  RefPtr<JSProcessActorProtocol> proto =
      actorSvc->GetJSProcessActorProtocol(aName);
  if (!proto) {
    aRv.ThrowNotFoundError(nsPrintfCString("No such JSProcessActor '%s'",
                                           PromiseFlatCString(aName).get()));
    return;
  }

  if (!proto->Matches(GetRemoteType())) {
    aRv.ThrowTypeMismatchError(
        nsPrintfCString("JSProcessActor '%s' does not match this process",
                        PromiseFlatCString(aName).get()));
    return;
  }

  // Load the module using mozJSComponentLoader.
  RefPtr<mozJSComponentLoader> loader = mozJSComponentLoader::Get();
  MOZ_ASSERT(loader);

  JS::RootedObject global(cx);
  JS::RootedObject exports(cx);

  const JSProcessActorProtocol::Sided* side;
  if (actorType == JSActor::Type::Parent) {
    side = &proto->Parent();
  } else {
    side = &proto->Child();
  }

  // Support basic functionally such as SendAsyncMessage and SendQuery for
  // unspecified moduleURI.
  if (!side->mModuleURI) {
    RefPtr<JSActor> actor;
    if (actorType == JSActor::Type::Parent) {
      actor = new JSProcessActorParent();
    } else {
      actor = new JSProcessActorChild();
    }

    JS::Rooted<JS::Value> wrapper(cx);
    if (!ToJSValue(cx, actor, &wrapper)) {
      aRv.NoteJSContextException(cx);
      return;
    }

    MOZ_ASSERT(wrapper.isObject());
    aActor.set(&wrapper.toObject());
    return;
  }

  aRv = loader->Import(cx, side->mModuleURI.ref(), &global, &exports);
  if (aRv.Failed()) {
    return;
  }

  MOZ_ASSERT(exports, "null exports!");

  // Load the specific property from our module.
  JS::RootedValue ctor(cx);
  nsAutoCString ctorName(aName);
  ctorName.Append(actorType == JSActor::Type::Parent
                      ? NS_LITERAL_CSTRING("Parent")
                      : NS_LITERAL_CSTRING("Child"));
  if (!JS_GetProperty(cx, exports, ctorName.get(), &ctor)) {
    aRv.NoteJSContextException(cx);
    return;
  }

  if (NS_WARN_IF(!ctor.isObject())) {
    nsPrintfCString message("Could not find actor constructor '%s'",
                            PromiseFlatCString(ctorName).get());
    aRv.ThrowNotFoundError(message);
    return;
  }

  // Invoke the constructor loaded from the module.
  if (!JS::Construct(cx, ctor, JS::HandleValueArray::empty(), aActor)) {
    aRv.NoteJSContextException(cx);
    return;
  }
}

}  // namespace dom
}  // namespace mozilla
