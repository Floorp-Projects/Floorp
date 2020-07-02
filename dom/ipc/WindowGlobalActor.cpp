/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/WindowGlobalActor.h"

#include "nsContentUtils.h"
#include "mozJSComponentLoader.h"
#include "mozilla/ContentBlockingAllowList.h"
#include "mozilla/Logging.h"
#include "mozilla/dom/JSActorService.h"
#include "mozilla/dom/JSWindowActorParent.h"
#include "mozilla/dom/JSWindowActorChild.h"
#include "mozilla/net/CookieJarSettings.h"

namespace mozilla {
namespace dom {

// CORPP 3.1.3 https://mikewest.github.io/corpp/#integration-html
static nsILoadInfo::CrossOriginEmbedderPolicy InheritedPolicy(
    dom::BrowsingContext* aBrowsingContext) {
  WindowContext* inherit = aBrowsingContext->GetParentWindowContext();
  if (inherit) {
    return inherit->GetEmbedderPolicy();
  }

  return nsILoadInfo::EMBEDDER_POLICY_NULL;
}

// Common WindowGlobalInit creation code used by both `AboutBlankInitializer`
// and `WindowInitializer`.
WindowGlobalInit WindowGlobalActor::BaseInitializer(
    dom::BrowsingContext* aBrowsingContext, uint64_t aInnerWindowId,
    uint64_t aOuterWindowId) {
  MOZ_DIAGNOSTIC_ASSERT(aBrowsingContext);

  WindowGlobalInit init;
  auto& ctx = init.context();
  ctx.mInnerWindowId = aInnerWindowId;
  ctx.mOuterWindowId = aOuterWindowId;
  ctx.mBrowsingContextId = aBrowsingContext->Id();

  // If any synced fields need to be initialized from our BrowsingContext, we
  // can initialize them here.
  mozilla::Get<WindowContext::IDX_EmbedderPolicy>(ctx.mFields) =
      InheritedPolicy(aBrowsingContext);
  mozilla::Get<WindowContext::IDX_AutoplayPermission>(init.context().mFields) =
      nsIPermissionManager::UNKNOWN_ACTION;
  return init;
}

WindowGlobalInit WindowGlobalActor::AboutBlankInitializer(
    dom::BrowsingContext* aBrowsingContext, nsIPrincipal* aPrincipal) {
  WindowGlobalInit init =
      BaseInitializer(aBrowsingContext, nsContentUtils::GenerateWindowId(),
                      nsContentUtils::GenerateWindowId());

  init.principal() = aPrincipal;
  Unused << NS_NewURI(getter_AddRefs(init.documentURI()), "about:blank");
  ContentBlockingAllowList::ComputePrincipal(
      aPrincipal, getter_AddRefs(init.contentBlockingAllowListPrincipal()));

  return init;
}

WindowGlobalInit WindowGlobalActor::WindowInitializer(
    nsGlobalWindowInner* aWindow) {
  WindowGlobalInit init =
      BaseInitializer(aWindow->GetBrowsingContext(), aWindow->WindowID(),
                      aWindow->GetOuterWindow()->WindowID());

  init.principal() = aWindow->GetPrincipal();
  init.contentBlockingAllowListPrincipal() =
      aWindow->GetDocumentContentBlockingAllowListPrincipal();
  init.documentURI() = aWindow->GetDocumentURI();

  Document* doc = aWindow->GetDocument();

  init.blockAllMixedContent() = doc->GetBlockAllMixedContent(false);
  init.upgradeInsecureRequests() = doc->GetUpgradeInsecureRequests(false);
  init.sandboxFlags() = doc->GetSandboxFlags();
  net::CookieJarSettings::Cast(doc->CookieJarSettings())
      ->Serialize(init.cookieJarSettings());
  init.httpsOnlyStatus() = doc->HttpsOnlyStatus();

  mozilla::Get<WindowContext::IDX_CookieBehavior>(init.context().mFields) =
      Some(doc->CookieJarSettings()->GetCookieBehavior());
  mozilla::Get<WindowContext::IDX_IsOnContentBlockingAllowList>(
      init.context().mFields) =
      doc->CookieJarSettings()->GetIsOnContentBlockingAllowList();
  mozilla::Get<WindowContext::IDX_IsThirdPartyWindow>(init.context().mFields) =
      doc->HasThirdPartyChannel();
  mozilla::Get<WindowContext::IDX_IsThirdPartyTrackingResourceWindow>(
      init.context().mFields) =
      nsContentUtils::IsThirdPartyTrackingResourceWindow(aWindow);
  mozilla::Get<WindowContext::IDX_IsSecureContext>(init.context().mFields) =
      aWindow->IsSecureContext();

  auto policy = doc->GetEmbedderPolicy();
  if (policy.isSome()) {
    mozilla::Get<WindowContext::IDX_EmbedderPolicy>(init.context().mFields) =
        policy.ref();
  }

  // Init Mixed Content Fields
  nsCOMPtr<nsIURI> innerDocURI = NS_GetInnermostURI(doc->GetDocumentURI());
  if (innerDocURI) {
    mozilla::Get<WindowContext::IDX_IsSecure>(init.context().mFields) =
        innerDocURI->SchemeIs("https");
  }
  nsCOMPtr<nsIChannel> mixedChannel;
  aWindow->GetDocShell()->GetMixedContentChannel(getter_AddRefs(mixedChannel));
  // A non null mixedContent channel on the docshell indicates,
  // that the user has overriden mixed content to allow mixed
  // content loads to happen.
  if (mixedChannel && (mixedChannel == doc->GetChannel())) {
    mozilla::Get<WindowContext::IDX_AllowMixedContent>(init.context().mFields) =
        true;
  }

  nsCOMPtr<nsITransportSecurityInfo> securityInfo;
  if (nsCOMPtr<nsIChannel> channel = doc->GetChannel()) {
    nsCOMPtr<nsISupports> securityInfoSupports;
    channel->GetSecurityInfo(getter_AddRefs(securityInfoSupports));
    securityInfo = do_QueryInterface(securityInfoSupports);
  }
  init.securityInfo() = securityInfo;

  // Most data here is specific to the Document, which can change without
  // creating a new WindowGlobal. Anything new added here which fits that
  // description should also be synchronized in
  // WindowGlobalChild::OnNewDocument.
  return init;
}

void WindowGlobalActor::ConstructActor(const nsACString& aName,
                                       JS::MutableHandleObject aActor,
                                       ErrorResult& aRv) {
  MOZ_ASSERT(nsContentUtils::IsSafeToRunScript());

  JSActor::Type actorType = GetSide();
  MOZ_ASSERT_IF(actorType == JSActor::Type::Parent, XRE_IsParentProcess());

  // Constructing an actor requires a running script, so push an AutoEntryScript
  // onto the stack.
  AutoEntryScript aes(xpc::PrivilegedJunkScope(),
                      "WindowGlobalActor construction");
  JSContext* cx = aes.cx();

  RefPtr<JSActorService> actorSvc = JSActorService::GetSingleton();
  if (!actorSvc) {
    aRv.ThrowNotSupportedError("Could not acquire actor service");
    return;
  }

  RefPtr<JSWindowActorProtocol> proto =
      actorSvc->GetJSWindowActorProtocol(aName);
  if (!proto) {
    aRv.ThrowNotSupportedError(nsPrintfCString(
        "Could not get JSWindowActorProtocol: %s is not registered",
        PromiseFlatCString(aName).get()));
    return;
  }

  if (!proto->Matches(BrowsingContext(), GetDocumentURI(), GetRemoteType())) {
    aRv.Throw(NS_ERROR_NOT_AVAILABLE);
    return;
  }

  // Load the module using mozJSComponentLoader.
  RefPtr<mozJSComponentLoader> loader = mozJSComponentLoader::Get();
  MOZ_ASSERT(loader);

  JS::RootedObject global(cx);
  JS::RootedObject exports(cx);

  const JSWindowActorProtocol::Sided* side;
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
      actor = new JSWindowActorParent();
    } else {
      actor = new JSWindowActorChild();
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
  ctorName.Append(actorType == JSActor::Type::Parent ? "Parent"_ns
                                                     : "Child"_ns);
  if (!JS_GetProperty(cx, exports, ctorName.get(), &ctor)) {
    aRv.NoteJSContextException(cx);
    return;
  }

  if (NS_WARN_IF(!ctor.isObject())) {
    nsPrintfCString message("Could not find actor constructor '%s'",
                            ctorName.get());
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
