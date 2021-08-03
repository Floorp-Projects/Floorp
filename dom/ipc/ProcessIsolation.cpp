/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/ProcessIsolation.h"

#include "mozilla/Assertions.h"
#include "mozilla/dom/BrowsingContextGroup.h"
#include "mozilla/dom/CanonicalBrowsingContext.h"
#include "mozilla/dom/ContentChild.h"
#include "mozilla/dom/ContentParent.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/RemoteType.h"
#include "mozilla/dom/WindowGlobalParent.h"
#include "mozilla/extensions/WebExtensionPolicy.h"
#include "mozilla/BasePrincipal.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/ContentPrincipal.h"
#include "mozilla/ExtensionPolicyService.h"
#include "mozilla/Logging.h"
#include "mozilla/NullPrincipal.h"
#include "mozilla/Preferences.h"
#include "mozilla/RefPtr.h"
#include "mozilla/StaticPrefs_browser.h"
#include "mozilla/StaticPtr.h"
#include "nsAboutProtocolUtils.h"
#include "nsDocShell.h"
#include "nsError.h"
#include "nsIChromeRegistry.h"
#include "nsIHttpChannel.h"
#include "nsIHttpChannelInternal.h"
#include "nsIProtocolHandler.h"
#include "nsIXULRuntime.h"
#include "nsNetUtil.h"
#include "nsServiceManagerUtils.h"
#include "nsSHistory.h"
#include "nsURLHelper.h"

namespace mozilla::dom {

mozilla::LazyLogModule gProcessIsolationLog{"ProcessIsolation"};

namespace {

/**
 * Helper class for caching the result of splitting prefs which are represented
 * as a comma-separated list of strings.
 */
struct CommaSeparatedPref {
 public:
  explicit constexpr CommaSeparatedPref(nsLiteralCString aPrefName)
      : mPrefName(aPrefName) {}

  void OnChange() {
    if (mValues) {
      mValues->Clear();
      nsAutoCString prefValue;
      if (NS_SUCCEEDED(Preferences::GetCString(mPrefName.get(), prefValue))) {
        for (const auto& value :
             nsCCharSeparatedTokenizer(prefValue, ',').ToRange()) {
          mValues->EmplaceBack(value);
        }
      }
    }
  }

  const nsTArray<nsCString>& Get() {
    if (!mValues) {
      mValues = new nsTArray<nsCString>;
      Preferences::RegisterCallbackAndCall(
          [](const char*, void* aData) {
            static_cast<CommaSeparatedPref*>(aData)->OnChange();
          },
          mPrefName, this);
      RunOnShutdown([this] {
        delete this->mValues;
        this->mValues = nullptr;
      });
    }
    return *mValues;
  }

  auto begin() { return Get().cbegin(); }
  auto end() { return Get().cend(); }

 private:
  nsLiteralCString mPrefName;
  nsTArray<nsCString>* MOZ_OWNING_REF mValues = nullptr;
};

CommaSeparatedPref sSeparatedMozillaDomains{
    "browser.tabs.remote.separatedMozillaDomains"_ns};

/**
 * Certain URIs have special isolation behaviour, and need to be loaded within
 * specific process types.
 */
enum class IsolationBehavior {
  // This URI loads web content and should be treated as a content load, being
  // isolated based on the response principal.
  WebContent,
  // Forcibly load in a process with the "web" remote type.
  ForceWebRemoteType,
  // Load this URI in the privileged about content process.
  PrivilegedAbout,
  // Load this URI in the extension process.
  Extension,
  // Load this URI in the file content process.
  File,
  // Load this URI in the priviliged mozilla content process.
  PrivilegedMozilla,
  // Load this URI explicitly in the parent process.
  Parent,
  // Load this URI wherever the browsing context is currently loaded. This is
  // generally used for error pages.
  Anywhere,
  // May only be returned for subframes. Inherits the remote type of the parent
  // document which is embedding this document.
  Inherit,
  // Special case for the `about:reader` URI which should be loaded in the same
  // process which would be used for the "url" query parameter.
  AboutReader,
  // There was a fatal error, and the load should be aborted.
  Error,
};

/**
 * Returns a static string with the name of the given isolation behaviour. For
 * use in logging code.
 */
static const char* IsolationBehaviorName(IsolationBehavior aBehavior) {
  switch (aBehavior) {
    case IsolationBehavior::WebContent:
      return "WebContent";
    case IsolationBehavior::ForceWebRemoteType:
      return "ForceWebRemoteType";
    case IsolationBehavior::PrivilegedAbout:
      return "PrivilegedAbout";
    case IsolationBehavior::Extension:
      return "Extension";
    case IsolationBehavior::File:
      return "File";
    case IsolationBehavior::PrivilegedMozilla:
      return "PrivilegedMozilla";
    case IsolationBehavior::Parent:
      return "Parent";
    case IsolationBehavior::Anywhere:
      return "Anywhere";
    case IsolationBehavior::Inherit:
      return "Inherit";
    case IsolationBehavior::AboutReader:
      return "AboutReader";
    case IsolationBehavior::Error:
      return "Error";
    default:
      return "Unknown";
  }
}

/**
 * Check if a given URI has specialized process isolation behaviour, such as
 * needing to be loaded within a specific type of content process.
 *
 * When handling a navigation, this method will be called twice: first with the
 * channel's creation URI, and then it will be called with a result principal's
 * URI.
 */
static IsolationBehavior IsolationBehaviorForURI(nsIURI* aURI, bool aIsSubframe,
                                                 bool aForChannelCreationURI) {
  nsAutoCString scheme;
  MOZ_ALWAYS_SUCCEEDS(aURI->GetScheme(scheme));

  if (scheme == "chrome"_ns) {
    // `chrome://` URIs are always loaded in the parent process, unless they
    // have opted in to loading in a content process. This is currently only
    // done in tests.
    //
    // FIXME: These flags should be removed from `chrome` URIs at some point.
    nsCOMPtr<nsIXULChromeRegistry> chromeReg =
        do_GetService("@mozilla.org/chrome/chrome-registry;1");
    bool mustLoadRemotely = false;
    if (NS_SUCCEEDED(chromeReg->MustLoadURLRemotely(aURI, &mustLoadRemotely)) &&
        mustLoadRemotely) {
      return IsolationBehavior::ForceWebRemoteType;
    }
    bool canLoadRemotely = false;
    if (NS_SUCCEEDED(chromeReg->CanLoadURLRemotely(aURI, &canLoadRemotely)) &&
        canLoadRemotely) {
      return IsolationBehavior::Anywhere;
    }
    return IsolationBehavior::Parent;
  }

  if (scheme == "about"_ns) {
    nsAutoCString path;
    MOZ_ALWAYS_SUCCEEDS(NS_GetAboutModuleName(aURI, path));

    // The `about:blank` and `about:srcdoc` pages are loaded by normal web
    // content, and should be allocated processes based on their simple content
    // principals.
    if (path == "blank"_ns || path == "srcdoc"_ns) {
      return IsolationBehavior::WebContent;
    }

    // If we're loading an `about:reader` URI, perform isolation based on the
    // principal of the URI being loaded.
    if (path == "reader"_ns && aForChannelCreationURI) {
      return IsolationBehavior::AboutReader;
    }

    // Otherwise, we're going to be loading an about: page. Consult the module.
    nsCOMPtr<nsIAboutModule> aboutModule;
    if (NS_FAILED(NS_GetAboutModule(aURI, getter_AddRefs(aboutModule))) ||
        !aboutModule) {
      // If we don't know of an about: module for this load, it's going to end
      // up being a network error. Allow the load to finish as normal.
      return IsolationBehavior::WebContent;
    }

    // NOTE: about modules can be implemented in JS, so this may run script, and
    // therefore can spuriously fail.
    uint32_t flags = 0;
    if (NS_FAILED(aboutModule->GetURIFlags(aURI, &flags))) {
      NS_WARNING(
          "nsIAboutModule::GetURIFlags unexpectedly failed. Abort the load");
      return IsolationBehavior::Error;
    }

    if (flags & nsIAboutModule::URI_MUST_LOAD_IN_EXTENSION_PROCESS) {
      return IsolationBehavior::Extension;
    }

    if (flags & nsIAboutModule::URI_MUST_LOAD_IN_CHILD) {
      if (flags & nsIAboutModule::URI_CAN_LOAD_IN_PRIVILEGEDABOUT_PROCESS) {
        return IsolationBehavior::PrivilegedAbout;
      }
      return IsolationBehavior::ForceWebRemoteType;
    }

    if (flags & nsIAboutModule::URI_CAN_LOAD_IN_CHILD) {
      return IsolationBehavior::Anywhere;
    }

    return IsolationBehavior::Parent;
  }

  // If the test-only `dataUriInDefaultWebProcess` pref is enabled, dump all
  // `data:` URIs in a "web" content process, rather than loading them in
  // content processes based on their precursor origins.
  if (StaticPrefs::browser_tabs_remote_dataUriInDefaultWebProcess() &&
      scheme == "data"_ns) {
    return IsolationBehavior::ForceWebRemoteType;
  }

  // Make sure to unwrap nested URIs before we early return for channel creation
  // URI. The checks past this point are intended to operate on the principal,
  // which has it's origin constructed from the innermost URI.
  nsCOMPtr<nsIURI> inner;
  if (nsCOMPtr<nsINestedURI> nested = do_QueryInterface(aURI);
      nested && NS_SUCCEEDED(nested->GetInnerURI(getter_AddRefs(inner)))) {
    return IsolationBehaviorForURI(inner, aIsSubframe, aForChannelCreationURI);
  }

  // If we're doing the initial check based on the channel creation URI, stop
  // here as we want to only perform the following checks on the true channel
  // result principal.
  if (aForChannelCreationURI) {
    return IsolationBehavior::WebContent;
  }

  // Protocols used by Thunderbird to display email messages.
  if (scheme == "imap"_ns || scheme == "mailbox"_ns || scheme == "news"_ns ||
      scheme == "nntp"_ns || scheme == "snews"_ns) {
    return IsolationBehavior::Parent;
  }

  // There is more handling for extension content processes in the caller, but
  // they should load in an extension content process unless we're loading a
  // subframe.
  if (scheme == "moz-extension"_ns) {
    if (aIsSubframe) {
      // As a temporary measure, extension iframes must be loaded within the
      // same process as their parent document.
      return IsolationBehavior::Inherit;
    }
    return IsolationBehavior::Extension;
  }

  if (scheme == "file"_ns) {
    return IsolationBehavior::File;
  }

  // Check if the URI is listed as a privileged mozilla content process.
  if (scheme == "https"_ns &&
      StaticPrefs::
          browser_tabs_remote_separatePrivilegedMozillaWebContentProcess()) {
    nsAutoCString host;
    if (NS_SUCCEEDED(aURI->GetAsciiHost(host))) {
      for (const auto& separatedDomain : sSeparatedMozillaDomains) {
        // If the domain exactly matches our host, or our host ends with "." +
        // separatedDomain, we consider it matching.
        if (separatedDomain == host ||
            (separatedDomain.Length() < host.Length() &&
             host.CharAt(host.Length() - separatedDomain.Length() - 1) == '.' &&
             StringEndsWith(host, separatedDomain))) {
          return IsolationBehavior::PrivilegedMozilla;
        }
      }
    }
  }

  nsCOMPtr<nsIScriptSecurityManager> secMan =
      nsContentUtils::GetSecurityManager();
  bool inFileURIAllowList = false;
  if (NS_SUCCEEDED(secMan->InFileURIAllowlist(aURI, &inFileURIAllowList)) &&
      inFileURIAllowList) {
    return IsolationBehavior::File;
  }

  return IsolationBehavior::WebContent;
}

/**
 * Helper method for logging the origin of a principal as a string.
 */
static nsAutoCString OriginString(nsIPrincipal* aPrincipal) {
  nsAutoCString origin;
  aPrincipal->GetOrigin(origin);
  return origin;
}

/**
 * Given an about:reader URI, extract the "url" query parameter, and use it to
 * construct a principal which should be sed for process selection.
 */
static already_AddRefed<BasePrincipal> GetAboutReaderURLPrincipal(
    nsIURI* aURI, const OriginAttributes& aAttrs) {
#ifdef DEBUG
  MOZ_ASSERT(aURI->SchemeIs("about"));
  nsAutoCString path;
  MOZ_ALWAYS_SUCCEEDS(NS_GetAboutModuleName(aURI, path));
  MOZ_ASSERT(path == "reader"_ns);
#endif

  nsAutoCString query;
  MOZ_ALWAYS_SUCCEEDS(aURI->GetQuery(query));

  // Extract the "url" parameter from the `about:reader`'s query parameters,
  // and recover a content principal from it.
  nsAutoString readerSpec;
  if (URLParams::Extract(query, u"url"_ns, readerSpec)) {
    nsCOMPtr<nsIURI> readerUri;
    if (NS_SUCCEEDED(NS_NewURI(getter_AddRefs(readerUri), readerSpec))) {
      return BasePrincipal::CreateContentPrincipal(readerUri, aAttrs);
    }
  }
  return nullptr;
}

/**
 * Check if the given load has the `Large-Allocation` header set, and the header
 * is enabled.
 */
static bool IsLargeAllocationLoad(CanonicalBrowsingContext* aBrowsingContext,
                                  nsIChannel* aChannel) {
  if (!StaticPrefs::dom_largeAllocationHeader_enabled() ||
      aBrowsingContext->UseRemoteSubframes()) {
    return false;
  }

  nsCOMPtr<nsIHttpChannel> httpChannel = do_QueryInterface(aChannel);
  if (!httpChannel) {
    return false;
  }

  nsAutoCString ignoredHeaderValue;
  nsresult rv =
      httpChannel->GetResponseHeader("Large-Allocation"_ns, ignoredHeaderValue);
  if (NS_FAILED(rv)) {
    return false;
  }

  // On all platforms other than win32, LargeAllocation is disabled by default,
  // and has to be force-enabled using `dom.largeAllocation.forceEnable`.
#if defined(XP_WIN) && defined(_X86_)
  return true;
#else
  return StaticPrefs::dom_largeAllocation_forceEnable();
#endif
}

/**
 * Returns `true` if loads for this site should be isolated on a per-site basis.
 * If `aTopBC` is nullptr, this is being called to check if a shared or service
 * worker should be isolated.
 */
static bool ShouldIsolateSite(nsIPrincipal* aPrincipal,
                              CanonicalBrowsingContext* aTopBC) {
  // non-content principals currently can't have webIsolated remote types
  // assigned to them, so should not be isolated.
  if (!aPrincipal->GetIsContentPrincipal()) {
    return false;
  }

  // FIXME: This should contain logic to allow enabling/disabling whether a
  // particular site should be isolated for e.g. android, where we may want to
  // turn on/off isolating certain sites at runtime.
  if (aTopBC) {
    return aTopBC->UseRemoteSubframes();
  }
  return mozilla::FissionAutostart();
}

enum class WebProcessType {
  Web,
  WebIsolated,
  WebCoopCoep,
};

}  // namespace

Result<NavigationIsolationOptions, nsresult> IsolationOptionsForNavigation(
    CanonicalBrowsingContext* aTopBC, WindowGlobalParent* aParentWindow,
    nsIURI* aChannelCreationURI, nsIChannel* aChannel,
    const nsACString& aCurrentRemoteType, bool aHasCOOPMismatch,
    uint32_t aLoadStateLoadType, const Maybe<uint64_t>& aChannelId,
    const Maybe<nsCString>& aRemoteTypeOverride) {
  // Get the final principal, used to select which process to load into.
  nsCOMPtr<nsIPrincipal> resultPrincipal;
  nsresult rv = nsContentUtils::GetSecurityManager()->GetChannelResultPrincipal(
      aChannel, getter_AddRefs(resultPrincipal));
  if (NS_FAILED(rv)) {
    MOZ_LOG(gProcessIsolationLog, LogLevel::Error,
            ("failed to get channel result principal"));
    return Err(rv);
  }

  MOZ_LOG(
      gProcessIsolationLog, LogLevel::Verbose,
      ("IsolationOptionsForNavigation principal:%s, uri:%s, parentUri:%s",
       OriginString(resultPrincipal).get(),
       aChannelCreationURI->GetSpecOrDefault().get(),
       aParentWindow ? aParentWindow->GetDocumentURI()->GetSpecOrDefault().get()
                     : ""));

  // If we're loading a null principal, we can't easily make a process
  // selection decision off ot it. Instead, we'll use our null principal's
  // precursor principal to make process selection decisions.
  bool principalIsSandboxed = false;
  nsCOMPtr<nsIPrincipal> resultOrPrecursor(resultPrincipal);
  if (nsCOMPtr<nsIPrincipal> precursor =
          resultOrPrecursor->GetPrecursorPrincipal()) {
    MOZ_LOG(gProcessIsolationLog, LogLevel::Verbose,
            ("using null principal precursor origin %s",
             OriginString(precursor).get()));
    resultOrPrecursor = precursor;
    principalIsSandboxed = true;
  }

  NavigationIsolationOptions options;
  options.mReplaceBrowsingContext = aHasCOOPMismatch;

  // Check if this load has an explicit remote type override. This is used to
  // perform an about:blank load within a specific content process.
  if (aRemoteTypeOverride) {
    MOZ_DIAGNOSTIC_ASSERT(
        NS_IsAboutBlank(aChannelCreationURI),
        "Should only have aRemoteTypeOverride for about:blank URIs");
    if (NS_WARN_IF(!resultPrincipal->GetIsNullPrincipal())) {
      MOZ_LOG(gProcessIsolationLog, LogLevel::Error,
              ("invalid remote type override on non-null principal"));
      return Err(NS_ERROR_DOM_SECURITY_ERR);
    }

    MOZ_LOG(gProcessIsolationLog, LogLevel::Verbose,
            ("using remote type override (%s) for load",
             aRemoteTypeOverride->get()));
    options.mRemoteType = *aRemoteTypeOverride;
    return options;
  }

  // First, check for any special cases which should be handled using the
  // channel creation URI, and handle them.
  auto behavior = IsolationBehaviorForURI(aChannelCreationURI, aParentWindow,
                                          /* aForChannelCreationURI */ true);
  MOZ_LOG(gProcessIsolationLog, LogLevel::Verbose,
          ("Channel Creation Isolation Behavior: %s",
           IsolationBehaviorName(behavior)));

  // In the about:reader special case, we want to fetch the relevant information
  // from the URI, an then treat it as a normal web content load.
  if (behavior == IsolationBehavior::AboutReader) {
    if (RefPtr<BasePrincipal> readerURIPrincipal = GetAboutReaderURLPrincipal(
            aChannelCreationURI, resultOrPrecursor->OriginAttributesRef())) {
      MOZ_LOG(gProcessIsolationLog, LogLevel::Verbose,
              ("using about:reader's url origin %s",
               OriginString(readerURIPrincipal).get()));
      resultOrPrecursor = readerURIPrincipal;
    }
    behavior = IsolationBehavior::WebContent;
  }

  // If we're loading for a specific extension, we'll need to perform a
  // BCG-switching load to get our toplevel extension window in the correct
  // BrowsingContextGroup.
  if (auto* addonPolicy =
          BasePrincipal::Cast(resultOrPrecursor)->AddonPolicy()) {
    if (aParentWindow) {
      // As a temporary measure, extension iframes must be loaded within the
      // same process as their parent document.
      MOZ_LOG(gProcessIsolationLog, LogLevel::Verbose,
              ("Loading extension subframe in same process as parent"));
      behavior = IsolationBehavior::Inherit;
    } else {
      MOZ_LOG(
          gProcessIsolationLog, LogLevel::Verbose,
          ("Found extension frame with addon policy. Will use group id %" PRIx64
           " (currentId: %" PRIx64 ")",
           addonPolicy->GetBrowsingContextGroupId(), aTopBC->Group()->Id()));
      behavior = IsolationBehavior::Extension;
      if (aTopBC->Group()->Id() != addonPolicy->GetBrowsingContextGroupId()) {
        options.mReplaceBrowsingContext = true;
        options.mSpecificGroupId = addonPolicy->GetBrowsingContextGroupId();
      }
    }
  }

  // Do a second run of `GetIsolationBehavior`, this time using the
  // principal's URI to handle additional special cases such as the file and
  // privilegedmozilla content process.
  if (behavior == IsolationBehavior::WebContent) {
    if (resultOrPrecursor->IsSystemPrincipal()) {
      // We're loading something with a system principal which isn't caught in
      // one of our other edge-cases. If the load started in the parent process,
      // and it's safe for it to end in the parent process, we should finish the
      // load there.
      bool isUIResource = false;
      if (aCurrentRemoteType.IsEmpty() &&
          (aChannelCreationURI->SchemeIs("about") ||
           (NS_SUCCEEDED(NS_URIChainHasFlags(
                aChannelCreationURI, nsIProtocolHandler::URI_IS_UI_RESOURCE,
                &isUIResource)) &&
            isUIResource))) {
        behavior = IsolationBehavior::Parent;
      } else {
        // In general, we don't want to load documents with a system principal
        // in a content process, however we need to in some cases, such as when
        // loading blob: URLs created by system code. We can force the load to
        // finish in a content process instead.
        behavior = IsolationBehavior::ForceWebRemoteType;
      }
    } else if (nsCOMPtr<nsIURI> principalURI = resultOrPrecursor->GetURI()) {
      behavior = IsolationBehaviorForURI(principalURI, aParentWindow,
                                         /* aForChannelCreationURI */ false);
    }
  }

  // If we're currently loaded in the extension process, and are going to switch
  // to some other remote type, make sure we leave the extension's BCG which we
  // may have entered earlier to separate extension and non-extension BCGs from
  // each-other.
  if (!aParentWindow && aCurrentRemoteType == EXTENSION_REMOTE_TYPE &&
      behavior != IsolationBehavior::Extension &&
      behavior != IsolationBehavior::Anywhere) {
    MOZ_LOG(gProcessIsolationLog, LogLevel::Verbose,
            ("Forcing BC replacement to leave extension BrowsingContextGroup "
             "%" PRIx64 " on navigation",
             aTopBC->Group()->Id()));
    options.mReplaceBrowsingContext = true;
  }

  // We don't want to load documents with sandboxed null principals, like
  // `data:` URIs, in the parent process, even if they were created by a
  // document which would otherwise be loaded in the parent process.
  if (behavior == IsolationBehavior::Parent && principalIsSandboxed) {
    MOZ_LOG(gProcessIsolationLog, LogLevel::Debug,
            ("Ensuring sandboxed null-principal load doesn't occur in the "
             "parent process"));
    behavior = IsolationBehavior::ForceWebRemoteType;
  }

  MOZ_LOG(
      gProcessIsolationLog, LogLevel::Debug,
      ("Using IsolationBehavior %s for %s (original uri %s)",
       IsolationBehaviorName(behavior), OriginString(resultOrPrecursor).get(),
       aChannelCreationURI->GetSpecOrDefault().get()));

  // Check if we can put the previous document into the BFCache.
  if (mozilla::BFCacheInParent() && nsSHistory::GetMaxTotalViewers() > 0 &&
      !aParentWindow && !aTopBC->HadOriginalOpener() &&
      behavior != IsolationBehavior::Parent &&
      (ExtensionPolicyService::GetSingleton().UseRemoteExtensions() ||
       behavior != IsolationBehavior::Extension) &&
      !aCurrentRemoteType.IsEmpty() &&
      aTopBC->GetHasLoadedNonInitialDocument() &&
      (aLoadStateLoadType == LOAD_NORMAL ||
       aLoadStateLoadType == LOAD_HISTORY || aLoadStateLoadType == LOAD_LINK ||
       aLoadStateLoadType == LOAD_STOP_CONTENT ||
       aLoadStateLoadType == LOAD_STOP_CONTENT_AND_REPLACE) &&
      (!aTopBC->GetActiveSessionHistoryEntry() ||
       aTopBC->GetActiveSessionHistoryEntry()->GetSaveLayoutStateFlag())) {
    if (nsCOMPtr<nsIURI> uri = aTopBC->GetCurrentURI()) {
      MOZ_LOG(gProcessIsolationLog, LogLevel::Verbose,
              ("current uri: %s", uri->GetSpecOrDefault().get()));
    }
    options.mTryUseBFCache = aTopBC->AllowedInBFCache(aChannelId);
    if (options.mTryUseBFCache) {
      options.mReplaceBrowsingContext = true;
      options.mActiveSessionHistoryEntry =
          aTopBC->GetActiveSessionHistoryEntry();
    }
  }

  // If the load has any special remote type handling, do so at this point.
  if (behavior != IsolationBehavior::WebContent) {
    switch (behavior) {
      case IsolationBehavior::ForceWebRemoteType:
        options.mRemoteType = WEB_REMOTE_TYPE;
        break;
      case IsolationBehavior::PrivilegedAbout:
        // The privileged about: content process cannot be disabled, as it
        // causes various actors to break.
        options.mRemoteType = PRIVILEGEDABOUT_REMOTE_TYPE;
        break;
      case IsolationBehavior::Extension:
        if (ExtensionPolicyService::GetSingleton().UseRemoteExtensions()) {
          options.mRemoteType = EXTENSION_REMOTE_TYPE;
        } else {
          options.mRemoteType = NOT_REMOTE_TYPE;
        }
        break;
      case IsolationBehavior::File:
        if (StaticPrefs::browser_tabs_remote_separateFileUriProcess()) {
          options.mRemoteType = FILE_REMOTE_TYPE;
        } else {
          options.mRemoteType = WEB_REMOTE_TYPE;
        }
        break;
      case IsolationBehavior::PrivilegedMozilla:
        options.mRemoteType = PRIVILEGEDMOZILLA_REMOTE_TYPE;
        break;
      case IsolationBehavior::Parent:
        options.mRemoteType = NOT_REMOTE_TYPE;
        break;
      case IsolationBehavior::Anywhere:
        options.mRemoteType = aCurrentRemoteType;
        break;
      case IsolationBehavior::Inherit:
        MOZ_DIAGNOSTIC_ASSERT(aParentWindow);
        options.mRemoteType = aParentWindow->GetRemoteType();
        break;

      case IsolationBehavior::WebContent:
      case IsolationBehavior::AboutReader:
        MOZ_ASSERT_UNREACHABLE();
        return Err(NS_ERROR_UNEXPECTED);

      case IsolationBehavior::Error:
        return Err(NS_ERROR_UNEXPECTED);
    }

    if (options.mRemoteType != aCurrentRemoteType &&
        (options.mRemoteType.IsEmpty() || aCurrentRemoteType.IsEmpty())) {
      options.mReplaceBrowsingContext = true;
    }

    MOZ_LOG(
        gProcessIsolationLog, LogLevel::Debug,
        ("Selecting specific remote type (%s) due to a special case isolation "
         "behavior %s",
         options.mRemoteType.get(), IsolationBehaviorName(behavior)));
    return options;
  }

  // At this point we're definitely not going to be loading in the parent
  // process anymore, so we're definitely going to be replacing BrowsingContext
  // if we're in the parent process.
  if (aCurrentRemoteType.IsEmpty()) {
    MOZ_ASSERT(!aParentWindow);
    options.mReplaceBrowsingContext = true;
  }

  // Handle the deprecated Large-Allocation header.
  if (!aTopBC->UseRemoteSubframes()) {
    MOZ_ASSERT(!aParentWindow,
               "subframe switch when `UseRemoteSubframes()` is false?");
    bool singleToplevel = aTopBC->Group()->Toplevels().Length() == 1;
    bool isLargeAllocLoad = IsLargeAllocationLoad(aTopBC, aChannel);
    // If we're starting a large-alloc load and have no opener relationships,
    // force the load to finish in the large-allocation remote type.
    if (isLargeAllocLoad && singleToplevel) {
      options.mRemoteType = LARGE_ALLOCATION_REMOTE_TYPE;
      options.mReplaceBrowsingContext = true;
      return options;
    }
    if (aCurrentRemoteType == LARGE_ALLOCATION_REMOTE_TYPE) {
      // If we're doing a non-large-alloc load, we may still need to finish in
      // the large-allocation remote type if we have opener relationships.
      if (!singleToplevel) {
        options.mRemoteType = LARGE_ALLOCATION_REMOTE_TYPE;
        return options;
      }
      options.mReplaceBrowsingContext = true;
    }
  }

  nsAutoCString siteOriginNoSuffix;
  MOZ_TRY(resultOrPrecursor->GetSiteOriginNoSuffix(siteOriginNoSuffix));

  // Check if we've already loaded a document with the given principal in some
  // content process. We want to finish the load in the same process in that
  // case.
  //
  // The exception to that is with extension loads and the system principal,
  // where we may have multiple documents with the same principal in different
  // processes. Those have been handled above, and will not be reaching here.
  //
  // If we're doing a replace load, we won't be staying in the same
  // BrowsingContext, so ignore this step.
  if (!options.mReplaceBrowsingContext) {
    // Helper for efficiently determining if a given origin is same-site. This
    // will attempt to do a fast equality check, and will only fall back to
    // computing the site-origin for content principals.
    auto principalIsSameSite = [&](nsIPrincipal* aDocumentPrincipal) -> bool {
      // If we're working with a null principal with a precursor, compare
      // precursors, as `resultOrPrecursor` has already been stripped to its
      // precursor.
      nsCOMPtr<nsIPrincipal> documentPrincipal(aDocumentPrincipal);
      if (nsCOMPtr<nsIPrincipal> precursor =
              documentPrincipal->GetPrecursorPrincipal()) {
        documentPrincipal = precursor;
      }

      // First, attempt to use `Equals` to compare principals, and if that
      // fails compare siteOrigins. Only compare siteOrigin for content
      // principals, as non-content principals will never have siteOrigin !=
      // origin.
      nsAutoCString documentSiteOrigin;
      return resultOrPrecursor->Equals(documentPrincipal) ||
             (documentPrincipal->GetIsContentPrincipal() &&
              resultOrPrecursor->GetIsContentPrincipal() &&
              NS_SUCCEEDED(documentPrincipal->GetSiteOriginNoSuffix(
                  documentSiteOrigin)) &&
              documentSiteOrigin == siteOriginNoSuffix);
    };

    // XXX: Consider also checking in-flight process switches to see if any have
    // matching principals?
    AutoTArray<RefPtr<BrowsingContext>, 8> contexts;
    aTopBC->Group()->GetToplevels(contexts);
    while (!contexts.IsEmpty()) {
      auto bc = contexts.PopLastElement();
      for (const auto& wc : bc->GetWindowContexts()) {
        WindowGlobalParent* wgp = wc->Canonical();

        // Check if this WindowGlobalParent has the given resultPrincipal, and
        // if it does, we need to load in that process.
        if (!wgp->GetRemoteType().IsEmpty() &&
            principalIsSameSite(wgp->DocumentPrincipal())) {
          MOZ_LOG(gProcessIsolationLog, LogLevel::Debug,
                  ("Found existing frame with matching principal "
                   "(remoteType:(%s), origin:%s)",
                   PromiseFlatCString(wgp->GetRemoteType()).get(),
                   OriginString(wgp->DocumentPrincipal()).get()));
          options.mRemoteType = wgp->GetRemoteType();
          return options;
        }

        // Also enumerate over this WindowContexts' subframes.
        contexts.AppendElements(wc->Children());
      }
    }
  }

  nsAutoCString originSuffix;
  OriginAttributes attrs = resultOrPrecursor->OriginAttributesRef();
  attrs.StripAttributes(OriginAttributes::STRIP_FIRST_PARTY_DOMAIN |
                        OriginAttributes::STRIP_PARITION_KEY);
  attrs.CreateSuffix(originSuffix);

  WebProcessType webProcessType = WebProcessType::Web;
  if (ShouldIsolateSite(resultOrPrecursor, aTopBC)) {
    webProcessType = WebProcessType::WebIsolated;
  }

  // Check if we should be loading in a webCOOP+COEP remote type due to our COOP
  // status.
  nsILoadInfo::CrossOriginOpenerPolicy coop =
      nsILoadInfo::OPENER_POLICY_UNSAFE_NONE;
  if (aParentWindow) {
    coop = aTopBC->GetOpenerPolicy();
  } else if (nsCOMPtr<nsIHttpChannelInternal> httpChannel =
                 do_QueryInterface(aChannel)) {
    MOZ_ALWAYS_SUCCEEDS(httpChannel->GetCrossOriginOpenerPolicy(&coop));
  }
  if (coop ==
      nsILoadInfo::OPENER_POLICY_SAME_ORIGIN_EMBEDDER_POLICY_REQUIRE_CORP) {
    webProcessType = WebProcessType::WebCoopCoep;
  }

  switch (webProcessType) {
    case WebProcessType::Web:
      options.mRemoteType = WEB_REMOTE_TYPE;
      break;
    case WebProcessType::WebIsolated:
      options.mRemoteType =
          FISSION_WEB_REMOTE_TYPE "="_ns + siteOriginNoSuffix + originSuffix;
      break;
    case WebProcessType::WebCoopCoep:
      options.mRemoteType =
          WITH_COOP_COEP_REMOTE_TYPE "="_ns + siteOriginNoSuffix + originSuffix;
      break;
  }
  return options;
}

}  // namespace mozilla::dom
