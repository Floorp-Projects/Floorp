/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsMixedContentBlocker.h"

#include "nsContentPolicyUtils.h"
#include "nsCSPContext.h"
#include "nsThreadUtils.h"
#include "nsINode.h"
#include "nsCOMPtr.h"
#include "nsDocShell.h"
#include "nsIWebProgressListener.h"
#include "nsContentUtils.h"
#include "nsIRequest.h"
#include "mozilla/dom/Document.h"
#include "nsIContentViewer.h"
#include "nsIChannel.h"
#include "nsIHttpChannel.h"
#include "nsIParentChannel.h"
#include "mozilla/Preferences.h"
#include "nsIScriptObjectPrincipal.h"
#include "nsISecureBrowserUI.h"
#include "nsIDocumentLoader.h"
#include "nsIWebNavigation.h"
#include "nsLoadGroup.h"
#include "nsIScriptError.h"
#include "nsIURI.h"
#include "nsIChannelEventSink.h"
#include "nsNetUtil.h"
#include "nsAsyncRedirectVerifyHelper.h"
#include "mozilla/LoadInfo.h"
#include "nsISiteSecurityService.h"
#include "prnetdb.h"

#include "mozilla/Logging.h"
#include "mozilla/StaticPrefs_dom.h"
#include "mozilla/Telemetry.h"
#include "mozilla/dom/ContentChild.h"
#include "mozilla/ipc/URIUtils.h"

using namespace mozilla;

enum nsMixedContentBlockerMessageType { eBlocked = 0x00, eUserOverride = 0x01 };

// Is mixed script blocking (fonts, plugin content, scripts, stylesheets,
// iframes, websockets, XHR) enabled?
bool nsMixedContentBlocker::sBlockMixedScript = false;

bool nsMixedContentBlocker::sBlockMixedObjectSubrequest = false;

// Is mixed display content blocking (images, audio, video) enabled?
bool nsMixedContentBlocker::sBlockMixedDisplay = false;

// Is mixed display content upgrading (images, audio, video) enabled?
bool nsMixedContentBlocker::sUpgradeMixedDisplay = false;

enum MixedContentHSTSState {
  MCB_HSTS_PASSIVE_NO_HSTS = 0,
  MCB_HSTS_PASSIVE_WITH_HSTS = 1,
  MCB_HSTS_ACTIVE_NO_HSTS = 2,
  MCB_HSTS_ACTIVE_WITH_HSTS = 3
};

// Fired at the document that attempted to load mixed content.  The UI could
// handle this event, for example, by displaying an info bar that offers the
// choice to reload the page with mixed content permitted.
class nsMixedContentEvent : public Runnable {
 public:
  nsMixedContentEvent(nsISupports* aContext, MixedContentTypes aType,
                      bool aRootHasSecureConnection)
      : mozilla::Runnable("nsMixedContentEvent"),
        mContext(aContext),
        mType(aType),
        mRootHasSecureConnection(aRootHasSecureConnection) {}

  NS_IMETHOD Run() override {
    NS_ASSERTION(mContext,
                 "You can't call this runnable without a requesting context");

    // To update the security UI in the tab with the blocked mixed content, call
    // nsDocLoader::OnSecurityChange.

    // Mixed content was allowed and is about to load; get the document and
    // set the approriate flag to true if we are about to load Mixed Active
    // Content.
    nsCOMPtr<nsIDocShell> docShell = NS_CP_GetDocShellFromContext(mContext);
    if (!docShell) {
      return NS_OK;
    }
    nsCOMPtr<nsIDocShellTreeItem> sameTypeRoot;
    docShell->GetInProcessSameTypeRootTreeItem(getter_AddRefs(sameTypeRoot));
    NS_ASSERTION(
        sameTypeRoot,
        "No document shell root tree item from document shell tree item!");

    // now get the document from sameTypeRoot
    nsCOMPtr<Document> rootDoc = sameTypeRoot->GetDocument();
    NS_ASSERTION(rootDoc,
                 "No root document from document shell root tree item.");

    nsDocShell* nativeDocShell = nsDocShell::Cast(docShell);
    nsCOMPtr<nsIDocShell> rootShell = do_GetInterface(sameTypeRoot);
    NS_ASSERTION(rootShell,
                 "No root docshell from document shell root tree item.");
    uint32_t state = nsIWebProgressListener::STATE_IS_BROKEN;
    nsCOMPtr<nsISecureBrowserUI> securityUI;
    rootShell->GetSecurityUI(getter_AddRefs(securityUI));
    // If there is no securityUI, document doesn't have a security state to
    // update.  But we still want to set the document flags, so we don't return
    // early.
    nsresult stateRV = NS_ERROR_FAILURE;
    if (securityUI) {
      stateRV = securityUI->GetState(&state);
    }

    if (mType == eMixedScript) {
      // See if the pref will change here. If it will, only then do we need to
      // call OnSecurityChange() to update the UI.
      if (rootDoc->GetHasMixedActiveContentLoaded()) {
        return NS_OK;
      }
      rootDoc->SetHasMixedActiveContentLoaded(true);

      // Update the security UI in the tab with the allowed mixed active content
      if (securityUI) {
        // Bug 1182551 - before changing the security state to broken, check
        // that the root is actually secure.
        if (mRootHasSecureConnection) {
          // reset state security flag
          state = state >> 4 << 4;
          // set state security flag to broken, since there is mixed content
          state |= nsIWebProgressListener::STATE_IS_BROKEN;

          // If mixed display content is loaded, make sure to include that in
          // the state.
          if (rootDoc->GetHasMixedDisplayContentLoaded()) {
            state |= nsIWebProgressListener::STATE_LOADED_MIXED_DISPLAY_CONTENT;
          }

          nativeDocShell->nsDocLoader::OnSecurityChange(
              mContext,
              (state |
               nsIWebProgressListener::STATE_LOADED_MIXED_ACTIVE_CONTENT));
        } else {
          // root not secure, mixed active content loaded in an https subframe
          if (NS_SUCCEEDED(stateRV)) {
            nativeDocShell->nsDocLoader::OnSecurityChange(
                mContext,
                (state |
                 nsIWebProgressListener::STATE_LOADED_MIXED_ACTIVE_CONTENT));
          }
        }
      }

    } else if (mType == eMixedDisplay) {
      // See if the pref will change here. If it will, only then do we need to
      // call OnSecurityChange() to update the UI.
      if (rootDoc->GetHasMixedDisplayContentLoaded()) {
        return NS_OK;
      }
      rootDoc->SetHasMixedDisplayContentLoaded(true);

      // Update the security UI in the tab with the allowed mixed display
      // content.
      if (securityUI) {
        // Bug 1182551 - before changing the security state to broken, check
        // that the root is actually secure.
        if (mRootHasSecureConnection) {
          // reset state security flag
          state = state >> 4 << 4;
          // set state security flag to broken, since there is mixed content
          state |= nsIWebProgressListener::STATE_IS_BROKEN;

          // If mixed active content is loaded, make sure to include that in the
          // state.
          if (rootDoc->GetHasMixedActiveContentLoaded()) {
            state |= nsIWebProgressListener::STATE_LOADED_MIXED_ACTIVE_CONTENT;
          }

          nativeDocShell->nsDocLoader::OnSecurityChange(
              mContext,
              (state |
               nsIWebProgressListener::STATE_LOADED_MIXED_DISPLAY_CONTENT));
        } else {
          // root not secure, mixed display content loaded in an https subframe
          if (NS_SUCCEEDED(stateRV)) {
            nativeDocShell->nsDocLoader::OnSecurityChange(
                mContext,
                (state |
                 nsIWebProgressListener::STATE_LOADED_MIXED_DISPLAY_CONTENT));
          }
        }
      }
    }

    return NS_OK;
  }

 private:
  // The requesting context for the content load. Generally, a DOM node from
  // the document that caused the load.
  nsCOMPtr<nsISupports> mContext;

  // The type of mixed content detected, e.g. active or display
  const MixedContentTypes mType;

  // Indicates whether the top level load is https or not.
  bool mRootHasSecureConnection;
};

nsMixedContentBlocker::nsMixedContentBlocker() {
  // Cache the pref for mixed script blocking
  Preferences::AddBoolVarCache(&sBlockMixedScript,
                               "security.mixed_content.block_active_content");

  Preferences::AddBoolVarCache(
      &sBlockMixedObjectSubrequest,
      "security.mixed_content.block_object_subrequest");

  // Cache the pref for mixed display blocking
  Preferences::AddBoolVarCache(&sBlockMixedDisplay,
                               "security.mixed_content.block_display_content");

  // Cache the pref for mixed display upgrading
  Preferences::AddBoolVarCache(
      &sUpgradeMixedDisplay, "security.mixed_content.upgrade_display_content");
}

nsMixedContentBlocker::~nsMixedContentBlocker() {}

NS_IMPL_ISUPPORTS(nsMixedContentBlocker, nsIContentPolicy, nsIChannelEventSink)

static void LogMixedContentMessage(
    MixedContentTypes aClassification, nsIURI* aContentLocation,
    Document* aRootDoc, nsMixedContentBlockerMessageType aMessageType) {
  nsAutoCString messageCategory;
  uint32_t severityFlag;
  nsAutoCString messageLookupKey;

  if (aMessageType == eBlocked) {
    severityFlag = nsIScriptError::errorFlag;
    messageCategory.AssignLiteral("Mixed Content Blocker");
    if (aClassification == eMixedDisplay) {
      messageLookupKey.AssignLiteral("BlockMixedDisplayContent");
    } else {
      messageLookupKey.AssignLiteral("BlockMixedActiveContent");
    }
  } else {
    severityFlag = nsIScriptError::warningFlag;
    messageCategory.AssignLiteral("Mixed Content Message");
    if (aClassification == eMixedDisplay) {
      messageLookupKey.AssignLiteral("LoadingMixedDisplayContent2");
    } else {
      messageLookupKey.AssignLiteral("LoadingMixedActiveContent2");
    }
  }

  AutoTArray<nsString, 1> strings;
  CopyUTF8toUTF16(aContentLocation->GetSpecOrDefault(),
                  *strings.AppendElement());
  nsContentUtils::ReportToConsole(severityFlag, messageCategory, aRootDoc,
                                  nsContentUtils::eSECURITY_PROPERTIES,
                                  messageLookupKey.get(), strings);
}

/* nsIChannelEventSink implementation
 * This code is called when a request is redirected.
 * We check the channel associated with the new uri is allowed to load
 * in the current context
 */
NS_IMETHODIMP
nsMixedContentBlocker::AsyncOnChannelRedirect(
    nsIChannel* aOldChannel, nsIChannel* aNewChannel, uint32_t aFlags,
    nsIAsyncVerifyRedirectCallback* aCallback) {
  mozilla::net::nsAsyncRedirectAutoCallback autoCallback(aCallback);

  if (!aOldChannel) {
    NS_ERROR("No channel when evaluating mixed content!");
    return NS_ERROR_FAILURE;
  }

  // If we are in the parent process in e10s, we don't have access to the
  // document node, and hence ShouldLoad will fail when we try to get
  // the docShell.  If that's the case, ignore mixed content checks
  // on redirects in the parent.  Let the child check for mixed content.
  nsCOMPtr<nsIParentChannel> is_ipc_channel;
  NS_QueryNotificationCallbacks(aNewChannel, is_ipc_channel);
  if (is_ipc_channel) {
    return NS_OK;
  }

  nsresult rv;
  nsCOMPtr<nsIURI> oldUri;
  rv = aOldChannel->GetURI(getter_AddRefs(oldUri));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIURI> newUri;
  rv = aNewChannel->GetURI(getter_AddRefs(newUri));
  NS_ENSURE_SUCCESS(rv, rv);

  // Get the loading Info from the old channel
  nsCOMPtr<nsILoadInfo> loadInfo = aOldChannel->LoadInfo();
  nsCOMPtr<nsIPrincipal> requestingPrincipal = loadInfo->LoadingPrincipal();

  // Since we are calling shouldLoad() directly on redirects, we don't go
  // through the code in nsContentPolicyUtils::NS_CheckContentLoadPolicy().
  // Hence, we have to duplicate parts of it here.
  if (requestingPrincipal) {
    // We check to see if the loadingPrincipal is systemPrincipal and return
    // early if it is
    if (nsContentUtils::IsSystemPrincipal(requestingPrincipal)) {
      return NS_OK;
    }
  }

  int16_t decision = REJECT_REQUEST;
  rv = ShouldLoad(newUri, loadInfo,
                  EmptyCString(),  // aMimeGuess
                  &decision);
  if (NS_FAILED(rv)) {
    autoCallback.DontCallback();
    aOldChannel->Cancel(NS_ERROR_DOM_BAD_URI);
    return NS_BINDING_FAILED;
  }

  // If the channel is about to load mixed content, abort the channel
  if (!NS_CP_ACCEPTED(decision)) {
    autoCallback.DontCallback();
    aOldChannel->Cancel(NS_ERROR_DOM_BAD_URI);
    return NS_BINDING_FAILED;
  }

  return NS_OK;
}

/* This version of ShouldLoad() is non-static and called by the Content Policy
 * API and AsyncOnChannelRedirect().  See nsIContentPolicy::ShouldLoad()
 * for detailed description of the parameters.
 */
NS_IMETHODIMP
nsMixedContentBlocker::ShouldLoad(nsIURI* aContentLocation,
                                  nsILoadInfo* aLoadInfo,
                                  const nsACString& aMimeGuess,
                                  int16_t* aDecision) {
  uint32_t contentType = aLoadInfo->InternalContentPolicyType();
  nsCOMPtr<nsISupports> requestingContext = aLoadInfo->GetLoadingContext();
  nsCOMPtr<nsIPrincipal> requestPrincipal = aLoadInfo->TriggeringPrincipal();
  nsCOMPtr<nsIURI> requestingLocation;
  nsCOMPtr<nsIPrincipal> loadingPrincipal = aLoadInfo->LoadingPrincipal();
  if (loadingPrincipal) {
    loadingPrincipal->GetURI(getter_AddRefs(requestingLocation));
  }

  // We pass in false as the first parameter to ShouldLoad(), because the
  // callers of this method don't know whether the load went through cached
  // image redirects.  This is handled by direct callers of the static
  // ShouldLoad.
  nsresult rv =
      ShouldLoad(false,  // aHadInsecureImageRedirect
                 contentType, aContentLocation, requestingLocation,
                 requestingContext, aMimeGuess, requestPrincipal, aDecision);

  if (*aDecision == nsIContentPolicy::REJECT_REQUEST) {
    NS_SetRequestBlockingReason(aLoadInfo,
                                nsILoadInfo::BLOCKING_REASON_MIXED_BLOCKED);
  }

  return rv;
}

bool nsMixedContentBlocker::IsPotentiallyTrustworthyLoopbackHost(
    const nsACString& aAsciiHost) {
  return aAsciiHost.EqualsLiteral("127.0.0.1") ||
         aAsciiHost.EqualsLiteral("::1") ||
         aAsciiHost.EqualsLiteral("localhost");
}

bool nsMixedContentBlocker::IsPotentiallyTrustworthyLoopbackURL(nsIURI* aURL) {
  nsAutoCString asciiHost;
  nsresult rv = aURL->GetAsciiHost(asciiHost);
  NS_ENSURE_SUCCESS(rv, false);
  return IsPotentiallyTrustworthyLoopbackHost(asciiHost);
}

/* Maybe we have a .onion URL. Treat it as whitelisted as well if
 * `dom.securecontext.whitelist_onions` is `true`.
 */
bool nsMixedContentBlocker::IsPotentiallyTrustworthyOnion(nsIURI* aURL) {
  if (!StaticPrefs::dom_securecontext_whitelist_onions()) {
    return false;
  }

  nsAutoCString host;
  nsresult rv = aURL->GetHost(host);
  NS_ENSURE_SUCCESS(rv, false);
  return StringEndsWith(host, NS_LITERAL_CSTRING(".onion"));
}

bool nsMixedContentBlocker::IsPotentiallyTrustworthyOrigin(nsIURI* aURI) {
  // The following implements:
  // https://w3c.github.io/webappsec-secure-contexts/#is-origin-trustworthy

  nsAutoCString scheme;
  nsresult rv = aURI->GetScheme(scheme);
  if (NS_FAILED(rv)) {
    return false;
  }

  // Blobs are expected to inherit their principal so we don't expect to have
  // a content principal with scheme 'blob' here.  We can't assert that though
  // since someone could mess with a non-blob URI to give it that scheme.
  NS_WARNING_ASSERTION(!scheme.EqualsLiteral("blob"),
                       "IsOriginPotentiallyTrustworthy ignoring blob scheme");

  // According to the specification, the user agent may choose to extend the
  // trust to other, vendor-specific URL schemes. We use this for "resource:",
  // which is technically a substituting protocol handler that is not limited to
  // local resource mapping, but in practice is never mapped remotely as this
  // would violate assumptions a lot of code makes.
  // We use nsIProtocolHandler flags to determine which protocols we consider a
  // priori authenticated.
  bool aPrioriAuthenticated = false;
  if (NS_FAILED(NS_URIChainHasFlags(
          aURI, nsIProtocolHandler::URI_IS_POTENTIALLY_TRUSTWORTHY,
          &aPrioriAuthenticated))) {
    return false;
  }

  if (aPrioriAuthenticated) {
    return true;
  }

  nsAutoCString host;
  rv = aURI->GetHost(host);
  if (NS_FAILED(rv)) {
    return false;
  }

  if (IsPotentiallyTrustworthyLoopbackURL(aURI)) {
    return true;
  }

  // If a host is not considered secure according to the default algorithm, then
  // check to see if it has been whitelisted by the user.  We only apply this
  // whitelist for network resources, i.e., those with scheme "http" or "ws".
  // The pref should contain a comma-separated list of hostnames.

  if (!scheme.EqualsLiteral("http") && !scheme.EqualsLiteral("ws")) {
    return false;
  }

  nsAutoCString whitelist;
  rv = Preferences::GetCString("dom.securecontext.whitelist", whitelist);
  if (NS_SUCCEEDED(rv)) {
    nsCCharSeparatedTokenizer tokenizer(whitelist, ',');
    while (tokenizer.hasMoreTokens()) {
      const nsACString& allowedHost = tokenizer.nextToken();
      if (host.Equals(allowedHost)) {
        return true;
      }
    }
  }
  // Maybe we have a .onion URL. Treat it as whitelisted as well if
  // `dom.securecontext.whitelist_onions` is `true`.
  if (nsMixedContentBlocker::IsPotentiallyTrustworthyOnion(aURI)) {
    return true;
  }
  return false;
}

/* Static version of ShouldLoad() that contains all the Mixed Content Blocker
 * logic.  Called from non-static ShouldLoad().
 */
nsresult nsMixedContentBlocker::ShouldLoad(
    bool aHadInsecureImageRedirect, uint32_t aContentType,
    nsIURI* aContentLocation, nsIURI* aRequestingLocation,
    nsISupports* aRequestingContext, const nsACString& aMimeGuess,
    nsIPrincipal* aRequestPrincipal, int16_t* aDecision) {
  // Asserting that we are on the main thread here and hence do not have to lock
  // and unlock sBlockMixedScript and sBlockMixedDisplay before reading/writing
  // to them.
  MOZ_ASSERT(NS_IsMainThread());

  bool isPreload = nsContentUtils::IsPreloadType(aContentType);

  // The content policy type that we receive may be an internal type for
  // scripts.  Let's remember if we have seen a worker type, and reset it to the
  // external type in all cases right now.
  bool isWorkerType =
      aContentType == nsIContentPolicy::TYPE_INTERNAL_WORKER ||
      aContentType == nsIContentPolicy::TYPE_INTERNAL_SHARED_WORKER ||
      aContentType == nsIContentPolicy::TYPE_INTERNAL_SERVICE_WORKER;
  aContentType =
      nsContentUtils::InternalContentPolicyTypeToExternal(aContentType);

  // Assume active (high risk) content and blocked by default
  MixedContentTypes classification = eMixedScript;
  // Make decision to block/reject by default
  *aDecision = REJECT_REQUEST;

  // Notes on non-obvious decisions:
  //
  // TYPE_DTD: A DTD can contain entity definitions that expand to scripts.
  //
  // TYPE_FONT: The TrueType hinting mechanism is basically a scripting
  // language that gets interpreted by the operating system's font rasterizer.
  // Mixed content web fonts are relatively uncommon, and we can can fall back
  // to built-in fonts with minimal disruption in almost all cases.
  //
  // TYPE_OBJECT_SUBREQUEST could actually be either active content (e.g. a
  // script that a plugin will execute) or display content (e.g. Flash video
  // content).  Until we have a way to determine active vs passive content
  // from plugin requests (bug 836352), we will treat this as passive content.
  // This is to prevent false positives from causing users to become
  // desensitized to the mixed content blocker.
  //
  // TYPE_CSP_REPORT: High-risk because they directly leak information about
  // the content of the page, and because blocking them does not have any
  // negative effect on the page loading.
  //
  // TYPE_PING: Ping requests are POSTS, not GETs like images and media.
  // Also, PING requests have no bearing on the rendering or operation of
  // the page when used as designed, so even though they are lower risk than
  // scripts, blocking them is basically risk-free as far as compatibility is
  // concerned.
  //
  // TYPE_STYLESHEET: XSLT stylesheets can insert scripts. CSS positioning
  // and other advanced CSS features can possibly be exploited to cause
  // spoofing attacks (e.g. make a "grant permission" button look like a
  // "refuse permission" button).
  //
  // TYPE_BEACON: Beacon requests are similar to TYPE_PING, and are blocked by
  // default.
  //
  // TYPE_WEBSOCKET: The Websockets API requires browsers to
  // reject mixed-content websockets: "If secure is false but the origin of
  // the entry script has a scheme component that is itself a secure protocol,
  // e.g. HTTPS, then throw a SecurityError exception." We already block mixed
  // content websockets within the websockets implementation, so we don't need
  // to do any blocking here, nor do we need to provide a way to undo or
  // override the blocking. Websockets without TLS are very flaky anyway in the
  // face of many HTTP-aware proxies. Compared to passive content, there is
  // additional risk that the script using WebSockets will disclose sensitive
  // information from the HTTPS page and/or eval (directly or indirectly)
  // received data.
  //
  // TYPE_XMLHTTPREQUEST: XHR requires either same origin or CORS, so most
  // mixed-content XHR will already be blocked by that check. This will also
  // block HTTPS-to-HTTP XHR with CORS. The same security concerns mentioned
  // above for WebSockets apply to XHR, and XHR should have the same security
  // properties as WebSockets w.r.t. mixed content. XHR's handling of redirects
  // amplifies these concerns.
  //
  // TYPE_SAVEAS_DOWNLOAD: Save-link-as feature is used to download a resource
  // without involving a docShell. This kind of loading must be always be
  // allowed.

  static_assert(TYPE_DATAREQUEST == TYPE_XMLHTTPREQUEST,
                "TYPE_DATAREQUEST is not a synonym for "
                "TYPE_XMLHTTPREQUEST");

  switch (aContentType) {
    // The top-level document cannot be mixed content by definition
    case TYPE_DOCUMENT:
      *aDecision = ACCEPT;
      return NS_OK;
    // Creating insecure websocket connections in a secure page is blocked
    // already in the websocket constructor. We don't need to check the blocking
    // here and we don't want to un-block
    case TYPE_WEBSOCKET:
      *aDecision = ACCEPT;
      return NS_OK;

    // Creating insecure connections for a save-as link download is acceptable.
    // This download is completely disconnected from the docShell, but still
    // using the same loading principal.
    case TYPE_SAVEAS_DOWNLOAD:
      *aDecision = ACCEPT;
      return NS_OK;

    // Static display content is considered moderate risk for mixed content so
    // these will be blocked according to the mixed display preference
    case TYPE_IMAGE:
    case TYPE_MEDIA:
      classification = eMixedDisplay;
      break;
    case TYPE_OBJECT_SUBREQUEST:
      if (sBlockMixedObjectSubrequest) {
        classification = eMixedScript;
      } else {
        classification = eMixedDisplay;
      }
      break;

    // Active content (or content with a low value/risk-of-blocking ratio)
    // that has been explicitly evaluated; listed here for documentation
    // purposes and to avoid the assertion and warning for the default case.
    case TYPE_BEACON:
    case TYPE_CSP_REPORT:
    case TYPE_DTD:
    case TYPE_FETCH:
    case TYPE_FONT:
    case TYPE_IMAGESET:
    case TYPE_OBJECT:
    case TYPE_SCRIPT:
    case TYPE_STYLESHEET:
    case TYPE_SUBDOCUMENT:
    case TYPE_PING:
    case TYPE_WEB_MANIFEST:
    case TYPE_XBL:
    case TYPE_XMLHTTPREQUEST:
    case TYPE_XSLT:
    case TYPE_OTHER:
    case TYPE_SPECULATIVE:
      break;

    // This content policy works as a whitelist.
    default:
      MOZ_ASSERT(false, "Mixed content of unknown type");
  }

  // Make sure to get the URI the load started with. No need to check
  // outer schemes because all the wrapping pseudo protocols inherit the
  // security properties of the actual network request represented
  // by the innerMost URL.
  nsCOMPtr<nsIURI> innerContentLocation = NS_GetInnermostURI(aContentLocation);
  if (!innerContentLocation) {
    NS_ERROR("Can't get innerURI from aContentLocation");
    *aDecision = REJECT_REQUEST;
    return NS_OK;
  }

  // TYPE_IMAGE redirects are cached based on the original URI, not the final
  // destination and hence cache hits for images may not have the correct
  // innerContentLocation.  Check if the cached hit went through an http
  // redirect, and if it did, we can't treat this as a secure subresource.
  if (!aHadInsecureImageRedirect &&
      URISafeToBeLoadedInSecureContext(innerContentLocation)) {
    *aDecision = ACCEPT;
    return NS_OK;
  }

  // Since there are cases where aRequestingLocation and aRequestPrincipal are
  // definitely not the owning document, we try to ignore them by extracting the
  // requestingLocation in the following order:
  // 1) from the aRequestingContext, either extracting
  //    a) the node's principal, or the
  //    b) script object's principal.
  // 2) if aRequestingContext yields a principal but no location, we check
  //    if its the system principal. If it is, allow the load.
  // 3) Special case handling for:
  //    a) speculative loads, where shouldLoad is called twice (bug 839235)
  //       and the first speculative load does not include a context.
  //       In this case we use aRequestingLocation to set requestingLocation.
  //    b) TYPE_CSP_REPORT which does not provide a context. In this case we
  //       use aRequestingLocation to set requestingLocation.
  //    c) content scripts from addon code that do not provide
  //       aRequestingContext or aRequestingLocation, but do provide
  //       aRequestPrincipal. If aRequestPrincipal is an expanded principal,
  //       we allow the load.
  // 4) If we still end up not having a requestingLocation, we reject the load.

  nsCOMPtr<nsIPrincipal> principal;
  // 1a) Try to get the principal if aRequestingContext is a node.
  nsCOMPtr<nsINode> node = do_QueryInterface(aRequestingContext);
  if (node) {
    principal = node->NodePrincipal();
  }

  // 1b) Try using the window's script object principal if it's not a node.
  if (!principal) {
    nsCOMPtr<nsIScriptObjectPrincipal> scriptObjPrin =
        do_QueryInterface(aRequestingContext);
    if (scriptObjPrin) {
      principal = scriptObjPrin->GetPrincipal();
    }
  }

  nsCOMPtr<nsIURI> requestingLocation;
  if (principal) {
    principal->GetURI(getter_AddRefs(requestingLocation));
  }

  // 2) if aRequestingContext yields a principal but no location, we check if
  // its a system principal.
  if (principal && !requestingLocation) {
    if (nsContentUtils::IsSystemPrincipal(principal)) {
      *aDecision = ACCEPT;
      return NS_OK;
    }
  }

  // 3a,b) Special case handling for speculative loads and TYPE_CSP_REPORT. In
  // such cases, aRequestingContext doesn't exist, so we use
  // aRequestingLocation. Unfortunately we can not distinguish between
  // speculative and normal loads here, otherwise we could special case this
  // assignment.
  if (!requestingLocation) {
    requestingLocation = aRequestingLocation;
  }

  // 3c) Special case handling for content scripts from addons code, which only
  // provide a aRequestPrincipal; aRequestingContext and aRequestingLocation are
  // both null; if the aRequestPrincipal is an expandedPrincipal, we allow the
  // load.
  if (!principal && !requestingLocation && aRequestPrincipal) {
    nsCOMPtr<nsIExpandedPrincipal> expanded =
        do_QueryInterface(aRequestPrincipal);
    if (expanded) {
      *aDecision = ACCEPT;
      return NS_OK;
    }
  }

  // 4) Giving up. We still don't have a requesting location, therefore we can't
  // tell
  //    if this is a mixed content load. Deny to be safe.
  if (!requestingLocation) {
    *aDecision = REJECT_REQUEST;
    return NS_OK;
  }

  // Check the parent scheme. If it is not an HTTPS page then mixed content
  // restrictions do not apply.
  nsCOMPtr<nsIURI> innerRequestingLocation =
      NS_GetInnermostURI(requestingLocation);
  if (!innerRequestingLocation) {
    NS_ERROR("Can't get innerURI from requestingLocation");
    *aDecision = REJECT_REQUEST;
    return NS_OK;
  }

  bool parentIsHttps = innerRequestingLocation->SchemeIs("https");
  if (!parentIsHttps) {
    *aDecision = ACCEPT;
    return NS_OK;
  }

  nsCOMPtr<nsIDocShell> docShell =
      NS_CP_GetDocShellFromContext(aRequestingContext);
  NS_ENSURE_TRUE(docShell, NS_OK);

  // Disallow mixed content loads for workers, shared workers and service
  // workers.
  if (isWorkerType) {
    // For workers, we can assume that we're mixed content at this point, since
    // the parent is https, and the protocol associated with
    // innerContentLocation doesn't map to the secure URI flags checked above.
    // Assert this for sanity's sake
#ifdef DEBUG
    bool isHttpsScheme = innerContentLocation->SchemeIs("https");
    MOZ_ASSERT(!isHttpsScheme);
#endif
    *aDecision = REJECT_REQUEST;
    return NS_OK;
  }

  bool isHttpScheme = innerContentLocation->SchemeIs("http");
  if (isHttpScheme && IsPotentiallyTrustworthyOrigin(innerContentLocation)) {
    *aDecision = ACCEPT;
    return NS_OK;
  }

  // The page might have set the CSP directive 'upgrade-insecure-requests'. In
  // such a case allow the http: load to succeed with the promise that the
  // channel will get upgraded to https before fetching any data from the
  // netwerk. Please see: nsHttpChannel::Connect()
  //
  // Please note that the CSP directive 'upgrade-insecure-requests' only applies
  // to http: and ws: (for websockets). Websockets are not subject to mixed
  // content blocking since insecure websockets are not allowed within secure
  // pages. Hence, we only have to check against http: here. Skip mixed content
  // blocking if the subresource load uses http: and the CSP directive
  // 'upgrade-insecure-requests' is present on the page.
  Document* document = docShell->GetDocument();
  MOZ_ASSERT(document, "Expected a document");
  if (isHttpScheme && document->GetUpgradeInsecureRequests(isPreload)) {
    *aDecision = ACCEPT;
    return NS_OK;
  }

  // Allow http: mixed content if we are choosing to upgrade them when the
  // pref "security.mixed_content.upgrade_display_content" is true.
  // This behaves like GetUpgradeInsecureRequests above in that the channel will
  // be upgraded to https before fetching any data from the netwerk.
  bool isUpgradableDisplayType =
      nsContentUtils::IsUpgradableDisplayType(aContentType) &&
      ShouldUpgradeMixedDisplayContent();
  if (isHttpScheme && isUpgradableDisplayType) {
    *aDecision = ACCEPT;
    return NS_OK;
  }

  // The page might have set the CSP directive 'block-all-mixed-content' which
  // should block not only active mixed content loads but in fact all mixed
  // content loads, see https://www.w3.org/TR/mixed-content/#strict-checking
  // Block all non secure loads in case the CSP directive is present. Please
  // note that at this point we already know, based on |schemeSecure| that the
  // load is not secure, so we can bail out early at this point.
  if (document->GetBlockAllMixedContent(isPreload)) {
    // log a message to the console before returning.
    nsAutoCString spec;
    nsresult rv = aContentLocation->GetSpec(spec);
    NS_ENSURE_SUCCESS(rv, rv);

    AutoTArray<nsString, 1> params;
    CopyUTF8toUTF16(spec, *params.AppendElement());

    CSP_LogLocalizedStr(
        "blockAllMixedContent", params,
        EmptyString(),  // aSourceFile
        EmptyString(),  // aScriptSample
        0,              // aLineNumber
        0,              // aColumnNumber
        nsIScriptError::errorFlag, NS_LITERAL_CSTRING("blockAllMixedContent"),
        document->InnerWindowID(),
        !!document->NodePrincipal()->OriginAttributesRef().mPrivateBrowsingId);
    *aDecision = REJECT_REQUEST;
    return NS_OK;
  }

  // Determine if the rootDoc is https and if the user decided to allow Mixed
  // Content
  bool rootHasSecureConnection = false;
  bool allowMixedContent = false;
  bool isRootDocShell = false;
  nsresult rv = docShell->GetAllowMixedContentAndConnectionData(
      &rootHasSecureConnection, &allowMixedContent, &isRootDocShell);
  if (NS_FAILED(rv)) {
    *aDecision = REJECT_REQUEST;
    return rv;
  }

  // Get the sameTypeRoot tree item from the docshell
  nsCOMPtr<nsIDocShellTreeItem> sameTypeRoot;
  docShell->GetInProcessSameTypeRootTreeItem(getter_AddRefs(sameTypeRoot));
  NS_ASSERTION(sameTypeRoot, "No root tree item from docshell!");

  // When navigating an iframe, the iframe may be https
  // but its parents may not be.  Check the parents to see if any of them are
  // https. If none of the parents are https, allow the load.
  if (aContentType == TYPE_SUBDOCUMENT && !rootHasSecureConnection) {
    bool httpsParentExists = false;

    nsCOMPtr<nsIDocShellTreeItem> parentTreeItem;
    parentTreeItem = docShell;

    while (!httpsParentExists && parentTreeItem) {
      nsCOMPtr<nsIWebNavigation> parentAsNav(do_QueryInterface(parentTreeItem));
      NS_ASSERTION(parentAsNav,
                   "No web navigation object from parent's docshell tree item");
      nsCOMPtr<nsIURI> parentURI;

      parentAsNav->GetCurrentURI(getter_AddRefs(parentURI));
      if (!parentURI) {
        // if getting the URI fails, assume there is a https parent and break.
        httpsParentExists = true;
        break;
      }

      nsCOMPtr<nsIURI> innerParentURI = NS_GetInnermostURI(parentURI);
      if (!innerParentURI) {
        NS_ERROR("Can't get innerURI from parentURI");
        *aDecision = REJECT_REQUEST;
        return NS_OK;
      }

      httpsParentExists = innerParentURI->SchemeIs("https");

      // When the parent and the root are the same, we have traversed all the
      // way up the same type docshell tree.  Break out of the while loop.
      if (sameTypeRoot == parentTreeItem) {
        break;
      }

      // update the parent to the grandparent.
      nsCOMPtr<nsIDocShellTreeItem> newParentTreeItem;
      parentTreeItem->GetInProcessSameTypeParent(
          getter_AddRefs(newParentTreeItem));
      parentTreeItem = newParentTreeItem;
    }  // end while loop.

    if (!httpsParentExists) {
      *aDecision = nsIContentPolicy::ACCEPT;
      return NS_OK;
    }
  }

  // Get the root document from the sameTypeRoot
  nsCOMPtr<Document> rootDoc = sameTypeRoot->GetDocument();
  NS_ASSERTION(rootDoc, "No root document from document shell root tree item.");

  nsDocShell* nativeDocShell = nsDocShell::Cast(docShell);
  nsCOMPtr<nsIDocShell> rootShell = do_GetInterface(sameTypeRoot);
  NS_ASSERTION(rootShell,
               "No root docshell from document shell root tree item.");
  uint32_t state = nsIWebProgressListener::STATE_IS_BROKEN;
  nsCOMPtr<nsISecureBrowserUI> securityUI;
  rootShell->GetSecurityUI(getter_AddRefs(securityUI));
  // If there is no securityUI, document doesn't have a security state.
  // Allow load and return early.
  if (!securityUI) {
    *aDecision = nsIContentPolicy::ACCEPT;
    return NS_OK;
  }
  nsresult stateRV = securityUI->GetState(&state);

  OriginAttributes originAttributes;
  if (principal) {
    originAttributes = principal->OriginAttributesRef();
  } else if (aRequestPrincipal) {
    originAttributes = aRequestPrincipal->OriginAttributesRef();
  }

  // At this point we know that the request is mixed content, and the only
  // question is whether we block it.  Record telemetry at this point as to
  // whether HSTS would have fixed things by making the content location
  // into an HTTPS URL.
  //
  // Note that we count this for redirects as well as primary requests. This
  // will cause some degree of double-counting, especially when mixed content
  // is not blocked (e.g., for images).  For more detail, see:
  //   https://bugzilla.mozilla.org/show_bug.cgi?id=1198572#c19
  //
  // We do not count requests aHadInsecureImageRedirect=true, since these are
  // just an artifact of the image caching system.
  bool active = (classification == eMixedScript);
  if (!aHadInsecureImageRedirect) {
    if (XRE_IsParentProcess()) {
      AccumulateMixedContentHSTS(innerContentLocation, active,
                                 originAttributes);
    } else {
      // Ask the parent process to do the same call
      mozilla::dom::ContentChild* cc =
          mozilla::dom::ContentChild::GetSingleton();
      if (cc) {
        mozilla::ipc::URIParams uri;
        SerializeURI(innerContentLocation, uri);
        cc->SendAccumulateMixedContentHSTS(uri, active, originAttributes);
      }
    }
  }

  // set hasMixedContentObjectSubrequest on this object if necessary
  if (aContentType == TYPE_OBJECT_SUBREQUEST) {
    if (!sBlockMixedObjectSubrequest) {
      rootDoc->WarnOnceAbout(Document::eMixedDisplayObjectSubrequest);
    }
    rootDoc->SetHasMixedContentObjectSubrequest(true);
  }

  // If the content is display content, and the pref says display content should
  // be blocked, block it.
  if (sBlockMixedDisplay && classification == eMixedDisplay) {
    if (allowMixedContent) {
      LogMixedContentMessage(classification, aContentLocation, rootDoc,
                             eUserOverride);
      *aDecision = nsIContentPolicy::ACCEPT;
      // See if mixed display content has already loaded on the page or if the
      // state needs to be updated here. If mixed display hasn't loaded
      // previously, then we need to call OnSecurityChange() to update the UI.
      if (rootDoc->GetHasMixedDisplayContentLoaded()) {
        return NS_OK;
      }
      rootDoc->SetHasMixedDisplayContentLoaded(true);

      if (rootHasSecureConnection) {
        // reset state security flag
        state = state >> 4 << 4;
        // set state security flag to broken, since there is mixed content
        state |= nsIWebProgressListener::STATE_IS_BROKEN;

        // If mixed active content is loaded, make sure to include that in the
        // state.
        if (rootDoc->GetHasMixedActiveContentLoaded()) {
          state |= nsIWebProgressListener::STATE_LOADED_MIXED_ACTIVE_CONTENT;
        }

        nativeDocShell->nsDocLoader::OnSecurityChange(
            aRequestingContext,
            (state |
             nsIWebProgressListener::STATE_LOADED_MIXED_DISPLAY_CONTENT));
      } else {
        // User has overriden the pref and the root is not https;
        // mixed display content was allowed on an https subframe.
        if (NS_SUCCEEDED(stateRV)) {
          nativeDocShell->nsDocLoader::OnSecurityChange(
              aRequestingContext,
              (state |
               nsIWebProgressListener::STATE_LOADED_MIXED_DISPLAY_CONTENT));
        }
      }
    } else {
      *aDecision = nsIContentPolicy::REJECT_REQUEST;
      LogMixedContentMessage(classification, aContentLocation, rootDoc,
                             eBlocked);
      if (!rootDoc->GetHasMixedDisplayContentBlocked() &&
          NS_SUCCEEDED(stateRV)) {
        rootDoc->SetHasMixedDisplayContentBlocked(true);
        nativeDocShell->nsDocLoader::OnSecurityChange(
            aRequestingContext,
            (state |
             nsIWebProgressListener::STATE_BLOCKED_MIXED_DISPLAY_CONTENT));
      }
    }
    return NS_OK;

  } else if (sBlockMixedScript && classification == eMixedScript) {
    // If the content is active content, and the pref says active content should
    // be blocked, block it unless the user has choosen to override the pref
    if (allowMixedContent) {
      LogMixedContentMessage(classification, aContentLocation, rootDoc,
                             eUserOverride);
      *aDecision = nsIContentPolicy::ACCEPT;
      // See if the state will change here. If it will, only then do we need to
      // call OnSecurityChange() to update the UI.
      if (rootDoc->GetHasMixedActiveContentLoaded()) {
        return NS_OK;
      }
      rootDoc->SetHasMixedActiveContentLoaded(true);

      if (rootHasSecureConnection) {
        // reset state security flag
        state = state >> 4 << 4;
        // set state security flag to broken, since there is mixed content
        state |= nsIWebProgressListener::STATE_IS_BROKEN;

        // If mixed display content is loaded, make sure to include that in the
        // state.
        if (rootDoc->GetHasMixedDisplayContentLoaded()) {
          state |= nsIWebProgressListener::STATE_LOADED_MIXED_DISPLAY_CONTENT;
        }

        nativeDocShell->nsDocLoader::OnSecurityChange(
            aRequestingContext,
            (state |
             nsIWebProgressListener::STATE_LOADED_MIXED_ACTIVE_CONTENT));

        return NS_OK;
      } else {
        // User has already overriden the pref and the root is not https;
        // mixed active content was allowed on an https subframe.
        if (NS_SUCCEEDED(stateRV)) {
          nativeDocShell->nsDocLoader::OnSecurityChange(
              aRequestingContext,
              (state |
               nsIWebProgressListener::STATE_LOADED_MIXED_ACTIVE_CONTENT));
        }
        return NS_OK;
      }
    } else {
      // User has not overriden the pref by Disabling protection. Reject the
      // request and update the security state.
      *aDecision = nsIContentPolicy::REJECT_REQUEST;
      LogMixedContentMessage(classification, aContentLocation, rootDoc,
                             eBlocked);
      // See if the pref will change here. If it will, only then do we need to
      // call OnSecurityChange() to update the UI.
      if (rootDoc->GetHasMixedActiveContentBlocked()) {
        return NS_OK;
      }
      rootDoc->SetHasMixedActiveContentBlocked(true);

      // The user has not overriden the pref, so make sure they still have an
      // option by calling nativeDocShell which will invoke the doorhanger
      if (NS_SUCCEEDED(stateRV)) {
        nativeDocShell->nsDocLoader::OnSecurityChange(
            aRequestingContext,
            (state |
             nsIWebProgressListener::STATE_BLOCKED_MIXED_ACTIVE_CONTENT));
      }
      return NS_OK;
    }
  } else {
    // The content is not blocked by the mixed content prefs.

    // Log a message that we are loading mixed content.
    LogMixedContentMessage(classification, aContentLocation, rootDoc,
                           eUserOverride);

    // Fire the event from a script runner as it is unsafe to run script
    // from within ShouldLoad
    nsContentUtils::AddScriptRunner(new nsMixedContentEvent(
        aRequestingContext, classification, rootHasSecureConnection));
    *aDecision = ACCEPT;
    return NS_OK;
  }
}

bool nsMixedContentBlocker::URISafeToBeLoadedInSecureContext(nsIURI* aURI) {
  /* Returns a bool if the URI can be loaded as a sub resource safely.
   *
   * Check Protocol Flags to determine if scheme is safe to load:
   * URI_DOES_NOT_RETURN_DATA - e.g.
   *   "mailto"
   * URI_IS_LOCAL_RESOURCE - e.g.
   *   "data",
   *   "resource",
   *   "moz-icon"
   * URI_INHERITS_SECURITY_CONTEXT - e.g.
   *   "javascript"
   * URI_IS_POTENTIALLY_TRUSTWORTHY - e.g.
   *   "https",
   *   "moz-safe-about"
   *
   */
  bool schemeLocal = false;
  bool schemeNoReturnData = false;
  bool schemeInherits = false;
  bool schemeSecure = false;
  if (NS_FAILED(NS_URIChainHasFlags(
          aURI, nsIProtocolHandler::URI_IS_LOCAL_RESOURCE, &schemeLocal)) ||
      NS_FAILED(NS_URIChainHasFlags(
          aURI, nsIProtocolHandler::URI_DOES_NOT_RETURN_DATA,
          &schemeNoReturnData)) ||
      NS_FAILED(NS_URIChainHasFlags(
          aURI, nsIProtocolHandler::URI_INHERITS_SECURITY_CONTEXT,
          &schemeInherits)) ||
      NS_FAILED(NS_URIChainHasFlags(
          aURI, nsIProtocolHandler::URI_IS_POTENTIALLY_TRUSTWORTHY,
          &schemeSecure))) {
    return false;
  }
  return (schemeLocal || schemeNoReturnData || schemeInherits || schemeSecure);
}

NS_IMETHODIMP
nsMixedContentBlocker::ShouldProcess(nsIURI* aContentLocation,
                                     nsILoadInfo* aLoadInfo,
                                     const nsACString& aMimeGuess,
                                     int16_t* aDecision) {
  if (!aContentLocation) {
    // aContentLocation may be null when a plugin is loading without an
    // associated URI resource
    if (aLoadInfo->GetExternalContentPolicyType() == TYPE_OBJECT) {
      *aDecision = ACCEPT;
      return NS_OK;
    }

    NS_SetRequestBlockingReason(aLoadInfo,
                                nsILoadInfo::BLOCKING_REASON_MIXED_BLOCKED);
    *aDecision = REJECT_REQUEST;
    return NS_ERROR_FAILURE;
  }

  return ShouldLoad(aContentLocation, aLoadInfo, aMimeGuess, aDecision);
}

// Record information on when HSTS would have made mixed content not mixed
// content (regardless of whether it was actually blocked)
void nsMixedContentBlocker::AccumulateMixedContentHSTS(
    nsIURI* aURI, bool aActive, const OriginAttributes& aOriginAttributes) {
  // This method must only be called in the parent, because
  // nsSiteSecurityService is only available in the parent
  if (!XRE_IsParentProcess()) {
    MOZ_ASSERT(false);
    return;
  }

  bool hsts;
  nsresult rv;
  nsCOMPtr<nsISiteSecurityService> sss =
      do_GetService(NS_SSSERVICE_CONTRACTID, &rv);
  if (NS_FAILED(rv)) {
    return;
  }
  rv = sss->IsSecureURI(nsISiteSecurityService::HEADER_HSTS, aURI, 0,
                        aOriginAttributes, nullptr, nullptr, &hsts);
  if (NS_FAILED(rv)) {
    return;
  }

  // states: would upgrade, would prime, hsts info cached
  // active, passive
  //
  if (!aActive) {
    if (!hsts) {
      Telemetry::Accumulate(Telemetry::MIXED_CONTENT_HSTS,
                            MCB_HSTS_PASSIVE_NO_HSTS);
    } else {
      Telemetry::Accumulate(Telemetry::MIXED_CONTENT_HSTS,
                            MCB_HSTS_PASSIVE_WITH_HSTS);
    }
  } else {
    if (!hsts) {
      Telemetry::Accumulate(Telemetry::MIXED_CONTENT_HSTS,
                            MCB_HSTS_ACTIVE_NO_HSTS);
    } else {
      Telemetry::Accumulate(Telemetry::MIXED_CONTENT_HSTS,
                            MCB_HSTS_ACTIVE_WITH_HSTS);
    }
  }
}

bool nsMixedContentBlocker::ShouldUpgradeMixedDisplayContent() {
  return sUpgradeMixedDisplay;
}
