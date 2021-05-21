/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/WindowGlobalActor.h"

#include "AutoplayPolicy.h"
#include "nsContentUtils.h"
#include "mozJSComponentLoader.h"
#include "mozilla/Components.h"
#include "mozilla/ContentBlockingAllowList.h"
#include "mozilla/Logging.h"
#include "mozilla/dom/Document.h"
#include "mozilla/dom/JSActorService.h"
#include "mozilla/dom/JSWindowActorParent.h"
#include "mozilla/dom/JSWindowActorChild.h"
#include "mozilla/dom/JSWindowActorProtocol.h"
#include "mozilla/dom/PopupBlocker.h"
#include "mozilla/net/CookieJarSettings.h"
#include "mozilla/dom/WindowGlobalChild.h"
#include "mozilla/dom/WindowGlobalParent.h"

#include "nsGlobalWindowInner.h"
#include "nsNetUtil.h"

namespace mozilla::dom {

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
  auto& fields = ctx.mFields;
  fields.mEmbedderPolicy = InheritedPolicy(aBrowsingContext);
  fields.mAutoplayPermission = nsIPermissionManager::UNKNOWN_ACTION;
  return init;
}

WindowGlobalInit WindowGlobalActor::AboutBlankInitializer(
    dom::BrowsingContext* aBrowsingContext, nsIPrincipal* aPrincipal) {
  WindowGlobalInit init =
      BaseInitializer(aBrowsingContext, nsContentUtils::GenerateWindowId(),
                      nsContentUtils::GenerateWindowId());

  init.principal() = aPrincipal;
  Unused << NS_NewURI(getter_AddRefs(init.documentURI()), "about:blank");

  return init;
}

WindowGlobalInit WindowGlobalActor::WindowInitializer(
    nsGlobalWindowInner* aWindow) {
  WindowGlobalInit init =
      BaseInitializer(aWindow->GetBrowsingContext(), aWindow->WindowID(),
                      aWindow->GetOuterWindow()->WindowID());

  init.principal() = aWindow->GetPrincipal();
  init.documentURI() = aWindow->GetDocumentURI();

  Document* doc = aWindow->GetDocument();

  init.blockAllMixedContent() = doc->GetBlockAllMixedContent(false);
  init.upgradeInsecureRequests() = doc->GetUpgradeInsecureRequests(false);
  init.sandboxFlags() = doc->GetSandboxFlags();
  net::CookieJarSettings::Cast(doc->CookieJarSettings())
      ->Serialize(init.cookieJarSettings());
  init.httpsOnlyStatus() = doc->HttpsOnlyStatus();

  auto& fields = init.context().mFields;
  fields.mCookieBehavior = Some(doc->CookieJarSettings()->GetCookieBehavior());
  fields.mIsOnContentBlockingAllowList =
      doc->CookieJarSettings()->GetIsOnContentBlockingAllowList();
  fields.mIsThirdPartyWindow = doc->HasThirdPartyChannel();
  fields.mIsThirdPartyTrackingResourceWindow =
      nsContentUtils::IsThirdPartyTrackingResourceWindow(aWindow);
  fields.mIsSecureContext = aWindow->IsSecureContext();

  // Initialze permission fields
  fields.mAutoplayPermission =
      AutoplayPolicy::GetSiteAutoplayPermission(init.principal());
  fields.mPopupPermission = PopupBlocker::GetPopupPermission(init.principal());

  // Initialize top level permission fields
  if (aWindow->GetBrowsingContext()->IsTop()) {
    fields.mAllowMixedContent = [&] {
      uint32_t permit = nsIPermissionManager::UNKNOWN_ACTION;
      nsCOMPtr<nsIPermissionManager> permissionManager =
          components::PermissionManager::Service();

      if (permissionManager) {
        permissionManager->TestPermissionFromPrincipal(
            init.principal(), "mixed-content"_ns, &permit);
      }

      return permit == nsIPermissionManager::ALLOW_ACTION;
    }();

    fields.mShortcutsPermission =
        nsGlobalWindowInner::GetShortcutsPermission(init.principal());
  }

  if (auto policy = doc->GetEmbedderPolicy()) {
    fields.mEmbedderPolicy = *policy;
  }

  // Init Mixed Content Fields
  nsCOMPtr<nsIURI> innerDocURI = NS_GetInnermostURI(doc->GetDocumentURI());
  fields.mIsSecure = innerDocURI && innerDocURI->SchemeIs("https");

  nsCOMPtr<nsITransportSecurityInfo> securityInfo;
  if (nsCOMPtr<nsIChannel> channel = doc->GetChannel()) {
    nsCOMPtr<nsILoadInfo> loadInfo(channel->LoadInfo());
    fields.mIsOriginalFrameSource = loadInfo->GetOriginalFrameSrcLoad();

    nsCOMPtr<nsISupports> securityInfoSupports;
    channel->GetSecurityInfo(getter_AddRefs(securityInfoSupports));
    securityInfo = do_QueryInterface(securityInfoSupports);
  }
  init.securityInfo() = securityInfo;

  fields.mIsLocalIP = init.principal()->GetIsLocalIpAddress();

  // Most data here is specific to the Document, which can change without
  // creating a new WindowGlobal. Anything new added here which fits that
  // description should also be synchronized in
  // WindowGlobalChild::OnNewDocument.
  return init;
}

already_AddRefed<JSActorProtocol> WindowGlobalActor::MatchingJSActorProtocol(
    JSActorService* aActorSvc, const nsACString& aName, ErrorResult& aRv) {
  RefPtr<JSWindowActorProtocol> proto =
      aActorSvc->GetJSWindowActorProtocol(aName);
  if (!proto) {
    aRv.ThrowNotFoundError(nsPrintfCString("No such JSWindowActor '%s'",
                                           PromiseFlatCString(aName).get()));
    return nullptr;
  }

  if (!proto->Matches(BrowsingContext(), GetDocumentURI(), GetRemoteType(),
                      aRv)) {
    MOZ_ASSERT(aRv.Failed());
    return nullptr;
  }
  MOZ_ASSERT(!aRv.Failed());
  return proto.forget();
}

}  // namespace mozilla::dom
