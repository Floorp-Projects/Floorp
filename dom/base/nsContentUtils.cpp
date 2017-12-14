/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* A namespace class for static layout utilities. */

#include "nsContentUtils.h"

#include <algorithm>
#include <math.h>

#include "DecoderTraits.h"
#include "harfbuzz/hb.h"
#include "imgICache.h"
#include "imgIContainer.h"
#include "imgINotificationObserver.h"
#include "imgLoader.h"
#include "imgRequestProxy.h"
#include "jsapi.h"
#include "jsfriendapi.h"
#include "js/Value.h"
#include "Layers.h"
#include "nsAppRunner.h"
// nsNPAPIPluginInstance must be included before nsIDocument.h, which is included in mozAutoDocUpdate.h.
#include "nsNPAPIPluginInstance.h"
#include "gfxDrawable.h"
#include "gfxPrefs.h"
#include "ImageOps.h"
#include "mozAutoDocUpdate.h"
#include "mozilla/ArrayUtils.h"
#include "mozilla/Attributes.h"
#include "mozilla/AutoRestore.h"
#include "mozilla/AutoTimelineMarker.h"
#include "mozilla/Base64.h"
#include "mozilla/CheckedInt.h"
#include "mozilla/DebugOnly.h"
#include "mozilla/LoadInfo.h"
#include "mozilla/dom/ContentChild.h"
#include "mozilla/dom/CustomElementRegistry.h"
#include "mozilla/dom/DocumentFragment.h"
#include "mozilla/dom/DOMException.h"
#include "mozilla/dom/DOMExceptionBinding.h"
#include "mozilla/dom/DOMTypes.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/ElementInlines.h"
#include "mozilla/dom/FileSystemSecurity.h"
#include "mozilla/dom/FileBlobImpl.h"
#include "mozilla/dom/HTMLInputElement.h"
#include "mozilla/dom/HTMLSlotElement.h"
#include "mozilla/dom/HTMLTemplateElement.h"
#include "mozilla/dom/IDTracker.h"
#include "mozilla/dom/IPCBlobUtils.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/ScriptSettings.h"
#include "mozilla/dom/TabParent.h"
#include "mozilla/dom/TouchEvent.h"
#include "mozilla/dom/ShadowRoot.h"
#include "mozilla/dom/XULCommandEvent.h"
#include "mozilla/dom/WorkerPrivate.h"
#include "mozilla/dom/workers/ServiceWorkerManager.h"
#include "mozilla/EventDispatcher.h"
#include "mozilla/EventListenerManager.h"
#include "mozilla/EventStateManager.h"
#include "mozilla/gfx/DataSurfaceHelpers.h"
#include "mozilla/HTMLEditor.h"
#include "mozilla/IMEStateManager.h"
#include "mozilla/InternalMutationEvent.h"
#include "mozilla/Likely.h"
#include "mozilla/ManualNAC.h"
#include "mozilla/MouseEvents.h"
#include "mozilla/Preferences.h"
#include "mozilla/ResultExtensions.h"
#include "mozilla/dom/Selection.h"
#include "mozilla/TextEvents.h"
#include "nsArrayUtils.h"
#include "nsAString.h"
#include "nsAttrName.h"
#include "nsAttrValue.h"
#include "nsAttrValueInlines.h"
#include "nsBindingManager.h"
#include "nsCanvasFrame.h"
#include "nsCaret.h"
#include "nsCCUncollectableMarker.h"
#include "nsCharSeparatedTokenizer.h"
#include "nsCOMPtr.h"
#include "nsContentCreatorFunctions.h"
#include "nsContentDLF.h"
#include "nsContentList.h"
#include "nsContentPolicyUtils.h"
#include "nsContentSecurityManager.h"
#include "nsCPrefetchService.h"
#include "nsCRT.h"
#include "nsCycleCollectionParticipant.h"
#include "nsCycleCollector.h"
#include "nsDataHashtable.h"
#include "nsDocShellCID.h"
#include "nsDocument.h"
#include "nsDOMCID.h"
#include "mozilla/dom/DataTransfer.h"
#include "nsDOMJSUtils.h"
#include "nsDOMMutationObserver.h"
#include "nsError.h"
#include "nsFocusManager.h"
#include "nsGenericHTMLElement.h"
#include "nsGenericHTMLFrameElement.h"
#include "nsGkAtoms.h"
#include "nsHostObjectProtocolHandler.h"
#include "nsHtml5Module.h"
#include "nsHtml5StringParser.h"
#include "nsHTMLDocument.h"
#include "nsHTMLTags.h"
#include "nsIAddonPolicyService.h"
#include "nsIAnonymousContentCreator.h"
#include "nsIAsyncVerifyRedirectCallback.h"
#include "nsICategoryManager.h"
#include "nsIChannelEventSink.h"
#include "nsICharsetDetectionObserver.h"
#include "nsIChromeRegistry.h"
#include "nsIConsoleService.h"
#include "nsIContent.h"
#include "nsIContentInlines.h"
#include "nsIContentSecurityPolicy.h"
#include "nsIContentSink.h"
#include "nsIContentViewer.h"
#include "nsIDocShell.h"
#include "nsIDocShellTreeOwner.h"
#include "nsIDocument.h"
#include "nsIDocumentEncoder.h"
#include "nsIDOMChromeWindow.h"
#include "nsIDOMDocument.h"
#include "nsIDOMDocumentType.h"
#include "nsIDOMEvent.h"
#include "nsIDOMElement.h"
#include "nsIDOMHTMLElement.h"
#include "nsIDOMHTMLFormElement.h"
#include "nsIDOMHTMLInputElement.h"
#include "nsIDOMNode.h"
#include "nsIDOMNodeList.h"
#include "nsIDOMWindowUtils.h"
#include "nsIDragService.h"
#include "nsIFormControl.h"
#include "nsIForm.h"
#include "nsIFragmentContentSink.h"
#include "nsContainerFrame.h"
#include "nsIHTMLDocument.h"
#include "nsIIdleService.h"
#include "nsIImageLoadingContent.h"
#include "nsIInterfaceRequestor.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIIOService.h"
#include "nsILoadContext.h"
#include "nsILoadGroup.h"
#include "nsIMemoryReporter.h"
#include "nsIMIMEHeaderParam.h"
#include "nsIMIMEService.h"
#include "nsINode.h"
#include "mozilla/dom/NodeInfo.h"
#include "nsIObjectLoadingContent.h"
#include "nsIObserver.h"
#include "nsIObserverService.h"
#include "nsIOfflineCacheUpdate.h"
#include "nsIParser.h"
#include "nsIPermissionManager.h"
#include "nsIPluginHost.h"
#include "nsIRemoteBrowser.h"
#include "nsIRequest.h"
#include "nsIRunnable.h"
#include "nsIScriptContext.h"
#include "nsIScriptError.h"
#include "nsIScriptGlobalObject.h"
#include "nsIScriptObjectPrincipal.h"
#include "nsIScriptSecurityManager.h"
#include "nsIScrollable.h"
#include "nsIStreamConverterService.h"
#include "nsIStringBundle.h"
#include "nsIURI.h"
#include "nsIURIWithPrincipal.h"
#include "nsIURL.h"
#include "nsIWebNavigation.h"
#include "nsIWindowMediator.h"
#include "nsIXPConnect.h"
#include "nsJSUtils.h"
#include "nsMappedAttributes.h"
#include "nsNetCID.h"
#include "nsNetUtil.h"
#include "nsNodeInfoManager.h"
#include "NullPrincipal.h"
#include "nsParserCIID.h"
#include "nsParserConstants.h"
#include "nsPIDOMWindow.h"
#include "nsPresContext.h"
#include "nsPrintfCString.h"
#include "nsSandboxFlags.h"
#include "nsScriptSecurityManager.h"
#include "nsSerializationHelper.h"
#include "nsStreamUtils.h"
#include "nsTextEditorState.h"
#include "nsTextFragment.h"
#include "nsTextNode.h"
#include "nsThreadUtils.h"
#include "nsUnicodeProperties.h"
#include "nsURLHelper.h"
#include "nsViewManager.h"
#include "nsViewportInfo.h"
#include "nsWidgetsCID.h"
#include "nsIWindowProvider.h"
#include "nsWrapperCacheInlines.h"
#include "nsXULPopupManager.h"
#include "xpcprivate.h" // nsXPConnect
#include "HTMLSplitOnSpacesTokenizer.h"
#include "nsContentTypeParser.h"
#include "nsICookiePermission.h"
#include "mozIThirdPartyUtil.h"
#include "nsICookieService.h"
#include "mozilla/EnumSet.h"
#include "mozilla/BloomFilter.h"
#include "TabChild.h"
#include "mozilla/dom/DocGroup.h"
#include "mozilla/dom/TabGroup.h"
#include "nsIWebNavigationInfo.h"
#include "nsPluginHost.h"
#include "mozilla/HangAnnotations.h"
#include "mozilla/Encoding.h"
#include "nsXULElement.h"

#include "nsIBidiKeyboard.h"

#if defined(XP_WIN)
// Undefine LoadImage to prevent naming conflict with Windows.
#undef LoadImage
#endif

extern "C" int MOZ_XMLTranslateEntity(const char* ptr, const char* end,
                                      const char** next, char16_t* result);
extern "C" int MOZ_XMLCheckQName(const char* ptr, const char* end,
                                 int ns_aware, const char** colon);

class imgLoader;
class nsAtom;

using namespace mozilla::dom;
using namespace mozilla::ipc;
using namespace mozilla::gfx;
using namespace mozilla::layers;
using namespace mozilla::widget;
using namespace mozilla;

const char kLoadAsData[] = "loadAsData";

nsIXPConnect *nsContentUtils::sXPConnect;
nsIScriptSecurityManager *nsContentUtils::sSecurityManager;
nsIPrincipal *nsContentUtils::sSystemPrincipal;
nsIPrincipal *nsContentUtils::sNullSubjectPrincipal;
nsNameSpaceManager *nsContentUtils::sNameSpaceManager;
nsIIOService *nsContentUtils::sIOService;
nsIUUIDGenerator *nsContentUtils::sUUIDGenerator;
nsIConsoleService *nsContentUtils::sConsoleService;
nsDataHashtable<nsRefPtrHashKey<nsAtom>, EventNameMapping>* nsContentUtils::sAtomEventTable = nullptr;
nsDataHashtable<nsStringHashKey, EventNameMapping>* nsContentUtils::sStringEventTable = nullptr;
nsTArray<RefPtr<nsAtom>>* nsContentUtils::sUserDefinedEvents = nullptr;
nsIStringBundleService *nsContentUtils::sStringBundleService;
nsIStringBundle *nsContentUtils::sStringBundles[PropertiesFile_COUNT];
nsIContentPolicy *nsContentUtils::sContentPolicyService;
bool nsContentUtils::sTriedToGetContentPolicy = false;
RefPtr<mozilla::intl::LineBreaker> nsContentUtils::sLineBreaker;
RefPtr<mozilla::intl::WordBreaker> nsContentUtils::sWordBreaker;
nsIBidiKeyboard *nsContentUtils::sBidiKeyboard = nullptr;
uint32_t nsContentUtils::sScriptBlockerCount = 0;
uint32_t nsContentUtils::sDOMNodeRemovedSuppressCount = 0;
AutoTArray<nsCOMPtr<nsIRunnable>, 8>* nsContentUtils::sBlockedScriptRunners = nullptr;
uint32_t nsContentUtils::sRunnersCountAtFirstBlocker = 0;
nsIInterfaceRequestor* nsContentUtils::sSameOriginChecker = nullptr;

bool nsContentUtils::sIsHandlingKeyBoardEvent = false;
bool nsContentUtils::sAllowXULXBL_for_file = false;

nsString* nsContentUtils::sShiftText = nullptr;
nsString* nsContentUtils::sControlText = nullptr;
nsString* nsContentUtils::sMetaText = nullptr;
nsString* nsContentUtils::sOSText = nullptr;
nsString* nsContentUtils::sAltText = nullptr;
nsString* nsContentUtils::sModifierSeparator = nullptr;

bool nsContentUtils::sInitialized = false;
bool nsContentUtils::sIsFullScreenApiEnabled = false;
bool nsContentUtils::sIsUnprefixedFullscreenApiEnabled = false;
bool nsContentUtils::sTrustedFullScreenOnly = true;
bool nsContentUtils::sIsCutCopyAllowed = true;
bool nsContentUtils::sIsFrameTimingPrefEnabled = false;
bool nsContentUtils::sIsPerformanceTimingEnabled = false;
bool nsContentUtils::sIsResourceTimingEnabled = false;
bool nsContentUtils::sIsPerformanceNavigationTimingEnabled = false;
bool nsContentUtils::sIsUserTimingLoggingEnabled = false;
bool nsContentUtils::sIsFormAutofillAutocompleteEnabled = false;
bool nsContentUtils::sIsWebComponentsEnabled = false;
bool nsContentUtils::sIsCustomElementsEnabled = false;
bool nsContentUtils::sDevToolsEnabled = false;
bool nsContentUtils::sSendPerformanceTimingNotifications = false;
bool nsContentUtils::sUseActivityCursor = false;
bool nsContentUtils::sAnimationsAPICoreEnabled = false;
bool nsContentUtils::sAnimationsAPIElementAnimateEnabled = false;
bool nsContentUtils::sAnimationsAPIPendingMemberEnabled = false;
bool nsContentUtils::sGetBoxQuadsEnabled = false;
bool nsContentUtils::sSkipCursorMoveForSameValueSet = false;
bool nsContentUtils::sRequestIdleCallbackEnabled = false;
bool nsContentUtils::sLowerNetworkPriority = false;
bool nsContentUtils::sTailingEnabled = false;
bool nsContentUtils::sShowInputPlaceholderOnFocus = true;
bool nsContentUtils::sAutoFocusEnabled = true;
#ifndef RELEASE_OR_BETA
bool nsContentUtils::sBypassCSSOMOriginCheck = false;
#endif
bool nsContentUtils::sIsScopedStyleEnabled = false;

bool nsContentUtils::sIsBytecodeCacheEnabled = false;
int32_t nsContentUtils::sBytecodeCacheStrategy = 0;
nsCString* nsContentUtils::sJSBytecodeMimeType = nullptr;

int32_t nsContentUtils::sPrivacyMaxInnerWidth = 1000;
int32_t nsContentUtils::sPrivacyMaxInnerHeight = 1000;

nsContentUtils::UserInteractionObserver*
nsContentUtils::sUserInteractionObserver = nullptr;

uint32_t nsContentUtils::sHandlingInputTimeout = 1000;

uint32_t nsContentUtils::sCookiesLifetimePolicy = nsICookieService::ACCEPT_NORMALLY;
uint32_t nsContentUtils::sCookiesBehavior = nsICookieService::BEHAVIOR_ACCEPT;

nsHtml5StringParser* nsContentUtils::sHTMLFragmentParser = nullptr;
nsIParser* nsContentUtils::sXMLFragmentParser = nullptr;
nsIFragmentContentSink* nsContentUtils::sXMLFragmentSink = nullptr;
bool nsContentUtils::sFragmentParsingActive = false;

#if !(defined(DEBUG) || defined(MOZ_ENABLE_JS_DUMP))
bool nsContentUtils::sDOMWindowDumpEnabled;
#endif

bool nsContentUtils::sDoNotTrackEnabled = false;

mozilla::LazyLogModule nsContentUtils::sDOMDumpLog("Dump");

PopupControlState nsContentUtils::sPopupControlState = openAbused;

int32_t nsContentUtils::sInnerOrOuterWindowCount = 0;
uint32_t nsContentUtils::sInnerOrOuterWindowSerialCounter = 0;

// Subset of http://www.whatwg.org/specs/web-apps/current-work/#autofill-field-name
enum AutocompleteUnsupportedFieldName : uint8_t
{
  #define AUTOCOMPLETE_UNSUPPORTED_FIELD_NAME(name_, value_) \
    eAutocompleteUnsupportedFieldName_##name_,
  #include "AutocompleteFieldList.h"
  #undef AUTOCOMPLETE_UNSUPPORTED_FIELD_NAME
};

enum AutocompleteUnsupportFieldContactHint : uint8_t
{
  #define AUTOCOMPLETE_UNSUPPORTED_FIELD_CONTACT_HINT(name_, value_) \
    eAutocompleteUnsupportedFieldContactHint_##name_,
  #include "AutocompleteFieldList.h"
  #undef AUTOCOMPLETE_UNSUPPORTED_FIELD_CONTACT_HINT
};

enum AutocompleteFieldName : uint8_t
{
  #define AUTOCOMPLETE_FIELD_NAME(name_, value_) \
    eAutocompleteFieldName_##name_,
  #define AUTOCOMPLETE_CONTACT_FIELD_NAME(name_, value_) \
    AUTOCOMPLETE_FIELD_NAME(name_, value_)
  #include "AutocompleteFieldList.h"
  #undef AUTOCOMPLETE_FIELD_NAME
  #undef AUTOCOMPLETE_CONTACT_FIELD_NAME
};

enum AutocompleteFieldHint : uint8_t
{
  #define AUTOCOMPLETE_FIELD_HINT(name_, value_) \
    eAutocompleteFieldHint_##name_,
  #include "AutocompleteFieldList.h"
  #undef AUTOCOMPLETE_FIELD_HINT
};

enum AutocompleteFieldContactHint : uint8_t
{
  #define AUTOCOMPLETE_FIELD_CONTACT_HINT(name_, value_) \
    eAutocompleteFieldContactHint_##name_,
  #include "AutocompleteFieldList.h"
  #undef AUTOCOMPLETE_FIELD_CONTACT_HINT
};

enum AutocompleteCategory
{
  #define AUTOCOMPLETE_CATEGORY(name_, value_) eAutocompleteCategory_##name_,
  #include "AutocompleteFieldList.h"
  #undef AUTOCOMPLETE_CATEGORY
};

static const nsAttrValue::EnumTable kAutocompleteUnsupportedFieldNameTable[] = {
  #define AUTOCOMPLETE_UNSUPPORTED_FIELD_NAME(name_, value_) \
    { value_, eAutocompleteUnsupportedFieldName_##name_ },
  #include "AutocompleteFieldList.h"
  #undef AUTOCOMPLETE_UNSUPPORTED_FIELD_NAME
  { nullptr, 0 }
};

static const nsAttrValue::EnumTable kAutocompleteUnsupportedContactFieldHintTable[] = {
  #define AUTOCOMPLETE_UNSUPPORTED_FIELD_CONTACT_HINT(name_, value_) \
    { value_, eAutocompleteUnsupportedFieldContactHint_##name_ },
  #include "AutocompleteFieldList.h"
  #undef AUTOCOMPLETE_UNSUPPORTED_FIELD_CONTACT_HINT
  { nullptr, 0 }
};


static const nsAttrValue::EnumTable kAutocompleteFieldNameTable[] = {
  #define AUTOCOMPLETE_FIELD_NAME(name_, value_) \
    { value_, eAutocompleteFieldName_##name_ },
  #include "AutocompleteFieldList.h"
  #undef AUTOCOMPLETE_FIELD_NAME
  { nullptr, 0 }
};

static const nsAttrValue::EnumTable kAutocompleteContactFieldNameTable[] = {
  #define AUTOCOMPLETE_CONTACT_FIELD_NAME(name_, value_) \
    { value_, eAutocompleteFieldName_##name_ },
  #include "AutocompleteFieldList.h"
  #undef AUTOCOMPLETE_CONTACT_FIELD_NAME
  { nullptr, 0 }
};

static const nsAttrValue::EnumTable kAutocompleteFieldHintTable[] = {
  #define AUTOCOMPLETE_FIELD_HINT(name_, value_) \
    { value_, eAutocompleteFieldHint_##name_ },
  #include "AutocompleteFieldList.h"
  #undef AUTOCOMPLETE_FIELD_HINT
  { nullptr, 0 }
};

static const nsAttrValue::EnumTable kAutocompleteContactFieldHintTable[] = {
  #define AUTOCOMPLETE_FIELD_CONTACT_HINT(name_, value_) \
    { value_, eAutocompleteFieldContactHint_##name_ },
  #include "AutocompleteFieldList.h"
  #undef AUTOCOMPLETE_FIELD_CONTACT_HINT
  { nullptr, 0 }
};

namespace {

static NS_DEFINE_CID(kCParserCID, NS_PARSER_CID);

static PLDHashTable* sEventListenerManagersHash;

class DOMEventListenerManagersHashReporter final : public nsIMemoryReporter
{
  MOZ_DEFINE_MALLOC_SIZE_OF(MallocSizeOf)

  ~DOMEventListenerManagersHashReporter() = default;

public:
  NS_DECL_ISUPPORTS

  NS_IMETHOD CollectReports(nsIHandleReportCallback* aHandleReport,
                            nsISupports* aData, bool aAnonymize) override
  {
    // We don't measure the |EventListenerManager| objects pointed to by the
    // entries because those references are non-owning.
    int64_t amount = sEventListenerManagersHash
                   ? sEventListenerManagersHash->ShallowSizeOfIncludingThis(
                       MallocSizeOf)
                   : 0;

    MOZ_COLLECT_REPORT(
      "explicit/dom/event-listener-managers-hash", KIND_HEAP, UNITS_BYTES,
      amount,
      "Memory used by the event listener manager's hash table.");

    return NS_OK;
  }
};

NS_IMPL_ISUPPORTS(DOMEventListenerManagersHashReporter, nsIMemoryReporter)

class EventListenerManagerMapEntry : public PLDHashEntryHdr
{
public:
  explicit EventListenerManagerMapEntry(const void* aKey)
    : mKey(aKey)
  {
  }

  ~EventListenerManagerMapEntry()
  {
    NS_ASSERTION(!mListenerManager, "caller must release and disconnect ELM");
  }

protected:          // declared protected to silence clang warnings
  const void *mKey; // must be first, to look like PLDHashEntryStub

public:
  RefPtr<EventListenerManager> mListenerManager;
};

static void
EventListenerManagerHashInitEntry(PLDHashEntryHdr *entry, const void *key)
{
  // Initialize the entry with placement new
  new (entry) EventListenerManagerMapEntry(key);
}

static void
EventListenerManagerHashClearEntry(PLDHashTable *table, PLDHashEntryHdr *entry)
{
  EventListenerManagerMapEntry *lm =
    static_cast<EventListenerManagerMapEntry *>(entry);

  // Let the EventListenerManagerMapEntry clean itself up...
  lm->~EventListenerManagerMapEntry();
}

class SameOriginCheckerImpl final : public nsIChannelEventSink,
                                    public nsIInterfaceRequestor
{
  ~SameOriginCheckerImpl() = default;

  NS_DECL_ISUPPORTS
  NS_DECL_NSICHANNELEVENTSINK
  NS_DECL_NSIINTERFACEREQUESTOR
};

} // namespace

/**
 * This class is used to determine whether or not the user is currently
 * interacting with the browser. It listens to observer events to toggle the
 * value of the sUserActive static.
 *
 * This class is an internal implementation detail.
 * nsContentUtils::GetUserIsInteracting() should be used to access current
 * user interaction status.
 */
class nsContentUtils::UserInteractionObserver final : public nsIObserver
                                                    , public HangMonitor::Annotator
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER

  void Init();
  void Shutdown();
  void AnnotateHang(HangMonitor::HangAnnotations& aAnnotations) override;

  static Atomic<bool> sUserActive;

private:
  ~UserInteractionObserver() {}
};

/* static */
TimeDuration
nsContentUtils::HandlingUserInputTimeout()
{
  return TimeDuration::FromMilliseconds(sHandlingInputTimeout);
}

// static
nsresult
nsContentUtils::Init()
{
  if (sInitialized) {
    NS_WARNING("Init() called twice");

    return NS_OK;
  }

  nsHTMLTags::AddRefTable();

  sNameSpaceManager = nsNameSpaceManager::GetInstance();
  NS_ENSURE_TRUE(sNameSpaceManager, NS_ERROR_OUT_OF_MEMORY);

  sXPConnect = nsXPConnect::XPConnect();

  sSecurityManager = nsScriptSecurityManager::GetScriptSecurityManager();
  if(!sSecurityManager)
    return NS_ERROR_FAILURE;
  NS_ADDREF(sSecurityManager);

  sSecurityManager->GetSystemPrincipal(&sSystemPrincipal);
  MOZ_ASSERT(sSystemPrincipal);

  RefPtr<NullPrincipal> nullPrincipal = NullPrincipal::Create();
  if (!nullPrincipal) {
    return NS_ERROR_FAILURE;
  }

  nullPrincipal.forget(&sNullSubjectPrincipal);

  nsresult rv = CallGetService(NS_IOSERVICE_CONTRACTID, &sIOService);
  if (NS_FAILED(rv)) {
    // This makes life easier, but we can live without it.

    sIOService = nullptr;
  }

  sLineBreaker = mozilla::intl::LineBreaker::Create();

  sWordBreaker = mozilla::intl::WordBreaker::Create();

  if (!InitializeEventTable())
    return NS_ERROR_FAILURE;

  if (!sEventListenerManagersHash) {
    static const PLDHashTableOps hash_table_ops =
    {
      PLDHashTable::HashVoidPtrKeyStub,
      PLDHashTable::MatchEntryStub,
      PLDHashTable::MoveEntryStub,
      EventListenerManagerHashClearEntry,
      EventListenerManagerHashInitEntry
    };

    sEventListenerManagersHash =
      new PLDHashTable(&hash_table_ops, sizeof(EventListenerManagerMapEntry));

    RegisterStrongMemoryReporter(new DOMEventListenerManagersHashReporter());
  }

  sBlockedScriptRunners = new AutoTArray<nsCOMPtr<nsIRunnable>, 8>;

  Preferences::AddBoolVarCache(&sAllowXULXBL_for_file,
                               "dom.allow_XUL_XBL_for_file");

  Preferences::AddBoolVarCache(&sIsFullScreenApiEnabled,
                               "full-screen-api.enabled");

  Preferences::AddBoolVarCache(&sIsUnprefixedFullscreenApiEnabled,
                               "full-screen-api.unprefix.enabled");

  Preferences::AddBoolVarCache(&sTrustedFullScreenOnly,
                               "full-screen-api.allow-trusted-requests-only");

  Preferences::AddBoolVarCache(&sIsCutCopyAllowed,
                               "dom.allow_cut_copy", true);

  Preferences::AddBoolVarCache(&sIsPerformanceTimingEnabled,
                               "dom.enable_performance", true);

  Preferences::AddBoolVarCache(&sIsResourceTimingEnabled,
                               "dom.enable_resource_timing", true);

  Preferences::AddBoolVarCache(&sIsPerformanceNavigationTimingEnabled,
                               "dom.enable_performance_navigation_timing", true);

  Preferences::AddBoolVarCache(&sIsUserTimingLoggingEnabled,
                               "dom.performance.enable_user_timing_logging", false);

  Preferences::AddBoolVarCache(&sIsFrameTimingPrefEnabled,
                               "dom.enable_frame_timing", false);

  Preferences::AddBoolVarCache(&sIsFormAutofillAutocompleteEnabled,
                               "dom.forms.autocomplete.formautofill", false);

  Preferences::AddBoolVarCache(&sIsWebComponentsEnabled,
                               "dom.webcomponents.enabled", false);

  Preferences::AddBoolVarCache(&sIsCustomElementsEnabled,
                               "dom.webcomponents.customelements.enabled", false);

  Preferences::AddBoolVarCache(&sDevToolsEnabled,
                               "devtools.enabled");

  Preferences::AddIntVarCache(&sPrivacyMaxInnerWidth,
                              "privacy.window.maxInnerWidth",
                              1000);

  Preferences::AddIntVarCache(&sPrivacyMaxInnerHeight,
                              "privacy.window.maxInnerHeight",
                              1000);

  Preferences::AddUintVarCache(&sHandlingInputTimeout,
                               "dom.event.handling-user-input-time-limit",
                               1000);

  Preferences::AddBoolVarCache(&sSendPerformanceTimingNotifications,
                               "dom.performance.enable_notify_performance_timing", false);

  Preferences::AddUintVarCache(&sCookiesLifetimePolicy,
                               "network.cookie.lifetimePolicy",
                               nsICookieService::ACCEPT_NORMALLY);

  Preferences::AddUintVarCache(&sCookiesBehavior,
                               "network.cookie.cookieBehavior",
                               nsICookieService::BEHAVIOR_ACCEPT);

#if !(defined(DEBUG) || defined(MOZ_ENABLE_JS_DUMP))
  Preferences::AddBoolVarCache(&sDOMWindowDumpEnabled,
                               "browser.dom.window.dump.enabled");
#endif

  Preferences::AddBoolVarCache(&sDoNotTrackEnabled,
                               "privacy.donottrackheader.enabled", false);

  Preferences::AddBoolVarCache(&sUseActivityCursor,
                               "ui.use_activity_cursor", false);

  Preferences::AddBoolVarCache(&sAnimationsAPICoreEnabled,
                               "dom.animations-api.core.enabled", false);

  Preferences::AddBoolVarCache(&sAnimationsAPIElementAnimateEnabled,
                               "dom.animations-api.element-animate.enabled", false);

  Preferences::AddBoolVarCache(&sAnimationsAPIPendingMemberEnabled,
                               "dom.animations-api.pending-member.enabled",
                               false);

  Preferences::AddBoolVarCache(&sGetBoxQuadsEnabled,
                               "layout.css.getBoxQuads.enabled", false);

  Preferences::AddBoolVarCache(&sSkipCursorMoveForSameValueSet,
                               "dom.input.skip_cursor_move_for_same_value_set",
                               true);

  Preferences::AddBoolVarCache(&sRequestIdleCallbackEnabled,
                               "dom.requestIdleCallback.enabled", false);

#ifndef RELEASE_OR_BETA
  sBypassCSSOMOriginCheck = getenv("MOZ_BYPASS_CSSOM_ORIGIN_CHECK");
#endif

  Preferences::AddBoolVarCache(&sIsScopedStyleEnabled,
                               "layout.css.scoped-style.enabled", false);

  Preferences::AddBoolVarCache(&sLowerNetworkPriority,
                               "privacy.trackingprotection.lower_network_priority", false);

  Preferences::AddBoolVarCache(&sTailingEnabled,
                               "network.http.tailing.enabled", true);

  Preferences::AddBoolVarCache(&sShowInputPlaceholderOnFocus,
                               "dom.placeholder.show_on_focus", true);

  Preferences::AddBoolVarCache(&sAutoFocusEnabled,
                               "browser.autofocus", true);

  Preferences::AddBoolVarCache(&sIsBytecodeCacheEnabled,
                               "dom.script_loader.bytecode_cache.enabled", false);

  Preferences::AddIntVarCache(&sBytecodeCacheStrategy,
                              "dom.script_loader.bytecode_cache.strategy", 0);

  nsDependentCString buildID(mozilla::PlatformBuildID());
  sJSBytecodeMimeType = new nsCString(NS_LITERAL_CSTRING("javascript/moz-bytecode-") + buildID);

  Element::InitCCCallbacks();

  Unused << nsRFPService::GetOrCreate();

  nsCOMPtr<nsIUUIDGenerator> uuidGenerator =
    do_GetService("@mozilla.org/uuid-generator;1", &rv);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  uuidGenerator.forget(&sUUIDGenerator);

  if (XRE_IsParentProcess()) {
    AsyncPrecreateStringBundles();
  }

  RefPtr<UserInteractionObserver> uio = new UserInteractionObserver();
  uio->Init();
  uio.forget(&sUserInteractionObserver);

  sInitialized = true;

  return NS_OK;
}

void
nsContentUtils::GetShiftText(nsAString& text)
{
  if (!sShiftText)
    InitializeModifierStrings();
  text.Assign(*sShiftText);
}

void
nsContentUtils::GetControlText(nsAString& text)
{
  if (!sControlText)
    InitializeModifierStrings();
  text.Assign(*sControlText);
}

void
nsContentUtils::GetMetaText(nsAString& text)
{
  if (!sMetaText)
    InitializeModifierStrings();
  text.Assign(*sMetaText);
}

void
nsContentUtils::GetOSText(nsAString& text)
{
  if (!sOSText) {
    InitializeModifierStrings();
  }
  text.Assign(*sOSText);
}

void
nsContentUtils::GetAltText(nsAString& text)
{
  if (!sAltText)
    InitializeModifierStrings();
  text.Assign(*sAltText);
}

void
nsContentUtils::GetModifierSeparatorText(nsAString& text)
{
  if (!sModifierSeparator)
    InitializeModifierStrings();
  text.Assign(*sModifierSeparator);
}

void
nsContentUtils::InitializeModifierStrings()
{
  //load the display strings for the keyboard accelerators
  nsCOMPtr<nsIStringBundleService> bundleService =
    mozilla::services::GetStringBundleService();
  nsCOMPtr<nsIStringBundle> bundle;
  DebugOnly<nsresult> rv = NS_OK;
  if (bundleService) {
    rv = bundleService->CreateBundle( "chrome://global-platform/locale/platformKeys.properties",
                                      getter_AddRefs(bundle));
  }

  NS_ASSERTION(NS_SUCCEEDED(rv) && bundle, "chrome://global/locale/platformKeys.properties could not be loaded");
  nsAutoString shiftModifier;
  nsAutoString metaModifier;
  nsAutoString osModifier;
  nsAutoString altModifier;
  nsAutoString controlModifier;
  nsAutoString modifierSeparator;
  if (bundle) {
    //macs use symbols for each modifier key, so fetch each from the bundle, which also covers i18n
    bundle->GetStringFromName("VK_SHIFT", shiftModifier);
    bundle->GetStringFromName("VK_META", metaModifier);
    bundle->GetStringFromName("VK_WIN", osModifier);
    bundle->GetStringFromName("VK_ALT", altModifier);
    bundle->GetStringFromName("VK_CONTROL", controlModifier);
    bundle->GetStringFromName("MODIFIER_SEPARATOR", modifierSeparator);
  }
  //if any of these don't exist, we get  an empty string
  sShiftText = new nsString(shiftModifier);
  sMetaText = new nsString(metaModifier);
  sOSText = new nsString(osModifier);
  sAltText = new nsString(altModifier);
  sControlText = new nsString(controlModifier);
  sModifierSeparator = new nsString(modifierSeparator);
}

mozilla::EventClassID
nsContentUtils::GetEventClassIDFromMessage(EventMessage aEventMessage)
{
  switch (aEventMessage) {
#define MESSAGE_TO_EVENT(name_, message_, type_, struct_) \
  case message_: return struct_;
#include "mozilla/EventNameList.h"
#undef MESSAGE_TO_EVENT
  default:
    MOZ_ASSERT_UNREACHABLE("Invalid event message?");
    return eBasicEventClass;
  }
}

static nsAtom*
GetEventTypeFromMessage(EventMessage aEventMessage)
{
  switch (aEventMessage) {
#define MESSAGE_TO_EVENT(name_, message_, type_, struct_) \
  case message_: return nsGkAtoms::on##name_;
#include "mozilla/EventNameList.h"
#undef MESSAGE_TO_EVENT
  default:
    return nullptr;
  }
}

// Because of SVG/SMIL we have several atoms mapped to the same
// id, but we can rely on MESSAGE_TO_EVENT to map id to only one atom.
static bool
ShouldAddEventToStringEventTable(const EventNameMapping& aMapping)
{
  MOZ_ASSERT(aMapping.mAtom);
  return GetEventTypeFromMessage(aMapping.mMessage) == aMapping.mAtom;
}

bool
nsContentUtils::InitializeEventTable() {
  NS_ASSERTION(!sAtomEventTable, "EventTable already initialized!");
  NS_ASSERTION(!sStringEventTable, "EventTable already initialized!");

  static const EventNameMapping eventArray[] = {
#define EVENT(name_,  _message, _type, _class)          \
    { nsGkAtoms::on##name_, _type, _message, _class, false },
#define WINDOW_ONLY_EVENT EVENT
#define DOCUMENT_ONLY_EVENT EVENT
#define NON_IDL_EVENT EVENT
#include "mozilla/EventNameList.h"
#undef WINDOW_ONLY_EVENT
#undef NON_IDL_EVENT
#undef EVENT
    { nullptr }
  };

  sAtomEventTable = new nsDataHashtable<nsRefPtrHashKey<nsAtom>, EventNameMapping>(
      ArrayLength(eventArray));
  sStringEventTable = new nsDataHashtable<nsStringHashKey, EventNameMapping>(
      ArrayLength(eventArray));
  sUserDefinedEvents = new nsTArray<RefPtr<nsAtom>>(64);

  // Subtract one from the length because of the trailing null
  for (uint32_t i = 0; i < ArrayLength(eventArray) - 1; ++i) {
    MOZ_ASSERT(!sAtomEventTable->Lookup(eventArray[i].mAtom),
               "Double-defining event name; fix your EventNameList.h");
    sAtomEventTable->Put(eventArray[i].mAtom, eventArray[i]);
    if (ShouldAddEventToStringEventTable(eventArray[i])) {
      sStringEventTable->Put(
        Substring(nsDependentAtomString(eventArray[i].mAtom), 2),
        eventArray[i]);
    }
  }

  return true;
}

void
nsContentUtils::InitializeTouchEventTable()
{
  static bool sEventTableInitialized = false;
  if (!sEventTableInitialized && sAtomEventTable && sStringEventTable) {
    sEventTableInitialized = true;
    static const EventNameMapping touchEventArray[] = {
#define EVENT(name_,  _message, _type, _class)
#define TOUCH_EVENT(name_,  _message, _type, _class)      \
      { nsGkAtoms::on##name_, _type, _message, _class },
#include "mozilla/EventNameList.h"
#undef TOUCH_EVENT
#undef EVENT
      { nullptr }
    };
    // Subtract one from the length because of the trailing null
    for (uint32_t i = 0; i < ArrayLength(touchEventArray) - 1; ++i) {
      sAtomEventTable->Put(touchEventArray[i].mAtom, touchEventArray[i]);
      sStringEventTable->Put(Substring(nsDependentAtomString(touchEventArray[i].mAtom), 2),
                             touchEventArray[i]);
    }
  }
}

static bool
Is8bit(const nsAString& aString)
{
  static const char16_t EIGHT_BIT = char16_t(~0x00FF);

  for (nsAString::const_char_iterator start = aString.BeginReading(),
         end = aString.EndReading();
       start != end;
       ++start) {
    if (*start & EIGHT_BIT) {
      return false;
    }
  }

  return true;
}

nsresult
nsContentUtils::Btoa(const nsAString& aBinaryData,
                     nsAString& aAsciiBase64String)
{
  if (!Is8bit(aBinaryData)) {
    aAsciiBase64String.Truncate();
    return NS_ERROR_DOM_INVALID_CHARACTER_ERR;
  }

  return Base64Encode(aBinaryData, aAsciiBase64String);
}

nsresult
nsContentUtils::Atob(const nsAString& aAsciiBase64String,
                     nsAString& aBinaryData)
{
  if (!Is8bit(aAsciiBase64String)) {
    aBinaryData.Truncate();
    return NS_ERROR_DOM_INVALID_CHARACTER_ERR;
  }

  const char16_t* start = aAsciiBase64String.BeginReading();
  const char16_t* cur = start;
  const char16_t* end = aAsciiBase64String.EndReading();
  bool hasWhitespace = false;

  while (cur < end) {
    if (nsContentUtils::IsHTMLWhitespace(*cur)) {
      hasWhitespace = true;
      break;
    }
    cur++;
  }

  nsresult rv;

  if (hasWhitespace) {
    nsString trimmedString;

    if (!trimmedString.SetCapacity(aAsciiBase64String.Length(), fallible)) {
      return NS_ERROR_DOM_INVALID_CHARACTER_ERR;
    }

    trimmedString.Append(start, cur - start);

    while (cur < end) {
      if (!nsContentUtils::IsHTMLWhitespace(*cur)) {
        trimmedString.Append(*cur);
      }
      cur++;
    }
    rv = Base64Decode(trimmedString, aBinaryData);
  } else {
    rv = Base64Decode(aAsciiBase64String, aBinaryData);
  }

  if (NS_FAILED(rv) && rv == NS_ERROR_INVALID_ARG) {
    return NS_ERROR_DOM_INVALID_CHARACTER_ERR;
  }
  return rv;
}

bool
nsContentUtils::IsAutocompleteEnabled(nsIDOMHTMLInputElement* aInput)
{
  NS_PRECONDITION(aInput, "aInput should not be null!");

  nsAutoString autocomplete;
  aInput->GetAutocomplete(autocomplete);

  if (autocomplete.IsEmpty()) {
    nsCOMPtr<nsIDOMHTMLFormElement> form;
    aInput->GetForm(getter_AddRefs(form));
    if (!form) {
      return true;
    }

    form->GetAutocomplete(autocomplete);
  }

  return !autocomplete.EqualsLiteral("off");
}

nsContentUtils::AutocompleteAttrState
nsContentUtils::SerializeAutocompleteAttribute(const nsAttrValue* aAttr,
                                               nsAString& aResult,
                                               AutocompleteAttrState aCachedState)
{
  if (!aAttr ||
      aCachedState == nsContentUtils::eAutocompleteAttrState_Invalid) {
    return aCachedState;
  }

  if (aCachedState == nsContentUtils::eAutocompleteAttrState_Valid) {
    uint32_t atomCount = aAttr->GetAtomCount();
    for (uint32_t i = 0; i < atomCount; i++) {
      if (i != 0) {
        aResult.Append(' ');
      }
      aResult.Append(nsDependentAtomString(aAttr->AtomAt(i)));
    }
    nsContentUtils::ASCIIToLower(aResult);
    return aCachedState;
  }

  aResult.Truncate();

  mozilla::dom::AutocompleteInfo info;
  AutocompleteAttrState state =
    InternalSerializeAutocompleteAttribute(aAttr, info);
  if (state == eAutocompleteAttrState_Valid) {
    // Concatenate the info fields.
    aResult = info.mSection;

    if (!info.mAddressType.IsEmpty()) {
      if (!aResult.IsEmpty()) {
        aResult += ' ';
      }
      aResult += info.mAddressType;
    }

    if (!info.mContactType.IsEmpty()) {
      if (!aResult.IsEmpty()) {
        aResult += ' ';
      }
      aResult += info.mContactType;
    }

    if (!info.mFieldName.IsEmpty()) {
      if (!aResult.IsEmpty()) {
        aResult += ' ';
      }
      aResult += info.mFieldName;
    }
  }

  return state;
}

nsContentUtils::AutocompleteAttrState
nsContentUtils::SerializeAutocompleteAttribute(const nsAttrValue* aAttr,
                                               mozilla::dom::AutocompleteInfo& aInfo,
                                               AutocompleteAttrState aCachedState,
                                               bool aGrantAllValidValue)
{
  if (!aAttr ||
      aCachedState == nsContentUtils::eAutocompleteAttrState_Invalid) {
    return aCachedState;
  }

  return InternalSerializeAutocompleteAttribute(aAttr, aInfo, aGrantAllValidValue);
}

/**
 * Helper to validate the @autocomplete tokens.
 *
 * @return {AutocompleteAttrState} The state of the attribute (invalid/valid).
 */
nsContentUtils::AutocompleteAttrState
nsContentUtils::InternalSerializeAutocompleteAttribute(const nsAttrValue* aAttrVal,
                                                       mozilla::dom::AutocompleteInfo& aInfo,
                                                       bool aGrantAllValidValue)
{
  // No autocomplete attribute so we are done
  if (!aAttrVal) {
    return eAutocompleteAttrState_Invalid;
  }

  uint32_t numTokens = aAttrVal->GetAtomCount();
  if (!numTokens) {
    return eAutocompleteAttrState_Invalid;
  }

  uint32_t index = numTokens - 1;
  nsString tokenString = nsDependentAtomString(aAttrVal->AtomAt(index));
  AutocompleteCategory category;
  nsAttrValue enumValue;

  bool unsupported = false;
  if (!aGrantAllValidValue) {
    unsupported = enumValue.ParseEnumValue(tokenString,
                                           kAutocompleteUnsupportedFieldNameTable,
                                           false);
    if (unsupported) {
      return eAutocompleteAttrState_Invalid;
    }
  }

  nsAutoString str;
  bool result = enumValue.ParseEnumValue(tokenString, kAutocompleteFieldNameTable, false);
  if (result) {
    // Off/Automatic/Normal categories.
    if (enumValue.Equals(NS_LITERAL_STRING("off"), eIgnoreCase) ||
        enumValue.Equals(NS_LITERAL_STRING("on"), eIgnoreCase)) {
      if (numTokens > 1) {
        return eAutocompleteAttrState_Invalid;
      }
      enumValue.ToString(str);
      ASCIIToLower(str);
      aInfo.mFieldName.Assign(str);
      return eAutocompleteAttrState_Valid;
    }

    // Only allow on/off if form autofill @autocomplete values aren't enabled
    // and it doesn't grant all valid values.
    if (!sIsFormAutofillAutocompleteEnabled && !aGrantAllValidValue) {
      return eAutocompleteAttrState_Invalid;
    }

    // Normal category
    if (numTokens > 3) {
      return eAutocompleteAttrState_Invalid;
    }
    category = eAutocompleteCategory_NORMAL;
  } else { // Check if the last token is of the contact category instead.
    // Only allow on/off if form autofill @autocomplete values aren't enabled
    // and it doesn't grant all valid values.
    if (!sIsFormAutofillAutocompleteEnabled && !aGrantAllValidValue) {
      return eAutocompleteAttrState_Invalid;
    }

    result = enumValue.ParseEnumValue(tokenString, kAutocompleteContactFieldNameTable, false);
    if (!result || numTokens > 4) {
      return eAutocompleteAttrState_Invalid;
    }

    category = eAutocompleteCategory_CONTACT;
  }

  enumValue.ToString(str);
  ASCIIToLower(str);
  aInfo.mFieldName.Assign(str);

  // We are done if this was the only token.
  if (numTokens == 1) {
    return eAutocompleteAttrState_Valid;
  }

  --index;
  tokenString = nsDependentAtomString(aAttrVal->AtomAt(index));

  if (category == eAutocompleteCategory_CONTACT) {
    if (!aGrantAllValidValue) {
      unsupported = enumValue.ParseEnumValue(tokenString,
                                             kAutocompleteUnsupportedContactFieldHintTable,
                                             false);
      if (unsupported) {
        return eAutocompleteAttrState_Invalid;
      }
    }


    nsAttrValue contactFieldHint;
    result = contactFieldHint.ParseEnumValue(tokenString, kAutocompleteContactFieldHintTable, false);
    if (result) {
      nsAutoString contactFieldHintString;
      contactFieldHint.ToString(contactFieldHintString);
      ASCIIToLower(contactFieldHintString);
      aInfo.mContactType.Assign(contactFieldHintString);
      if (index == 0) {
        return eAutocompleteAttrState_Valid;
      }
      --index;
      tokenString = nsDependentAtomString(aAttrVal->AtomAt(index));
    }
  }

  // Check for billing/shipping tokens
  nsAttrValue fieldHint;
  if (fieldHint.ParseEnumValue(tokenString, kAutocompleteFieldHintTable, false)) {
    nsString fieldHintString;
    fieldHint.ToString(fieldHintString);
    ASCIIToLower(fieldHintString);
    aInfo.mAddressType.Assign(fieldHintString);
    if (index == 0) {
      return eAutocompleteAttrState_Valid;
    }
    --index;
    tokenString = nsDependentAtomString(aAttrVal->AtomAt(index));
  }

  // Check for section-* token
  const nsDependentSubstring& section = Substring(tokenString, 0, 8);
  if (section.LowerCaseEqualsASCII("section-")) {
    ASCIIToLower(tokenString);
    aInfo.mSection.Assign(tokenString);
    if (index == 0) {
      return eAutocompleteAttrState_Valid;
    }
  }

  // Clear the fields as the autocomplete attribute is invalid.
  aInfo.mSection.Truncate();
  aInfo.mAddressType.Truncate();
  aInfo.mContactType.Truncate();
  aInfo.mFieldName.Truncate();

  return eAutocompleteAttrState_Invalid;
}

// Parse an integer according to HTML spec
int32_t
nsContentUtils::ParseHTMLInteger(const nsAString& aValue,
                                 ParseHTMLIntegerResultFlags *aResult)
{
  int result = eParseHTMLInteger_NoFlags;

  nsAString::const_iterator iter, end;
  aValue.BeginReading(iter);
  aValue.EndReading(end);

  while (iter != end && nsContentUtils::IsHTMLWhitespace(*iter)) {
    result |= eParseHTMLInteger_NonStandard;
    ++iter;
  }

  if (iter == end) {
    result |= eParseHTMLInteger_Error | eParseHTMLInteger_ErrorNoValue;
    *aResult = (ParseHTMLIntegerResultFlags)result;
    return 0;
  }

  int sign = 1;
  if (*iter == char16_t('-')) {
    sign = -1;
    result |= eParseHTMLInteger_Negative;
    ++iter;
  } else if (*iter == char16_t('+')) {
    result |= eParseHTMLInteger_NonStandard;
    ++iter;
  }

  bool foundValue = false;
  CheckedInt32 value = 0;

  // Check for leading zeros first.
  uint64_t leadingZeros = 0;
  while (iter != end) {
    if (*iter != char16_t('0')) {
      break;
    }

    ++leadingZeros;
    foundValue = true;
    ++iter;
  }

  while (iter != end) {
    if (*iter >= char16_t('0') && *iter <= char16_t('9')) {
      value = (value * 10) + (*iter - char16_t('0')) * sign;
      ++iter;
      if (!value.isValid()) {
        result |= eParseHTMLInteger_Error | eParseHTMLInteger_ErrorOverflow;
        break;
      }
      foundValue = true;
    } else if (*iter == char16_t('%')) {
      ++iter;
      result |= eParseHTMLInteger_IsPercent;
      break;
    } else {
      break;
    }
  }

  if (!foundValue) {
    result |= eParseHTMLInteger_Error | eParseHTMLInteger_ErrorNoValue;
  }

  if (value.isValid() &&
       ((leadingZeros > 1 || (leadingZeros == 1 && !(value == 0))) ||
       (sign == -1 && value == 0))) {
    result |= eParseHTMLInteger_NonStandard;
  }

  if (iter != end) {
    result |= eParseHTMLInteger_DidNotConsumeAllInput;
  }

  *aResult = (ParseHTMLIntegerResultFlags)result;
  return value.isValid() ? value.value() : 0;
}

#define SKIP_WHITESPACE(iter, end_iter, end_res)                 \
  while ((iter) != (end_iter) && nsCRT::IsAsciiSpace(*(iter))) { \
    ++(iter);                                                    \
  }                                                              \
  if ((iter) == (end_iter)) {                                    \
    return (end_res);                                            \
  }

#define SKIP_ATTR_NAME(iter, end_iter)                            \
  while ((iter) != (end_iter) && !nsCRT::IsAsciiSpace(*(iter)) && \
         *(iter) != '=') {                                        \
    ++(iter);                                                     \
  }

bool
nsContentUtils::GetPseudoAttributeValue(const nsString& aSource, nsAtom *aName,
                                        nsAString& aValue)
{
  aValue.Truncate();

  const char16_t *start = aSource.get();
  const char16_t *end = start + aSource.Length();
  const char16_t *iter;

  while (start != end) {
    SKIP_WHITESPACE(start, end, false)
    iter = start;
    SKIP_ATTR_NAME(iter, end)

    if (start == iter) {
      return false;
    }

    // Remember the attr name.
    const nsDependentSubstring & attrName = Substring(start, iter);

    // Now check whether this is a valid name="value" pair.
    start = iter;
    SKIP_WHITESPACE(start, end, false)
    if (*start != '=') {
      // No '=', so this is not a name="value" pair.  We don't know
      // what it is, and we have no way to handle it.
      return false;
    }

    // Have to skip the value.
    ++start;
    SKIP_WHITESPACE(start, end, false)
    char16_t q = *start;
    if (q != kQuote && q != kApostrophe) {
      // Not a valid quoted value, so bail.
      return false;
    }

    ++start;  // Point to the first char of the value.
    iter = start;

    while (iter != end && *iter != q) {
      ++iter;
    }

    if (iter == end) {
      // Oops, unterminated quoted string.
      return false;
    }

    // At this point attrName holds the name of the "attribute" and
    // the value is between start and iter.

    if (aName->Equals(attrName)) {
      // We'll accumulate as many characters as possible (until we hit either
      // the end of the string or the beginning of an entity). Chunks will be
      // delimited by start and chunkEnd.
      const char16_t *chunkEnd = start;
      while (chunkEnd != iter) {
        if (*chunkEnd == kLessThan) {
          aValue.Truncate();

          return false;
        }

        if (*chunkEnd == kAmpersand) {
          aValue.Append(start, chunkEnd - start);

          const char16_t *afterEntity = nullptr;
          char16_t result[2];
          uint32_t count =
            MOZ_XMLTranslateEntity(reinterpret_cast<const char*>(chunkEnd),
                                   reinterpret_cast<const char*>(iter),
                                   reinterpret_cast<const char**>(&afterEntity),
                                   result);
          if (count == 0) {
            aValue.Truncate();

            return false;
          }

          aValue.Append(result, count);

          // Advance to after the entity and begin a new chunk.
          start = chunkEnd = afterEntity;
        }
        else {
          ++chunkEnd;
        }
      }

      // Append remainder.
      aValue.Append(start, iter - start);

      return true;
    }

    // Resume scanning after the end of the attribute value (past the quote
    // char).
    start = iter + 1;
  }

  return false;
}

bool
nsContentUtils::IsJavaScriptLanguage(const nsString& aName)
{
  return aName.LowerCaseEqualsLiteral("javascript") ||
         aName.LowerCaseEqualsLiteral("livescript") ||
         aName.LowerCaseEqualsLiteral("mocha") ||
         aName.LowerCaseEqualsLiteral("javascript1.0") ||
         aName.LowerCaseEqualsLiteral("javascript1.1") ||
         aName.LowerCaseEqualsLiteral("javascript1.2") ||
         aName.LowerCaseEqualsLiteral("javascript1.3") ||
         aName.LowerCaseEqualsLiteral("javascript1.4") ||
         aName.LowerCaseEqualsLiteral("javascript1.5");
}

void
nsContentUtils::SplitMimeType(const nsAString& aValue, nsString& aType,
                              nsString& aParams)
{
  aType.Truncate();
  aParams.Truncate();
  int32_t semiIndex = aValue.FindChar(char16_t(';'));
  if (-1 != semiIndex) {
    aType = Substring(aValue, 0, semiIndex);
    aParams = Substring(aValue, semiIndex + 1,
                       aValue.Length() - (semiIndex + 1));
    aParams.StripWhitespace();
  }
  else {
    aType = aValue;
  }
  aType.StripWhitespace();
}

nsresult
nsContentUtils::IsUserIdle(uint32_t aRequestedIdleTimeInMS, bool* aUserIsIdle)
{
  nsresult rv;
  nsCOMPtr<nsIIdleService> idleService =
    do_GetService("@mozilla.org/widget/idleservice;1", &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  uint32_t idleTimeInMS;
  rv = idleService->GetIdleTime(&idleTimeInMS);
  NS_ENSURE_SUCCESS(rv, rv);

  *aUserIsIdle = idleTimeInMS >= aRequestedIdleTimeInMS;
  return NS_OK;
}

/**
* A helper function that parses a sandbox attribute (of an <iframe> or a CSP
* directive) and converts it to the set of flags used internally.
*
* @param aSandboxAttr  the sandbox attribute
* @return              the set of flags (SANDBOXED_NONE if aSandboxAttr is
*                      null)
*/
uint32_t
nsContentUtils::ParseSandboxAttributeToFlags(const nsAttrValue* aSandboxAttr)
{
  if (!aSandboxAttr) {
    return SANDBOXED_NONE;
  }

  uint32_t out = SANDBOX_ALL_FLAGS;

#define SANDBOX_KEYWORD(string, atom, flags)                  \
  if (aSandboxAttr->Contains(nsGkAtoms::atom, eIgnoreCase)) { \
    out &= ~(flags);                                          \
  }
#include "IframeSandboxKeywordList.h"
#undef SANDBOX_KEYWORD

  return out;
}

/**
* A helper function that checks if a string matches a valid sandbox flag.
*
* @param aFlag   the potential sandbox flag.
* @return        true if the flag is a sandbox flag.
*/
bool
nsContentUtils::IsValidSandboxFlag(const nsAString& aFlag)
{
#define SANDBOX_KEYWORD(string, atom, flags)                                  \
  if (EqualsIgnoreASCIICase(nsDependentAtomString(nsGkAtoms::atom), aFlag)) { \
    return true;                                                              \
  }
#include "IframeSandboxKeywordList.h"
#undef SANDBOX_KEYWORD
  return false;
}

/**
 * A helper function that returns a string attribute corresponding to the
 * sandbox flags.
 *
 * @param aFlags    the sandbox flags
 * @param aString   the attribute corresponding to the flags (null if aFlags
 *                  is zero)
 */
void
nsContentUtils::SandboxFlagsToString(uint32_t aFlags, nsAString& aString)
{
  if (!aFlags) {
    SetDOMStringToNull(aString);
    return;
  }

  aString.Truncate();

#define SANDBOX_KEYWORD(string, atom, flags)                \
  if (!(aFlags & (flags))) {                                \
    if (!aString.IsEmpty()) {                               \
      aString.AppendLiteral(u" ");                          \
    }                                                       \
    aString.Append(nsDependentAtomString(nsGkAtoms::atom)); \
  }
#include "IframeSandboxKeywordList.h"
#undef SANDBOX_KEYWORD
}

nsIBidiKeyboard*
nsContentUtils::GetBidiKeyboard()
{
  if (!sBidiKeyboard) {
    nsresult rv = CallGetService("@mozilla.org/widget/bidikeyboard;1", &sBidiKeyboard);
    if (NS_FAILED(rv)) {
      sBidiKeyboard = nullptr;
    }
  }
  return sBidiKeyboard;
}

template <class OutputIterator>
struct NormalizeNewlinesCharTraits {
  public:
    typedef typename OutputIterator::value_type value_type;

  public:
    explicit NormalizeNewlinesCharTraits(OutputIterator& aIterator) : mIterator(aIterator) { }
    void writechar(typename OutputIterator::value_type aChar) {
      *mIterator++ = aChar;
    }

  private:
    OutputIterator mIterator;
};

template <class CharT>
struct NormalizeNewlinesCharTraits<CharT*> {
  public:
    typedef CharT value_type;

  public:
    explicit NormalizeNewlinesCharTraits(CharT* aCharPtr) : mCharPtr(aCharPtr) { }
    void writechar(CharT aChar) {
      *mCharPtr++ = aChar;
    }

  private:
    CharT* mCharPtr;
};

template <class OutputIterator>
class CopyNormalizeNewlines
{
  public:
    typedef typename OutputIterator::value_type value_type;

  public:
    explicit CopyNormalizeNewlines(OutputIterator* aDestination,
                                   bool aLastCharCR = false) :
      mLastCharCR(aLastCharCR),
      mDestination(aDestination),
      mWritten(0)
    { }

    uint32_t GetCharsWritten() {
      return mWritten;
    }

    bool IsLastCharCR() {
      return mLastCharCR;
    }

    void write(const typename OutputIterator::value_type* aSource, uint32_t aSourceLength) {

      const typename OutputIterator::value_type* done_writing = aSource + aSourceLength;

      // If the last source buffer ended with a CR...
      if (mLastCharCR) {
        // ..and if the next one is a LF, then skip it since
        // we've already written out a newline
        if (aSourceLength && (*aSource == value_type('\n'))) {
          ++aSource;
        }
        mLastCharCR = false;
      }

      uint32_t num_written = 0;
      while ( aSource < done_writing ) {
        if (*aSource == value_type('\r')) {
          mDestination->writechar('\n');
          ++aSource;
          // If we've reached the end of the buffer, record
          // that we wrote out a CR
          if (aSource == done_writing) {
            mLastCharCR = true;
          }
          // If the next character is a LF, skip it
          else if (*aSource == value_type('\n')) {
            ++aSource;
          }
        }
        else {
          mDestination->writechar(*aSource++);
        }
        ++num_written;
      }

      mWritten += num_written;
    }

  private:
    bool mLastCharCR;
    OutputIterator* mDestination;
    uint32_t mWritten;
};

// static
uint32_t
nsContentUtils::CopyNewlineNormalizedUnicodeTo(const nsAString& aSource,
                                               uint32_t aSrcOffset,
                                               char16_t* aDest,
                                               uint32_t aLength,
                                               bool& aLastCharCR)
{
  typedef NormalizeNewlinesCharTraits<char16_t*> sink_traits;

  sink_traits dest_traits(aDest);
  CopyNormalizeNewlines<sink_traits> normalizer(&dest_traits,aLastCharCR);
  nsReadingIterator<char16_t> fromBegin, fromEnd;
  copy_string(aSource.BeginReading(fromBegin).advance( int32_t(aSrcOffset) ),
              aSource.BeginReading(fromEnd).advance( int32_t(aSrcOffset+aLength) ),
              normalizer);
  aLastCharCR = normalizer.IsLastCharCR();
  return normalizer.GetCharsWritten();
}

// static
uint32_t
nsContentUtils::CopyNewlineNormalizedUnicodeTo(nsReadingIterator<char16_t>& aSrcStart, const nsReadingIterator<char16_t>& aSrcEnd, nsAString& aDest)
{
  typedef nsWritingIterator<char16_t> WritingIterator;
  typedef NormalizeNewlinesCharTraits<WritingIterator> sink_traits;

  WritingIterator iter;
  aDest.BeginWriting(iter);
  sink_traits dest_traits(iter);
  CopyNormalizeNewlines<sink_traits> normalizer(&dest_traits);
  copy_string(aSrcStart, aSrcEnd, normalizer);
  return normalizer.GetCharsWritten();
}

/**
 * This is used to determine whether a character is in one of the classes
 * which CSS says should be part of the first-letter.  Currently, that is
 * all punctuation classes (P*).  Note that this is a change from CSS2
 * which excluded Pc and Pd.
 *
 * https://www.w3.org/TR/css-pseudo-4/#first-letter-pseudo
 * "Punctuation (i.e, characters that belong to the Punctuation (P*) Unicode
 *  general category [UAX44]) [...]"
 */

// static
bool
nsContentUtils::IsFirstLetterPunctuation(uint32_t aChar)
{
  switch (mozilla::unicode::GetGeneralCategory(aChar)) {
    case HB_UNICODE_GENERAL_CATEGORY_CONNECT_PUNCTUATION: /* Pc */
    case HB_UNICODE_GENERAL_CATEGORY_DASH_PUNCTUATION:    /* Pd */
    case HB_UNICODE_GENERAL_CATEGORY_CLOSE_PUNCTUATION:   /* Pe */
    case HB_UNICODE_GENERAL_CATEGORY_FINAL_PUNCTUATION:   /* Pf */
    case HB_UNICODE_GENERAL_CATEGORY_INITIAL_PUNCTUATION: /* Pi */
    case HB_UNICODE_GENERAL_CATEGORY_OTHER_PUNCTUATION:   /* Po */
    case HB_UNICODE_GENERAL_CATEGORY_OPEN_PUNCTUATION:    /* Ps */
      return true;
    default:
      return false;
  }
}

// static
bool
nsContentUtils::IsFirstLetterPunctuationAt(const nsTextFragment* aFrag, uint32_t aOffset)
{
  char16_t h = aFrag->CharAt(aOffset);
  if (!IS_SURROGATE(h)) {
    return IsFirstLetterPunctuation(h);
  }
  if (NS_IS_HIGH_SURROGATE(h) && aOffset + 1 < aFrag->GetLength()) {
    char16_t l = aFrag->CharAt(aOffset + 1);
    if (NS_IS_LOW_SURROGATE(l)) {
      return IsFirstLetterPunctuation(SURROGATE_TO_UCS4(h, l));
    }
  }
  return false;
}

// static
bool nsContentUtils::IsAlphanumeric(uint32_t aChar)
{
  nsUGenCategory cat = mozilla::unicode::GetGenCategory(aChar);

  return (cat == nsUGenCategory::kLetter || cat == nsUGenCategory::kNumber);
}

// static
bool nsContentUtils::IsAlphanumericAt(const nsTextFragment* aFrag, uint32_t aOffset)
{
  char16_t h = aFrag->CharAt(aOffset);
  if (!IS_SURROGATE(h)) {
    return IsAlphanumeric(h);
  }
  if (NS_IS_HIGH_SURROGATE(h) && aOffset + 1 < aFrag->GetLength()) {
    char16_t l = aFrag->CharAt(aOffset + 1);
    if (NS_IS_LOW_SURROGATE(l)) {
      return IsAlphanumeric(SURROGATE_TO_UCS4(h, l));
    }
  }
  return false;
}

/* static */
bool
nsContentUtils::IsHTMLWhitespace(char16_t aChar)
{
  return aChar == char16_t(0x0009) ||
         aChar == char16_t(0x000A) ||
         aChar == char16_t(0x000C) ||
         aChar == char16_t(0x000D) ||
         aChar == char16_t(0x0020);
}

/* static */
bool
nsContentUtils::IsHTMLWhitespaceOrNBSP(char16_t aChar)
{
  return IsHTMLWhitespace(aChar) || aChar == char16_t(0xA0);
}

/* static */
bool
nsContentUtils::IsHTMLBlock(nsIContent* aContent)
{
  return aContent->IsAnyOfHTMLElements(nsGkAtoms::address,
                                       nsGkAtoms::article,
                                       nsGkAtoms::aside,
                                       nsGkAtoms::blockquote,
                                       nsGkAtoms::center,
                                       nsGkAtoms::dir,
                                       nsGkAtoms::div,
                                       nsGkAtoms::dl, // XXX why not dt and dd?
                                       nsGkAtoms::fieldset,
                                       nsGkAtoms::figure, // XXX shouldn't figcaption be on this list
                                       nsGkAtoms::footer,
                                       nsGkAtoms::form,
                                       nsGkAtoms::h1,
                                       nsGkAtoms::h2,
                                       nsGkAtoms::h3,
                                       nsGkAtoms::h4,
                                       nsGkAtoms::h5,
                                       nsGkAtoms::h6,
                                       nsGkAtoms::header,
                                       nsGkAtoms::hgroup,
                                       nsGkAtoms::hr,
                                       nsGkAtoms::li,
                                       nsGkAtoms::listing,
                                       nsGkAtoms::menu,
                                       nsGkAtoms::multicol, // XXX get rid of this one?
                                       nsGkAtoms::nav,
                                       nsGkAtoms::ol,
                                       nsGkAtoms::p,
                                       nsGkAtoms::pre,
                                       nsGkAtoms::section,
                                       nsGkAtoms::table,
                                       nsGkAtoms::ul,
                                       nsGkAtoms::xmp);
}

/* static */
bool
nsContentUtils::ParseIntMarginValue(const nsAString& aString, nsIntMargin& result)
{
  nsAutoString marginStr(aString);
  marginStr.CompressWhitespace(true, true);
  if (marginStr.IsEmpty()) {
    return false;
  }

  int32_t start = 0, end = 0;
  for (int count = 0; count < 4; count++) {
    if ((uint32_t)end >= marginStr.Length())
      return false;

    // top, right, bottom, left
    if (count < 3)
      end = Substring(marginStr, start).FindChar(',');
    else
      end = Substring(marginStr, start).Length();

    if (end <= 0)
      return false;

    nsresult ec;
    int32_t val = nsString(Substring(marginStr, start, end)).ToInteger(&ec);
    if (NS_FAILED(ec))
      return false;

    switch(count) {
      case 0:
        result.top = val;
      break;
      case 1:
        result.right = val;
      break;
      case 2:
        result.bottom = val;
      break;
      case 3:
        result.left = val;
      break;
    }
    start += end + 1;
  }
  return true;
}

// static
int32_t
nsContentUtils::ParseLegacyFontSize(const nsAString& aValue)
{
  nsAString::const_iterator iter, end;
  aValue.BeginReading(iter);
  aValue.EndReading(end);

  while (iter != end && nsContentUtils::IsHTMLWhitespace(*iter)) {
    ++iter;
  }

  if (iter == end) {
    return 0;
  }

  bool relative = false;
  bool negate = false;
  if (*iter == char16_t('-')) {
    relative = true;
    negate = true;
    ++iter;
  } else if (*iter == char16_t('+')) {
    relative = true;
    ++iter;
  }

  if (iter == end || *iter < char16_t('0') || *iter > char16_t('9')) {
    return 0;
  }

  // We don't have to worry about overflow, since we can bail out as soon as
  // we're bigger than 7.
  int32_t value = 0;
  while (iter != end && *iter >= char16_t('0') && *iter <= char16_t('9')) {
    value = 10*value + (*iter - char16_t('0'));
    if (value >= 7) {
      break;
    }
    ++iter;
  }

  if (relative) {
    if (negate) {
      value = 3 - value;
    } else {
      value = 3 + value;
    }
  }

  return clamped(value, 1, 7);
}

/* static */
bool
nsContentUtils::IsControlledByServiceWorker(nsIDocument* aDocument)
{
  if (nsContentUtils::IsInPrivateBrowsing(aDocument)) {
    return false;
  }

  RefPtr<workers::ServiceWorkerManager> swm =
    workers::ServiceWorkerManager::GetInstance();
  MOZ_ASSERT(swm);

  ErrorResult rv;
  bool controlled = swm->IsControlled(aDocument, rv);
  if (NS_WARN_IF(rv.Failed())) {
    rv.SuppressException();
    return false;
  }

  return controlled;
}

/* static */
void
nsContentUtils::GetOfflineAppManifest(nsIDocument *aDocument, nsIURI **aURI)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aDocument);
  *aURI = nullptr;

  if (IsControlledByServiceWorker(aDocument)) {
    return;
  }

  Element* docElement = aDocument->GetRootElement();
  if (!docElement) {
    return;
  }

  nsAutoString manifestSpec;
  docElement->GetAttr(kNameSpaceID_None, nsGkAtoms::manifest, manifestSpec);

  // Manifest URIs can't have fragment identifiers.
  if (manifestSpec.IsEmpty() ||
      manifestSpec.Contains('#')) {
    return;
  }

  nsContentUtils::NewURIWithDocumentCharset(aURI, manifestSpec,
                                            aDocument,
                                            aDocument->GetDocBaseURI());
}

/* static */
bool
nsContentUtils::OfflineAppAllowed(nsIURI *aURI)
{
  nsCOMPtr<nsIOfflineCacheUpdateService> updateService =
    do_GetService(NS_OFFLINECACHEUPDATESERVICE_CONTRACTID);
  if (!updateService) {
    return false;
  }

  bool allowed;
  nsresult rv =
    updateService->OfflineAppAllowedForURI(aURI,
                                           Preferences::GetRootBranch(),
                                           &allowed);
  return NS_SUCCEEDED(rv) && allowed;
}

/* static */
bool
nsContentUtils::OfflineAppAllowed(nsIPrincipal *aPrincipal)
{
  nsCOMPtr<nsIOfflineCacheUpdateService> updateService =
    do_GetService(NS_OFFLINECACHEUPDATESERVICE_CONTRACTID);
  if (!updateService) {
    return false;
  }

  bool allowed;
  nsresult rv = updateService->OfflineAppAllowed(aPrincipal,
                                                 Preferences::GetRootBranch(),
                                                 &allowed);
  return NS_SUCCEEDED(rv) && allowed;
}

bool
nsContentUtils::MaybeAllowOfflineAppByDefault(nsIPrincipal *aPrincipal)
{
  if (!Preferences::GetRootBranch())
    return false;

  nsresult rv;

  bool allowedByDefault;
  rv = Preferences::GetRootBranch()->GetBoolPref(
    "offline-apps.allow_by_default", &allowedByDefault);
  if (NS_FAILED(rv))
    return false;

  if (!allowedByDefault)
    return false;

  nsCOMPtr<nsIOfflineCacheUpdateService> updateService =
    do_GetService(NS_OFFLINECACHEUPDATESERVICE_CONTRACTID);
  if (!updateService) {
    return false;
  }

  rv = updateService->AllowOfflineApp(aPrincipal);
  return NS_SUCCEEDED(rv);
}

// static
void
nsContentUtils::Shutdown()
{
  sInitialized = false;

  nsHTMLTags::ReleaseTable();

  NS_IF_RELEASE(sContentPolicyService);
  sTriedToGetContentPolicy = false;
  uint32_t i;
  for (i = 0; i < PropertiesFile_COUNT; ++i)
    NS_IF_RELEASE(sStringBundles[i]);

  NS_IF_RELEASE(sStringBundleService);
  NS_IF_RELEASE(sConsoleService);
  sXPConnect = nullptr;
  NS_IF_RELEASE(sSecurityManager);
  NS_IF_RELEASE(sSystemPrincipal);
  NS_IF_RELEASE(sNullSubjectPrincipal);
  NS_IF_RELEASE(sIOService);
  NS_IF_RELEASE(sUUIDGenerator);
  sLineBreaker = nullptr;
  sWordBreaker = nullptr;
  NS_IF_RELEASE(sBidiKeyboard);

  delete sAtomEventTable;
  sAtomEventTable = nullptr;
  delete sStringEventTable;
  sStringEventTable = nullptr;
  delete sUserDefinedEvents;
  sUserDefinedEvents = nullptr;

  if (sEventListenerManagersHash) {
    NS_ASSERTION(sEventListenerManagersHash->EntryCount() == 0,
                 "Event listener manager hash not empty at shutdown!");

    // See comment above.

    // However, we have to handle this table differently.  If it still
    // has entries, we want to leak it too, so that we can keep it alive
    // in case any elements are destroyed.  Because if they are, we need
    // their event listener managers to be destroyed too, or otherwise
    // it could leave dangling references in DOMClassInfo's preserved
    // wrapper table.

    if (sEventListenerManagersHash->EntryCount() == 0) {
      delete sEventListenerManagersHash;
      sEventListenerManagersHash = nullptr;
    }
  }

  NS_ASSERTION(!sBlockedScriptRunners ||
               sBlockedScriptRunners->Length() == 0,
               "How'd this happen?");
  delete sBlockedScriptRunners;
  sBlockedScriptRunners = nullptr;

  delete sShiftText;
  sShiftText = nullptr;
  delete sControlText;
  sControlText = nullptr;
  delete sMetaText;
  sMetaText = nullptr;
  delete sOSText;
  sOSText = nullptr;
  delete sAltText;
  sAltText = nullptr;
  delete sModifierSeparator;
  sModifierSeparator = nullptr;

  delete sJSBytecodeMimeType;
  sJSBytecodeMimeType = nullptr;

  NS_IF_RELEASE(sSameOriginChecker);

  if (sUserInteractionObserver) {
    sUserInteractionObserver->Shutdown();
    NS_RELEASE(sUserInteractionObserver);
  }

  HTMLInputElement::Shutdown();
  nsMappedAttributes::Shutdown();
}

/**
 * Checks whether two nodes come from the same origin. aTrustedNode is
 * considered 'safe' in that a user can operate on it and that it isn't
 * a js-object that implements nsIDOMNode.
 * Never call this function with the first node provided by script, it
 * must always be known to be a 'real' node!
 */
// static
nsresult
nsContentUtils::CheckSameOrigin(const nsINode *aTrustedNode,
                                nsIDOMNode *aUnTrustedNode)
{
  MOZ_ASSERT(aTrustedNode);

  // Make sure it's a real node.
  nsCOMPtr<nsINode> unTrustedNode = do_QueryInterface(aUnTrustedNode);
  NS_ENSURE_TRUE(unTrustedNode, NS_ERROR_UNEXPECTED);
  return CheckSameOrigin(aTrustedNode, unTrustedNode);
}

nsresult
nsContentUtils::CheckSameOrigin(const nsINode* aTrustedNode,
                                const nsINode* unTrustedNode)
{
  MOZ_ASSERT(aTrustedNode);
  MOZ_ASSERT(unTrustedNode);

  /*
   * Get hold of each node's principal
   */

  nsIPrincipal* trustedPrincipal = aTrustedNode->NodePrincipal();
  nsIPrincipal* unTrustedPrincipal = unTrustedNode->NodePrincipal();

  if (trustedPrincipal == unTrustedPrincipal) {
    return NS_OK;
  }

  bool equal;
  // XXXbz should we actually have a Subsumes() check here instead?  Or perhaps
  // a separate method for that, with callers using one or the other?
  if (NS_FAILED(trustedPrincipal->Equals(unTrustedPrincipal, &equal)) ||
      !equal) {
    return NS_ERROR_DOM_PROP_ACCESS_DENIED;
  }

  return NS_OK;
}

// static
bool
nsContentUtils::CanCallerAccess(nsIPrincipal* aSubjectPrincipal,
                                nsIPrincipal* aPrincipal)
{
  bool subsumes;
  nsresult rv = aSubjectPrincipal->Subsumes(aPrincipal, &subsumes);
  NS_ENSURE_SUCCESS(rv, false);

  if (subsumes) {
    return true;
  }

  // The subject doesn't subsume aPrincipal. Allow access only if the subject
  // is chrome.
  return IsCallerChrome();
}

// static
bool
nsContentUtils::CanCallerAccess(nsIDOMNode *aNode)
{
  nsCOMPtr<nsINode> node = do_QueryInterface(aNode);
  NS_ENSURE_TRUE(node, false);
  return CanCallerAccess(node);
}

// static
bool
nsContentUtils::CanCallerAccess(nsINode* aNode)
{
  return CanCallerAccess(SubjectPrincipal(), aNode->NodePrincipal());
}

// static
bool
nsContentUtils::CanCallerAccess(nsPIDOMWindowInner* aWindow)
{
  nsCOMPtr<nsIScriptObjectPrincipal> scriptObject = do_QueryInterface(aWindow);
  NS_ENSURE_TRUE(scriptObject, false);

  return CanCallerAccess(SubjectPrincipal(), scriptObject->GetPrincipal());
}

// static
bool
nsContentUtils::PrincipalHasPermission(nsIPrincipal* aPrincipal, const nsAtom* aPerm)
{
  // Chrome gets access by default.
  if (IsSystemPrincipal(aPrincipal)) {
    return true;
  }

  // Otherwise, only allow if caller is an addon with the permission.
  return BasePrincipal::Cast(aPrincipal)->AddonHasPermission(aPerm);
}

// static
bool
nsContentUtils::CallerHasPermission(JSContext* aCx, const nsAtom* aPerm)
{
  return PrincipalHasPermission(SubjectPrincipal(aCx), aPerm);
}

// static
nsIPrincipal*
nsContentUtils::GetAttrTriggeringPrincipal(nsIContent* aContent, const nsAString& aAttrValue,
                                           nsIPrincipal* aSubjectPrincipal)
{
  nsIPrincipal* contentPrin = aContent ? aContent->NodePrincipal() : nullptr;

  // If the subject principal is the same as the content principal, or no
  // explicit subject principal was provided, we don't need to do any further
  // checks. Just return the content principal.
  if (contentPrin == aSubjectPrincipal || !aSubjectPrincipal) {
    return contentPrin;
  }

  // If the attribute value is empty, it's not an absolute URL, so don't bother
  // with more expensive checks.
  if (!aAttrValue.IsEmpty() &&
      IsAbsoluteURL(NS_ConvertUTF16toUTF8(aAttrValue))) {
    return aSubjectPrincipal;
  }

  return contentPrin;
}

// static
bool
nsContentUtils::IsAbsoluteURL(const nsACString& aURL)
{
  nsAutoCString scheme;
  if (NS_FAILED(net_ExtractURLScheme(aURL, scheme))) {
    // If we can't extract a scheme, it's not an absolute URL.
    return false;
  }

  // If it parses as an absolute StandardURL, it's definitely an absolute URL,
  // so no need to check with the IO service.
  if (net_IsAbsoluteURL(aURL)) {
    return true;
  }

  uint32_t flags;
  if (NS_SUCCEEDED(sIOService->GetProtocolFlags(scheme.get(), &flags))) {
    return flags & nsIProtocolHandler::URI_NORELATIVE;
  }

  return false;
}

//static
bool
nsContentUtils::InProlog(nsINode *aNode)
{
  NS_PRECONDITION(aNode, "missing node to nsContentUtils::InProlog");

  nsINode* parent = aNode->GetParentNode();
  if (!parent || !parent->IsNodeOfType(nsINode::eDOCUMENT)) {
    return false;
  }

  nsIDocument* doc = static_cast<nsIDocument*>(parent);
  nsIContent* root = doc->GetRootElement();

  return !root || doc->IndexOf(aNode) < doc->IndexOf(root);
}

nsIDocument*
nsContentUtils::GetDocumentFromCaller()
{
  AutoJSContext cx;

  nsCOMPtr<nsPIDOMWindowInner> win =
    do_QueryInterface(nsJSUtils::GetStaticScriptGlobal(JS::CurrentGlobalOrNull(cx)));
  if (!win) {
    return nullptr;
  }

  return win->GetExtantDoc();
}

bool
nsContentUtils::IsCallerChrome()
{
  MOZ_ASSERT(NS_IsMainThread());
  if (SubjectPrincipal() == sSystemPrincipal) {
    return true;
  }

  // If the check failed, look for UniversalXPConnect on the cx compartment.
  return xpc::IsUniversalXPConnectEnabled(GetCurrentJSContext());
}

/* static */
bool
nsContentUtils::ShouldResistFingerprinting()
{
  if (NS_IsMainThread()) {
    return nsRFPService::IsResistFingerprintingEnabled();
  }

  workers::WorkerPrivate* workerPrivate = workers::GetCurrentThreadWorkerPrivate();
  if (NS_WARN_IF(!workerPrivate)) {
    return false;
  }
  workerPrivate->AssertIsOnWorkerThread();

  return workerPrivate->ResistFingerprintingEnabled();
}

bool
nsContentUtils::ShouldResistFingerprinting(nsIDocShell* aDocShell)
{
  if (!aDocShell) {
    return false;
  }
  return ShouldResistFingerprinting(aDocShell->GetDocument());
}

/* static */
bool
nsContentUtils::ShouldResistFingerprinting(nsIDocument* aDoc) {
  if (!aDoc) {
    return false;
  }
  bool isChrome = nsContentUtils::IsChromeDoc(aDoc);
  return !isChrome && ShouldResistFingerprinting();
}

/* static */
void
nsContentUtils::CalcRoundedWindowSizeForResistingFingerprinting(int32_t  aChromeWidth,
                                                                int32_t  aChromeHeight,
                                                                int32_t  aScreenWidth,
                                                                int32_t  aScreenHeight,
                                                                int32_t  aInputWidth,
                                                                int32_t  aInputHeight,
                                                                bool     aSetOuterWidth,
                                                                bool     aSetOuterHeight,
                                                                int32_t* aOutputWidth,
                                                                int32_t* aOutputHeight)
{
  MOZ_ASSERT(aOutputWidth);
  MOZ_ASSERT(aOutputHeight);

  int32_t availContentWidth  = 0;
  int32_t availContentHeight = 0;

  availContentWidth = std::min(sPrivacyMaxInnerWidth,
                               aScreenWidth - aChromeWidth);
#ifdef MOZ_WIDGET_GTK
  // In the GTK window, it will not report outside system decorations
  // when we get available window size, see Bug 581863. So, we leave a
  // 40 pixels space for them when calculating the available content
  // height. It is not necessary for the width since the content width
  // is usually pretty much the same as the chrome width.
  availContentHeight = std::min(sPrivacyMaxInnerHeight,
                                (-40 + aScreenHeight) - aChromeHeight);
#else
  availContentHeight = std::min(sPrivacyMaxInnerHeight,
                                aScreenHeight - aChromeHeight);
#endif

  // Ideally, we'd like to round window size to 1000x1000, but the
  // screen space could be too small to accommodate this size in some
  // cases. If it happens, we would round the window size to the nearest
  // 200x100.
  availContentWidth = availContentWidth - (availContentWidth % 200);
  availContentHeight = availContentHeight - (availContentHeight % 100);

  // If aIsOuter is true, we are setting the outer window. So we
  // have to consider the chrome UI.
  int32_t chromeOffsetWidth = aSetOuterWidth ? aChromeWidth : 0;
  int32_t chromeOffsetHeight = aSetOuterHeight ? aChromeHeight : 0;
  int32_t resultWidth = 0, resultHeight = 0;

  // if the original size is greater than the maximum available size, we set
  // it to the maximum size. And if the original value is less than the
  // minimum rounded size, we set it to the minimum 200x100.
  if (aInputWidth > (availContentWidth + chromeOffsetWidth)) {
    resultWidth = availContentWidth + chromeOffsetWidth;
  } else if (aInputWidth < (200 + chromeOffsetWidth)) {
    resultWidth = 200 + chromeOffsetWidth;
  } else {
    // Otherwise, we round the window to the nearest upper rounded 200x100.
    resultWidth = NSToIntCeil((aInputWidth - chromeOffsetWidth) / 200.0) * 200 + chromeOffsetWidth;
  }

  if (aInputHeight > (availContentHeight + chromeOffsetHeight)) {
    resultHeight = availContentHeight + chromeOffsetHeight;
  } else if (aInputHeight < (100 + chromeOffsetHeight)) {
    resultHeight = 100 + chromeOffsetHeight;
  } else {
    resultHeight = NSToIntCeil((aInputHeight - chromeOffsetHeight) / 100.0) * 100 + chromeOffsetHeight;
  }

  *aOutputWidth = resultWidth;
  *aOutputHeight = resultHeight;
}

bool
nsContentUtils::ThreadsafeIsCallerChrome()
{
  return NS_IsMainThread() ?
    IsCallerChrome() :
    mozilla::dom::workers::IsCurrentThreadRunningChromeWorker();
}

bool
nsContentUtils::IsCallerContentXBL()
{
  JSContext *cx = GetCurrentJSContext();
  if (!cx)
    return false;

  JS::Realm* realm = JS::GetCurrentRealmOrNull(cx);
  if (!realm)
    return false;

  // For remote XUL, we run XBL in the XUL scope. Given that we care about
  // compat and not security for remote XUL, just always claim to be XBL.
  if (!xpc::AllowContentXBLScope(realm)) {
    DebugOnly<JSCompartment*> c = JS::GetCompartmentForRealm(realm);
    MOZ_ASSERT(nsContentUtils::AllowXULXBLForPrincipal(xpc::GetCompartmentPrincipal(c)));
    return true;
  }

  return xpc::IsContentXBLScope(realm);
}

bool
nsContentUtils::IsSystemCaller(JSContext* aCx)
{
  // Note that SubjectPrincipal() assumes we are in a compartment here.
  return SubjectPrincipal(aCx) == sSystemPrincipal;
}

bool
nsContentUtils::ThreadsafeIsSystemCaller(JSContext* aCx)
{
  if (NS_IsMainThread()) {
    return IsSystemCaller(aCx);
  }

  return workers::GetWorkerPrivateFromContext(aCx)->UsesSystemPrincipal();
}

// static
bool
nsContentUtils::LookupBindingMember(JSContext* aCx, nsIContent *aContent,
                                    JS::Handle<jsid> aId,
                                    JS::MutableHandle<JS::PropertyDescriptor> aDesc)
{
  nsXBLBinding* binding = aContent->GetXBLBinding();
  if (!binding)
    return true;
  return binding->LookupMember(aCx, aId, aDesc);
}

// static
nsINode*
nsContentUtils::GetCrossDocParentNode(nsINode* aChild)
{
  NS_PRECONDITION(aChild, "The child is null!");

  nsINode* parent = aChild->GetParentNode();
  if (parent && parent->IsContent() && aChild->IsContent()) {
    parent = aChild->AsContent()->GetFlattenedTreeParent();
  }

  if (parent || !aChild->IsNodeOfType(nsINode::eDOCUMENT))
    return parent;

  nsIDocument* doc = static_cast<nsIDocument*>(aChild);
  nsIDocument* parentDoc = doc->GetParentDocument();
  return parentDoc ? parentDoc->FindContentForSubDocument(doc) : nullptr;
}

// static
bool
nsContentUtils::ContentIsDescendantOf(const nsINode* aPossibleDescendant,
                                      const nsINode* aPossibleAncestor)
{
  NS_PRECONDITION(aPossibleDescendant, "The possible descendant is null!");
  NS_PRECONDITION(aPossibleAncestor, "The possible ancestor is null!");

  do {
    if (aPossibleDescendant == aPossibleAncestor)
      return true;
    aPossibleDescendant = aPossibleDescendant->GetParentNode();
  } while (aPossibleDescendant);

  return false;
}

bool
nsContentUtils::ContentIsHostIncludingDescendantOf(
  const nsINode* aPossibleDescendant, const nsINode* aPossibleAncestor)
{
  NS_PRECONDITION(aPossibleDescendant, "The possible descendant is null!");
  NS_PRECONDITION(aPossibleAncestor, "The possible ancestor is null!");

  do {
    if (aPossibleDescendant == aPossibleAncestor)
      return true;
    if (aPossibleDescendant->NodeType() == nsIDOMNode::DOCUMENT_FRAGMENT_NODE) {
      aPossibleDescendant =
        static_cast<const DocumentFragment*>(aPossibleDescendant)->GetHost();
    } else {
      aPossibleDescendant = aPossibleDescendant->GetParentNode();
    }
  } while (aPossibleDescendant);

  return false;
}

// static
bool
nsContentUtils::ContentIsCrossDocDescendantOf(nsINode* aPossibleDescendant,
                                              nsINode* aPossibleAncestor)
{
  NS_PRECONDITION(aPossibleDescendant, "The possible descendant is null!");
  NS_PRECONDITION(aPossibleAncestor, "The possible ancestor is null!");

  do {
    if (aPossibleDescendant == aPossibleAncestor)
      return true;

    aPossibleDescendant = GetCrossDocParentNode(aPossibleDescendant);
  } while (aPossibleDescendant);

  return false;
}

// static
bool
nsContentUtils::ContentIsFlattenedTreeDescendantOf(
  const nsINode* aPossibleDescendant,
  const nsINode* aPossibleAncestor)
{
  NS_PRECONDITION(aPossibleDescendant, "The possible descendant is null!");
  NS_PRECONDITION(aPossibleAncestor, "The possible ancestor is null!");

  do {
    if (aPossibleDescendant == aPossibleAncestor) {
      return true;
    }
    aPossibleDescendant = aPossibleDescendant->GetFlattenedTreeParentNode();
  } while (aPossibleDescendant);

  return false;
}

// static
bool
nsContentUtils::ContentIsFlattenedTreeDescendantOfForStyle(
  const nsINode* aPossibleDescendant,
  const nsINode* aPossibleAncestor)
{
  NS_PRECONDITION(aPossibleDescendant, "The possible descendant is null!");
  NS_PRECONDITION(aPossibleAncestor, "The possible ancestor is null!");

  do {
    if (aPossibleDescendant == aPossibleAncestor) {
      return true;
    }
    aPossibleDescendant =
      aPossibleDescendant->GetFlattenedTreeParentNodeForStyle();
  } while (aPossibleDescendant);

  return false;
}

// static
nsresult
nsContentUtils::GetAncestors(nsINode* aNode,
                             nsTArray<nsINode*>& aArray)
{
  while (aNode) {
    aArray.AppendElement(aNode);
    aNode = aNode->GetParentNode();
  }
  return NS_OK;
}

// static
nsresult
nsContentUtils::GetAncestorsAndOffsets(nsIDOMNode* aNode,
                                       int32_t aOffset,
                                       nsTArray<nsIContent*>* aAncestorNodes,
                                       nsTArray<int32_t>* aAncestorOffsets)
{
  NS_ENSURE_ARG_POINTER(aNode);

  nsCOMPtr<nsIContent> content(do_QueryInterface(aNode));

  if (!content) {
    return NS_ERROR_FAILURE;
  }

  if (!aAncestorNodes->IsEmpty()) {
    NS_WARNING("aAncestorNodes is not empty");
    aAncestorNodes->Clear();
  }

  if (!aAncestorOffsets->IsEmpty()) {
    NS_WARNING("aAncestorOffsets is not empty");
    aAncestorOffsets->Clear();
  }

  // insert the node itself
  aAncestorNodes->AppendElement(content.get());
  aAncestorOffsets->AppendElement(aOffset);

  // insert all the ancestors
  nsIContent* child = content;
  nsIContent* parent = child->GetParent();
  while (parent) {
    aAncestorNodes->AppendElement(parent);
    aAncestorOffsets->AppendElement(parent->IndexOf(child));
    child = parent;
    parent = parent->GetParent();
  }

  return NS_OK;
}

// static
nsresult
nsContentUtils::GetCommonAncestor(nsIDOMNode *aNode,
                                  nsIDOMNode *aOther,
                                  nsIDOMNode** aCommonAncestor)
{
  *aCommonAncestor = nullptr;

  nsCOMPtr<nsINode> node1 = do_QueryInterface(aNode);
  nsCOMPtr<nsINode> node2 = do_QueryInterface(aOther);

  NS_ENSURE_TRUE(node1 && node2, NS_ERROR_UNEXPECTED);

  nsINode* common = GetCommonAncestor(node1, node2);
  NS_ENSURE_TRUE(common, NS_ERROR_NOT_AVAILABLE);

  return CallQueryInterface(common, aCommonAncestor);
}

template <typename Node, typename GetParentFunc>
static Node*
GetCommonAncestorInternal(Node* aNode1,
                          Node* aNode2,
                          GetParentFunc aGetParentFunc)
{
  MOZ_ASSERT(aNode1 != aNode2);

  // Build the chain of parents
  AutoTArray<Node*, 30> parents1, parents2;
  do {
    parents1.AppendElement(aNode1);
    aNode1 = aGetParentFunc(aNode1);
  } while (aNode1);
  do {
    parents2.AppendElement(aNode2);
    aNode2 = aGetParentFunc(aNode2);
  } while (aNode2);

  // Find where the parent chain differs
  uint32_t pos1 = parents1.Length();
  uint32_t pos2 = parents2.Length();
  Node* parent = nullptr;
  uint32_t len;
  for (len = std::min(pos1, pos2); len > 0; --len) {
    Node* child1 = parents1.ElementAt(--pos1);
    Node* child2 = parents2.ElementAt(--pos2);
    if (child1 != child2) {
      break;
    }
    parent = child1;
  }

  return parent;
}

/* static */
nsINode*
nsContentUtils::GetCommonAncestorHelper(nsINode* aNode1, nsINode* aNode2)
{
  return GetCommonAncestorInternal(aNode1, aNode2, [](nsINode* aNode) {
    return aNode->GetParentNode();
  });
}

/* static */
nsIContent*
nsContentUtils::GetCommonFlattenedTreeAncestorHelper(nsIContent* aContent1,
                                                     nsIContent* aContent2)
{
  return GetCommonAncestorInternal(aContent1, aContent2, [](nsIContent* aContent) {
    return aContent->GetFlattenedTreeParent();
  });
}

/* static */
Element*
nsContentUtils::GetCommonFlattenedTreeAncestorForStyle(Element* aElement1,
                                                       Element* aElement2)
{
  return GetCommonAncestorInternal(aElement1, aElement2, [](Element* aElement) {
    return aElement->GetFlattenedTreeParentElementForStyle();
  });
}

/* static */
bool
nsContentUtils::PositionIsBefore(nsINode* aNode1, nsINode* aNode2)
{
  return (aNode2->CompareDocumentPosition(*aNode1) &
    (nsIDOMNode::DOCUMENT_POSITION_PRECEDING |
     nsIDOMNode::DOCUMENT_POSITION_DISCONNECTED)) ==
    nsIDOMNode::DOCUMENT_POSITION_PRECEDING;
}

/* static */
int32_t
nsContentUtils::ComparePoints(nsINode* aParent1, int32_t aOffset1,
                              nsINode* aParent2, int32_t aOffset2,
                              bool* aDisconnected)
{
  if (aParent1 == aParent2) {
    // XXX This is odd.  aOffset1 and/or aOffset2 may be -1, e.g., it's result
    //     of nsINode::IndexOf(), but this compares such invalid offset with
    //     valid offset.
    return aOffset1 < aOffset2 ? -1 :
           aOffset1 > aOffset2 ? 1 :
           0;
  }

  AutoTArray<nsINode*, 32> parents1, parents2;
  nsINode* node1 = aParent1;
  nsINode* node2 = aParent2;
  do {
    parents1.AppendElement(node1);
    node1 = node1->GetParentNode();
  } while (node1);
  do {
    parents2.AppendElement(node2);
    node2 = node2->GetParentNode();
  } while (node2);

  uint32_t pos1 = parents1.Length() - 1;
  uint32_t pos2 = parents2.Length() - 1;

  bool disconnected = parents1.ElementAt(pos1) != parents2.ElementAt(pos2);
  if (aDisconnected) {
    *aDisconnected = disconnected;
  }
  if (disconnected) {
    NS_ASSERTION(aDisconnected, "unexpected disconnected nodes");
    return 1;
  }

  // Find where the parent chains differ
  nsINode* parent = parents1.ElementAt(pos1);
  uint32_t len;
  for (len = std::min(pos1, pos2); len > 0; --len) {
    nsINode* child1 = parents1.ElementAt(--pos1);
    nsINode* child2 = parents2.ElementAt(--pos2);
    if (child1 != child2) {
      return parent->IndexOf(child1) < parent->IndexOf(child2) ? -1 : 1;
    }
    parent = child1;
  }


  // The parent chains never differed, so one of the nodes is an ancestor of
  // the other

  NS_ASSERTION(!pos1 || !pos2,
               "should have run out of parent chain for one of the nodes");

  if (!pos1) {
    nsINode* child2 = parents2.ElementAt(--pos2);
    // XXX aOffset1 may be -1 as mentioned above.  So, why does this return
    //     it's *before* of the valid DOM point?
    return aOffset1 <= parent->IndexOf(child2) ? -1 : 1;
  }

  nsINode* child1 = parents1.ElementAt(--pos1);
  // XXX aOffset2 may be -1 as mentioned above.  So, why does this return it's
  //     *after* of the valid DOM point?
  return parent->IndexOf(child1) < aOffset2 ? -1 : 1;
}

/* static */
int32_t
nsContentUtils::ComparePoints(nsIDOMNode* aParent1, int32_t aOffset1,
                              nsIDOMNode* aParent2, int32_t aOffset2,
                              bool* aDisconnected)
{
  nsCOMPtr<nsINode> parent1 = do_QueryInterface(aParent1);
  nsCOMPtr<nsINode> parent2 = do_QueryInterface(aParent2);
  NS_ENSURE_TRUE(parent1 && parent2, -1);
  return ComparePoints(parent1, aOffset1, parent2, aOffset2);
}

/* static */
int32_t
nsContentUtils::ComparePoints(const RawRangeBoundary& aFirst,
                              const RawRangeBoundary& aSecond,
                              bool* aDisconnected)
{
  if (NS_WARN_IF(!aFirst.IsSet()) || NS_WARN_IF(!aSecond.IsSet())) {
    return -1;
  }
  return ComparePoints(aFirst.Container(), aFirst.Offset(),
                       aSecond.Container(), aSecond.Offset(),
                       aDisconnected);
}

inline bool
IsCharInSet(const char* aSet,
            const char16_t aChar)
{
  char16_t ch;
  while ((ch = *aSet)) {
    if (aChar == char16_t(ch)) {
      return true;
    }
    ++aSet;
  }
  return false;
}

/**
 * This method strips leading/trailing chars, in given set, from string.
 */

// static
const nsDependentSubstring
nsContentUtils::TrimCharsInSet(const char* aSet,
                               const nsAString& aValue)
{
  nsAString::const_iterator valueCurrent, valueEnd;

  aValue.BeginReading(valueCurrent);
  aValue.EndReading(valueEnd);

  // Skip characters in the beginning
  while (valueCurrent != valueEnd) {
    if (!IsCharInSet(aSet, *valueCurrent)) {
      break;
    }
    ++valueCurrent;
  }

  if (valueCurrent != valueEnd) {
    for (;;) {
      --valueEnd;
      if (!IsCharInSet(aSet, *valueEnd)) {
        break;
      }
    }
    ++valueEnd; // Step beyond the last character we want in the value.
  }

  // valueEnd should point to the char after the last to copy
  return Substring(valueCurrent, valueEnd);
}

/**
 * This method strips leading and trailing whitespace from a string.
 */

// static
template<bool IsWhitespace(char16_t)>
const nsDependentSubstring
nsContentUtils::TrimWhitespace(const nsAString& aStr, bool aTrimTrailing)
{
  nsAString::const_iterator start, end;

  aStr.BeginReading(start);
  aStr.EndReading(end);

  // Skip whitespace characters in the beginning
  while (start != end && IsWhitespace(*start)) {
    ++start;
  }

  if (aTrimTrailing) {
    // Skip whitespace characters in the end.
    while (end != start) {
      --end;

      if (!IsWhitespace(*end)) {
        // Step back to the last non-whitespace character.
        ++end;

        break;
      }
    }
  }

  // Return a substring for the string w/o leading and/or trailing
  // whitespace

  return Substring(start, end);
}

// Declaring the templates we are going to use avoid linking issues without
// inlining the method. Considering there is not so much spaces checking
// methods we can consider this to be better than inlining.
template
const nsDependentSubstring
nsContentUtils::TrimWhitespace<nsCRT::IsAsciiSpace>(const nsAString&, bool);
template
const nsDependentSubstring
nsContentUtils::TrimWhitespace<nsContentUtils::IsHTMLWhitespace>(const nsAString&, bool);
template
const nsDependentSubstring
nsContentUtils::TrimWhitespace<nsContentUtils::IsHTMLWhitespaceOrNBSP>(const nsAString&, bool);

static inline void KeyAppendSep(nsACString& aKey)
{
  if (!aKey.IsEmpty()) {
    aKey.Append('>');
  }
}

static inline void KeyAppendString(const nsAString& aString, nsACString& aKey)
{
  KeyAppendSep(aKey);

  // Could escape separator here if collisions happen.  > is not a legal char
  // for a name or type attribute, so we should be safe avoiding that extra work.

  AppendUTF16toUTF8(aString, aKey);
}

static inline void KeyAppendString(const nsACString& aString, nsACString& aKey)
{
  KeyAppendSep(aKey);

  // Could escape separator here if collisions happen.  > is not a legal char
  // for a name or type attribute, so we should be safe avoiding that extra work.

  aKey.Append(aString);
}

static inline void KeyAppendInt(int32_t aInt, nsACString& aKey)
{
  KeyAppendSep(aKey);

  aKey.Append(nsPrintfCString("%d", aInt));
}

static inline bool IsAutocompleteOff(const nsIContent* aElement)
{
  return aElement->AttrValueIs(kNameSpaceID_None, nsGkAtoms::autocomplete,
                               NS_LITERAL_STRING("off"), eIgnoreCase);
}

/*static*/ nsresult
nsContentUtils::GenerateStateKey(nsIContent* aContent,
                                 nsIDocument* aDocument,
                                 nsACString& aKey)
{
  aKey.Truncate();

  uint32_t partID = aDocument ? aDocument->GetPartID() : 0;

  // We must have content if we're not using a special state id
  NS_ENSURE_TRUE(aContent, NS_ERROR_FAILURE);

  // Don't capture state for anonymous content
  if (aContent->IsInAnonymousSubtree()) {
    return NS_OK;
  }

  if (IsAutocompleteOff(aContent)) {
    return NS_OK;
  }

  nsCOMPtr<nsIHTMLDocument> htmlDocument =
    do_QueryInterface(aContent->GetUncomposedDoc());

  KeyAppendInt(partID, aKey);  // first append a partID
  bool generatedUniqueKey = false;

  if (htmlDocument) {
    nsHTMLDocument* htmlDoc = static_cast<nsHTMLDocument*> (htmlDocument.get());
    RefPtr<nsContentList> htmlForms;
    RefPtr<nsContentList> htmlFormControls;
    htmlDoc->GetFormsAndFormControls(getter_AddRefs(htmlForms),
                                     getter_AddRefs(htmlFormControls));

    // If we have a form control and can calculate form information, use that
    // as the key - it is more reliable than just recording position in the
    // DOM.
    // XXXbz Is it, really?  We have bugs on this, I think...
    // Important to have a unique key, and tag/type/name may not be.
    //
    // If the control has a form, the format of the key is:
    // f>type>IndOfFormInDoc>IndOfControlInForm>FormName>name
    // else:
    // d>type>IndOfControlInDoc>name
    //
    // XXX We don't need to use index if name is there
    // XXXbz We don't?  Why not?  I don't follow.
    //
    nsCOMPtr<nsIFormControl> control(do_QueryInterface(aContent));
    if (control) {

      // Append the control type
      KeyAppendInt(control->ControlType(), aKey);

      // If in a form, add form name / index of form / index in form
      int32_t index = -1;
      Element *formElement = control->GetFormElement();
      if (formElement) {
        if (IsAutocompleteOff(formElement)) {
          aKey.Truncate();
          return NS_OK;
        }

        KeyAppendString(NS_LITERAL_CSTRING("f"), aKey);

        // Append the index of the form in the document
        index = htmlForms->IndexOf(formElement, false);
        if (index <= -1) {
          //
          // XXX HACK this uses some state that was dumped into the document
          // specifically to fix bug 138892.  What we are trying to do is *guess*
          // which form this control's state is found in, with the highly likely
          // guess that the highest form parsed so far is the one.
          // This code should not be on trunk, only branch.
          //
          index = htmlDocument->GetNumFormsSynchronous() - 1;
        }
        if (index > -1) {
          KeyAppendInt(index, aKey);

          // Append the index of the control in the form
          nsCOMPtr<nsIForm> form(do_QueryInterface(formElement));
          index = form->IndexOfControl(control);

          if (index > -1) {
            KeyAppendInt(index, aKey);
            generatedUniqueKey = true;
          }
        }

        // Append the form name
        nsAutoString formName;
        formElement->GetAttr(kNameSpaceID_None, nsGkAtoms::name, formName);
        KeyAppendString(formName, aKey);

      } else {

        KeyAppendString(NS_LITERAL_CSTRING("d"), aKey);

        // If not in a form, add index of control in document
        // Less desirable than indexing by form info.

        // Hash by index of control in doc (we are not in a form)
        // These are important as they are unique, and type/name may not be.

        // We have to flush sink notifications at this point to make
        // sure that htmlFormControls is up to date.
        index = htmlFormControls->IndexOf(aContent, true);
        if (index > -1) {
          KeyAppendInt(index, aKey);
          generatedUniqueKey = true;
        }
      }

      // Append the control name
      nsAutoString name;
      aContent->GetAttr(kNameSpaceID_None, nsGkAtoms::name, name);
      KeyAppendString(name, aKey);
    }
  }

  if (!generatedUniqueKey) {
    // Either we didn't have a form control or we aren't in an HTML document so
    // we can't figure out form info.  Append the tag name if it's an element
    // to avoid restoring state for one type of element on another type.
    if (aContent->IsElement()) {
      KeyAppendString(nsDependentAtomString(aContent->NodeInfo()->NameAtom()),
                      aKey);
    }
    else {
      // Append a character that is not "d" or "f" to disambiguate from
      // the case when we were a form control in an HTML document.
      KeyAppendString(NS_LITERAL_CSTRING("o"), aKey);
    }

    // Now start at aContent and append the indices of it and all its ancestors
    // in their containers.  That should at least pin down its position in the
    // DOM...
    nsINode* parent = aContent->GetParentNode();
    nsINode* content = aContent;
    while (parent) {
      KeyAppendInt(parent->IndexOf(content), aKey);
      content = parent;
      parent = content->GetParentNode();
    }
  }

  return NS_OK;
}

// static
nsIPrincipal*
nsContentUtils::SubjectPrincipal(JSContext* aCx)
{
  MOZ_ASSERT(NS_IsMainThread());

  // As opposed to SubjectPrincipal(), we do in fact assume that
  // we're in a compartment here; anyone who calls this function
  // in situations where that's not the case is doing it wrong.
  JSCompartment* compartment = js::GetContextCompartment(aCx);
  MOZ_ASSERT(compartment);

  JSPrincipals* principals = JS_GetCompartmentPrincipals(compartment);
  return nsJSPrincipals::get(principals);
}

// static
nsIPrincipal*
nsContentUtils::SubjectPrincipal()
{
  MOZ_ASSERT(IsInitialized());
  MOZ_ASSERT(NS_IsMainThread());
  JSContext* cx = GetCurrentJSContext();
  if (!cx) {
    MOZ_CRASH("Accessing the Subject Principal without an AutoJSAPI on the stack is forbidden");
  }

  JSCompartment *compartment = js::GetContextCompartment(cx);

  // When an AutoJSAPI is instantiated, we are in a null compartment until the
  // first JSAutoCompartment, which is kind of a purgatory as far as permissions
  // go. It would be nice to just hard-abort if somebody does a security check
  // in this purgatory zone, but that would be too fragile, since it could be
  // triggered by random IsCallerChrome() checks 20-levels deep.
  //
  // So we want to return _something_ here - and definitely not the System
  // Principal, since that would make an AutoJSAPI a very dangerous thing to
  // instantiate.
  //
  // The natural thing to return is a null principal. Ideally, we'd return a
  // different null principal each time, to avoid any unexpected interactions
  // when the principal accidentally gets inherited somewhere. But
  // SubjectPrincipal doesn't return strong references, so there's no way to
  // sanely manage the lifetime of multiple null principals.
  //
  // So we use a singleton null principal. To avoid it being accidentally
  // inherited and becoming a "real" subject or object principal, we do a
  // release-mode assert during compartment creation against using this
  // principal on an actual global.
  if (!compartment) {
    return sNullSubjectPrincipal;
  }

  return SubjectPrincipal(cx);
}

// static
nsIPrincipal*
nsContentUtils::ObjectPrincipal(JSObject* aObj)
{
  MOZ_ASSERT(NS_IsMainThread());

#ifdef DEBUG
  JS::AssertObjectBelongsToCurrentThread(aObj);
#endif

  // This is duplicated from nsScriptSecurityManager. We don't call through there
  // because the API unnecessarily requires a JSContext for historical reasons.
  JSCompartment *compartment = js::GetObjectCompartment(aObj);
  JSPrincipals *principals = JS_GetCompartmentPrincipals(compartment);
  return nsJSPrincipals::get(principals);
}

// static
nsresult
nsContentUtils::NewURIWithDocumentCharset(nsIURI** aResult,
                                          const nsAString& aSpec,
                                          nsIDocument* aDocument,
                                          nsIURI* aBaseURI)
{
  if (aDocument) {
    return NS_NewURI(aResult, aSpec,
                     aDocument->GetDocumentCharacterSet(),
                     aBaseURI, sIOService);
  }
  return NS_NewURI(aResult, aSpec, nullptr, aBaseURI, sIOService);
}

// static
bool
nsContentUtils::IsCustomElementName(nsAtom* aName)
{
  // A valid custom element name is a sequence of characters name which
  // must match the PotentialCustomElementName production:
  // PotentialCustomElementName ::= [a-z] (PCENChar)* '-' (PCENChar)*
  const char16_t* name = aName->GetUTF16String();
  uint32_t len = aName->GetLength();
  bool hasDash = false;

  if (!len || name[0] < 'a' || name[0] > 'z') {
    return false;
  }

  uint32_t i = 1;
  while (i < len) {
    if (NS_IS_HIGH_SURROGATE(name[i]) && i + 1 < len &&
        NS_IS_LOW_SURROGATE(name[i + 1])) {
      // Merged two 16-bit surrogate pairs into code point.
      char32_t code = SURROGATE_TO_UCS4(name[i], name[i + 1]);

      if (code < 0x10000 || code > 0xEFFFF) {
        return false;
      }

      i += 2;
    } else {
      if (name[i] == '-') {
        hasDash = true;
      }

      if (name[i] != '-' && name[i] != '.' &&
          name[i] != '_' && name[i] != 0xB7 &&
         (name[i] < '0' || name[i] > '9') &&
         (name[i] < 'a' || name[i] > 'z') &&
         (name[i] < 0xC0 || name[i] > 0xD6) &&
         (name[i] < 0xF8 || name[i] > 0x37D) &&
         (name[i] < 0x37F || name[i] > 0x1FFF) &&
         (name[i] < 0x200C || name[i] > 0x200D) &&
         (name[i] < 0x203F || name[i] > 0x2040) &&
         (name[i] < 0x2070 || name[i] > 0x218F) &&
         (name[i] < 0x2C00 || name[i] > 0x2FEF) &&
         (name[i] < 0x3001 || name[i] > 0xD7FF) &&
         (name[i] < 0xF900 || name[i] > 0xFDCF) &&
         (name[i] < 0xFDF0 || name[i] > 0xFFFD)) {
        return false;
      }

      i++;
    }
  }

  if (!hasDash) {
    return false;
  }

  // The custom element name must not be one of the following values:
  //  annotation-xml
  //  color-profile
  //  font-face
  //  font-face-src
  //  font-face-uri
  //  font-face-format
  //  font-face-name
  //  missing-glyph
  return aName != nsGkAtoms::annotation_xml_ &&
         aName != nsGkAtoms::colorProfile &&
         aName != nsGkAtoms::font_face &&
         aName != nsGkAtoms::font_face_src &&
         aName != nsGkAtoms::font_face_uri &&
         aName != nsGkAtoms::font_face_format &&
         aName != nsGkAtoms::font_face_name &&
         aName != nsGkAtoms::missingGlyph;
}

// static
nsresult
nsContentUtils::CheckQName(const nsAString& aQualifiedName,
                           bool aNamespaceAware,
                           const char16_t** aColon)
{
  const char* colon = nullptr;
  const char16_t* begin = aQualifiedName.BeginReading();
  const char16_t* end = aQualifiedName.EndReading();

  int result = MOZ_XMLCheckQName(reinterpret_cast<const char*>(begin),
                                 reinterpret_cast<const char*>(end),
                                 aNamespaceAware, &colon);

  if (!result) {
    if (aColon) {
      *aColon = reinterpret_cast<const char16_t*>(colon);
    }

    return NS_OK;
  }

  return NS_ERROR_DOM_INVALID_CHARACTER_ERR;
}

//static
nsresult
nsContentUtils::SplitQName(const nsIContent* aNamespaceResolver,
                           const nsString& aQName,
                           int32_t *aNamespace, nsAtom **aLocalName)
{
  const char16_t* colon;
  nsresult rv = nsContentUtils::CheckQName(aQName, true, &colon);
  NS_ENSURE_SUCCESS(rv, rv);

  if (colon) {
    const char16_t* end;
    aQName.EndReading(end);
    nsAutoString nameSpace;
    rv = aNamespaceResolver->LookupNamespaceURIInternal(Substring(aQName.get(),
                                                                  colon),
                                                        nameSpace);
    NS_ENSURE_SUCCESS(rv, rv);

    *aNamespace = NameSpaceManager()->GetNameSpaceID(nameSpace,
                                                     nsContentUtils::IsChromeDoc(aNamespaceResolver->OwnerDoc()));
    if (*aNamespace == kNameSpaceID_Unknown)
      return NS_ERROR_FAILURE;

    *aLocalName = NS_AtomizeMainThread(Substring(colon + 1, end)).take();
  }
  else {
    *aNamespace = kNameSpaceID_None;
    *aLocalName = NS_AtomizeMainThread(aQName).take();
  }
  NS_ENSURE_TRUE(aLocalName, NS_ERROR_OUT_OF_MEMORY);
  return NS_OK;
}

// static
nsresult
nsContentUtils::GetNodeInfoFromQName(const nsAString& aNamespaceURI,
                                     const nsAString& aQualifiedName,
                                     nsNodeInfoManager* aNodeInfoManager,
                                     uint16_t aNodeType,
                                     mozilla::dom::NodeInfo** aNodeInfo)
{
  const nsString& qName = PromiseFlatString(aQualifiedName);
  const char16_t* colon;
  nsresult rv = nsContentUtils::CheckQName(qName, true, &colon);
  NS_ENSURE_SUCCESS(rv, rv);

  int32_t nsID;
  sNameSpaceManager->RegisterNameSpace(aNamespaceURI, nsID);
  if (colon) {
    const char16_t* end;
    qName.EndReading(end);

    RefPtr<nsAtom> prefix =
      NS_AtomizeMainThread(Substring(qName.get(), colon));

    rv = aNodeInfoManager->GetNodeInfo(Substring(colon + 1, end), prefix,
                                       nsID, aNodeType, aNodeInfo);
  }
  else {
    rv = aNodeInfoManager->GetNodeInfo(aQualifiedName, nullptr, nsID,
                                       aNodeType, aNodeInfo);
  }
  NS_ENSURE_SUCCESS(rv, rv);

  return nsContentUtils::IsValidNodeName((*aNodeInfo)->NameAtom(),
                                         (*aNodeInfo)->GetPrefixAtom(),
                                         (*aNodeInfo)->NamespaceID()) ?
         NS_OK : NS_ERROR_DOM_NAMESPACE_ERR;
}

// static
void
nsContentUtils::SplitExpatName(const char16_t *aExpatName, nsAtom **aPrefix,
                               nsAtom **aLocalName, int32_t* aNameSpaceID)
{
  /**
   *  Expat can send the following:
   *    localName
   *    namespaceURI<separator>localName
   *    namespaceURI<separator>localName<separator>prefix
   *
   *  and we use 0xFFFF for the <separator>.
   *
   */

  const char16_t *uriEnd = nullptr;
  const char16_t *nameEnd = nullptr;
  const char16_t *pos;
  for (pos = aExpatName; *pos; ++pos) {
    if (*pos == 0xFFFF) {
      if (uriEnd) {
        nameEnd = pos;
      }
      else {
        uriEnd = pos;
      }
    }
  }

  const char16_t *nameStart;
  if (uriEnd) {
    if (sNameSpaceManager) {
      sNameSpaceManager->RegisterNameSpace(nsDependentSubstring(aExpatName,
                                                                uriEnd),
                                           *aNameSpaceID);
    }
    else {
      *aNameSpaceID = kNameSpaceID_Unknown;
    }

    nameStart = (uriEnd + 1);
    if (nameEnd)  {
      const char16_t *prefixStart = nameEnd + 1;
      *aPrefix = NS_AtomizeMainThread(Substring(prefixStart, pos)).take();
    }
    else {
      nameEnd = pos;
      *aPrefix = nullptr;
    }
  }
  else {
    *aNameSpaceID = kNameSpaceID_None;
    nameStart = aExpatName;
    nameEnd = pos;
    *aPrefix = nullptr;
  }
  *aLocalName = NS_AtomizeMainThread(Substring(nameStart, nameEnd)).take();
}

// static
nsPresContext*
nsContentUtils::GetContextForContent(const nsIContent* aContent)
{
  nsIDocument* doc = aContent->GetComposedDoc();
  if (doc) {
    nsIPresShell *presShell = doc->GetShell();
    if (presShell) {
      return presShell->GetPresContext();
    }
  }
  return nullptr;
}

// static
bool
nsContentUtils::CanLoadImage(nsIURI* aURI, nsISupports* aContext,
                             nsIDocument* aLoadingDocument,
                             nsIPrincipal* aLoadingPrincipal,
                             int16_t* aImageBlockingStatus,
                             uint32_t aContentType)
{
  NS_PRECONDITION(aURI, "Must have a URI");
  NS_PRECONDITION(aLoadingDocument, "Must have a document");
  NS_PRECONDITION(aLoadingPrincipal, "Must have a loading principal");

  nsresult rv;

  uint32_t appType = nsIDocShell::APP_TYPE_UNKNOWN;

  {
    nsCOMPtr<nsIDocShellTreeItem> docShellTreeItem = aLoadingDocument->GetDocShell();
    if (docShellTreeItem) {
      nsCOMPtr<nsIDocShellTreeItem> root;
      docShellTreeItem->GetRootTreeItem(getter_AddRefs(root));

      nsCOMPtr<nsIDocShell> docShell(do_QueryInterface(root));

      if (!docShell || NS_FAILED(docShell->GetAppType(&appType))) {
        appType = nsIDocShell::APP_TYPE_UNKNOWN;
      }
    }
  }

  if (appType != nsIDocShell::APP_TYPE_EDITOR) {
    // Editor apps get special treatment here, editors can load images
    // from anywhere.  This allows editor to insert images from file://
    // into documents that are being edited.
    rv = sSecurityManager->
      CheckLoadURIWithPrincipal(aLoadingPrincipal, aURI,
                                nsIScriptSecurityManager::ALLOW_CHROME);
    if (NS_FAILED(rv)) {
      if (aImageBlockingStatus) {
        // Reject the request itself, not all requests to the relevant
        // server...
        *aImageBlockingStatus = nsIContentPolicy::REJECT_REQUEST;
      }
      return false;
    }
  }

  int16_t decision = nsIContentPolicy::ACCEPT;

  rv = NS_CheckContentLoadPolicy(aContentType,
                                 aURI,
                                 aLoadingPrincipal,
                                 aLoadingPrincipal, // triggering principal
                                 aContext,
                                 EmptyCString(), //mime guess
                                 nullptr,         //extra
                                 &decision,
                                 GetContentPolicy());

  if (aImageBlockingStatus) {
    *aImageBlockingStatus =
      NS_FAILED(rv) ? nsIContentPolicy::REJECT_REQUEST : decision;
  }
  return NS_FAILED(rv) ? false : NS_CP_ACCEPTED(decision);
}

// static
mozilla::OriginAttributes
nsContentUtils::GetOriginAttributes(nsIDocument* aDocument)
{
  if (!aDocument) {
    return mozilla::OriginAttributes();
  }

  nsCOMPtr<nsILoadGroup> loadGroup = aDocument->GetDocumentLoadGroup();
  if (loadGroup) {
    return GetOriginAttributes(loadGroup);
  }

  mozilla::OriginAttributes attrs;
  nsCOMPtr<nsIChannel> channel = aDocument->GetChannel();
  if (channel) {
    NS_GetOriginAttributes(channel, attrs);
  }
  return attrs;
}

// static
mozilla::OriginAttributes
nsContentUtils::GetOriginAttributes(nsILoadGroup* aLoadGroup)
{
  if (!aLoadGroup) {
    return mozilla::OriginAttributes();
  }
  mozilla::OriginAttributes attrs;
  nsCOMPtr<nsIInterfaceRequestor> callbacks;
  aLoadGroup->GetNotificationCallbacks(getter_AddRefs(callbacks));
  if (callbacks) {
    nsCOMPtr<nsILoadContext> loadContext = do_GetInterface(callbacks);
    if (loadContext) {
      loadContext->GetOriginAttributes(attrs);
    }
  }
  return attrs;
}

// static
bool
nsContentUtils::IsInPrivateBrowsing(nsIDocument* aDoc)
{
  if (!aDoc) {
    return false;
  }

  nsCOMPtr<nsILoadGroup> loadGroup = aDoc->GetDocumentLoadGroup();
  if (loadGroup) {
    nsCOMPtr<nsIInterfaceRequestor> callbacks;
    loadGroup->GetNotificationCallbacks(getter_AddRefs(callbacks));
    if (callbacks) {
      nsCOMPtr<nsILoadContext> loadContext = do_GetInterface(callbacks);
      if (loadContext) {
        return loadContext->UsePrivateBrowsing();
      }
    }
  }

  nsCOMPtr<nsIChannel> channel = aDoc->GetChannel();
  return channel && NS_UsePrivateBrowsing(channel);
}

// static
bool
nsContentUtils::IsInPrivateBrowsing(nsILoadGroup* aLoadGroup)
{
  if (!aLoadGroup) {
    return false;
  }
  bool isPrivate = false;
  nsCOMPtr<nsIInterfaceRequestor> callbacks;
  aLoadGroup->GetNotificationCallbacks(getter_AddRefs(callbacks));
  if (callbacks) {
    nsCOMPtr<nsILoadContext> loadContext = do_GetInterface(callbacks);
    isPrivate = loadContext && loadContext->UsePrivateBrowsing();
  }
  return isPrivate;
}

bool
nsContentUtils::DocumentInactiveForImageLoads(nsIDocument* aDocument)
{
  if (aDocument && !IsChromeDoc(aDocument) && !aDocument->IsResourceDoc()) {
    nsCOMPtr<nsPIDOMWindowInner> win =
      do_QueryInterface(aDocument->GetScopeObject());
    return !win || !win->GetDocShell();
  }
  return false;
}

imgLoader*
nsContentUtils::GetImgLoaderForDocument(nsIDocument* aDoc)
{
  NS_ENSURE_TRUE(!DocumentInactiveForImageLoads(aDoc), nullptr);

  if (!aDoc) {
    return imgLoader::NormalLoader();
  }
  bool isPrivate = IsInPrivateBrowsing(aDoc);
  return isPrivate ? imgLoader::PrivateBrowsingLoader()
                   : imgLoader::NormalLoader();
}

// static
imgLoader*
nsContentUtils::GetImgLoaderForChannel(nsIChannel* aChannel,
                                       nsIDocument* aContext)
{
  NS_ENSURE_TRUE(!DocumentInactiveForImageLoads(aContext), nullptr);

  if (!aChannel) {
    return imgLoader::NormalLoader();
  }
  nsCOMPtr<nsILoadContext> context;
  NS_QueryNotificationCallbacks(aChannel, context);
  return context && context->UsePrivateBrowsing() ?
                      imgLoader::PrivateBrowsingLoader() :
                      imgLoader::NormalLoader();
}

// static
bool
nsContentUtils::IsImageInCache(nsIURI* aURI, nsIDocument* aDocument)
{
    imgILoader* loader = GetImgLoaderForDocument(aDocument);
    nsCOMPtr<imgICache> cache = do_QueryInterface(loader);

    // If something unexpected happened we return false, otherwise if props
    // is set, the image is cached and we return true
    nsCOMPtr<nsIProperties> props;
    nsCOMPtr<nsIDOMDocument> domDoc = do_QueryInterface(aDocument);
    nsresult rv = cache->FindEntryProperties(aURI, domDoc, getter_AddRefs(props));
    return (NS_SUCCEEDED(rv) && props);
}

// static
nsresult
nsContentUtils::LoadImage(nsIURI* aURI, nsINode* aContext,
                          nsIDocument* aLoadingDocument,
                          nsIPrincipal* aLoadingPrincipal,
                          uint64_t aRequestContextID,
                          nsIURI* aReferrer,
                          net::ReferrerPolicy aReferrerPolicy,
                          imgINotificationObserver* aObserver, int32_t aLoadFlags,
                          const nsAString& initiatorType,
                          imgRequestProxy** aRequest,
                          uint32_t aContentPolicyType,
                          bool aUseUrgentStartForChannel)
{
  NS_PRECONDITION(aURI, "Must have a URI");
  NS_PRECONDITION(aContext, "Must have a context");
  NS_PRECONDITION(aLoadingDocument, "Must have a document");
  NS_PRECONDITION(aLoadingPrincipal, "Must have a principal");
  NS_PRECONDITION(aRequest, "Null out param");

  imgLoader* imgLoader = GetImgLoaderForDocument(aLoadingDocument);
  if (!imgLoader) {
    // nothing we can do here
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsILoadGroup> loadGroup = aLoadingDocument->GetDocumentLoadGroup();

  nsIURI *documentURI = aLoadingDocument->GetDocumentURI();

  NS_ASSERTION(loadGroup || IsFontTableURI(documentURI),
               "Could not get loadgroup; onload may fire too early");

  // Make the URI immutable so people won't change it under us
  NS_TryToSetImmutable(aURI);

  // XXXbz using "documentURI" for the initialDocumentURI is not quite
  // right, but the best we can do here...
  return imgLoader->LoadImage(aURI,                 /* uri to load */
                              documentURI,          /* initialDocumentURI */
                              aReferrer,            /* referrer */
                              aReferrerPolicy,      /* referrer policy */
                              aLoadingPrincipal,    /* loading principal */
                              aRequestContextID,    /* request context ID */
                              loadGroup,            /* loadgroup */
                              aObserver,            /* imgINotificationObserver */
                              aContext,             /* loading context */
                              aLoadingDocument,     /* uniquification key */
                              aLoadFlags,           /* load flags */
                              nullptr,              /* cache key */
                              aContentPolicyType,   /* content policy type */
                              initiatorType,        /* the load initiator */
                              aUseUrgentStartForChannel, /* urgent-start flag */
                              aRequest);
}

// static
already_AddRefed<imgIContainer>
nsContentUtils::GetImageFromContent(nsIImageLoadingContent* aContent,
                                    imgIRequest **aRequest)
{
  if (aRequest) {
    *aRequest = nullptr;
  }

  NS_ENSURE_TRUE(aContent, nullptr);

  nsCOMPtr<imgIRequest> imgRequest;
  aContent->GetRequest(nsIImageLoadingContent::CURRENT_REQUEST,
                      getter_AddRefs(imgRequest));
  if (!imgRequest) {
    return nullptr;
  }

  nsCOMPtr<imgIContainer> imgContainer;
  imgRequest->GetImage(getter_AddRefs(imgContainer));

  if (!imgContainer) {
    return nullptr;
  }

  if (aRequest) {
    imgRequest.swap(*aRequest);
  }

  return imgContainer.forget();
}

//static
already_AddRefed<imgRequestProxy>
nsContentUtils::GetStaticRequest(nsIDocument* aLoadingDocument,
                                 imgRequestProxy* aRequest)
{
  NS_ENSURE_TRUE(aRequest, nullptr);
  RefPtr<imgRequestProxy> retval;
  aRequest->GetStaticRequest(aLoadingDocument, getter_AddRefs(retval));
  return retval.forget();
}

// static
bool
nsContentUtils::ContentIsDraggable(nsIContent* aContent)
{
  nsCOMPtr<nsIDOMHTMLElement> htmlElement = do_QueryInterface(aContent);
  if (htmlElement) {
    bool draggable = false;
    htmlElement->GetDraggable(&draggable);
    if (draggable)
      return true;

    if (aContent->AttrValueIs(kNameSpaceID_None, nsGkAtoms::draggable,
                              nsGkAtoms::_false, eIgnoreCase))
      return false;
  }

  // special handling for content area image and link dragging
  return IsDraggableImage(aContent) || IsDraggableLink(aContent);
}

// static
bool
nsContentUtils::IsDraggableImage(nsIContent* aContent)
{
  NS_PRECONDITION(aContent, "Must have content node to test");

  nsCOMPtr<nsIImageLoadingContent> imageContent(do_QueryInterface(aContent));
  if (!imageContent) {
    return false;
  }

  nsCOMPtr<imgIRequest> imgRequest;
  imageContent->GetRequest(nsIImageLoadingContent::CURRENT_REQUEST,
                           getter_AddRefs(imgRequest));

  // XXXbz It may be draggable even if the request resulted in an error.  Why?
  // Not sure; that's what the old nsContentAreaDragDrop/nsFrame code did.
  return imgRequest != nullptr;
}

// static
bool
nsContentUtils::IsDraggableLink(const nsIContent* aContent) {
  nsCOMPtr<nsIURI> absURI;
  return aContent->IsLink(getter_AddRefs(absURI));
}

// static
nsresult
nsContentUtils::QNameChanged(mozilla::dom::NodeInfo* aNodeInfo, nsAtom* aName,
                             mozilla::dom::NodeInfo** aResult)
{
  nsNodeInfoManager *niMgr = aNodeInfo->NodeInfoManager();

  *aResult = niMgr->GetNodeInfo(aName, nullptr,
                                aNodeInfo->NamespaceID(),
                                aNodeInfo->NodeType(),
                                aNodeInfo->GetExtraName()).take();
  return NS_OK;
}


static bool
TestSitePerm(nsIPrincipal* aPrincipal, const char* aType, uint32_t aPerm, bool aExactHostMatch)
{
  if (!aPrincipal) {
    // We always deny (i.e. don't allow) the permission if we don't have a
    // principal.
    return aPerm != nsIPermissionManager::ALLOW_ACTION;
  }

  nsCOMPtr<nsIPermissionManager> permMgr = services::GetPermissionManager();
  NS_ENSURE_TRUE(permMgr, false);

  uint32_t perm;
  nsresult rv;
  if (aExactHostMatch) {
    rv = permMgr->TestExactPermissionFromPrincipal(aPrincipal, aType, &perm);
  } else {
    rv = permMgr->TestPermissionFromPrincipal(aPrincipal, aType, &perm);
  }
  NS_ENSURE_SUCCESS(rv, false);

  return perm == aPerm;
}

bool
nsContentUtils::IsSitePermAllow(nsIPrincipal* aPrincipal, const char* aType)
{
  return TestSitePerm(aPrincipal, aType, nsIPermissionManager::ALLOW_ACTION, false);
}

bool
nsContentUtils::IsSitePermDeny(nsIPrincipal* aPrincipal, const char* aType)
{
  return TestSitePerm(aPrincipal, aType, nsIPermissionManager::DENY_ACTION, false);
}

bool
nsContentUtils::IsExactSitePermAllow(nsIPrincipal* aPrincipal, const char* aType)
{
  return TestSitePerm(aPrincipal, aType, nsIPermissionManager::ALLOW_ACTION, true);
}

bool
nsContentUtils::IsExactSitePermDeny(nsIPrincipal* aPrincipal, const char* aType)
{
  return TestSitePerm(aPrincipal, aType, nsIPermissionManager::DENY_ACTION, true);
}

static const char *gEventNames[] = {"event"};
static const char *gSVGEventNames[] = {"evt"};
// for b/w compat, the first name to onerror is still 'event', even though it
// is actually the error message
static const char *gOnErrorNames[] = {"event", "source", "lineno",
                                      "colno", "error"};

// static
void
nsContentUtils::GetEventArgNames(int32_t aNameSpaceID,
                                 nsAtom *aEventName,
                                 bool aIsForWindow,
                                 uint32_t *aArgCount,
                                 const char*** aArgArray)
{
#define SET_EVENT_ARG_NAMES(names) \
    *aArgCount = sizeof(names)/sizeof(names[0]); \
    *aArgArray = names;

  // JSEventHandler is what does the arg magic for onerror, and it does
  // not seem to take the namespace into account.  So we let onerror in all
  // namespaces get the 3 arg names.
  if (aEventName == nsGkAtoms::onerror && aIsForWindow) {
    SET_EVENT_ARG_NAMES(gOnErrorNames);
  } else if (aNameSpaceID == kNameSpaceID_SVG) {
    SET_EVENT_ARG_NAMES(gSVGEventNames);
  } else {
    SET_EVENT_ARG_NAMES(gEventNames);
  }
}

static const char gPropertiesFiles[nsContentUtils::PropertiesFile_COUNT][56] = {
  // Must line up with the enum values in |PropertiesFile| enum.
  "chrome://global/locale/css.properties",
  "chrome://global/locale/xbl.properties",
  "chrome://global/locale/xul.properties",
  "chrome://global/locale/layout_errors.properties",
  "chrome://global/locale/layout/HtmlForm.properties",
  "chrome://global/locale/printing.properties",
  "chrome://global/locale/dom/dom.properties",
  "chrome://global/locale/layout/htmlparser.properties",
  "chrome://global/locale/svg/svg.properties",
  "chrome://branding/locale/brand.properties",
  "chrome://global/locale/commonDialogs.properties",
  "chrome://global/locale/mathml/mathml.properties",
  "chrome://global/locale/security/security.properties",
  "chrome://necko/locale/necko.properties"
};

/* static */ nsresult
nsContentUtils::EnsureStringBundle(PropertiesFile aFile)
{
  if (!sStringBundles[aFile]) {
    if (!sStringBundleService) {
      nsresult rv =
        CallGetService(NS_STRINGBUNDLE_CONTRACTID, &sStringBundleService);
      NS_ENSURE_SUCCESS(rv, rv);
    }
    nsIStringBundle *bundle;
    nsresult rv =
      sStringBundleService->CreateBundle(gPropertiesFiles[aFile], &bundle);
    NS_ENSURE_SUCCESS(rv, rv);
    sStringBundles[aFile] = bundle; // transfer ownership
  }
  return NS_OK;
}

/* static */
void
nsContentUtils::AsyncPrecreateStringBundles()
{
  for (uint32_t bundleIndex = 0; bundleIndex < PropertiesFile_COUNT; ++bundleIndex) {
    nsresult rv = NS_IdleDispatchToCurrentThread(
      NS_NewRunnableFunction("AsyncPrecreateStringBundles",
                             [bundleIndex]() {
                               PropertiesFile file = static_cast<PropertiesFile>(bundleIndex);
                               EnsureStringBundle(file);
                               nsIStringBundle *bundle = sStringBundles[file];
                               bundle->AsyncPreload();
                             }));
    Unused << NS_WARN_IF(NS_FAILED(rv));
  }
}

/* static */
nsresult nsContentUtils::GetLocalizedString(PropertiesFile aFile,
                                            const char* aKey,
                                            nsAString& aResult)
{
  nsresult rv = EnsureStringBundle(aFile);
  NS_ENSURE_SUCCESS(rv, rv);
  nsIStringBundle *bundle = sStringBundles[aFile];
  return bundle->GetStringFromName(aKey, aResult);
}

/* static */
nsresult nsContentUtils::FormatLocalizedString(PropertiesFile aFile,
                                               const char* aKey,
                                               const char16_t **aParams,
                                               uint32_t aParamsLength,
                                               nsAString& aResult)
{
  nsresult rv = EnsureStringBundle(aFile);
  NS_ENSURE_SUCCESS(rv, rv);
  nsIStringBundle *bundle = sStringBundles[aFile];

  if (!aParams || !aParamsLength) {
    return bundle->GetStringFromName(aKey, aResult);
  }

  return bundle->FormatStringFromName(aKey, aParams, aParamsLength, aResult);
}

/* static */
nsresult nsContentUtils::FormatLocalizedString(
                                          PropertiesFile aFile,
                                          const char* aKey,
                                          const nsTArray<nsString>& aParamArray,
                                          nsAString& aResult)
{
  MOZ_ASSERT(NS_IsMainThread());

  UniquePtr<const char16_t*[]> params;
  uint32_t paramsLength = aParamArray.Length();
  if (paramsLength > 0) {
    params = MakeUnique<const char16_t*[]>(paramsLength);
    for (uint32_t i = 0; i < paramsLength; ++i) {
      params[i] = aParamArray[i].get();
    }
  }
  return FormatLocalizedString(aFile, aKey, params.get(), paramsLength,
                               aResult);
}


/* static */ void
nsContentUtils::LogSimpleConsoleError(const nsAString& aErrorText,
                                      const char * classification)
{
  nsCOMPtr<nsIScriptError> scriptError =
    do_CreateInstance(NS_SCRIPTERROR_CONTRACTID);
  if (scriptError) {
    nsCOMPtr<nsIConsoleService> console =
      do_GetService(NS_CONSOLESERVICE_CONTRACTID);
    if (console && NS_SUCCEEDED(scriptError->Init(aErrorText, EmptyString(),
                                                  EmptyString(), 0, 0,
                                                  nsIScriptError::errorFlag,
                                                  classification))) {
      console->LogMessage(scriptError);
    }
  }
}

/* static */ nsresult
nsContentUtils::ReportToConsole(uint32_t aErrorFlags,
                                const nsACString& aCategory,
                                const nsIDocument* aDocument,
                                PropertiesFile aFile,
                                const char *aMessageName,
                                const char16_t **aParams,
                                uint32_t aParamsLength,
                                nsIURI* aURI,
                                const nsString& aSourceLine,
                                uint32_t aLineNumber,
                                uint32_t aColumnNumber)
{
  NS_ASSERTION((aParams && aParamsLength) || (!aParams && !aParamsLength),
               "Supply either both parameters and their number or no"
               "parameters and 0.");

  nsresult rv;
  nsAutoString errorText;
  if (aParams) {
    rv = FormatLocalizedString(aFile, aMessageName, aParams, aParamsLength,
                               errorText);
  }
  else {
    rv = GetLocalizedString(aFile, aMessageName, errorText);
  }
  NS_ENSURE_SUCCESS(rv, rv);

  return ReportToConsoleNonLocalized(errorText, aErrorFlags, aCategory,
                                     aDocument, aURI, aSourceLine,
                                     aLineNumber, aColumnNumber);
}


/* static */ nsresult
nsContentUtils::ReportToConsoleNonLocalized(const nsAString& aErrorText,
                                            uint32_t aErrorFlags,
                                            const nsACString& aCategory,
                                            const nsIDocument* aDocument,
                                            nsIURI* aURI,
                                            const nsString& aSourceLine,
                                            uint32_t aLineNumber,
                                            uint32_t aColumnNumber,
                                            MissingErrorLocationMode aLocationMode)
{
  uint64_t innerWindowID = 0;
  if (aDocument) {
    if (!aURI) {
      aURI = aDocument->GetDocumentURI();
    }
    innerWindowID = aDocument->InnerWindowID();
  }

  return ReportToConsoleByWindowID(aErrorText, aErrorFlags, aCategory,
                                   innerWindowID, aURI, aSourceLine,
                                   aLineNumber, aColumnNumber, aLocationMode);
}

/* static */ nsresult
nsContentUtils::ReportToConsoleByWindowID(const nsAString& aErrorText,
                                          uint32_t aErrorFlags,
                                          const nsACString& aCategory,
                                          uint64_t aInnerWindowID,
                                          nsIURI* aURI,
                                          const nsString& aSourceLine,
                                          uint32_t aLineNumber,
                                          uint32_t aColumnNumber,
                                          MissingErrorLocationMode aLocationMode)
{
  nsresult rv;
  if (!sConsoleService) { // only need to bother null-checking here
    rv = CallGetService(NS_CONSOLESERVICE_CONTRACTID, &sConsoleService);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  nsAutoCString spec;
  if (!aLineNumber && aLocationMode == eUSE_CALLING_LOCATION) {
    JSContext *cx = GetCurrentJSContext();
    if (cx) {
      nsJSUtils::GetCallingLocation(cx, spec, &aLineNumber, &aColumnNumber);
    }
  }

  nsCOMPtr<nsIScriptError> errorObject =
      do_CreateInstance(NS_SCRIPTERROR_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  if (!spec.IsEmpty()) {
    rv = errorObject->InitWithWindowID(aErrorText,
                                       NS_ConvertUTF8toUTF16(spec), // file name
                                       aSourceLine,
                                       aLineNumber, aColumnNumber,
                                       aErrorFlags, aCategory,
                                       aInnerWindowID);
  } else {
    rv = errorObject->InitWithSourceURI(aErrorText,
                                        aURI,
                                        aSourceLine,
                                        aLineNumber, aColumnNumber,
                                        aErrorFlags, aCategory,
                                        aInnerWindowID);
  }
  NS_ENSURE_SUCCESS(rv, rv);

  return sConsoleService->LogMessage(errorObject);
}

void
nsContentUtils::LogMessageToConsole(const char* aMsg)
{
  if (!sConsoleService) { // only need to bother null-checking here
    CallGetService(NS_CONSOLESERVICE_CONTRACTID, &sConsoleService);
    if (!sConsoleService) {
      return;
    }
  }
  sConsoleService->LogStringMessage(NS_ConvertUTF8toUTF16(aMsg).get());
}

bool
nsContentUtils::IsChromeDoc(nsIDocument *aDocument)
{
  if (!aDocument) {
    return false;
  }
  return aDocument->NodePrincipal() == sSystemPrincipal;
}

bool
nsContentUtils::IsChildOfSameType(nsIDocument* aDoc)
{
  nsCOMPtr<nsIDocShellTreeItem> docShellAsItem(aDoc->GetDocShell());
  nsCOMPtr<nsIDocShellTreeItem> sameTypeParent;
  if (docShellAsItem) {
    docShellAsItem->GetSameTypeParent(getter_AddRefs(sameTypeParent));
  }
  return sameTypeParent != nullptr;
}

bool
nsContentUtils::IsScriptType(const nsACString& aContentType)
{
  // NOTE: if you add a type here, add it to the CONTENTDLF_CATEGORIES
  // define in nsContentDLF.h as well.
  return aContentType.EqualsLiteral(APPLICATION_JAVASCRIPT) ||
         aContentType.EqualsLiteral(APPLICATION_XJAVASCRIPT) ||
         aContentType.EqualsLiteral(TEXT_ECMASCRIPT) ||
         aContentType.EqualsLiteral(APPLICATION_ECMASCRIPT) ||
         aContentType.EqualsLiteral(TEXT_JAVASCRIPT) ||
         aContentType.EqualsLiteral(APPLICATION_JSON) ||
         aContentType.EqualsLiteral(TEXT_JSON);
}

bool
nsContentUtils::IsPlainTextType(const nsACString& aContentType)
{
  // NOTE: if you add a type here, add it to the CONTENTDLF_CATEGORIES
  // define in nsContentDLF.h as well.
  return aContentType.EqualsLiteral(TEXT_PLAIN) ||
         aContentType.EqualsLiteral(TEXT_CSS) ||
         aContentType.EqualsLiteral(TEXT_CACHE_MANIFEST) ||
         aContentType.EqualsLiteral(TEXT_VTT) ||
         IsScriptType(aContentType);
}

bool
nsContentUtils::IsUtf8OnlyPlainTextType(const nsACString& aContentType)
{
  // NOTE: This must be a subset of the list in IsPlainTextType().
  return aContentType.EqualsLiteral(TEXT_CACHE_MANIFEST) ||
         aContentType.EqualsLiteral(APPLICATION_JSON) ||
         aContentType.EqualsLiteral(TEXT_JSON) ||
         aContentType.EqualsLiteral(TEXT_VTT);
}

bool
nsContentUtils::GetWrapperSafeScriptFilename(nsIDocument* aDocument,
                                             nsIURI* aURI,
                                             nsACString& aScriptURI,
                                             nsresult* aRv)
{
  MOZ_ASSERT(aRv);
  bool scriptFileNameModified = false;
  *aRv = NS_OK;

  *aRv = aURI->GetSpec(aScriptURI);
  NS_ENSURE_SUCCESS(*aRv, false);

  if (IsChromeDoc(aDocument)) {
    nsCOMPtr<nsIChromeRegistry> chromeReg =
      mozilla::services::GetChromeRegistryService();

    if (!chromeReg) {
      // If we're running w/o a chrome registry we won't modify any
      // script file names.

      return scriptFileNameModified;
    }

    bool docWrappersEnabled =
      chromeReg->WrappersEnabled(aDocument->GetDocumentURI());

    bool uriWrappersEnabled = chromeReg->WrappersEnabled(aURI);

    nsIURI *docURI = aDocument->GetDocumentURI();

    if (docURI && docWrappersEnabled && !uriWrappersEnabled) {
      // aURI is a script from a URL that doesn't get wrapper
      // automation. aDocument is a chrome document that does get
      // wrapper automation. Prepend the chrome document's URI
      // followed by the string " -> " to the URI of the script we're
      // loading here so that script in that URI gets the same wrapper
      // automation that the chrome document expects.
      nsAutoCString spec;
      *aRv = docURI->GetSpec(spec);
      if (NS_WARN_IF(NS_FAILED(*aRv))) {
        return false;
      }

      spec.AppendLiteral(" -> ");
      spec.Append(aScriptURI);

      aScriptURI = spec;

      scriptFileNameModified = true;
    }
  }

  return scriptFileNameModified;
}

// static
bool
nsContentUtils::IsInChromeDocshell(nsIDocument *aDocument)
{
  if (!aDocument) {
    return false;
  }

  if (aDocument->GetDisplayDocument()) {
    return IsInChromeDocshell(aDocument->GetDisplayDocument());
  }

  nsCOMPtr<nsIDocShellTreeItem> docShell = aDocument->GetDocShell();
  if (!docShell) {
    return false;
  }

  return docShell->ItemType() == nsIDocShellTreeItem::typeChrome;
}

// static
nsIContentPolicy*
nsContentUtils::GetContentPolicy()
{
  if (!sTriedToGetContentPolicy) {
    CallGetService(NS_CONTENTPOLICY_CONTRACTID, &sContentPolicyService);
    // It's OK to not have a content policy service
    sTriedToGetContentPolicy = true;
  }

  return sContentPolicyService;
}

// static
bool
nsContentUtils::IsEventAttributeName(nsAtom* aName, int32_t aType)
{
  const char16_t* name = aName->GetUTF16String();
  if (name[0] != 'o' || name[1] != 'n')
    return false;

  EventNameMapping mapping;
  return (sAtomEventTable->Get(aName, &mapping) && mapping.mType & aType);
}

// static
EventMessage
nsContentUtils::GetEventMessage(nsAtom* aName)
{
  if (aName) {
    EventNameMapping mapping;
    if (sAtomEventTable->Get(aName, &mapping)) {
      return mapping.mMessage;
    }
  }

  return eUnidentifiedEvent;
}

// static
mozilla::EventClassID
nsContentUtils::GetEventClassID(const nsAString& aName)
{
  EventNameMapping mapping;
  if (sStringEventTable->Get(aName, &mapping))
    return mapping.mEventClassID;

  return eBasicEventClass;
}

nsAtom*
nsContentUtils::GetEventMessageAndAtom(const nsAString& aName,
                                       mozilla::EventClassID aEventClassID,
                                       EventMessage* aEventMessage)
{
  EventNameMapping mapping;
  if (sStringEventTable->Get(aName, &mapping)) {
    *aEventMessage =
      mapping.mEventClassID == aEventClassID ? mapping.mMessage :
                                               eUnidentifiedEvent;
    return mapping.mAtom;
  }

  // If we have cached lots of user defined event names, clear some of them.
  if (sUserDefinedEvents->Length() > 127) {
    while (sUserDefinedEvents->Length() > 64) {
      nsAtom* first = sUserDefinedEvents->ElementAt(0);
      sStringEventTable->Remove(Substring(nsDependentAtomString(first), 2));
      sUserDefinedEvents->RemoveElementAt(0);
    }
  }

  *aEventMessage = eUnidentifiedEvent;
  RefPtr<nsAtom> atom =
    NS_AtomizeMainThread(NS_LITERAL_STRING("on") + aName);
  sUserDefinedEvents->AppendElement(atom);
  mapping.mAtom = atom;
  mapping.mMessage = eUnidentifiedEvent;
  mapping.mType = EventNameType_None;
  mapping.mEventClassID = eBasicEventClass;
  // This is a slow hashtable call, but at least we cache the result for the
  // following calls. Because GetEventMessageAndAtomForListener utilizes
  // sStringEventTable, it needs to know in which cases sStringEventTable
  // doesn't contain the information it needs so that it can use
  // sAtomEventTable instead.
  mapping.mMaybeSpecialSVGorSMILEvent =
    GetEventMessage(atom) != eUnidentifiedEvent;
  sStringEventTable->Put(aName, mapping);
  return mapping.mAtom;
}

// static
EventMessage
nsContentUtils::GetEventMessageAndAtomForListener(const nsAString& aName,
                                                  nsAtom** aOnName)
{
  // Because of SVG/SMIL sStringEventTable contains a subset of the event names
  // comparing to the sAtomEventTable. However, usually sStringEventTable
  // contains the information we need, so in order to reduce hashtable
  // lookups, start from it.
  EventNameMapping mapping;
  EventMessage msg = eUnidentifiedEvent;
  RefPtr<nsAtom> atom;
  if (sStringEventTable->Get(aName, &mapping)) {
    if (mapping.mMaybeSpecialSVGorSMILEvent) {
      // Try the atom version so that we should get the right message for
      // SVG/SMIL.
      atom = NS_AtomizeMainThread(NS_LITERAL_STRING("on") + aName);
      msg = GetEventMessage(atom);
    } else {
      atom = mapping.mAtom;
      msg = mapping.mMessage;
    }
    atom.forget(aOnName);
    return msg;
  }

  // GetEventMessageAndAtom will cache the event type for the future usage...
  GetEventMessageAndAtom(aName, eBasicEventClass, &msg);

  // ...and then call this method recursively to get the message and atom from
  // now updated sStringEventTable.
  return GetEventMessageAndAtomForListener(aName, aOnName);
}

static
nsresult GetEventAndTarget(nsIDocument* aDoc, nsISupports* aTarget,
                           const nsAString& aEventName,
                           bool aCanBubble, bool aCancelable,
                           bool aTrusted, nsIDOMEvent** aEvent,
                           EventTarget** aTargetOut)
{
  nsCOMPtr<nsIDOMDocument> domDoc = do_QueryInterface(aDoc);
  nsCOMPtr<EventTarget> target(do_QueryInterface(aTarget));
  NS_ENSURE_TRUE(domDoc && target, NS_ERROR_INVALID_ARG);

  nsCOMPtr<nsIDOMEvent> event;
  nsresult rv =
    domDoc->CreateEvent(NS_LITERAL_STRING("Events"), getter_AddRefs(event));
  NS_ENSURE_SUCCESS(rv, rv);

  event->InitEvent(aEventName, aCanBubble, aCancelable);
  event->SetTrusted(aTrusted);

  rv = event->SetTarget(target);
  NS_ENSURE_SUCCESS(rv, rv);

  event.forget(aEvent);
  target.forget(aTargetOut);
  return NS_OK;
}

// static
nsresult
nsContentUtils::DispatchTrustedEvent(nsIDocument* aDoc, nsISupports* aTarget,
                                     const nsAString& aEventName,
                                     bool aCanBubble, bool aCancelable,
                                     bool* aDefaultAction)
{
  return DispatchEvent(aDoc, aTarget, aEventName, aCanBubble, aCancelable,
                       true, aDefaultAction);
}

// static
nsresult
nsContentUtils::DispatchUntrustedEvent(nsIDocument* aDoc, nsISupports* aTarget,
                                       const nsAString& aEventName,
                                       bool aCanBubble, bool aCancelable,
                                       bool* aDefaultAction)
{
  return DispatchEvent(aDoc, aTarget, aEventName, aCanBubble, aCancelable,
                       false, aDefaultAction);
}

// static
nsresult
nsContentUtils::DispatchEvent(nsIDocument* aDoc, nsISupports* aTarget,
                              const nsAString& aEventName,
                              bool aCanBubble, bool aCancelable,
                              bool aTrusted, bool* aDefaultAction,
                              bool aOnlyChromeDispatch)
{
  nsCOMPtr<nsIDOMEvent> event;
  nsCOMPtr<EventTarget> target;
  nsresult rv = GetEventAndTarget(aDoc, aTarget, aEventName, aCanBubble,
                                  aCancelable, aTrusted, getter_AddRefs(event),
                                  getter_AddRefs(target));
  NS_ENSURE_SUCCESS(rv, rv);
  event->WidgetEventPtr()->mFlags.mOnlyChromeDispatch = aOnlyChromeDispatch;

  bool dummy;
  return target->DispatchEvent(event, aDefaultAction ? aDefaultAction : &dummy);
}

// static
nsresult
nsContentUtils::DispatchEvent(nsIDocument* aDoc, nsISupports* aTarget,
                              WidgetEvent& aEvent,
                              EventMessage aEventMessage,
                              bool aCanBubble, bool aCancelable,
                              bool aTrusted, bool *aDefaultAction,
                              bool aOnlyChromeDispatch)
{
  MOZ_ASSERT_IF(aOnlyChromeDispatch, aTrusted);

  nsCOMPtr<EventTarget> target(do_QueryInterface(aTarget));

  aEvent.mTime = PR_Now();

  aEvent.mSpecifiedEventType = GetEventTypeFromMessage(aEventMessage);
  aEvent.SetDefaultComposed();
  aEvent.SetDefaultComposedInNativeAnonymousContent();

  aEvent.mFlags.mBubbles = aCanBubble;
  aEvent.mFlags.mCancelable = aCancelable;
  aEvent.mFlags.mOnlyChromeDispatch = aOnlyChromeDispatch;

  aEvent.mTarget = target;

  nsEventStatus status = nsEventStatus_eIgnore;
  nsresult rv = EventDispatcher::DispatchDOMEvent(target, &aEvent, nullptr,
                                                  nullptr, &status);
  if (aDefaultAction) {
    *aDefaultAction = (status != nsEventStatus_eConsumeNoDefault);
  }
  return rv;
}

nsresult
nsContentUtils::DispatchChromeEvent(nsIDocument *aDoc,
                                    nsISupports *aTarget,
                                    const nsAString& aEventName,
                                    bool aCanBubble, bool aCancelable,
                                    bool* aDefaultAction)
{

  nsCOMPtr<nsIDOMEvent> event;
  nsCOMPtr<EventTarget> target;
  nsresult rv = GetEventAndTarget(aDoc, aTarget, aEventName, aCanBubble,
                                  aCancelable, true, getter_AddRefs(event),
                                  getter_AddRefs(target));
  NS_ENSURE_SUCCESS(rv, rv);

  NS_ASSERTION(aDoc, "GetEventAndTarget lied?");
  if (!aDoc->GetWindow())
    return NS_ERROR_INVALID_ARG;

  EventTarget* piTarget = aDoc->GetWindow()->GetParentTarget();
  if (!piTarget)
    return NS_ERROR_INVALID_ARG;

  bool defaultActionEnabled;
  rv = piTarget->DispatchEvent(event, &defaultActionEnabled);
  if (aDefaultAction) {
    *aDefaultAction = defaultActionEnabled;
  }
  return rv;
}

/* static */
nsresult
nsContentUtils::DispatchFocusChromeEvent(nsPIDOMWindowOuter* aWindow)
{
  MOZ_ASSERT(aWindow);

  nsCOMPtr<nsIDocument> doc = aWindow->GetExtantDoc();
  if (!doc) {
    return NS_ERROR_FAILURE;
  }

  return DispatchChromeEvent(doc, aWindow,
                             NS_LITERAL_STRING("DOMWindowFocus"),
                             true, true);
}

nsresult
nsContentUtils::DispatchEventOnlyToChrome(nsIDocument* aDoc,
                                          nsISupports* aTarget,
                                          const nsAString& aEventName,
                                          bool aCanBubble, bool aCancelable,
                                          bool* aDefaultAction)
{
  return DispatchEvent(aDoc, aTarget, aEventName, aCanBubble, aCancelable,
                       true, aDefaultAction, true);
}

/* static */
Element*
nsContentUtils::MatchElementId(nsIContent *aContent, const nsAtom* aId)
{
  for (nsIContent* cur = aContent;
       cur;
       cur = cur->GetNextNode(aContent)) {
    if (aId == cur->GetID()) {
      return cur->AsElement();
    }
  }

  return nullptr;
}

/* static */
Element *
nsContentUtils::MatchElementId(nsIContent *aContent, const nsAString& aId)
{
  NS_PRECONDITION(!aId.IsEmpty(), "Will match random elements");

  // ID attrs are generally stored as atoms, so just atomize this up front
  RefPtr<nsAtom> id(NS_Atomize(aId));
  if (!id) {
    // OOM, so just bail
    return nullptr;
  }

  return MatchElementId(aContent, id);
}

/* static */
nsIDocument*
nsContentUtils::GetSubdocumentWithOuterWindowId(nsIDocument *aDocument,
                                                uint64_t aOuterWindowId)
{
  if (!aDocument || !aOuterWindowId) {
    return nullptr;
  }

  RefPtr<nsGlobalWindowOuter> window =
    nsGlobalWindowOuter::GetOuterWindowWithId(aOuterWindowId);
  if (!window) {
    return nullptr;
  }

  nsCOMPtr<nsPIDOMWindowOuter> outerWindow = window->AsOuter();
  nsCOMPtr<nsIDocument> foundDoc = outerWindow->GetDoc();
  if (nsContentUtils::ContentIsCrossDocDescendantOf(foundDoc, aDocument)) {
    // Note that ContentIsCrossDocDescendantOf will return true if
    // foundDoc == aDocument.
    return foundDoc;
  }

  return nullptr;
}

/* static */
void
nsContentUtils::RegisterShutdownObserver(nsIObserver* aObserver)
{
  nsCOMPtr<nsIObserverService> observerService =
    mozilla::services::GetObserverService();
  if (observerService) {
    observerService->AddObserver(aObserver,
                                 NS_XPCOM_SHUTDOWN_OBSERVER_ID,
                                 false);
  }
}

/* static */
void
nsContentUtils::UnregisterShutdownObserver(nsIObserver* aObserver)
{
  nsCOMPtr<nsIObserverService> observerService =
    mozilla::services::GetObserverService();
  if (observerService) {
    observerService->RemoveObserver(aObserver, NS_XPCOM_SHUTDOWN_OBSERVER_ID);
  }
}

/* static */
bool
nsContentUtils::HasNonEmptyAttr(const nsIContent* aContent, int32_t aNameSpaceID,
                                nsAtom* aName)
{
  static nsIContent::AttrValuesArray strings[] = {&nsGkAtoms::_empty, nullptr};
  return aContent->FindAttrValueIn(aNameSpaceID, aName, strings, eCaseMatters)
    == nsIContent::ATTR_VALUE_NO_MATCH;
}

/* static */
bool
nsContentUtils::HasMutationListeners(nsINode* aNode,
                                     uint32_t aType,
                                     nsINode* aTargetForSubtreeModified)
{
  nsIDocument* doc = aNode->OwnerDoc();

  // global object will be null for documents that don't have windows.
  nsPIDOMWindowInner* window = doc->GetInnerWindow();
  // This relies on EventListenerManager::AddEventListener, which sets
  // all mutation bits when there is a listener for DOMSubtreeModified event.
  if (window && !window->HasMutationListeners(aType)) {
    return false;
  }

  if (aNode->IsContent() && aNode->AsContent()->ChromeOnlyAccess()) {
    return false;
  }

  doc->MayDispatchMutationEvent(aTargetForSubtreeModified);

  // If we have a window, we can check it for mutation listeners now.
  if (aNode->IsInUncomposedDoc()) {
    nsCOMPtr<EventTarget> piTarget(do_QueryInterface(window));
    if (piTarget) {
      EventListenerManager* manager = piTarget->GetExistingListenerManager();
      if (manager && manager->HasMutationListeners()) {
        return true;
      }
    }
  }

  // If we have a window, we know a mutation listener is registered, but it
  // might not be in our chain.  If we don't have a window, we might have a
  // mutation listener.  Check quickly to see.
  while (aNode) {
    EventListenerManager* manager = aNode->GetExistingListenerManager();
    if (manager && manager->HasMutationListeners()) {
      return true;
    }

    if (aNode->IsContent()) {
      nsIContent* insertionPoint = aNode->AsContent()->GetXBLInsertionPoint();
      if (insertionPoint) {
        aNode = insertionPoint->GetParent();
        MOZ_ASSERT(aNode);
        continue;
      }
    }
    aNode = aNode->GetParentNode();
  }

  return false;
}

/* static */
bool
nsContentUtils::HasMutationListeners(nsIDocument* aDocument,
                                     uint32_t aType)
{
  nsPIDOMWindowInner* window = aDocument ?
    aDocument->GetInnerWindow() : nullptr;

  // This relies on EventListenerManager::AddEventListener, which sets
  // all mutation bits when there is a listener for DOMSubtreeModified event.
  return !window || window->HasMutationListeners(aType);
}

void
nsContentUtils::MaybeFireNodeRemoved(nsINode* aChild, nsINode* aParent,
                                     nsIDocument* aOwnerDoc)
{
  NS_PRECONDITION(aChild, "Missing child");
  NS_PRECONDITION(aChild->GetParentNode() == aParent, "Wrong parent");
  NS_PRECONDITION(aChild->OwnerDoc() == aOwnerDoc, "Wrong owner-doc");

  // Having an explicit check here since it's an easy mistake to fall into,
  // and there might be existing code with problems. We'd rather be safe
  // than fire DOMNodeRemoved in all corner cases. We also rely on it for
  // nsAutoScriptBlockerSuppressNodeRemoved.
  if (!IsSafeToRunScript()) {
    // This checks that IsSafeToRunScript is true since we don't want to fire
    // events when that is false. We can't rely on EventDispatcher to assert
    // this in this situation since most of the time there are no mutation
    // event listeners, in which case we won't even attempt to dispatch events.
    // However this also allows for two exceptions. First off, we don't assert
    // if the mutation happens to native anonymous content since we never fire
    // mutation events on such content anyway.
    // Second, we don't assert if sDOMNodeRemovedSuppressCount is true since
    // that is a know case when we'd normally fire a mutation event, but can't
    // make that safe and so we suppress it at this time. Ideally this should
    // go away eventually.
    if (!(aChild->IsContent() && aChild->AsContent()->IsInNativeAnonymousSubtree()) &&
        !sDOMNodeRemovedSuppressCount) {
      NS_ERROR("Want to fire DOMNodeRemoved event, but it's not safe");
      WarnScriptWasIgnored(aOwnerDoc);
    }
    return;
  }

  if (HasMutationListeners(aChild,
        NS_EVENT_BITS_MUTATION_NODEREMOVED, aParent)) {
    InternalMutationEvent mutation(true, eLegacyNodeRemoved);
    mutation.mRelatedNode = do_QueryInterface(aParent);

    mozAutoSubtreeModified subtree(aOwnerDoc, aParent);
    EventDispatcher::Dispatch(aChild, nullptr, &mutation);
  }
}

void
nsContentUtils::UnmarkGrayJSListenersInCCGenerationDocuments()
{
  if (!sEventListenerManagersHash) {
    return;
  }

  for (auto i = sEventListenerManagersHash->Iter(); !i.Done(); i.Next()) {
    auto entry = static_cast<EventListenerManagerMapEntry*>(i.Get());
    nsINode* n = static_cast<nsINode*>(entry->mListenerManager->GetTarget());
    if (n && n->IsInUncomposedDoc() &&
        nsCCUncollectableMarker::InGeneration(n->OwnerDoc()->GetMarkedCCGeneration())) {
      entry->mListenerManager->MarkForCC();
    }
  }
}

/* static */
void
nsContentUtils::TraverseListenerManager(nsINode *aNode,
                                        nsCycleCollectionTraversalCallback &cb)
{
  if (!sEventListenerManagersHash) {
    // We're already shut down, just return.
    return;
  }

  auto entry = static_cast<EventListenerManagerMapEntry*>
                          (sEventListenerManagersHash->Search(aNode));
  if (entry) {
    CycleCollectionNoteChild(cb, entry->mListenerManager.get(),
                             "[via hash] mListenerManager");
  }
}

EventListenerManager*
nsContentUtils::GetListenerManagerForNode(nsINode *aNode)
{
  if (!sEventListenerManagersHash) {
    // We're already shut down, don't bother creating an event listener
    // manager.

    return nullptr;
  }

  auto entry =
    static_cast<EventListenerManagerMapEntry*>
               (sEventListenerManagersHash->Add(aNode, fallible));

  if (!entry) {
    return nullptr;
  }

  if (!entry->mListenerManager) {
    entry->mListenerManager = new EventListenerManager(aNode);

    aNode->SetFlags(NODE_HAS_LISTENERMANAGER);
  }

  return entry->mListenerManager;
}

EventListenerManager*
nsContentUtils::GetExistingListenerManagerForNode(const nsINode *aNode)
{
  if (!aNode->HasFlag(NODE_HAS_LISTENERMANAGER)) {
    return nullptr;
  }

  if (!sEventListenerManagersHash) {
    // We're already shut down, don't bother creating an event listener
    // manager.

    return nullptr;
  }

  auto entry = static_cast<EventListenerManagerMapEntry*>
                          (sEventListenerManagersHash->Search(aNode));
  if (entry) {
    return entry->mListenerManager;
  }

  return nullptr;
}

/* static */
void
nsContentUtils::RemoveListenerManager(nsINode *aNode)
{
  if (sEventListenerManagersHash) {
    auto entry = static_cast<EventListenerManagerMapEntry*>
                            (sEventListenerManagersHash->Search(aNode));
    if (entry) {
      RefPtr<EventListenerManager> listenerManager;
      listenerManager.swap(entry->mListenerManager);
      // Remove the entry and *then* do operations that could cause further
      // modification of sEventListenerManagersHash.  See bug 334177.
      sEventListenerManagersHash->RawRemove(entry);
      if (listenerManager) {
        listenerManager->Disconnect();
      }
    }
  }
}

/* static */
bool
nsContentUtils::IsValidNodeName(nsAtom *aLocalName, nsAtom *aPrefix,
                                int32_t aNamespaceID)
{
  if (aNamespaceID == kNameSpaceID_Unknown) {
    return false;
  }

  if (!aPrefix) {
    // If the prefix is null, then either the QName must be xmlns or the
    // namespace must not be XMLNS.
    return (aLocalName == nsGkAtoms::xmlns) ==
           (aNamespaceID == kNameSpaceID_XMLNS);
  }

  // If the prefix is non-null then the namespace must not be null.
  if (aNamespaceID == kNameSpaceID_None) {
    return false;
  }

  // If the namespace is the XMLNS namespace then the prefix must be xmlns,
  // but the localname must not be xmlns.
  if (aNamespaceID == kNameSpaceID_XMLNS) {
    return aPrefix == nsGkAtoms::xmlns && aLocalName != nsGkAtoms::xmlns;
  }

  // If the namespace is not the XMLNS namespace then the prefix must not be
  // xmlns.
  // If the namespace is the XML namespace then the prefix can be anything.
  // If the namespace is not the XML namespace then the prefix must not be xml.
  return aPrefix != nsGkAtoms::xmlns &&
         (aNamespaceID == kNameSpaceID_XML || aPrefix != nsGkAtoms::xml);
}

/* static */
nsresult
nsContentUtils::CreateContextualFragment(nsINode* aContextNode,
                                         const nsAString& aFragment,
                                         bool aPreventScriptExecution,
                                         nsIDOMDocumentFragment** aReturn)
{
  ErrorResult rv;
  *aReturn = CreateContextualFragment(aContextNode, aFragment,
                                      aPreventScriptExecution, rv).take();
  return rv.StealNSResult();
}

already_AddRefed<DocumentFragment>
nsContentUtils::CreateContextualFragment(nsINode* aContextNode,
                                         const nsAString& aFragment,
                                         bool aPreventScriptExecution,
                                         ErrorResult& aRv)
{
  if (!aContextNode) {
    aRv.Throw(NS_ERROR_INVALID_ARG);
    return nullptr;
  }

  // If we don't have a document here, we can't get the right security context
  // for compiling event handlers... so just bail out.
  nsCOMPtr<nsIDocument> document = aContextNode->OwnerDoc();
  bool isHTML = document->IsHTMLDocument();
#ifdef DEBUG
  nsCOMPtr<nsIHTMLDocument> htmlDoc = do_QueryInterface(document);
  NS_ASSERTION(!isHTML || htmlDoc, "Should have HTMLDocument here!");
#endif

  if (isHTML) {
    RefPtr<DocumentFragment> frag =
      new DocumentFragment(document->NodeInfoManager());

    nsCOMPtr<nsIContent> contextAsContent = do_QueryInterface(aContextNode);
    if (contextAsContent && !contextAsContent->IsElement()) {
      contextAsContent = contextAsContent->GetParent();
      if (contextAsContent && !contextAsContent->IsElement()) {
        // can this even happen?
        contextAsContent = nullptr;
      }
    }

    if (contextAsContent && !contextAsContent->IsHTMLElement(nsGkAtoms::html)) {
      aRv = ParseFragmentHTML(aFragment, frag,
                              contextAsContent->NodeInfo()->NameAtom(),
                              contextAsContent->GetNameSpaceID(),
                              (document->GetCompatibilityMode() ==
                               eCompatibility_NavQuirks),
                              aPreventScriptExecution);
    } else {
      aRv = ParseFragmentHTML(aFragment, frag,
                              nsGkAtoms::body,
                              kNameSpaceID_XHTML,
                              (document->GetCompatibilityMode() ==
                               eCompatibility_NavQuirks),
                              aPreventScriptExecution);
    }

    return frag.forget();
  }

  AutoTArray<nsString, 32> tagStack;
  nsAutoString uriStr, nameStr;
  nsCOMPtr<nsIContent> content = do_QueryInterface(aContextNode);
  // just in case we have a text node
  if (content && !content->IsElement())
    content = content->GetParent();

  while (content && content->IsElement()) {
    nsString& tagName = *tagStack.AppendElement();
    tagName = content->NodeInfo()->QualifiedName();

    // see if we need to add xmlns declarations
    uint32_t count = content->AsElement()->GetAttrCount();
    bool setDefaultNamespace = false;
    if (count > 0) {
      uint32_t index;

      for (index = 0; index < count; index++) {
        const BorrowedAttrInfo info = content->AsElement()->GetAttrInfoAt(index);
        const nsAttrName* name = info.mName;
        if (name->NamespaceEquals(kNameSpaceID_XMLNS)) {
          info.mValue->ToString(uriStr);

          // really want something like nsXMLContentSerializer::SerializeAttr
          tagName.AppendLiteral(" xmlns"); // space important
          if (name->GetPrefix()) {
            tagName.Append(char16_t(':'));
            name->LocalName()->ToString(nameStr);
            tagName.Append(nameStr);
          } else {
            setDefaultNamespace = true;
          }
          tagName.AppendLiteral(R"(=")");
          tagName.Append(uriStr);
          tagName.Append('"');
        }
      }
    }

    if (!setDefaultNamespace) {
      mozilla::dom::NodeInfo* info = content->NodeInfo();
      if (!info->GetPrefixAtom() &&
          info->NamespaceID() != kNameSpaceID_None) {
        // We have no namespace prefix, but have a namespace ID.  Push
        // default namespace attr in, so that our kids will be in our
        // namespace.
        info->GetNamespaceURI(uriStr);
        tagName.AppendLiteral(R"( xmlns=")");
        tagName.Append(uriStr);
        tagName.Append('"');
      }
    }

    content = content->GetParent();
  }

  nsCOMPtr<nsIDOMDocumentFragment> frag;
  aRv = ParseFragmentXML(aFragment, document, tagStack,
                         aPreventScriptExecution, getter_AddRefs(frag));
  return frag.forget().downcast<DocumentFragment>();
}

/* static */
void
nsContentUtils::DropFragmentParsers()
{
  NS_IF_RELEASE(sHTMLFragmentParser);
  NS_IF_RELEASE(sXMLFragmentParser);
  NS_IF_RELEASE(sXMLFragmentSink);
}

/* static */
void
nsContentUtils::XPCOMShutdown()
{
  nsContentUtils::DropFragmentParsers();
}

/* static */
nsresult
nsContentUtils::ParseFragmentHTML(const nsAString& aSourceBuffer,
                                  nsIContent* aTargetNode,
                                  nsAtom* aContextLocalName,
                                  int32_t aContextNamespace,
                                  bool aQuirks,
                                  bool aPreventScriptExecution)
{
  AutoTimelineMarker m(aTargetNode->OwnerDoc()->GetDocShell(), "Parse HTML");

  if (nsContentUtils::sFragmentParsingActive) {
    NS_NOTREACHED("Re-entrant fragment parsing attempted.");
    return NS_ERROR_DOM_INVALID_STATE_ERR;
  }
  mozilla::AutoRestore<bool> guard(nsContentUtils::sFragmentParsingActive);
  nsContentUtils::sFragmentParsingActive = true;
  if (!sHTMLFragmentParser) {
    NS_ADDREF(sHTMLFragmentParser = new nsHtml5StringParser());
    // Now sHTMLFragmentParser owns the object
  }
  nsresult rv =
    sHTMLFragmentParser->ParseFragment(aSourceBuffer,
                                       aTargetNode,
                                       aContextLocalName,
                                       aContextNamespace,
                                       aQuirks,
                                       aPreventScriptExecution);
  return rv;
}

/* static */
nsresult
nsContentUtils::ParseDocumentHTML(const nsAString& aSourceBuffer,
                                  nsIDocument* aTargetDocument,
                                  bool aScriptingEnabledForNoscriptParsing)
{
  AutoTimelineMarker m(aTargetDocument->GetDocShell(), "Parse HTML");

  if (nsContentUtils::sFragmentParsingActive) {
    NS_NOTREACHED("Re-entrant fragment parsing attempted.");
    return NS_ERROR_DOM_INVALID_STATE_ERR;
  }
  mozilla::AutoRestore<bool> guard(nsContentUtils::sFragmentParsingActive);
  nsContentUtils::sFragmentParsingActive = true;
  if (!sHTMLFragmentParser) {
    NS_ADDREF(sHTMLFragmentParser = new nsHtml5StringParser());
    // Now sHTMLFragmentParser owns the object
  }
  nsresult rv =
    sHTMLFragmentParser->ParseDocument(aSourceBuffer,
                                       aTargetDocument,
                                       aScriptingEnabledForNoscriptParsing);
  return rv;
}

/* static */
nsresult
nsContentUtils::ParseFragmentXML(const nsAString& aSourceBuffer,
                                 nsIDocument* aDocument,
                                 nsTArray<nsString>& aTagStack,
                                 bool aPreventScriptExecution,
                                 nsIDOMDocumentFragment** aReturn)
{
  AutoTimelineMarker m(aDocument->GetDocShell(), "Parse XML");

  if (nsContentUtils::sFragmentParsingActive) {
    NS_NOTREACHED("Re-entrant fragment parsing attempted.");
    return NS_ERROR_DOM_INVALID_STATE_ERR;
  }
  mozilla::AutoRestore<bool> guard(nsContentUtils::sFragmentParsingActive);
  nsContentUtils::sFragmentParsingActive = true;
  if (!sXMLFragmentParser) {
    nsCOMPtr<nsIParser> parser = do_CreateInstance(kCParserCID);
    parser.forget(&sXMLFragmentParser);
    // sXMLFragmentParser now owns the parser
  }
  if (!sXMLFragmentSink) {
    NS_NewXMLFragmentContentSink(&sXMLFragmentSink);
    // sXMLFragmentSink now owns the sink
  }
  nsCOMPtr<nsIContentSink> contentsink = do_QueryInterface(sXMLFragmentSink);
  MOZ_ASSERT(contentsink, "Sink doesn't QI to nsIContentSink!");
  sXMLFragmentParser->SetContentSink(contentsink);

  sXMLFragmentSink->SetTargetDocument(aDocument);
  sXMLFragmentSink->SetPreventScriptExecution(aPreventScriptExecution);

  nsresult rv =
    sXMLFragmentParser->ParseFragment(aSourceBuffer,
                                      aTagStack);
  if (NS_FAILED(rv)) {
    // Drop the fragment parser and sink that might be in an inconsistent state
    NS_IF_RELEASE(sXMLFragmentParser);
    NS_IF_RELEASE(sXMLFragmentSink);
    return rv;
  }

  rv = sXMLFragmentSink->FinishFragmentParsing(aReturn);

  sXMLFragmentParser->Reset();

  return rv;
}

/* static */
nsresult
nsContentUtils::ConvertToPlainText(const nsAString& aSourceBuffer,
                                   nsAString& aResultBuffer,
                                   uint32_t aFlags,
                                   uint32_t aWrapCol)
{
  nsCOMPtr<nsIURI> uri;
  NS_NewURI(getter_AddRefs(uri), "about:blank");
  nsCOMPtr<nsIPrincipal> principal = NullPrincipal::Create();
  nsCOMPtr<nsIDOMDocument> domDocument;
  nsresult rv = NS_NewDOMDocument(getter_AddRefs(domDocument),
                                  EmptyString(),
                                  EmptyString(),
                                  nullptr,
                                  uri,
                                  uri,
                                  principal,
                                  true,
                                  nullptr,
                                  DocumentFlavorHTML,
                                  StyleBackendType::None);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIDocument> document = do_QueryInterface(domDocument);
  rv = nsContentUtils::ParseDocumentHTML(aSourceBuffer, document,
    !(aFlags & nsIDocumentEncoder::OutputNoScriptContent));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIDocumentEncoder> encoder = do_CreateInstance(
    "@mozilla.org/layout/documentEncoder;1?type=text/plain");

  rv = encoder->Init(domDocument, NS_LITERAL_STRING("text/plain"), aFlags);
  NS_ENSURE_SUCCESS(rv, rv);

  encoder->SetWrapColumn(aWrapCol);

  return encoder->EncodeToString(aResultBuffer);
}

/* static */
nsresult
nsContentUtils::SetNodeTextContent(nsIContent* aContent,
                                   const nsAString& aValue,
                                   bool aTryReuse)
{
  // Fire DOMNodeRemoved mutation events before we do anything else.
  nsCOMPtr<nsIContent> owningContent;

  // Batch possible DOMSubtreeModified events.
  mozAutoSubtreeModified subtree(nullptr, nullptr);

  // Scope firing mutation events so that we don't carry any state that
  // might be stale
  {
    // We're relying on mozAutoSubtreeModified to keep a strong reference if
    // needed.
    nsIDocument* doc = aContent->OwnerDoc();

    // Optimize the common case of there being no observers
    if (HasMutationListeners(doc, NS_EVENT_BITS_MUTATION_NODEREMOVED)) {
      subtree.UpdateTarget(doc, nullptr);
      owningContent = aContent;
      nsCOMPtr<nsINode> child;
      bool skipFirst = aTryReuse;
      for (child = aContent->GetFirstChild();
           child && child->GetParentNode() == aContent;
           child = child->GetNextSibling()) {
        if (skipFirst && child->IsNodeOfType(nsINode::eTEXT)) {
          skipFirst = false;
          continue;
        }
        nsContentUtils::MaybeFireNodeRemoved(child, aContent, doc);
      }
    }
  }

  // Might as well stick a batch around this since we're performing several
  // mutations.
  mozAutoDocUpdate updateBatch(aContent->GetComposedDoc(),
    UPDATE_CONTENT_MODEL, true);
  nsAutoMutationBatch mb;

  uint32_t childCount = aContent->GetChildCount();

  if (aTryReuse && !aValue.IsEmpty()) {
    uint32_t removeIndex = 0;

    for (uint32_t i = 0; i < childCount; ++i) {
      nsIContent* child = aContent->GetChildAt(removeIndex);
      if (removeIndex == 0 && child && child->IsNodeOfType(nsINode::eTEXT)) {
        nsresult rv = child->SetText(aValue, true);
        NS_ENSURE_SUCCESS(rv, rv);

        removeIndex = 1;
      }
      else {
        aContent->RemoveChildAt(removeIndex, true);
      }
    }

    if (removeIndex == 1) {
      return NS_OK;
    }
  }
  else {
    mb.Init(aContent, true, false);
    for (uint32_t i = 0; i < childCount; ++i) {
      aContent->RemoveChildAt(0, true);
    }
  }
  mb.RemovalDone();

  if (aValue.IsEmpty()) {
    return NS_OK;
  }

  RefPtr<nsTextNode> textContent =
    new nsTextNode(aContent->NodeInfo()->NodeInfoManager());

  textContent->SetText(aValue, true);

  nsresult rv = aContent->AppendChildTo(textContent, true);
  mb.NodesAdded();
  return rv;
}

static bool
AppendNodeTextContentsRecurse(nsINode* aNode, nsAString& aResult,
                              const fallible_t& aFallible)
{
  for (nsIContent* child = aNode->GetFirstChild();
       child;
       child = child->GetNextSibling()) {
    if (child->IsElement()) {
      bool ok = AppendNodeTextContentsRecurse(child, aResult,
                                              aFallible);
      if (!ok) {
        return false;
      }
    }
    else if (child->IsNodeOfType(nsINode::eTEXT)) {
      bool ok = child->AppendTextTo(aResult, aFallible);
      if (!ok) {
        return false;
      }
    }
  }

  return true;
}

/* static */
bool
nsContentUtils::AppendNodeTextContent(nsINode* aNode, bool aDeep,
                                      nsAString& aResult,
                                      const fallible_t& aFallible)
{
  if (aNode->IsNodeOfType(nsINode::eTEXT)) {
    return static_cast<nsIContent*>(aNode)->AppendTextTo(aResult,
                                                         aFallible);
  }
  if (aDeep) {
    return AppendNodeTextContentsRecurse(aNode, aResult, aFallible);
  }

  for (nsIContent* child = aNode->GetFirstChild();
       child;
       child = child->GetNextSibling()) {
    if (child->IsNodeOfType(nsINode::eTEXT)) {
      bool ok = child->AppendTextTo(aResult, fallible);
      if (!ok) {
        return false;
      }
    }
  }
  return true;
}

bool
nsContentUtils::HasNonEmptyTextContent(nsINode* aNode,
                                       TextContentDiscoverMode aDiscoverMode)
{
  for (nsIContent* child = aNode->GetFirstChild();
       child;
       child = child->GetNextSibling()) {
    if (child->IsNodeOfType(nsINode::eTEXT) &&
        child->TextLength() > 0) {
        return true;
    }

    if (aDiscoverMode == eRecurseIntoChildren &&
        HasNonEmptyTextContent(child, aDiscoverMode)) {
      return true;
    }
  }

  return false;
}

/* static */
bool
nsContentUtils::IsInSameAnonymousTree(const nsINode* aNode,
                                      const nsIContent* aContent)
{
  NS_PRECONDITION(aNode,
                  "Must have a node to work with");
  NS_PRECONDITION(aContent,
                  "Must have a content to work with");

  if (!aNode->IsContent()) {
    /**
     * The root isn't an nsIContent, so it's a document or attribute.  The only
     * nodes in the same anonymous subtree as it will have a null
     * bindingParent.
     *
     * XXXbz strictly speaking, that's not true for attribute nodes.
     */
    return aContent->GetBindingParent() == nullptr;
  }

  return aNode->AsContent()->GetBindingParent() == aContent->GetBindingParent();
}

/* static */
void
nsContentUtils::NotifyInstalledMenuKeyboardListener(bool aInstalling)
{
  IMEStateManager::OnInstalledMenuKeyboardListener(aInstalling);
}

/* static */ bool
nsContentUtils::SchemeIs(nsIURI* aURI, const char* aScheme)
{
  nsCOMPtr<nsIURI> baseURI = NS_GetInnermostURI(aURI);
  NS_ENSURE_TRUE(baseURI, false);

  bool isScheme = false;
  return NS_SUCCEEDED(baseURI->SchemeIs(aScheme, &isScheme)) && isScheme;
}

bool
nsContentUtils::IsSystemPrincipal(nsIPrincipal* aPrincipal)
{
  MOZ_ASSERT(IsInitialized());
  return aPrincipal == sSystemPrincipal;
}

bool
nsContentUtils::IsExpandedPrincipal(nsIPrincipal* aPrincipal)
{
  nsCOMPtr<nsIExpandedPrincipal> ep = do_QueryInterface(aPrincipal);
  return !!ep;
}

nsIPrincipal*
nsContentUtils::GetSystemPrincipal()
{
  MOZ_ASSERT(IsInitialized());
  return sSystemPrincipal;
}

bool
nsContentUtils::CombineResourcePrincipals(nsCOMPtr<nsIPrincipal>* aResourcePrincipal,
                                          nsIPrincipal* aExtraPrincipal)
{
  if (!aExtraPrincipal) {
    return false;
  }
  if (!*aResourcePrincipal) {
    *aResourcePrincipal = aExtraPrincipal;
    return true;
  }
  if (*aResourcePrincipal == aExtraPrincipal) {
    return false;
  }
  bool subsumes;
  if (NS_SUCCEEDED((*aResourcePrincipal)->Subsumes(aExtraPrincipal, &subsumes)) &&
      subsumes) {
    return false;
  }
  *aResourcePrincipal = sSystemPrincipal;
  return true;
}

/* static */
void
nsContentUtils::TriggerLink(nsIContent *aContent, nsPresContext *aPresContext,
                            nsIURI *aLinkURI, const nsString &aTargetSpec,
                            bool aClick, bool aIsUserTriggered,
                            bool aIsTrusted)
{
  NS_ASSERTION(aPresContext, "Need a nsPresContext");
  NS_PRECONDITION(aLinkURI, "No link URI");

  if (aContent->IsEditable()) {
    return;
  }

  nsILinkHandler *handler = aPresContext->GetLinkHandler();
  if (!handler) {
    return;
  }

  if (!aClick) {
    handler->OnOverLink(aContent, aLinkURI, aTargetSpec.get());
    return;
  }

  // Check that this page is allowed to load this URI.
  nsresult proceed = NS_OK;

  if (sSecurityManager) {
    uint32_t flag =
      aIsUserTriggered ?
      (uint32_t)nsIScriptSecurityManager::STANDARD :
      (uint32_t)nsIScriptSecurityManager::LOAD_IS_AUTOMATIC_DOCUMENT_REPLACEMENT;
    proceed =
      sSecurityManager->CheckLoadURIWithPrincipal(aContent->NodePrincipal(),
                                                  aLinkURI, flag);
  }

  // Only pass off the click event if the script security manager says it's ok.
  // We need to rest aTargetSpec for forced downloads.
  if (NS_SUCCEEDED(proceed)) {

    // A link/area element with a download attribute is allowed to set
    // a pseudo Content-Disposition header.
    // For security reasons we only allow websites to declare same-origin resources
    // as downloadable. If this check fails we will just do the normal thing
    // (i.e. navigate to the resource).
    nsAutoString fileName;
    if ((!aContent->IsHTMLElement(nsGkAtoms::a) &&
         !aContent->IsHTMLElement(nsGkAtoms::area) &&
         !aContent->IsSVGElement(nsGkAtoms::a)) ||
        !aContent->GetAttr(kNameSpaceID_None, nsGkAtoms::download, fileName) ||
        NS_FAILED(aContent->NodePrincipal()->CheckMayLoad(aLinkURI, false, true))) {
      fileName.SetIsVoid(true); // No actionable download attribute was found.
    }

    handler->OnLinkClick(aContent, aLinkURI,
                         fileName.IsVoid() ? aTargetSpec.get() : EmptyString().get(),
                         fileName, nullptr, -1, nullptr, aIsTrusted,
                         aContent->NodePrincipal());
  }
}

/* static */
void
nsContentUtils::GetLinkLocation(Element* aElement, nsString& aLocationString)
{
  nsCOMPtr<nsIURI> hrefURI = aElement->GetHrefURI();
  if (hrefURI) {
    nsAutoCString specUTF8;
    nsresult rv = hrefURI->GetSpec(specUTF8);
    if (NS_SUCCEEDED(rv))
      CopyUTF8toUTF16(specUTF8, aLocationString);
  }
}

/* static */
nsIWidget*
nsContentUtils::GetTopLevelWidget(nsIWidget* aWidget)
{
  if (!aWidget)
    return nullptr;

  return aWidget->GetTopLevelWidget();
}

/* static */
const nsDependentString
nsContentUtils::GetLocalizedEllipsis()
{
  static char16_t sBuf[4] = { 0, 0, 0, 0 };
  if (!sBuf[0]) {
    nsAutoString tmp;
    Preferences::GetLocalizedString("intl.ellipsis", tmp);
    uint32_t len = std::min(uint32_t(tmp.Length()),
                          uint32_t(ArrayLength(sBuf) - 1));
    CopyUnicodeTo(tmp, 0, sBuf, len);
    if (!sBuf[0])
      sBuf[0] = char16_t(0x2026);
  }
  return nsDependentString(sBuf);
}

/* static */
void
nsContentUtils::AddScriptBlocker()
{
  MOZ_ASSERT(NS_IsMainThread());
  if (!sScriptBlockerCount) {
    MOZ_ASSERT(sRunnersCountAtFirstBlocker == 0,
               "Should not already have a count");
    sRunnersCountAtFirstBlocker = sBlockedScriptRunners ? sBlockedScriptRunners->Length() : 0;
  }
  ++sScriptBlockerCount;
}

#ifdef DEBUG
static bool sRemovingScriptBlockers = false;
#endif

/* static */
void
nsContentUtils::RemoveScriptBlocker()
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!sRemovingScriptBlockers);
  NS_ASSERTION(sScriptBlockerCount != 0, "Negative script blockers");
  --sScriptBlockerCount;
  if (sScriptBlockerCount) {
    return;
  }

  if (!sBlockedScriptRunners) {
    return;
  }

  uint32_t firstBlocker = sRunnersCountAtFirstBlocker;
  uint32_t lastBlocker = sBlockedScriptRunners->Length();
  uint32_t originalFirstBlocker = firstBlocker;
  uint32_t blockersCount = lastBlocker - firstBlocker;
  sRunnersCountAtFirstBlocker = 0;
  NS_ASSERTION(firstBlocker <= lastBlocker,
               "bad sRunnersCountAtFirstBlocker");

  while (firstBlocker < lastBlocker) {
    nsCOMPtr<nsIRunnable> runnable;
    runnable.swap((*sBlockedScriptRunners)[firstBlocker]);
    ++firstBlocker;

    // Calling the runnable can reenter us
    runnable->Run();
    // So can dropping the reference to the runnable
    runnable = nullptr;

    NS_ASSERTION(sRunnersCountAtFirstBlocker == 0,
                 "Bad count");
    NS_ASSERTION(!sScriptBlockerCount, "This is really bad");
  }
#ifdef DEBUG
  AutoRestore<bool> removingScriptBlockers(sRemovingScriptBlockers);
  sRemovingScriptBlockers = true;
#endif
  sBlockedScriptRunners->RemoveElementsAt(originalFirstBlocker, blockersCount);
}

/* static */
nsIWindowProvider*
nsContentUtils::GetWindowProviderForContentProcess()
{
  MOZ_ASSERT(XRE_IsContentProcess());
  return ContentChild::GetSingleton();
}

/* static */
already_AddRefed<nsPIDOMWindowOuter>
nsContentUtils::GetMostRecentNonPBWindow()
{
  nsCOMPtr<nsIWindowMediator> windowMediator =
    do_GetService(NS_WINDOWMEDIATOR_CONTRACTID);
  nsCOMPtr<nsIWindowMediator_44> wm = do_QueryInterface(windowMediator);

  nsCOMPtr<mozIDOMWindowProxy> window;
  wm->GetMostRecentNonPBWindow(u"navigator:browser",
                               getter_AddRefs(window));
  nsCOMPtr<nsPIDOMWindowOuter> pwindow;
  pwindow = do_QueryInterface(window);

  return pwindow.forget();
}

/* static */
void
nsContentUtils::WarnScriptWasIgnored(nsIDocument* aDocument)
{
  nsAutoString msg;
  if (aDocument) {
    nsCOMPtr<nsIURI> uri = aDocument->GetDocumentURI();
    if (uri) {
      msg.Append(NS_ConvertUTF8toUTF16(uri->GetSpecOrDefault()));
      msg.AppendLiteral(" : ");
    }
  }
  msg.AppendLiteral("Unable to run script because scripts are blocked internally.");

  LogSimpleConsoleError(msg, "DOM");
}

/* static */
void
nsContentUtils::AddScriptRunner(already_AddRefed<nsIRunnable> aRunnable)
{
  nsCOMPtr<nsIRunnable> runnable = aRunnable;
  if (!runnable) {
    return;
  }

  if (sScriptBlockerCount) {
    sBlockedScriptRunners->AppendElement(runnable.forget());
    return;
  }

  runnable->Run();
}

/* static */
void
nsContentUtils::AddScriptRunner(nsIRunnable* aRunnable) {
  nsCOMPtr<nsIRunnable> runnable = aRunnable;
  AddScriptRunner(runnable.forget());
}

/* static */
void
nsContentUtils::RunInStableState(already_AddRefed<nsIRunnable> aRunnable)
{
  MOZ_ASSERT(CycleCollectedJSContext::Get(), "Must be on a script thread!");
  CycleCollectedJSContext::Get()->RunInStableState(Move(aRunnable));
}

/* static */
void
nsContentUtils::RunInMetastableState(already_AddRefed<nsIRunnable> aRunnable)
{
  MOZ_ASSERT(CycleCollectedJSContext::Get(), "Must be on a script thread!");
  CycleCollectedJSContext::Get()->RunInMetastableState(Move(aRunnable));
}

/* static */
bool
nsContentUtils::IsInStableOrMetaStableState()
{
  MOZ_ASSERT(CycleCollectedJSContext::Get(), "Must be on a script thread!");
  return CycleCollectedJSContext::Get()->IsInStableOrMetaStableState();
}

/*
 * Helper function for nsContentUtils::ProcessViewportInfo.
 *
 * Handles a single key=value pair. If it corresponds to a valid viewport
 * attribute, add it to the document header data. No validation is done on the
 * value itself (this is done at display time).
 */
static void ProcessViewportToken(nsIDocument *aDocument,
                                 const nsAString &token) {

  /* Iterators. */
  nsAString::const_iterator tip, tail, end;
  token.BeginReading(tip);
  tail = tip;
  token.EndReading(end);

  /* Move tip to the '='. */
  while ((tip != end) && (*tip != '='))
    ++tip;

  /* If we didn't find an '=', punt. */
  if (tip == end)
    return;

  /* Extract the key and value. */
  const nsAString &key =
    nsContentUtils::TrimWhitespace<nsCRT::IsAsciiSpace>(Substring(tail, tip),
                                                        true);
  const nsAString &value =
    nsContentUtils::TrimWhitespace<nsCRT::IsAsciiSpace>(Substring(++tip, end),
                                                        true);

  /* Check for known keys. If we find a match, insert the appropriate
   * information into the document header. */
  RefPtr<nsAtom> key_atom = NS_Atomize(key);
  if (key_atom == nsGkAtoms::height)
    aDocument->SetHeaderData(nsGkAtoms::viewport_height, value);
  else if (key_atom == nsGkAtoms::width)
    aDocument->SetHeaderData(nsGkAtoms::viewport_width, value);
  else if (key_atom == nsGkAtoms::initial_scale)
    aDocument->SetHeaderData(nsGkAtoms::viewport_initial_scale, value);
  else if (key_atom == nsGkAtoms::minimum_scale)
    aDocument->SetHeaderData(nsGkAtoms::viewport_minimum_scale, value);
  else if (key_atom == nsGkAtoms::maximum_scale)
    aDocument->SetHeaderData(nsGkAtoms::viewport_maximum_scale, value);
  else if (key_atom == nsGkAtoms::user_scalable)
    aDocument->SetHeaderData(nsGkAtoms::viewport_user_scalable, value);
}

#define IS_SEPARATOR(c) ((c == '=') || (c == ',') || (c == ';') || \
                         (c == '\t') || (c == '\n') || (c == '\r'))

/* static */
nsresult
nsContentUtils::ProcessViewportInfo(nsIDocument *aDocument,
                                    const nsAString &viewportInfo) {

  /* We never fail. */
  nsresult rv = NS_OK;

  aDocument->SetHeaderData(nsGkAtoms::viewport, viewportInfo);

  /* Iterators. */
  nsAString::const_iterator tip, tail, end;
  viewportInfo.BeginReading(tip);
  tail = tip;
  viewportInfo.EndReading(end);

  /* Read the tip to the first non-separator character. */
  while ((tip != end) && (IS_SEPARATOR(*tip) || nsCRT::IsAsciiSpace(*tip)))
    ++tip;

  /* Read through and find tokens separated by separators. */
  while (tip != end) {

    /* Synchronize tip and tail. */
    tail = tip;

    /* Advance tip past non-separator characters. */
    while ((tip != end) && !IS_SEPARATOR(*tip))
      ++tip;

    /* Allow white spaces that surround the '=' character */
    if ((tip != end) && (*tip == '=')) {
      ++tip;

      while ((tip != end) && nsCRT::IsAsciiSpace(*tip))
        ++tip;

      while ((tip != end) && !(IS_SEPARATOR(*tip) || nsCRT::IsAsciiSpace(*tip)))
        ++tip;
    }

    /* Our token consists of the characters between tail and tip. */
    ProcessViewportToken(aDocument, Substring(tail, tip));

    /* Skip separators. */
    while ((tip != end) && (IS_SEPARATOR(*tip) || nsCRT::IsAsciiSpace(*tip)))
      ++tip;
  }

  return rv;

}

#undef IS_SEPARATOR

/* static */
void
nsContentUtils::HidePopupsInDocument(nsIDocument* aDocument)
{
#ifdef MOZ_XUL
  nsXULPopupManager* pm = nsXULPopupManager::GetInstance();
  if (pm && aDocument) {
    nsCOMPtr<nsIDocShellTreeItem> docShellToHide = aDocument->GetDocShell();
    if (docShellToHide)
      pm->HidePopupsInDocShell(docShellToHide);
  }
#endif
}

/* static */
already_AddRefed<nsIDragSession>
nsContentUtils::GetDragSession()
{
  nsCOMPtr<nsIDragSession> dragSession;
  nsCOMPtr<nsIDragService> dragService =
    do_GetService("@mozilla.org/widget/dragservice;1");
  if (dragService)
    dragService->GetCurrentSession(getter_AddRefs(dragSession));
  return dragSession.forget();
}

/* static */
nsresult
nsContentUtils::SetDataTransferInEvent(WidgetDragEvent* aDragEvent)
{
  if (aDragEvent->mDataTransfer || !aDragEvent->IsTrusted()) {
    return NS_OK;
  }

  // For dragstart events, the data transfer object is
  // created before the event fires, so it should already be set. For other
  // drag events, get the object from the drag session.
  NS_ASSERTION(aDragEvent->mMessage != eDragStart,
               "draggesture event created without a dataTransfer");

  nsCOMPtr<nsIDragSession> dragSession = GetDragSession();
  NS_ENSURE_TRUE(dragSession, NS_OK); // no drag in progress

  nsCOMPtr<nsIDOMDataTransfer> dataTransfer;
  nsCOMPtr<DataTransfer> initialDataTransfer;
  dragSession->GetDataTransfer(getter_AddRefs(dataTransfer));
  if (dataTransfer) {
    initialDataTransfer = do_QueryInterface(dataTransfer);
    if (!initialDataTransfer) {
      return NS_ERROR_FAILURE;
    }
  } else {
    // A dataTransfer won't exist when a drag was started by some other
    // means, for instance calling the drag service directly, or a drag
    // from another application. In either case, a new dataTransfer should
    // be created that reflects the data.
    initialDataTransfer =
      new DataTransfer(aDragEvent->mTarget, aDragEvent->mMessage, true, -1);

    // now set it in the drag session so we don't need to create it again
    dragSession->SetDataTransfer(initialDataTransfer);
  }

  bool isCrossDomainSubFrameDrop = false;
  if (aDragEvent->mMessage == eDrop) {
    isCrossDomainSubFrameDrop = CheckForSubFrameDrop(dragSession, aDragEvent);
  }

  // each event should use a clone of the original dataTransfer.
  initialDataTransfer->Clone(aDragEvent->mTarget, aDragEvent->mMessage,
                             aDragEvent->mUserCancelled,
                             isCrossDomainSubFrameDrop,
                             getter_AddRefs(aDragEvent->mDataTransfer));
  if (NS_WARN_IF(!aDragEvent->mDataTransfer)) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  // for the dragenter and dragover events, initialize the drop effect
  // from the drop action, which platform specific widget code sets before
  // the event is fired based on the keyboard state.
  if (aDragEvent->mMessage == eDragEnter || aDragEvent->mMessage == eDragOver) {
    uint32_t action, effectAllowed;
    dragSession->GetDragAction(&action);
    aDragEvent->mDataTransfer->GetEffectAllowedInt(&effectAllowed);
    aDragEvent->mDataTransfer->SetDropEffectInt(
                                 FilterDropEffect(action, effectAllowed));
  }
  else if (aDragEvent->mMessage == eDrop ||
           aDragEvent->mMessage == eDragEnd) {
    // For the drop and dragend events, set the drop effect based on the
    // last value that the dropEffect had. This will have been set in
    // EventStateManager::PostHandleEvent for the last dragenter or
    // dragover event.
    uint32_t dropEffect;
    initialDataTransfer->GetDropEffectInt(&dropEffect);
    aDragEvent->mDataTransfer->SetDropEffectInt(dropEffect);
  }

  return NS_OK;
}

/* static */
uint32_t
nsContentUtils::FilterDropEffect(uint32_t aAction, uint32_t aEffectAllowed)
{
  // It is possible for the drag action to include more than one action, but
  // the widget code which sets the action from the keyboard state should only
  // be including one. If multiple actions were set, we just consider them in
  //  the following order:
  //   copy, link, move
  if (aAction & nsIDragService::DRAGDROP_ACTION_COPY)
    aAction = nsIDragService::DRAGDROP_ACTION_COPY;
  else if (aAction & nsIDragService::DRAGDROP_ACTION_LINK)
    aAction = nsIDragService::DRAGDROP_ACTION_LINK;
  else if (aAction & nsIDragService::DRAGDROP_ACTION_MOVE)
    aAction = nsIDragService::DRAGDROP_ACTION_MOVE;

  // Filter the action based on the effectAllowed. If the effectAllowed
  // doesn't include the action, then that action cannot be done, so adjust
  // the action to something that is allowed. For a copy, adjust to move or
  // link. For a move, adjust to copy or link. For a link, adjust to move or
  // link. Otherwise, use none.
  if (aAction & aEffectAllowed ||
      aEffectAllowed == nsIDragService::DRAGDROP_ACTION_UNINITIALIZED)
    return aAction;
  if (aEffectAllowed & nsIDragService::DRAGDROP_ACTION_MOVE)
    return nsIDragService::DRAGDROP_ACTION_MOVE;
  if (aEffectAllowed & nsIDragService::DRAGDROP_ACTION_COPY)
    return nsIDragService::DRAGDROP_ACTION_COPY;
  if (aEffectAllowed & nsIDragService::DRAGDROP_ACTION_LINK)
    return nsIDragService::DRAGDROP_ACTION_LINK;
  return nsIDragService::DRAGDROP_ACTION_NONE;
}

/* static */
bool
nsContentUtils::CheckForSubFrameDrop(nsIDragSession* aDragSession,
                                     WidgetDragEvent* aDropEvent)
{
  nsCOMPtr<nsIContent> target = do_QueryInterface(aDropEvent->mOriginalTarget);
  if (!target) {
    return true;
  }

  nsIDocument* targetDoc = target->OwnerDoc();
  nsPIDOMWindowOuter* targetWin = targetDoc->GetWindow();
  if (!targetWin) {
    return true;
  }

  nsCOMPtr<nsIDocShellTreeItem> tdsti = targetWin->GetDocShell();
  if (!tdsti) {
    return true;
  }

  // Always allow dropping onto chrome shells.
  if (tdsti->ItemType() == nsIDocShellTreeItem::typeChrome) {
    return false;
  }

  // If there is no source node, then this is a drag from another
  // application, which should be allowed.
  nsCOMPtr<nsIDOMDocument> sourceDocument;
  aDragSession->GetSourceDocument(getter_AddRefs(sourceDocument));
  nsCOMPtr<nsIDocument> doc = do_QueryInterface(sourceDocument);
  if (doc) {
    // Get each successive parent of the source document and compare it to
    // the drop document. If they match, then this is a drag from a child frame.
    do {
      doc = doc->GetParentDocument();
      if (doc == targetDoc) {
        // The drag is from a child frame.
        return true;
      }
    } while (doc);
  }

  return false;
}

/* static */
bool
nsContentUtils::URIIsLocalFile(nsIURI *aURI)
{
  bool isFile;
  nsCOMPtr<nsINetUtil> util = do_QueryInterface(sIOService);

  // Important: we do NOT test the entire URI chain here!
  return util && NS_SUCCEEDED(util->ProtocolHasFlags(aURI,
                                nsIProtocolHandler::URI_IS_LOCAL_FILE,
                                &isFile)) &&
         isFile;
}

/* static */
nsIScriptContext*
nsContentUtils::GetContextForEventHandlers(nsINode* aNode,
                                           nsresult* aRv)
{
  *aRv = NS_OK;
  bool hasHadScriptObject = true;
  nsIScriptGlobalObject* sgo =
    aNode->OwnerDoc()->GetScriptHandlingObject(hasHadScriptObject);
  // It is bad if the document doesn't have event handling context,
  // but it used to have one.
  if (!sgo && hasHadScriptObject) {
    *aRv = NS_ERROR_UNEXPECTED;
    return nullptr;
  }

  if (sgo) {
    nsIScriptContext* scx = sgo->GetContext();
    // Bad, no context from script global object!
    if (!scx) {
      *aRv = NS_ERROR_UNEXPECTED;
      return nullptr;
    }
    return scx;
  }

  return nullptr;
}

/* static */
JSContext *
nsContentUtils::GetCurrentJSContext()
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(IsInitialized());
  if (!IsJSAPIActive()) {
    return nullptr;
  }
  return danger::GetJSContext();
}

/* static */
JSContext *
nsContentUtils::GetCurrentJSContextForThread()
{
  MOZ_ASSERT(IsInitialized());
  if (MOZ_LIKELY(NS_IsMainThread())) {
    return GetCurrentJSContext();
  }
  return workers::GetCurrentThreadJSContext();
}

template<typename StringType, typename CharType>
void
_ASCIIToLowerInSitu(StringType& aStr)
{
  CharType* iter = aStr.BeginWriting();
  CharType* end = aStr.EndWriting();
  MOZ_ASSERT(iter && end);

  while (iter != end) {
    CharType c = *iter;
    if (c >= 'A' && c <= 'Z') {
      *iter = c + ('a' - 'A');
    }
    ++iter;
  }
}

/* static */
void
nsContentUtils::ASCIIToLower(nsAString& aStr)
{
  return _ASCIIToLowerInSitu<nsAString, char16_t>(aStr);
}

/* static */
void
nsContentUtils::ASCIIToLower(nsACString& aStr)
{
  return _ASCIIToLowerInSitu<nsACString, char>(aStr);
}

template<typename StringType, typename CharType>
void
_ASCIIToLowerCopy(const StringType& aSource, StringType& aDest)
{
  uint32_t len = aSource.Length();
  aDest.SetLength(len);
  MOZ_ASSERT(aDest.Length() == len);

  CharType* dest = aDest.BeginWriting();
  MOZ_ASSERT(dest);

  const CharType* iter = aSource.BeginReading();
  const CharType* end = aSource.EndReading();
  while (iter != end) {
    CharType c = *iter;
    *dest = (c >= 'A' && c <= 'Z') ?
       c + ('a' - 'A') : c;
    ++iter;
    ++dest;
  }
}

/* static */
void
nsContentUtils::ASCIIToLower(const nsAString& aSource, nsAString& aDest) {
  return _ASCIIToLowerCopy<nsAString, char16_t>(aSource, aDest);
}

/* static */
void
nsContentUtils::ASCIIToLower(const nsACString& aSource, nsACString& aDest) {
  return _ASCIIToLowerCopy<nsACString, char>(aSource, aDest);
}


template<typename StringType, typename CharType>
void
_ASCIIToUpperInSitu(StringType& aStr)
{
  CharType* iter = aStr.BeginWriting();
  CharType* end = aStr.EndWriting();
  MOZ_ASSERT(iter && end);

  while (iter != end) {
    CharType c = *iter;
    if (c >= 'a' && c <= 'z') {
      *iter = c + ('A' - 'a');
    }
    ++iter;
  }
}

/* static */
void
nsContentUtils::ASCIIToUpper(nsAString& aStr)
{
  return _ASCIIToUpperInSitu<nsAString, char16_t>(aStr);
}

/* static */
void
nsContentUtils::ASCIIToUpper(nsACString& aStr)
{
  return _ASCIIToUpperInSitu<nsACString, char>(aStr);
}

template<typename StringType, typename CharType>
void
_ASCIIToUpperCopy(const StringType& aSource, StringType& aDest)
{
  uint32_t len = aSource.Length();
  aDest.SetLength(len);
  MOZ_ASSERT(aDest.Length() == len);

  CharType* dest = aDest.BeginWriting();
  MOZ_ASSERT(dest);

  const CharType* iter = aSource.BeginReading();
  const CharType* end = aSource.EndReading();
  while (iter != end) {
    CharType c = *iter;
    *dest = (c >= 'a' && c <= 'z') ?
      c + ('A' - 'a') : c;
    ++iter;
    ++dest;
  }
}

/* static */
void
nsContentUtils::ASCIIToUpper(const nsAString& aSource, nsAString& aDest)
{
  return _ASCIIToUpperCopy<nsAString, char16_t>(aSource, aDest);
}

/* static */
void
nsContentUtils::ASCIIToUpper(const nsACString& aSource, nsACString& aDest)
{
  return _ASCIIToUpperCopy<nsACString, char>(aSource, aDest);
}

/* static */
bool
nsContentUtils::EqualsIgnoreASCIICase(const nsAString& aStr1,
                                      const nsAString& aStr2)
{
  uint32_t len = aStr1.Length();
  if (len != aStr2.Length()) {
    return false;
  }

  const char16_t* str1 = aStr1.BeginReading();
  const char16_t* str2 = aStr2.BeginReading();
  const char16_t* end = str1 + len;

  while (str1 < end) {
    char16_t c1 = *str1++;
    char16_t c2 = *str2++;

    // First check if any bits other than the 0x0020 differs
    if ((c1 ^ c2) & 0xffdf) {
      return false;
    }

    // We know they can only differ in the 0x0020 bit.
    // Likely the two chars are the same, so check that first
    if (c1 != c2) {
      // They do differ, but since it's only in the 0x0020 bit, check if it's
      // the same ascii char, but just differing in case
      char16_t c1Upper = c1 & 0xffdf;
      if (!('A' <= c1Upper && c1Upper <= 'Z')) {
        return false;
      }
    }
  }

  return true;
}

/* static */
bool
nsContentUtils::StringContainsASCIIUpper(const nsAString& aStr)
{
  const char16_t* iter = aStr.BeginReading();
  const char16_t* end = aStr.EndReading();
  while (iter != end) {
    char16_t c = *iter;
    if (c >= 'A' && c <= 'Z') {
      return true;
    }
    ++iter;
  }

  return false;
}

/* static */
nsIInterfaceRequestor*
nsContentUtils::SameOriginChecker()
{
  if (!sSameOriginChecker) {
    sSameOriginChecker = new SameOriginCheckerImpl();
    NS_ADDREF(sSameOriginChecker);
  }
  return sSameOriginChecker;
}

/* static */
nsresult
nsContentUtils::CheckSameOrigin(nsIChannel *aOldChannel, nsIChannel *aNewChannel)
{
  if (!nsContentUtils::GetSecurityManager())
    return NS_ERROR_NOT_AVAILABLE;

  nsCOMPtr<nsIPrincipal> oldPrincipal;
  nsContentUtils::GetSecurityManager()->
    GetChannelResultPrincipal(aOldChannel, getter_AddRefs(oldPrincipal));

  nsCOMPtr<nsIURI> newURI;
  aNewChannel->GetURI(getter_AddRefs(newURI));
  nsCOMPtr<nsIURI> newOriginalURI;
  aNewChannel->GetOriginalURI(getter_AddRefs(newOriginalURI));

  NS_ENSURE_STATE(oldPrincipal && newURI && newOriginalURI);

  nsresult rv = oldPrincipal->CheckMayLoad(newURI, false, false);
  if (NS_SUCCEEDED(rv) && newOriginalURI != newURI) {
    rv = oldPrincipal->CheckMayLoad(newOriginalURI, false, false);
  }

  return rv;
}

NS_IMPL_ISUPPORTS(SameOriginCheckerImpl,
                  nsIChannelEventSink,
                  nsIInterfaceRequestor)

NS_IMETHODIMP
SameOriginCheckerImpl::AsyncOnChannelRedirect(nsIChannel* aOldChannel,
                                              nsIChannel* aNewChannel,
                                              uint32_t aFlags,
                                              nsIAsyncVerifyRedirectCallback* cb)
{
  NS_PRECONDITION(aNewChannel, "Redirecting to null channel?");

  nsresult rv = nsContentUtils::CheckSameOrigin(aOldChannel, aNewChannel);
  if (NS_SUCCEEDED(rv)) {
    cb->OnRedirectVerifyCallback(NS_OK);
  }

  return rv;
}

NS_IMETHODIMP
SameOriginCheckerImpl::GetInterface(const nsIID& aIID, void** aResult)
{
  return QueryInterface(aIID, aResult);
}

/* static */
nsresult
nsContentUtils::GetASCIIOrigin(nsIPrincipal* aPrincipal, nsACString& aOrigin)
{
  NS_PRECONDITION(aPrincipal, "missing principal");

  aOrigin.Truncate();

  nsCOMPtr<nsIURI> uri;
  nsresult rv = aPrincipal->GetURI(getter_AddRefs(uri));
  NS_ENSURE_SUCCESS(rv, rv);

  if (uri) {
    return GetASCIIOrigin(uri, aOrigin);
  }

  aOrigin.AssignLiteral("null");

  return NS_OK;
}

/* static */
nsresult
nsContentUtils::GetASCIIOrigin(nsIURI* aURI, nsACString& aOrigin)
{
  NS_PRECONDITION(aURI, "missing uri");

  // For Blob URI we have to return the origin of page using its principal.
  nsCOMPtr<nsIURIWithPrincipal> uriWithPrincipal = do_QueryInterface(aURI);
  if (uriWithPrincipal) {
    nsCOMPtr<nsIPrincipal> principal;
    uriWithPrincipal->GetPrincipal(getter_AddRefs(principal));

    if (principal) {
      nsCOMPtr<nsIURI> uri;
      nsresult rv = principal->GetURI(getter_AddRefs(uri));
      NS_ENSURE_SUCCESS(rv, rv);

      if (uri && uri != aURI) {
        return GetASCIIOrigin(uri, aOrigin);
      }
    }
  }

  aOrigin.Truncate();

  nsCOMPtr<nsIURI> uri = NS_GetInnermostURI(aURI);
  NS_ENSURE_TRUE(uri, NS_ERROR_UNEXPECTED);

  nsCString host;
  nsresult rv = uri->GetAsciiHost(host);

  if (NS_SUCCEEDED(rv) && !host.IsEmpty()) {
    nsCString scheme;
    rv = uri->GetScheme(scheme);
    NS_ENSURE_SUCCESS(rv, rv);

    int32_t port = -1;
    uri->GetPort(&port);
    if (port != -1 && port == NS_GetDefaultPort(scheme.get()))
      port = -1;

    nsCString hostPort;
    rv = NS_GenerateHostPort(host, port, hostPort);
    NS_ENSURE_SUCCESS(rv, rv);

    aOrigin = scheme + NS_LITERAL_CSTRING("://") + hostPort;
  }
  else {
    aOrigin.AssignLiteral("null");
  }

  return NS_OK;
}

/* static */
nsresult
nsContentUtils::GetUTFOrigin(nsIPrincipal* aPrincipal, nsAString& aOrigin)
{
  NS_PRECONDITION(aPrincipal, "missing principal");

  aOrigin.Truncate();

  nsCOMPtr<nsIURI> uri;
  nsresult rv = aPrincipal->GetURI(getter_AddRefs(uri));
  NS_ENSURE_SUCCESS(rv, rv);

  if (uri) {
    return GetUTFOrigin(uri, aOrigin);
  }

  aOrigin.AssignLiteral("null");

  return NS_OK;
}

/* static */
nsresult
nsContentUtils::GetUTFOrigin(nsIURI* aURI, nsAString& aOrigin)
{
  NS_PRECONDITION(aURI, "missing uri");

  // For Blob URI we have to return the origin of page using its principal.
  nsCOMPtr<nsIURIWithPrincipal> uriWithPrincipal = do_QueryInterface(aURI);
  if (uriWithPrincipal) {
    nsCOMPtr<nsIPrincipal> principal;
    uriWithPrincipal->GetPrincipal(getter_AddRefs(principal));

    if (principal) {
      nsCOMPtr<nsIURI> uri;
      nsresult rv = principal->GetURI(getter_AddRefs(uri));
      NS_ENSURE_SUCCESS(rv, rv);

      if (uri && uri != aURI) {
        return GetUTFOrigin(uri, aOrigin);
      }
    } else {
      // We are probably dealing with an unknown blob URL.
      bool isBlobURL = false;
      nsresult rv = aURI->SchemeIs(BLOBURI_SCHEME, &isBlobURL);
      NS_ENSURE_SUCCESS(rv, rv);

      if (isBlobURL) {
        nsAutoCString path;
        rv = aURI->GetPathQueryRef(path);
        NS_ENSURE_SUCCESS(rv, rv);

        nsCOMPtr<nsIURI> uri;
        nsresult rv = NS_NewURI(getter_AddRefs(uri), path);
        if (NS_FAILED(rv)) {
          aOrigin.AssignLiteral("null");
          return NS_OK;
        }

        return GetUTFOrigin(uri, aOrigin);
      }
    }
  }

  aOrigin.Truncate();

  nsCOMPtr<nsIURI> uri = NS_GetInnermostURI(aURI);
  NS_ENSURE_TRUE(uri, NS_ERROR_UNEXPECTED);

  nsCString host;
  nsresult rv = uri->GetHost(host);

  if (NS_SUCCEEDED(rv) && !host.IsEmpty()) {
    nsCString scheme;
    rv = uri->GetScheme(scheme);
    NS_ENSURE_SUCCESS(rv, rv);

    int32_t port = -1;
    uri->GetPort(&port);
    if (port != -1 && port == NS_GetDefaultPort(scheme.get()))
      port = -1;

    nsCString hostPort;
    rv = NS_GenerateHostPort(host, port, hostPort);
    NS_ENSURE_SUCCESS(rv, rv);

    aOrigin = NS_ConvertUTF8toUTF16(
      scheme + NS_LITERAL_CSTRING("://") + hostPort);
  }
  else {
    aOrigin.AssignLiteral("null");
  }

  return NS_OK;
}

/* static */
bool
nsContentUtils::CheckMayLoad(nsIPrincipal* aPrincipal, nsIChannel* aChannel, bool aAllowIfInheritsPrincipal)
{
  nsCOMPtr<nsIURI> channelURI;
  nsresult rv = NS_GetFinalChannelURI(aChannel, getter_AddRefs(channelURI));
  NS_ENSURE_SUCCESS(rv, false);

  return NS_SUCCEEDED(aPrincipal->CheckMayLoad(channelURI, false, aAllowIfInheritsPrincipal));
}

nsContentTypeParser::nsContentTypeParser(const nsAString& aString)
  : mString(aString), mService(nullptr)
{
  CallGetService("@mozilla.org/network/mime-hdrparam;1", &mService);
}

nsContentTypeParser::~nsContentTypeParser()
{
  NS_IF_RELEASE(mService);
}

nsresult
nsContentTypeParser::GetParameter(const char* aParameterName,
                                  nsAString& aResult) const
{
  NS_ENSURE_TRUE(mService, NS_ERROR_FAILURE);
  return mService->GetParameterHTTP(mString, aParameterName,
                                    EmptyCString(), false, nullptr,
                                    aResult);
}

nsresult
nsContentTypeParser::GetType(nsAString& aResult) const
{
  nsresult rv = GetParameter(nullptr, aResult);
  if (NS_FAILED(rv)) {
    return rv;
  }
  nsContentUtils::ASCIIToLower(aResult);
  return NS_OK;
}

/* static */

bool
nsContentUtils::CanAccessNativeAnon()
{
  return LegacyIsCallerChromeOrNativeCode() || IsCallerContentXBL();
}

/* static */ nsresult
nsContentUtils::DispatchXULCommand(nsIContent* aTarget,
                                   bool aTrusted,
                                   nsIDOMEvent* aSourceEvent,
                                   nsIPresShell* aShell,
                                   bool aCtrl,
                                   bool aAlt,
                                   bool aShift,
                                   bool aMeta,
                                   uint16_t aInputSource)
{
  NS_ENSURE_STATE(aTarget);
  nsIDocument* doc = aTarget->OwnerDoc();
  nsIPresShell* shell = doc->GetShell();
  nsPresContext* presContext = nullptr;
  if (shell) {
    presContext = shell->GetPresContext();
  }
  RefPtr<XULCommandEvent> xulCommand = new XULCommandEvent(doc, presContext,
                                                           nullptr);
  xulCommand->InitCommandEvent(NS_LITERAL_STRING("command"), true, true,
                               doc->GetInnerWindow(), 0, aCtrl, aAlt, aShift,
                               aMeta, aSourceEvent, aInputSource);

  if (aShell) {
    nsEventStatus status = nsEventStatus_eIgnore;
    nsCOMPtr<nsIPresShell> kungFuDeathGrip = aShell;
    return aShell->HandleDOMEventWithTarget(aTarget, xulCommand, &status);
  }

  nsCOMPtr<EventTarget> target = do_QueryInterface(aTarget);
  NS_ENSURE_STATE(target);
  bool dummy;
  return target->DispatchEvent(xulCommand, &dummy);
}

// static
nsresult
nsContentUtils::WrapNative(JSContext *cx, nsISupports *native,
                           nsWrapperCache *cache, const nsIID* aIID,
                           JS::MutableHandle<JS::Value> vp, bool aAllowWrapping)
{
  MOZ_ASSERT(cx == GetCurrentJSContext());

  if (!native) {
    vp.setNull();

    return NS_OK;
  }

  JSObject *wrapper = xpc_FastGetCachedWrapper(cx, cache, vp);
  if (wrapper) {
    return NS_OK;
  }

  NS_ENSURE_TRUE(sXPConnect, NS_ERROR_UNEXPECTED);

  if (!NS_IsMainThread()) {
    MOZ_CRASH();
  }

  nsresult rv = NS_OK;
  JS::Rooted<JSObject*> scope(cx, JS::CurrentGlobalOrNull(cx));
  rv = sXPConnect->WrapNativeToJSVal(cx, scope, native, cache, aIID,
                                     aAllowWrapping, vp);
  return rv;
}

nsresult
nsContentUtils::CreateArrayBuffer(JSContext *aCx, const nsACString& aData,
                                  JSObject** aResult)
{
  if (!aCx) {
    return NS_ERROR_FAILURE;
  }

  int32_t dataLen = aData.Length();
  *aResult = JS_NewArrayBuffer(aCx, dataLen);
  if (!*aResult) {
    return NS_ERROR_FAILURE;
  }

  if (dataLen > 0) {
    NS_ASSERTION(JS_IsArrayBufferObject(*aResult), "What happened?");
    JS::AutoCheckCannotGC nogc;
    bool isShared;
    memcpy(JS_GetArrayBufferData(*aResult, &isShared, nogc), aData.BeginReading(), dataLen);
    MOZ_ASSERT(!isShared);
  }

  return NS_OK;
}

void
nsContentUtils::StripNullChars(const nsAString& aInStr, nsAString& aOutStr)
{
  // In common cases where we don't have nulls in the
  // string we can simple simply bypass the checking code.
  int32_t firstNullPos = aInStr.FindChar('\0');
  if (firstNullPos == kNotFound) {
    aOutStr.Assign(aInStr);
    return;
  }

  aOutStr.SetCapacity(aInStr.Length() - 1);
  nsAString::const_iterator start, end;
  aInStr.BeginReading(start);
  aInStr.EndReading(end);
  while (start != end) {
    if (*start != '\0')
      aOutStr.Append(*start);
    ++start;
  }
}

struct ClassMatchingInfo {
  AtomArray mClasses;
  nsCaseTreatment mCaseTreatment;
};

// static
bool
nsContentUtils::MatchClassNames(Element* aElement, int32_t aNamespaceID,
                                nsAtom* aAtom, void* aData)
{
  // We can't match if there are no class names
  const nsAttrValue* classAttr = aElement->GetClasses();
  if (!classAttr) {
    return false;
  }

  // need to match *all* of the classes
  ClassMatchingInfo* info = static_cast<ClassMatchingInfo*>(aData);
  uint32_t length = info->mClasses.Length();
  if (!length) {
    // If we actually had no classes, don't match.
    return false;
  }
  uint32_t i;
  for (i = 0; i < length; ++i) {
    if (!classAttr->Contains(info->mClasses[i],
                             info->mCaseTreatment)) {
      return false;
    }
  }

  return true;
}

// static
void
nsContentUtils::DestroyClassNameArray(void* aData)
{
  ClassMatchingInfo* info = static_cast<ClassMatchingInfo*>(aData);
  delete info;
}

// static
void*
nsContentUtils::AllocClassMatchingInfo(nsINode* aRootNode,
                                       const nsString* aClasses)
{
  nsAttrValue attrValue;
  attrValue.ParseAtomArray(*aClasses);
  // nsAttrValue::Equals is sensitive to order, so we'll send an array
  auto* info = new ClassMatchingInfo;
  if (attrValue.Type() == nsAttrValue::eAtomArray) {
    info->mClasses.SwapElements(*(attrValue.GetAtomArrayValue()));
  } else if (attrValue.Type() == nsAttrValue::eAtom) {
    info->mClasses.AppendElement(attrValue.GetAtomValue());
  }

  info->mCaseTreatment =
    aRootNode->OwnerDoc()->GetCompatibilityMode() == eCompatibility_NavQuirks ?
    eIgnoreCase : eCaseMatters;
  return info;
}

// static
bool
nsContentUtils::IsFocusedContent(const nsIContent* aContent)
{
  nsFocusManager* fm = nsFocusManager::GetFocusManager();

  return fm && fm->GetFocusedContent() == aContent;
}

bool
nsContentUtils::IsSubDocumentTabbable(nsIContent* aContent)
{
  //XXXsmaug Shadow DOM spec issue!
  //         We may need to change this to GetComposedDoc().
  nsIDocument* doc = aContent->GetUncomposedDoc();
  if (!doc) {
    return false;
  }

  // If the subdocument lives in another process, the frame is
  // tabbable.
  if (EventStateManager::IsRemoteTarget(aContent)) {
    return true;
  }

  // XXXbz should this use OwnerDoc() for GetSubDocumentFor?
  // sXBL/XBL2 issue!
  nsIDocument* subDoc = doc->GetSubDocumentFor(aContent);
  if (!subDoc) {
    return false;
  }

  nsCOMPtr<nsIDocShell> docShell = subDoc->GetDocShell();
  if (!docShell) {
    return false;
  }

  nsCOMPtr<nsIContentViewer> contentViewer;
  docShell->GetContentViewer(getter_AddRefs(contentViewer));
  if (!contentViewer) {
    return false;
  }

  nsCOMPtr<nsIContentViewer> zombieViewer;
  contentViewer->GetPreviousViewer(getter_AddRefs(zombieViewer));

  // If there are 2 viewers for the current docshell, that
  // means the current document is a zombie document.
  // Only navigate into the subdocument if it's not a zombie.
  return !zombieViewer;
}

bool
nsContentUtils::IsUserFocusIgnored(nsINode* aNode)
{
  if (!nsGenericHTMLFrameElement::BrowserFramesEnabled()) {
    return false;
  }

  // Check if our mozbrowser iframe ancestors has ignoreuserfocus attribute.
  while (aNode) {
    nsCOMPtr<nsIMozBrowserFrame> browserFrame = do_QueryInterface(aNode);
    if (browserFrame &&
        aNode->AsElement()->HasAttr(kNameSpaceID_None, nsGkAtoms::ignoreuserfocus) &&
        browserFrame->GetReallyIsBrowser()) {
      return true;
    }
    nsPIDOMWindowOuter* win = aNode->OwnerDoc()->GetWindow();
    aNode = win ? win->GetFrameElementInternal() : nullptr;
  }

  return false;
}

bool
nsContentUtils::HasScrollgrab(nsIContent* aContent)
{
  // If we ever standardize this feature we'll want to hook this up properly
  // again. For now we're removing all the DOM-side code related to it but
  // leaving the layout and APZ handling for it in place.
  return false;
}

void
nsContentUtils::FlushLayoutForTree(nsPIDOMWindowOuter* aWindow)
{
  if (!aWindow) {
    return;
  }

  // Note that because FlushPendingNotifications flushes parents, this
  // is O(N^2) in docshell tree depth.  However, the docshell tree is
  // usually pretty shallow.

  if (nsCOMPtr<nsIDocument> doc = aWindow->GetDoc()) {
    doc->FlushPendingNotifications(FlushType::Layout);
  }

  if (nsCOMPtr<nsIDocShell> docShell = aWindow->GetDocShell()) {
    int32_t i = 0, i_end;
    docShell->GetChildCount(&i_end);
    for (; i < i_end; ++i) {
      nsCOMPtr<nsIDocShellTreeItem> item;
      if (docShell->GetChildAt(i, getter_AddRefs(item)) == NS_OK && item) {
        if (nsCOMPtr<nsPIDOMWindowOuter> win = item->GetWindow()) {
          FlushLayoutForTree(win);
        }
      }
    }
  }
}

void nsContentUtils::RemoveNewlines(nsString &aString)
{
  aString.StripCRLF();
}

void
nsContentUtils::PlatformToDOMLineBreaks(nsString &aString)
{
  if (!PlatformToDOMLineBreaks(aString, fallible)) {
    aString.AllocFailed(aString.Length());
  }
}

bool
nsContentUtils::PlatformToDOMLineBreaks(nsString& aString, const fallible_t& aFallible)
{
  if (aString.FindChar(char16_t('\r')) != -1) {
    // Windows linebreaks: Map CRLF to LF:
    if (!aString.ReplaceSubstring(u"\r\n", u"\n", aFallible)) {
      return false;
    }

    // Mac linebreaks: Map any remaining CR to LF:
    if (!aString.ReplaceSubstring(u"\r", u"\n", aFallible)) {
      return false;
    }
  }

  return true;
}

void
nsContentUtils::PopulateStringFromStringBuffer(nsStringBuffer* aBuf,
                                               nsAString& aResultString)
{
  MOZ_ASSERT(aBuf, "Expecting a non-null string buffer");

  uint32_t stringLen = NS_strlen(static_cast<char16_t*>(aBuf->Data()));

  // SANITY CHECK: In case the nsStringBuffer isn't correctly
  // null-terminated, let's clamp its length using the allocated size, to be
  // sure the resulting string doesn't sample past the end of the the buffer.
  // (Note that StorageSize() is in units of bytes, so we have to convert that
  // to units of PRUnichars, and subtract 1 for the null-terminator.)
  uint32_t allocStringLen = (aBuf->StorageSize() / sizeof(char16_t)) - 1;
  MOZ_ASSERT(stringLen <= allocStringLen,
             "string buffer lacks null terminator!");
  stringLen = std::min(stringLen, allocStringLen);

  aBuf->ToString(stringLen, aResultString);
}

nsIPresShell*
nsContentUtils::FindPresShellForDocument(const nsIDocument* aDoc)
{
  const nsIDocument* doc = aDoc;
  nsIDocument* displayDoc = doc->GetDisplayDocument();
  if (displayDoc) {
    doc = displayDoc;
  }

  nsIPresShell* shell = doc->GetShell();
  if (shell) {
    return shell;
  }

  nsCOMPtr<nsIDocShellTreeItem> docShellTreeItem = doc->GetDocShell();
  while (docShellTreeItem) {
    // We may be in a display:none subdocument, or we may not have a presshell
    // created yet.
    // Walk the docshell tree to find the nearest container that has a presshell,
    // and return that.
    nsCOMPtr<nsIDocShell> docShell = do_QueryInterface(docShellTreeItem);
    nsIPresShell* presShell = docShell->GetPresShell();
    if (presShell) {
      return presShell;
    }
    nsCOMPtr<nsIDocShellTreeItem> parent;
    docShellTreeItem->GetParent(getter_AddRefs(parent));
    docShellTreeItem = parent;
  }

  return nullptr;
}

nsIWidget*
nsContentUtils::WidgetForDocument(const nsIDocument* aDoc)
{
  nsIPresShell* shell = FindPresShellForDocument(aDoc);
  if (shell) {
    nsViewManager* VM = shell->GetViewManager();
    if (VM) {
      nsView* rootView = VM->GetRootView();
      if (rootView) {
        nsView* displayRoot = nsViewManager::GetDisplayRootFor(rootView);
        if (displayRoot) {
          return displayRoot->GetNearestWidget(nullptr);
        }
      }
    }
  }

  return nullptr;
}

nsIWidget*
nsContentUtils::WidgetForContent(const nsIContent* aContent)
{
  nsIFrame* frame = aContent->GetPrimaryFrame();
  if (frame) {
    frame = nsLayoutUtils::GetDisplayRootFrame(frame);

    nsView* view = frame->GetView();
    if (view) {
      return view->GetWidget();
    }
  }

  return nullptr;
}

already_AddRefed<LayerManager>
nsContentUtils::LayerManagerForContent(const nsIContent *aContent)
{
  nsIWidget* widget = nsContentUtils::WidgetForContent(aContent);
  if (widget) {
    RefPtr<LayerManager> manager = widget->GetLayerManager();
    return manager.forget();
  }

  return nullptr;
}

static already_AddRefed<LayerManager>
LayerManagerForDocumentInternal(const nsIDocument *aDoc, bool aRequirePersistent)
{
  nsIWidget *widget = nsContentUtils::WidgetForDocument(aDoc);
  if (widget) {
    RefPtr<LayerManager> manager =
      widget->GetLayerManager(aRequirePersistent ? nsIWidget::LAYER_MANAGER_PERSISTENT :
                              nsIWidget::LAYER_MANAGER_CURRENT);
    return manager.forget();
  }

  return nullptr;
}

already_AddRefed<LayerManager>
nsContentUtils::LayerManagerForDocument(const nsIDocument *aDoc)
{
  return LayerManagerForDocumentInternal(aDoc, false);
}

already_AddRefed<LayerManager>
nsContentUtils::PersistentLayerManagerForDocument(nsIDocument *aDoc)
{
  return LayerManagerForDocumentInternal(aDoc, true);
}

bool
nsContentUtils::AllowXULXBLForPrincipal(nsIPrincipal* aPrincipal)
{
  if (IsSystemPrincipal(aPrincipal)) {
    return true;
  }

  nsCOMPtr<nsIURI> princURI;
  aPrincipal->GetURI(getter_AddRefs(princURI));

  return princURI &&
         ((sAllowXULXBL_for_file && SchemeIs(princURI, "file")) ||
          IsSitePermAllow(aPrincipal, "allowXULXBL"));
}

bool
nsContentUtils::IsPDFJSEnabled()
{
   nsCOMPtr<nsIStreamConverterService> convServ =
     do_GetService("@mozilla.org/streamConverters;1");
   nsresult rv = NS_ERROR_FAILURE;
   bool canConvert = false;
   if (convServ) {
     rv = convServ->CanConvert("application/pdf", "text/html", &canConvert);
   }
   return NS_SUCCEEDED(rv) && canConvert;
}

already_AddRefed<nsIDocumentLoaderFactory>
nsContentUtils::FindInternalContentViewer(const nsACString& aType,
                                          ContentViewerType* aLoaderType)
{
  if (aLoaderType) {
    *aLoaderType = TYPE_UNSUPPORTED;
  }

  // one helper factory, please
  nsCOMPtr<nsICategoryManager> catMan(do_GetService(NS_CATEGORYMANAGER_CONTRACTID));
  if (!catMan)
    return nullptr;

  nsCOMPtr<nsIDocumentLoaderFactory> docFactory;

  nsCString contractID;
  nsresult rv = catMan->GetCategoryEntry("Gecko-Content-Viewers",
                                         PromiseFlatCString(aType).get(),
                                         getter_Copies(contractID));
  if (NS_SUCCEEDED(rv)) {
    docFactory = do_GetService(contractID.get());
    if (docFactory && aLoaderType) {
      if (contractID.EqualsLiteral(CONTENT_DLF_CONTRACTID))
        *aLoaderType = TYPE_CONTENT;
      else if (contractID.EqualsLiteral(PLUGIN_DLF_CONTRACTID))
        *aLoaderType = TYPE_PLUGIN;
      else
      *aLoaderType = TYPE_UNKNOWN;
    }
    return docFactory.forget();
  }

  if (DecoderTraits::IsSupportedInVideoDocument(aType)) {
    docFactory = do_GetService("@mozilla.org/content/document-loader-factory;1");
    if (docFactory && aLoaderType) {
      *aLoaderType = TYPE_CONTENT;
    }
    return docFactory.forget();
  }

  return nullptr;
}

static void
ReportPatternCompileFailure(nsAString& aPattern, nsIDocument* aDocument,
                            JSContext* cx)
{
    MOZ_ASSERT(JS_IsExceptionPending(cx));

    JS::RootedValue exn(cx);
    if (!JS_GetPendingException(cx, &exn)) {
      return;
    }
    if (!exn.isObject()) {
      // If pending exception is not an object, it should be OOM.
      return;
    }

    JS::AutoSaveExceptionState savedExc(cx);
    JS::RootedObject exnObj(cx, &exn.toObject());
    JS::RootedValue messageVal(cx);
    if (!JS_GetProperty(cx, exnObj, "message", &messageVal)) {
      return;
    }
    MOZ_ASSERT(messageVal.isString());

    JS::RootedString messageStr(cx, messageVal.toString());
    MOZ_ASSERT(messageStr);

    nsAutoString wideMessage;
    if (!AssignJSString(cx, wideMessage, messageStr)) {
        return;
    }

    const nsString& pattern = PromiseFlatString(aPattern);
    const char16_t *strings[] = { pattern.get(), wideMessage.get() };
    nsContentUtils::ReportToConsole(nsIScriptError::errorFlag,
                                    NS_LITERAL_CSTRING("DOM"),
                                    aDocument,
                                    nsContentUtils::eDOM_PROPERTIES,
                                    "PatternAttributeCompileFailure",
                                    strings, ArrayLength(strings));
    savedExc.drop();
}

// static
bool
nsContentUtils::IsPatternMatching(nsAString& aValue, nsAString& aPattern,
                                  nsIDocument* aDocument)
{
  NS_ASSERTION(aDocument, "aDocument should be a valid pointer (not null)");

  AutoJSContext cx;
  AutoDisableJSInterruptCallback disabler(cx);

  // We can use the junk scope here, because we're just using it for
  // regexp evaluation, not actual script execution.
  JSAutoCompartment ac(cx, xpc::UnprivilegedJunkScope());

  // The pattern has to match the entire value.
  aPattern.InsertLiteral(u"^(?:", 0);
  aPattern.AppendLiteral(")$");

  JS::Rooted<JSObject*> re(cx,
    JS_NewUCRegExpObject(cx,
                         static_cast<char16_t*>(aPattern.BeginWriting()),
                         aPattern.Length(), JSREG_UNICODE));
  if (!re) {
    // Remove extra patterns added above to report with the original pattern.
    aPattern.Cut(0, 4);
    aPattern.Cut(aPattern.Length() - 2, 2);
    ReportPatternCompileFailure(aPattern, aDocument, cx);
    return true;
  }

  JS::Rooted<JS::Value> rval(cx, JS::NullValue());
  size_t idx = 0;
  if (!JS_ExecuteRegExpNoStatics(cx, re,
                                 static_cast<char16_t*>(aValue.BeginWriting()),
                                 aValue.Length(), &idx, true, &rval)) {
    return true;
  }

  return !rval.isNull();
}

// static
nsresult
nsContentUtils::URIInheritsSecurityContext(nsIURI *aURI, bool *aResult)
{
  // Note: about:blank URIs do NOT inherit the security context from the
  // current document, which is what this function tests for...
  return NS_URIChainHasFlags(aURI,
                             nsIProtocolHandler::URI_INHERITS_SECURITY_CONTEXT,
                             aResult);
}

// static
bool
nsContentUtils::ChannelShouldInheritPrincipal(nsIPrincipal* aLoadingPrincipal,
                                              nsIURI* aURI,
                                              bool aInheritForAboutBlank,
                                              bool aForceInherit)
{
  MOZ_ASSERT(aLoadingPrincipal, "Can not check inheritance without a principal");

  // Only tell the channel to inherit if it can't provide its own security context.
  //
  // XXX: If this is ever changed, check all callers for what owners
  //      they're passing in.  In particular, see the code and
  //      comments in nsDocShell::LoadURI where we fall back on
  //      inheriting the owner if called from chrome.  That would be
  //      very wrong if this code changed anything but channels that
  //      can't provide their own security context!
  //
  // If aForceInherit is true, we will inherit, even for a channel that
  // can provide its own security context. This is used for srcdoc loads.
  bool inherit = aForceInherit;
  if (!inherit) {
    bool uriInherits;
    // We expect URIInheritsSecurityContext to return success for an
    // about:blank URI, so don't call NS_IsAboutBlank() if this call fails.
    // This condition needs to match the one in nsDocShell::InternalLoad where
    // we're checking for things that will use the owner.
    inherit =
      (NS_SUCCEEDED(URIInheritsSecurityContext(aURI, &uriInherits)) &&
       (uriInherits || (aInheritForAboutBlank && NS_IsAboutBlank(aURI)))) ||
      //
      // file: uri special-casing
      //
      // If this is a file: load opened from another file: then it may need
      // to inherit the owner from the referrer so they can script each other.
      // If we don't set the owner explicitly then each file: gets an owner
      // based on its own codebase later.
      //
      (URIIsLocalFile(aURI) &&
       NS_SUCCEEDED(aLoadingPrincipal->CheckMayLoad(aURI, false, false)) &&
       // One more check here.  CheckMayLoad will always return true for the
       // system principal, but we do NOT want to inherit in that case.
       !IsSystemPrincipal(aLoadingPrincipal));
  }
  return inherit;
}

/* static */
bool
nsContentUtils::IsFullScreenApiEnabled()
{
  return sIsFullScreenApiEnabled;
}

/* static */
bool
nsContentUtils::IsRequestFullScreenAllowed(CallerType aCallerType)
{
  // If more time has elapsed since the user input than is specified by the
  // dom.event.handling-user-input-time-limit pref (default 1 second), this
  // function also returns false.

  if (!sTrustedFullScreenOnly || aCallerType == CallerType::System) {
    return true;
  }

  if (EventStateManager::IsHandlingUserInput()) {
    TimeDuration timeout = HandlingUserInputTimeout();
    return timeout <= TimeDuration(nullptr) ||
      (TimeStamp::Now() -
       EventStateManager::GetHandlingInputStart()) <= timeout;
  }

  return false;
}

/* static */
bool
nsContentUtils::IsCutCopyAllowed(nsIPrincipal* aSubjectPrincipal)
{
  if (!IsCutCopyRestricted() && EventStateManager::IsHandlingUserInput()) {
    return true;
  }

  return PrincipalHasPermission(aSubjectPrincipal, nsGkAtoms::clipboardWrite);
}

/* static */
bool
nsContentUtils::IsFrameTimingEnabled()
{
  return sIsFrameTimingPrefEnabled;
}

/* static */
bool
nsContentUtils::HaveEqualPrincipals(nsIDocument* aDoc1, nsIDocument* aDoc2)
{
  if (!aDoc1 || !aDoc2) {
    return false;
  }
  bool principalsEqual = false;
  aDoc1->NodePrincipal()->Equals(aDoc2->NodePrincipal(), &principalsEqual);
  return principalsEqual;
}

/* static */
bool
nsContentUtils::HasPluginWithUncontrolledEventDispatch(nsIContent* aContent)
{
#ifdef XP_MACOSX
  // We control dispatch to all mac plugins.
  return false;
#else
  if (!aContent || !aContent->IsInUncomposedDoc()) {
    return false;
  }

  nsCOMPtr<nsIObjectLoadingContent> olc = do_QueryInterface(aContent);
  if (!olc) {
    return false;
  }

  RefPtr<nsNPAPIPluginInstance> plugin;
  olc->GetPluginInstance(getter_AddRefs(plugin));
  if (!plugin) {
    return false;
  }

  bool isWindowless = false;
  nsresult res = plugin->IsWindowless(&isWindowless);
  if (NS_FAILED(res)) {
    return false;
  }

  return !isWindowless;
#endif
}

/* static */
void
nsContentUtils::FireMutationEventsForDirectParsing(nsIDocument* aDoc,
                                                   nsIContent* aDest,
                                                   int32_t aOldChildCount)
{
  // Fire mutation events. Optimize for the case when there are no listeners
  int32_t newChildCount = aDest->GetChildCount();
  if (newChildCount && nsContentUtils::
        HasMutationListeners(aDoc, NS_EVENT_BITS_MUTATION_NODEINSERTED)) {
    AutoTArray<nsCOMPtr<nsIContent>, 50> childNodes;
    NS_ASSERTION(newChildCount - aOldChildCount >= 0,
                 "What, some unexpected dom mutation has happened?");
    childNodes.SetCapacity(newChildCount - aOldChildCount);
    for (nsIContent* child = aDest->GetFirstChild();
         child;
         child = child->GetNextSibling()) {
      childNodes.AppendElement(child);
    }
    FragmentOrElement::FireNodeInserted(aDoc, aDest, childNodes);
  }
}

/* static */
nsIDocument*
nsContentUtils::GetRootDocument(nsIDocument* aDoc)
{
  if (!aDoc) {
    return nullptr;
  }
  nsIDocument* doc = aDoc;
  while (doc->GetParentDocument()) {
    doc = doc->GetParentDocument();
  }
  return doc;
}

/* static */
bool
nsContentUtils::IsInPointerLockContext(nsPIDOMWindowOuter* aWin)
{
  if (!aWin) {
    return false;
  }

  nsCOMPtr<nsIDocument> pointerLockedDoc =
    do_QueryReferent(EventStateManager::sPointerLockedDoc);
  if (!pointerLockedDoc || !pointerLockedDoc->GetWindow()) {
    return false;
  }

  nsCOMPtr<nsPIDOMWindowOuter> lockTop =
    pointerLockedDoc->GetWindow()->GetScriptableTop();
  nsCOMPtr<nsPIDOMWindowOuter> top = aWin->GetScriptableTop();

  return top == lockTop;
}

// static
int32_t
nsContentUtils::GetAdjustedOffsetInTextControl(nsIFrame* aOffsetFrame,
                                               int32_t aOffset)
{
  // The structure of the anonymous frames within a text control frame is
  // an optional block frame, followed by an optional br frame.

  // If the offset frame has a child, then this frame is the block which
  // has the text frames (containing the content) as its children. This will
  // be the case if we click to the right of any of the text frames, or at the
  // bottom of the text area.
  nsIFrame* firstChild = aOffsetFrame->PrincipalChildList().FirstChild();
  if (firstChild) {
    // In this case, the passed-in offset is incorrect, and we want the length
    // of the entire content in the text control frame.
    return firstChild->GetContent()->Length();
  }

  if (aOffsetFrame->GetPrevSibling() &&
      !aOffsetFrame->GetNextSibling()) {
    // In this case, we're actually within the last frame, which is a br
    // frame. Our offset should therefore be the length of the first child of
    // our parent.
    int32_t aOutOffset =
      aOffsetFrame->GetParent()->PrincipalChildList().FirstChild()->GetContent()->Length();
    return aOutOffset;
  }

  // Otherwise, we're within one of the text frames, in which case our offset
  // has already been correctly calculated.
  return aOffset;
}

// static
void
nsContentUtils::GetSelectionInTextControl(Selection* aSelection,
                                          Element* aRoot,
                                          uint32_t& aOutStartOffset,
                                          uint32_t& aOutEndOffset)
{
  MOZ_ASSERT(aSelection && aRoot);

  // We don't care which end of this selection is anchor and which is focus.  In
  // fact, we explicitly want to know which is the _start_ and which is the
  // _end_, not anchor vs focus.
  const nsRange* range = aSelection->GetAnchorFocusRange();
  if (!range) {
    // Nothing selected
    aOutStartOffset = aOutEndOffset = 0;
    return;
  }

  // All the node pointers here are raw pointers for performance.  We shouldn't
  // be doing anything in this function that invalidates the node tree.
  nsINode* startContainer = range->GetStartContainer();
  uint32_t startOffset = range->StartOffset();
  nsINode* endContainer = range->GetEndContainer();
  uint32_t endOffset = range->EndOffset();

  // We have at most two children, consisting of an optional text node followed
  // by an optional <br>.
  NS_ASSERTION(aRoot->GetChildCount() <= 2, "Unexpected children");
  nsIContent* firstChild = aRoot->GetFirstChild();
#ifdef DEBUG
  nsCOMPtr<nsIContent> lastChild = aRoot->GetLastChild();
  NS_ASSERTION(startContainer == aRoot || startContainer == firstChild ||
               startContainer == lastChild, "Unexpected startContainer");
  NS_ASSERTION(endContainer == aRoot || endContainer == firstChild ||
               endContainer == lastChild, "Unexpected endContainer");
  // firstChild is either text or a <br> (hence an element).
  MOZ_ASSERT_IF(firstChild,
                firstChild->IsNodeOfType(nsINode::eTEXT) || firstChild->IsElement());
#endif
  // Testing IsElement() is faster than testing IsNodeOfType(), since it's
  // non-virtual.
  if (!firstChild || firstChild->IsElement()) {
    // No text node, so everything is 0
    startOffset = endOffset = 0;
  } else {
    // First child is text.  If the start/end is already in the text node,
    // or the start of the root node, no change needed.  If it's in the root
    // node but not the start, or in the trailing <br>, we need to set the
    // offset to the end.
    if ((startContainer == aRoot && startOffset != 0) ||
        (startContainer != aRoot && startContainer != firstChild)) {
      startOffset = firstChild->Length();
    }
    if ((endContainer == aRoot && endOffset != 0) ||
        (endContainer != aRoot && endContainer != firstChild)) {
      endOffset = firstChild->Length();
    }
  }

  MOZ_ASSERT(startOffset <= endOffset);
  aOutStartOffset = startOffset;
  aOutEndOffset = endOffset;
}


HTMLEditor*
nsContentUtils::GetHTMLEditor(nsPresContext* aPresContext)
{
  nsCOMPtr<nsIDocShell> docShell(aPresContext->GetDocShell());
  bool isEditable;
  if (!docShell ||
      NS_FAILED(docShell->GetEditable(&isEditable)) || !isEditable)
    return nullptr;

  return docShell->GetHTMLEditor();
}

bool
nsContentUtils::IsContentInsertionPoint(nsIContent* aContent)
{
  // Check if the content is a XBL insertion point.
  if (aContent->IsActiveChildrenElement()) {
    return true;
  }

  // Check if the content is a web components content insertion point.
  // XXX handle <slot>?
  return false;
}

// static
bool
nsContentUtils::HasDistributedChildren(nsIContent* aContent)
{
  if (!aContent || !nsDocument::IsWebComponentsEnabled(aContent)) {
    return false;
  }

  if (aContent->GetShadowRoot()) {
    // Children of a shadow root host are distributed
    // to content insertion points in the shadow root.
    return true;
  }

  HTMLSlotElement* slotEl = HTMLSlotElement::FromContent(aContent);
  if (slotEl && slotEl->GetContainingShadow()) {
    // Children of a slot are rendered if the slot does not have any assigned
    // nodes (fallback content).
    return slotEl->AssignedNodes().IsEmpty();
  }

  return false;
}

// static
bool
nsContentUtils::IsForbiddenRequestHeader(const nsACString& aHeader)
{
  if (IsForbiddenSystemRequestHeader(aHeader)) {
    return true;
  }

  return StringBeginsWith(aHeader, NS_LITERAL_CSTRING("proxy-"),
                          nsCaseInsensitiveCStringComparator()) ||
         StringBeginsWith(aHeader, NS_LITERAL_CSTRING("sec-"),
                          nsCaseInsensitiveCStringComparator());
}

// static
bool
nsContentUtils::IsForbiddenSystemRequestHeader(const nsACString& aHeader)
{
  static const char *kInvalidHeaders[] = {
    "accept-charset", "accept-encoding", "access-control-request-headers",
    "access-control-request-method", "connection", "content-length",
    "cookie", "cookie2", "date", "dnt", "expect", "host", "keep-alive",
    "origin", "referer", "te", "trailer", "transfer-encoding", "upgrade", "via"
  };
  for (auto& kInvalidHeader : kInvalidHeaders) {
    if (aHeader.LowerCaseEqualsASCII(kInvalidHeader)) {
      return true;
    }
  }
  return false;
}

// static
bool
nsContentUtils::IsForbiddenResponseHeader(const nsACString& aHeader)
{
  return (aHeader.LowerCaseEqualsASCII("set-cookie") ||
          aHeader.LowerCaseEqualsASCII("set-cookie2"));
}

// static
bool
nsContentUtils::IsAllowedNonCorsContentType(const nsACString& aHeaderValue)
{
  nsAutoCString contentType;
  nsAutoCString unused;

  nsresult rv = NS_ParseRequestContentType(aHeaderValue, contentType, unused);
  if (NS_FAILED(rv)) {
    return false;
  }

  return contentType.LowerCaseEqualsLiteral("text/plain") ||
         contentType.LowerCaseEqualsLiteral("application/x-www-form-urlencoded") ||
         contentType.LowerCaseEqualsLiteral("multipart/form-data");
}

bool
nsContentUtils::DOMWindowDumpEnabled()
{
#if !(defined(DEBUG) || defined(MOZ_ENABLE_JS_DUMP))
  // In optimized builds we check a pref that controls if we should
  // enable output from dump() or not, in debug builds it's always
  // enabled.
  return nsContentUtils::sDOMWindowDumpEnabled;
#else
  return true;
#endif
}

bool
nsContentUtils::DoNotTrackEnabled()
{
  return nsContentUtils::sDoNotTrackEnabled;
}

mozilla::LogModule*
nsContentUtils::DOMDumpLog()
{
  return sDOMDumpLog;
}

bool
nsContentUtils::GetNodeTextContent(nsINode* aNode, bool aDeep, nsAString& aResult,
                                   const fallible_t& aFallible)
{
  aResult.Truncate();
  return AppendNodeTextContent(aNode, aDeep, aResult, aFallible);
}

void
nsContentUtils::GetNodeTextContent(nsINode* aNode, bool aDeep, nsAString& aResult)
{
  if (!GetNodeTextContent(aNode, aDeep, aResult, fallible)) {
    NS_ABORT_OOM(0); // Unfortunately we don't know the allocation size
  }
}

void
nsContentUtils::DestroyMatchString(void* aData)
{
  if (aData) {
    nsString* matchString = static_cast<nsString*>(aData);
    delete matchString;
  }
}

bool
nsContentUtils::IsJavascriptMIMEType(const nsAString& aMIMEType)
{
  // Table ordered from most to least likely JS MIME types.
  static const char* jsTypes[] = {
    "text/javascript",
    "text/ecmascript",
    "application/javascript",
    "application/ecmascript",
    "application/x-javascript",
    "application/x-ecmascript",
    "text/javascript1.0",
    "text/javascript1.1",
    "text/javascript1.2",
    "text/javascript1.3",
    "text/javascript1.4",
    "text/javascript1.5",
    "text/jscript",
    "text/livescript",
    "text/x-ecmascript",
    "text/x-javascript",
    nullptr
  };

  for (uint32_t i = 0; jsTypes[i]; ++i) {
    if (aMIMEType.LowerCaseEqualsASCII(jsTypes[i])) {
      return true;
    }
  }

  return false;
}

nsresult
nsContentUtils::GenerateUUIDInPlace(nsID& aUUID)
{
  MOZ_ASSERT(sUUIDGenerator);

  nsresult rv = sUUIDGenerator->GenerateUUIDInPlace(&aUUID);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

bool
nsContentUtils::PrefetchPreloadEnabled(nsIDocShell* aDocShell)
{
  //
  // SECURITY CHECK: disable prefetching and preloading from mailnews!
  //
  // walk up the docshell tree to see if any containing
  // docshell are of type MAIL.
  //

  if (!aDocShell) {
    return false;
  }

  nsCOMPtr<nsIDocShell> docshell = aDocShell;
  nsCOMPtr<nsIDocShellTreeItem> parentItem;

  do {
    uint32_t appType = 0;
    nsresult rv = docshell->GetAppType(&appType);
    if (NS_FAILED(rv) || appType == nsIDocShell::APP_TYPE_MAIL) {
      return false; // do not prefetch, preload, preconnect from mailnews
    }

    docshell->GetParent(getter_AddRefs(parentItem));
    if (parentItem) {
      docshell = do_QueryInterface(parentItem);
      if (!docshell) {
        NS_ERROR("cannot get a docshell from a treeItem!");
        return false;
      }
    }
  } while (parentItem);

  return true;
}

uint64_t
nsContentUtils::GetInnerWindowID(nsIRequest* aRequest)
{
  // can't do anything if there's no nsIRequest!
  if (!aRequest) {
    return 0;
  }

  nsCOMPtr<nsILoadGroup> loadGroup;
  nsresult rv = aRequest->GetLoadGroup(getter_AddRefs(loadGroup));

  if (NS_FAILED(rv) || !loadGroup) {
    return 0;
  }

  return GetInnerWindowID(loadGroup);
}

uint64_t
nsContentUtils::GetInnerWindowID(nsILoadGroup* aLoadGroup)
{
  if (!aLoadGroup) {
    return 0;
  }

  nsCOMPtr<nsIInterfaceRequestor> callbacks;
  nsresult rv = aLoadGroup->GetNotificationCallbacks(getter_AddRefs(callbacks));
  if (NS_FAILED(rv) || !callbacks) {
    return 0;
  }

  nsCOMPtr<nsILoadContext> loadContext = do_GetInterface(callbacks);
  if (!loadContext) {
    return 0;
  }

  nsCOMPtr<mozIDOMWindowProxy> window;
  rv = loadContext->GetAssociatedWindow(getter_AddRefs(window));
  if (NS_FAILED(rv) || !window) {
    return 0;
  }

  auto* pwindow = nsPIDOMWindowOuter::From(window);
  if (!pwindow) {
    return 0;
  }

  nsPIDOMWindowInner* inner = pwindow->GetCurrentInnerWindow();
  return inner ? inner->WindowID() : 0;
}

nsresult
nsContentUtils::GetHostOrIPv6WithBrackets(nsIURI* aURI, nsCString& aHost)
{
  aHost.Truncate();
  nsresult rv = aURI->GetHost(aHost);
  if (NS_FAILED(rv)) { // Some URIs do not have a host
    return rv;
  }

  if (aHost.FindChar(':') != -1) { // Escape IPv6 address
    MOZ_ASSERT(!aHost.Length() ||
      (aHost[0] !='[' && aHost[aHost.Length() - 1] != ']'));
    aHost.Insert('[', 0);
    aHost.Append(']');
  }

  return NS_OK;
}

nsresult
nsContentUtils::GetHostOrIPv6WithBrackets(nsIURI* aURI, nsAString& aHost)
{
  nsAutoCString hostname;
  nsresult rv = GetHostOrIPv6WithBrackets(aURI, hostname);
  if (NS_FAILED(rv)) {
    return rv;
  }
  CopyUTF8toUTF16(hostname, aHost);
  return NS_OK;
}

bool
nsContentUtils::CallOnAllRemoteChildren(nsIMessageBroadcaster* aManager,
                                        CallOnRemoteChildFunction aCallback,
                                        void* aArg)
{
  uint32_t tabChildCount = 0;
  aManager->GetChildCount(&tabChildCount);
  for (uint32_t j = 0; j < tabChildCount; ++j) {
    nsCOMPtr<nsIMessageListenerManager> childMM;
    aManager->GetChildAt(j, getter_AddRefs(childMM));
    if (!childMM) {
      continue;
    }

    nsCOMPtr<nsIMessageBroadcaster> nonLeafMM = do_QueryInterface(childMM);
    if (nonLeafMM) {
      if (CallOnAllRemoteChildren(nonLeafMM, aCallback, aArg)) {
        return true;
      }
      continue;
    }

    nsCOMPtr<nsIMessageSender> tabMM = do_QueryInterface(childMM);

    mozilla::dom::ipc::MessageManagerCallback* cb =
     static_cast<nsFrameMessageManager*>(tabMM.get())->GetCallback();
    if (cb) {
      nsFrameLoader* fl = static_cast<nsFrameLoader*>(cb);
      TabParent* remote = TabParent::GetFrom(fl);
      if (remote && aCallback) {
        if (aCallback(remote, aArg)) {
          return true;
        }
      }
    }
  }

  return false;
}

void
nsContentUtils::CallOnAllRemoteChildren(nsPIDOMWindowOuter* aWindow,
                                        CallOnRemoteChildFunction aCallback,
                                        void* aArg)
{
  nsCOMPtr<nsIDOMChromeWindow> chromeWindow(do_QueryInterface(aWindow));
  if (chromeWindow) {
    nsCOMPtr<nsIMessageBroadcaster> windowMM;
    chromeWindow->GetMessageManager(getter_AddRefs(windowMM));
    if (windowMM) {
      CallOnAllRemoteChildren(windowMM, aCallback, aArg);
    }
  }
}

struct UIStateChangeInfo {
  UIStateChangeType mShowAccelerators;
  UIStateChangeType mShowFocusRings;

  UIStateChangeInfo(UIStateChangeType aShowAccelerators,
                    UIStateChangeType aShowFocusRings)
    : mShowAccelerators(aShowAccelerators),
      mShowFocusRings(aShowFocusRings)
  {}
};

bool
SetKeyboardIndicatorsChild(TabParent* aParent, void* aArg)
{
  UIStateChangeInfo* stateInfo = static_cast<UIStateChangeInfo*>(aArg);
  Unused << aParent->SendSetKeyboardIndicators(stateInfo->mShowAccelerators,
                                               stateInfo->mShowFocusRings);
  return false;
}

void
nsContentUtils::SetKeyboardIndicatorsOnRemoteChildren(nsPIDOMWindowOuter* aWindow,
                                                      UIStateChangeType aShowAccelerators,
                                                      UIStateChangeType aShowFocusRings)
{
  UIStateChangeInfo stateInfo(aShowAccelerators, aShowFocusRings);
  CallOnAllRemoteChildren(aWindow, SetKeyboardIndicatorsChild,
                          (void *)&stateInfo);
}

nsresult
nsContentUtils::IPCTransferableToTransferable(const IPCDataTransfer& aDataTransfer,
                                              const bool& aIsPrivateData,
                                              nsIPrincipal* aRequestingPrincipal,
                                              nsITransferable* aTransferable,
                                              mozilla::dom::nsIContentParent* aContentParent,
                                              mozilla::dom::TabChild* aTabChild)
{
  nsresult rv;

  const nsTArray<IPCDataTransferItem>& items = aDataTransfer.items();
  for (const auto& item : items) {
    aTransferable->AddDataFlavor(item.flavor().get());

    if (item.data().type() == IPCDataTransferData::TnsString) {
      nsCOMPtr<nsISupportsString> dataWrapper =
        do_CreateInstance(NS_SUPPORTS_STRING_CONTRACTID, &rv);
      NS_ENSURE_SUCCESS(rv, rv);

      const nsString& text = item.data().get_nsString();
      rv = dataWrapper->SetData(text);
      NS_ENSURE_SUCCESS(rv, rv);

      rv = aTransferable->SetTransferData(item.flavor().get(), dataWrapper,
                                  text.Length() * sizeof(char16_t));

      NS_ENSURE_SUCCESS(rv, rv);
    } else if (item.data().type() == IPCDataTransferData::TShmem) {
      if (nsContentUtils::IsFlavorImage(item.flavor())) {
        nsCOMPtr<imgIContainer> imageContainer;
        rv = nsContentUtils::DataTransferItemToImage(item,
                                                     getter_AddRefs(imageContainer));
        NS_ENSURE_SUCCESS(rv, rv);

        nsCOMPtr<nsISupportsInterfacePointer> imgPtr =
          do_CreateInstance(NS_SUPPORTS_INTERFACE_POINTER_CONTRACTID);
        NS_ENSURE_TRUE(imgPtr, NS_ERROR_FAILURE);

        rv = imgPtr->SetData(imageContainer);
        NS_ENSURE_SUCCESS(rv, rv);

        aTransferable->SetTransferData(item.flavor().get(), imgPtr, sizeof(nsISupports*));
      } else {
        nsCOMPtr<nsISupportsCString> dataWrapper =
          do_CreateInstance(NS_SUPPORTS_CSTRING_CONTRACTID, &rv);
        NS_ENSURE_SUCCESS(rv, rv);

        // The buffer contains the terminating null.
        Shmem itemData = item.data().get_Shmem();
        const nsDependentCString text(itemData.get<char>(),
                                      itemData.Size<char>());
        rv = dataWrapper->SetData(text);
        NS_ENSURE_SUCCESS(rv, rv);

        rv = aTransferable->SetTransferData(item.flavor().get(), dataWrapper, text.Length());

        NS_ENSURE_SUCCESS(rv, rv);
      }

      if (aContentParent) {
        Unused << aContentParent->DeallocShmem(item.data().get_Shmem());
      } else if (aTabChild) {
        Unused << aTabChild->DeallocShmem(item.data().get_Shmem());
      }
    }
  }

  aTransferable->SetIsPrivateData(aIsPrivateData);
  aTransferable->SetRequestingPrincipal(aRequestingPrincipal);
  return NS_OK;
}

void
nsContentUtils::TransferablesToIPCTransferables(nsIArray* aTransferables,
                                                nsTArray<IPCDataTransfer>& aIPC,
                                                bool aInSyncMessage,
                                                mozilla::dom::nsIContentChild* aChild,
                                                mozilla::dom::nsIContentParent* aParent)
{
  aIPC.Clear();
  if (aTransferables) {
    uint32_t transferableCount = 0;
    aTransferables->GetLength(&transferableCount);
    for (uint32_t i = 0; i < transferableCount; ++i) {
      IPCDataTransfer* dt = aIPC.AppendElement();
      nsCOMPtr<nsITransferable> transferable = do_QueryElementAt(aTransferables, i);
      TransferableToIPCTransferable(transferable, dt, aInSyncMessage, aChild, aParent);
    }
  }
}

nsresult
nsContentUtils::SlurpFileToString(nsIFile* aFile, nsACString& aString)
{
  aString.Truncate();

  nsCOMPtr<nsIURI> fileURI;
  nsresult rv = NS_NewFileURI(getter_AddRefs(fileURI), aFile);
  if (NS_FAILED(rv)) {
    return rv;
  }

  nsCOMPtr<nsIChannel> channel;
  rv = NS_NewChannel(getter_AddRefs(channel),
                     fileURI,
                     nsContentUtils::GetSystemPrincipal(),
                     nsILoadInfo::SEC_ALLOW_CROSS_ORIGIN_DATA_IS_NULL,
                     nsIContentPolicy::TYPE_OTHER);
  if (NS_FAILED(rv)) {
    return rv;
  }

  nsCOMPtr<nsIInputStream> stream;
  rv = channel->Open2(getter_AddRefs(stream));
  if (NS_FAILED(rv)) {
    return rv;
  }

  rv = NS_ConsumeStream(stream, UINT32_MAX, aString);
  if (NS_FAILED(rv)) {
    return rv;
  }

  rv = stream->Close();
  if (NS_FAILED(rv)) {
    return rv;
  }

  return NS_OK;
}

bool
nsContentUtils::IsFileImage(nsIFile* aFile, nsACString& aType)
{
  nsCOMPtr<nsIMIMEService> mime = do_GetService("@mozilla.org/mime;1");
  if (!mime) {
    return false;
  }

  nsresult rv = mime->GetTypeFromFile(aFile, aType);
  if (NS_FAILED(rv)) {
    return false;
  }

  return StringBeginsWith(aType, NS_LITERAL_CSTRING("image/"));
}

nsresult
nsContentUtils::DataTransferItemToImage(const IPCDataTransferItem& aItem,
                                        imgIContainer** aContainer)
{
  MOZ_ASSERT(aItem.data().type() == IPCDataTransferData::TShmem);
  MOZ_ASSERT(IsFlavorImage(aItem.flavor()));

  const IPCDataTransferImage& imageDetails = aItem.imageDetails();
  const IntSize size(imageDetails.width(), imageDetails.height());
  if (!size.width || !size.height) {
    return NS_ERROR_FAILURE;
  }

  Shmem data = aItem.data().get_Shmem();

  RefPtr<DataSourceSurface> image =
      CreateDataSourceSurfaceFromData(size,
                                      static_cast<SurfaceFormat>(imageDetails.format()),
                                      data.get<uint8_t>(),
                                      imageDetails.stride());

  RefPtr<gfxDrawable> drawable = new gfxSurfaceDrawable(image, size);
  nsCOMPtr<imgIContainer> imageContainer =
    image::ImageOps::CreateFromDrawable(drawable);
  imageContainer.forget(aContainer);

  return NS_OK;
}

bool
nsContentUtils::IsFlavorImage(const nsACString& aFlavor)
{
  return aFlavor.EqualsLiteral(kNativeImageMime) ||
         aFlavor.EqualsLiteral(kJPEGImageMime) ||
         aFlavor.EqualsLiteral(kJPGImageMime) ||
         aFlavor.EqualsLiteral(kPNGImageMime) ||
         aFlavor.EqualsLiteral(kGIFImageMime);
}

static Shmem
ConvertToShmem(mozilla::dom::nsIContentChild* aChild,
               mozilla::dom::nsIContentParent* aParent,
               const nsACString& aInput)
{
  MOZ_ASSERT((aChild && !aParent) || (!aChild && aParent));

  IShmemAllocator* allocator =
    aChild ? static_cast<IShmemAllocator*>(aChild)
           : static_cast<IShmemAllocator*>(aParent);

  Shmem result;
  if (!allocator->AllocShmem(aInput.Length() + 1,
                             SharedMemory::TYPE_BASIC,
                             &result)) {
    return result;
  }

  memcpy(result.get<char>(),
         aInput.BeginReading(),
         aInput.Length() + 1);

  return result;
}

void
nsContentUtils::TransferableToIPCTransferable(nsITransferable* aTransferable,
                                              IPCDataTransfer* aIPCDataTransfer,
                                              bool aInSyncMessage,
                                              mozilla::dom::nsIContentChild* aChild,
                                              mozilla::dom::nsIContentParent* aParent)
{
  MOZ_ASSERT((aChild && !aParent) || (!aChild && aParent));

  if (aTransferable) {
    nsCOMPtr<nsIArray> flavorList;
    aTransferable->FlavorsTransferableCanExport(getter_AddRefs(flavorList));
    if (flavorList) {
      uint32_t flavorCount = 0;
      flavorList->GetLength(&flavorCount);
      for (uint32_t j = 0; j < flavorCount; ++j) {
        nsCOMPtr<nsISupportsCString> flavor = do_QueryElementAt(flavorList, j);
        if (!flavor) {
          continue;
        }

        nsAutoCString flavorStr;
        flavor->GetData(flavorStr);
        if (!flavorStr.Length()) {
          continue;
        }

        nsCOMPtr<nsISupports> data;
        uint32_t dataLen = 0;
        aTransferable->GetTransferData(flavorStr.get(), getter_AddRefs(data), &dataLen);

        nsCOMPtr<nsISupportsString> text = do_QueryInterface(data);
        nsCOMPtr<nsISupportsCString> ctext = do_QueryInterface(data);
        if (text) {
          nsAutoString dataAsString;
          text->GetData(dataAsString);
          IPCDataTransferItem* item = aIPCDataTransfer->items().AppendElement();
          item->flavor() = flavorStr;
          item->data() = dataAsString;
        } else if (ctext) {
          nsAutoCString dataAsString;
          ctext->GetData(dataAsString);
          IPCDataTransferItem* item = aIPCDataTransfer->items().AppendElement();
          item->flavor() = flavorStr;

          Shmem dataAsShmem = ConvertToShmem(aChild, aParent, dataAsString);
          if (!dataAsShmem.IsReadable() || !dataAsShmem.Size<char>()) {
            continue;
          }

          item->data() = dataAsShmem;
        } else {
          nsCOMPtr<nsISupportsInterfacePointer> sip =
            do_QueryInterface(data);
          if (sip) {
            sip->GetData(getter_AddRefs(data));
          }

          // Images to be pasted on the clipboard are nsIInputStreams
          nsCOMPtr<nsIInputStream> stream(do_QueryInterface(data));
          if (stream) {
            IPCDataTransferItem* item = aIPCDataTransfer->items().AppendElement();
            item->flavor() = flavorStr;

            nsCString imageData;
            NS_ConsumeStream(stream, UINT32_MAX, imageData);

            Shmem imageDataShmem = ConvertToShmem(aChild, aParent, imageData);
            if (!imageDataShmem.IsReadable() || !imageDataShmem.Size<char>()) {
              continue;
            }

            item->data() = imageDataShmem;
            continue;
          }

          // Images to be placed on the clipboard are imgIContainers.
          nsCOMPtr<imgIContainer> image(do_QueryInterface(data));
          if (image) {
            RefPtr<mozilla::gfx::SourceSurface> surface =
              image->GetFrame(imgIContainer::FRAME_CURRENT,
                              imgIContainer::FLAG_SYNC_DECODE);
            if (!surface) {
              continue;
            }
            RefPtr<mozilla::gfx::DataSourceSurface> dataSurface =
              surface->GetDataSurface();
            if (!dataSurface) {
              continue;
            }
            size_t length;
            int32_t stride;
            IShmemAllocator* allocator = aChild ? static_cast<IShmemAllocator*>(aChild)
                                                : static_cast<IShmemAllocator*>(aParent);
            Maybe<Shmem> surfaceData = GetSurfaceData(dataSurface, &length, &stride,
                                                      allocator);

            if (surfaceData.isNothing()) {
              continue;
            }

            IPCDataTransferItem* item = aIPCDataTransfer->items().AppendElement();
            item->flavor() = flavorStr;
            // Turn item->data() into an nsCString prior to accessing it.
            item->data() = surfaceData.ref();

            IPCDataTransferImage& imageDetails = item->imageDetails();
            mozilla::gfx::IntSize size = dataSurface->GetSize();
            imageDetails.width() = size.width;
            imageDetails.height() = size.height;
            imageDetails.stride() = stride;
            imageDetails.format() = static_cast<uint8_t>(dataSurface->GetFormat());

            continue;
          }

          // Otherwise, handle this as a file.
          nsCOMPtr<BlobImpl> blobImpl;
          nsCOMPtr<nsIFile> file = do_QueryInterface(data);
          if (file) {
            // If we can send this over as a blob, do so. Otherwise, we're
            // responding to a sync message and the child can't process the blob
            // constructor before processing our response, which would crash. In
            // that case, hope that the caller is nsClipboardProxy::GetData,
            // called from editor and send over images as raw data.
            if (aInSyncMessage) {
              nsAutoCString type;
              if (IsFileImage(file, type)) {
                IPCDataTransferItem* item = aIPCDataTransfer->items().AppendElement();
                item->flavor() = type;
                nsAutoCString data;
                SlurpFileToString(file, data);

                Shmem dataAsShmem = ConvertToShmem(aChild, aParent, data);
                item->data() = dataAsShmem;
              }

              continue;
            }

            if (aParent) {
              bool isDir = false;
              if (NS_SUCCEEDED(file->IsDirectory(&isDir)) && isDir) {
                nsAutoString path;
                if (NS_WARN_IF(NS_FAILED(file->GetPath(path)))) {
                  continue;
                }

                RefPtr<FileSystemSecurity> fss = FileSystemSecurity::GetOrCreate();
                fss->GrantAccessToContentProcess(aParent->ChildID(), path);
              }
            }

            blobImpl = new FileBlobImpl(file);

            IgnoredErrorResult rv;

            // Ensure that file data is cached no that the content process
            // has this data available to it when passed over:
            blobImpl->GetSize(rv);
            if (NS_WARN_IF(rv.Failed())) {
              continue;
            }

            blobImpl->GetLastModified(rv);
            if (NS_WARN_IF(rv.Failed())) {
              continue;
            }
          } else {
            if (aInSyncMessage) {
              // Can't do anything.
              continue;
            }
            blobImpl = do_QueryInterface(data);
          }
          if (blobImpl) {
            IPCDataTransferData data;
            IPCBlob ipcBlob;

            // If we failed to create the blob actor, then this blob probably
            // can't get the file size for the underlying file, ignore it for
            // now. TODO pass this through anyway.
            if (aChild) {
              nsresult rv = IPCBlobUtils::Serialize(blobImpl, aChild, ipcBlob);
              if (NS_WARN_IF(NS_FAILED(rv))) {
                continue;
              }

              data = ipcBlob;
            } else if (aParent) {
              nsresult rv = IPCBlobUtils::Serialize(blobImpl, aParent, ipcBlob);
              if (NS_WARN_IF(NS_FAILED(rv))) {
                continue;
              }

              data = ipcBlob;
            }

            IPCDataTransferItem* item = aIPCDataTransfer->items().AppendElement();
            item->flavor() = flavorStr;
            item->data() = data;
          } else {
            // This is a hack to support kFilePromiseMime.
            // On Windows there just needs to be an entry for it,
            // and for OSX we need to create
            // nsContentAreaDragDropDataProvider as nsIFlavorDataProvider.
            if (flavorStr.EqualsLiteral(kFilePromiseMime)) {
              IPCDataTransferItem* item = aIPCDataTransfer->items().AppendElement();
              item->flavor() = flavorStr;
              item->data() = NS_ConvertUTF8toUTF16(flavorStr);
            } else if (!data) {
              // Empty element, transfer only the flavor
              IPCDataTransferItem* item = aIPCDataTransfer->items().AppendElement();
              item->flavor() = flavorStr;
              item->data() = nsString();
              continue;
            }
          }
        }
      }
    }
  }
}

namespace {
// The default type used for calling GetSurfaceData(). Gets surface data as
// raw buffer.
struct GetSurfaceDataRawBuffer
{
  using ReturnType = mozilla::UniquePtr<char[]>;
  using BufferType = char*;

  ReturnType Allocate(size_t aSize)
  {
    return ReturnType(new char[aSize]);
  }

  static BufferType
  GetBuffer(const ReturnType& aReturnValue)
  {
    return aReturnValue.get();
  }

  static ReturnType
  NullValue()
  {
    return ReturnType();
  }
};

// The type used for calling GetSurfaceData() that allocates and writes to
// a shared memory buffer.
struct GetSurfaceDataShmem
{
  using ReturnType = Maybe<Shmem>;
  using BufferType = char*;

  explicit GetSurfaceDataShmem(IShmemAllocator* aAllocator)
    : mAllocator(aAllocator)
  { }

  ReturnType Allocate(size_t aSize)
  {
    Shmem shmem;
    if (!mAllocator->AllocShmem(aSize, SharedMemory::TYPE_BASIC, &shmem)) {
      return Nothing();
    }

    return Some(shmem);
  }

  static BufferType
  GetBuffer(const ReturnType& aReturnValue)
  {
    return aReturnValue.isSome() ? aReturnValue.ref().get<char>() : nullptr;
  }

  static ReturnType
  NullValue()
  {
    return ReturnType();
  }
private:
  IShmemAllocator* mAllocator;
};

/*
 * Get the pixel data from the given source surface and return it as a buffer.
 * The length and stride will be assigned from the surface.
 */
template <typename GetSurfaceDataContext = GetSurfaceDataRawBuffer>
typename GetSurfaceDataContext::ReturnType
GetSurfaceDataImpl(mozilla::gfx::DataSourceSurface* aSurface,
                   size_t* aLength, int32_t* aStride,
                   GetSurfaceDataContext aContext = GetSurfaceDataContext())
{
  mozilla::gfx::DataSourceSurface::MappedSurface map;
  if (!aSurface->Map(mozilla::gfx::DataSourceSurface::MapType::READ, &map)) {
    return GetSurfaceDataContext::NullValue();
  }

  mozilla::gfx::IntSize size = aSurface->GetSize();
  mozilla::CheckedInt32 requiredBytes =
    mozilla::CheckedInt32(map.mStride) * mozilla::CheckedInt32(size.height);
  if (!requiredBytes.isValid()) {
    return GetSurfaceDataContext::NullValue();
  }

  size_t maxBufLen = requiredBytes.value();
  mozilla::gfx::SurfaceFormat format = aSurface->GetFormat();

  // Surface data handling is totally nuts. This is the magic one needs to
  // know to access the data.
  size_t bufLen = maxBufLen - map.mStride + (size.width * BytesPerPixel(format));

  // nsDependentCString wants null-terminated string.
  typename GetSurfaceDataContext::ReturnType surfaceData = aContext.Allocate(maxBufLen + 1);
  if (GetSurfaceDataContext::GetBuffer(surfaceData)) {
    memcpy(GetSurfaceDataContext::GetBuffer(surfaceData),
           reinterpret_cast<char*>(map.mData),
           bufLen);
    memset(GetSurfaceDataContext::GetBuffer(surfaceData) + bufLen,
           0,
           maxBufLen - bufLen + 1);
  }

  *aLength = maxBufLen;
  *aStride = map.mStride;

  aSurface->Unmap();
  return surfaceData;
}
} // Anonymous namespace.

mozilla::UniquePtr<char[]>
nsContentUtils::GetSurfaceData(
  NotNull<mozilla::gfx::DataSourceSurface*> aSurface,
  size_t* aLength, int32_t* aStride)
{
  return GetSurfaceDataImpl(aSurface, aLength, aStride);
}

Maybe<Shmem>
nsContentUtils::GetSurfaceData(mozilla::gfx::DataSourceSurface* aSurface,
                               size_t* aLength, int32_t* aStride,
                               IShmemAllocator* aAllocator)
{
  return GetSurfaceDataImpl(aSurface, aLength, aStride,
                            GetSurfaceDataShmem(aAllocator));
}

mozilla::Modifiers
nsContentUtils::GetWidgetModifiers(int32_t aModifiers)
{
  Modifiers result = 0;
  if (aModifiers & nsIDOMWindowUtils::MODIFIER_SHIFT) {
    result |= mozilla::MODIFIER_SHIFT;
  }
  if (aModifiers & nsIDOMWindowUtils::MODIFIER_CONTROL) {
    result |= mozilla::MODIFIER_CONTROL;
  }
  if (aModifiers & nsIDOMWindowUtils::MODIFIER_ALT) {
    result |= mozilla::MODIFIER_ALT;
  }
  if (aModifiers & nsIDOMWindowUtils::MODIFIER_META) {
    result |= mozilla::MODIFIER_META;
  }
  if (aModifiers & nsIDOMWindowUtils::MODIFIER_ALTGRAPH) {
    result |= mozilla::MODIFIER_ALTGRAPH;
  }
  if (aModifiers & nsIDOMWindowUtils::MODIFIER_CAPSLOCK) {
    result |= mozilla::MODIFIER_CAPSLOCK;
  }
  if (aModifiers & nsIDOMWindowUtils::MODIFIER_FN) {
    result |= mozilla::MODIFIER_FN;
  }
  if (aModifiers & nsIDOMWindowUtils::MODIFIER_FNLOCK) {
    result |= mozilla::MODIFIER_FNLOCK;
  }
  if (aModifiers & nsIDOMWindowUtils::MODIFIER_NUMLOCK) {
    result |= mozilla::MODIFIER_NUMLOCK;
  }
  if (aModifiers & nsIDOMWindowUtils::MODIFIER_SCROLLLOCK) {
    result |= mozilla::MODIFIER_SCROLLLOCK;
  }
  if (aModifiers & nsIDOMWindowUtils::MODIFIER_SYMBOL) {
    result |= mozilla::MODIFIER_SYMBOL;
  }
  if (aModifiers & nsIDOMWindowUtils::MODIFIER_SYMBOLLOCK) {
    result |= mozilla::MODIFIER_SYMBOLLOCK;
  }
  if (aModifiers & nsIDOMWindowUtils::MODIFIER_OS) {
    result |= mozilla::MODIFIER_OS;
  }
  return result;
}

nsIWidget*
nsContentUtils::GetWidget(nsIPresShell* aPresShell, nsPoint* aOffset) {
  if (aPresShell) {
    nsIFrame* frame = aPresShell->GetRootFrame();
    if (frame)
      return frame->GetView()->GetNearestWidget(aOffset);
  }
  return nullptr;
}

int16_t
nsContentUtils::GetButtonsFlagForButton(int32_t aButton)
{
  switch (aButton) {
    case -1:
      return WidgetMouseEvent::eNoButtonFlag;
    case WidgetMouseEvent::eLeftButton:
      return WidgetMouseEvent::eLeftButtonFlag;
    case WidgetMouseEvent::eMiddleButton:
      return WidgetMouseEvent::eMiddleButtonFlag;
    case WidgetMouseEvent::eRightButton:
      return WidgetMouseEvent::eRightButtonFlag;
    case 4:
      return WidgetMouseEvent::e4thButtonFlag;
    case 5:
      return WidgetMouseEvent::e5thButtonFlag;
    default:
      NS_ERROR("Button not known.");
      return 0;
  }
}

LayoutDeviceIntPoint
nsContentUtils::ToWidgetPoint(const CSSPoint& aPoint,
                              const nsPoint& aOffset,
                              nsPresContext* aPresContext)
{
  return LayoutDeviceIntPoint::FromAppUnitsRounded(
    (CSSPoint::ToAppUnits(aPoint) +
    aOffset).ApplyResolution(nsLayoutUtils::GetCurrentAPZResolutionScale(aPresContext->PresShell())),
    aPresContext->AppUnitsPerDevPixel());
}

nsView*
nsContentUtils::GetViewToDispatchEvent(nsPresContext* presContext,
                                       nsIPresShell** presShell)
{
  if (presContext && presShell) {
    *presShell = presContext->PresShell();
    if (*presShell) {
      NS_ADDREF(*presShell);
      if (nsViewManager* viewManager = (*presShell)->GetViewManager()) {
        if (nsView* view = viewManager->GetRootView()) {
          return view;
        }
      }
    }
  }
  return nullptr;
}

nsresult
nsContentUtils::SendKeyEvent(nsIWidget* aWidget,
                             const nsAString& aType,
                             int32_t aKeyCode,
                             int32_t aCharCode,
                             int32_t aModifiers,
                             uint32_t aAdditionalFlags,
                             bool* aDefaultActionTaken)
{
  // get the widget to send the event to
  if (!aWidget)
    return NS_ERROR_FAILURE;

  EventMessage msg;
  if (aType.EqualsLiteral("keydown"))
    msg = eKeyDown;
  else if (aType.EqualsLiteral("keyup"))
    msg = eKeyUp;
  else if (aType.EqualsLiteral("keypress"))
    msg = eKeyPress;
  else
    return NS_ERROR_FAILURE;

  WidgetKeyboardEvent event(true, msg, aWidget);
  event.mModifiers = GetWidgetModifiers(aModifiers);

  if (msg == eKeyPress) {
    event.mKeyCode = aCharCode ? 0 : aKeyCode;
    event.mCharCode = aCharCode;
  } else {
    event.mKeyCode = aKeyCode;
    event.mCharCode = 0;
  }

  uint32_t locationFlag = (aAdditionalFlags &
    (nsIDOMWindowUtils::KEY_FLAG_LOCATION_STANDARD | nsIDOMWindowUtils::KEY_FLAG_LOCATION_LEFT |
     nsIDOMWindowUtils::KEY_FLAG_LOCATION_RIGHT | nsIDOMWindowUtils::KEY_FLAG_LOCATION_NUMPAD));
  switch (locationFlag) {
    case nsIDOMWindowUtils::KEY_FLAG_LOCATION_STANDARD:
      event.mLocation = eKeyLocationStandard;
      break;
    case nsIDOMWindowUtils::KEY_FLAG_LOCATION_LEFT:
      event.mLocation = eKeyLocationLeft;
      break;
    case nsIDOMWindowUtils::KEY_FLAG_LOCATION_RIGHT:
      event.mLocation = eKeyLocationRight;
      break;
    case nsIDOMWindowUtils::KEY_FLAG_LOCATION_NUMPAD:
      event.mLocation = eKeyLocationNumpad;
      break;
    default:
      if (locationFlag != 0) {
        return NS_ERROR_INVALID_ARG;
      }
      // If location flag isn't set, choose the location from keycode.
      switch (aKeyCode) {
        case nsIDOMKeyEvent::DOM_VK_NUMPAD0:
        case nsIDOMKeyEvent::DOM_VK_NUMPAD1:
        case nsIDOMKeyEvent::DOM_VK_NUMPAD2:
        case nsIDOMKeyEvent::DOM_VK_NUMPAD3:
        case nsIDOMKeyEvent::DOM_VK_NUMPAD4:
        case nsIDOMKeyEvent::DOM_VK_NUMPAD5:
        case nsIDOMKeyEvent::DOM_VK_NUMPAD6:
        case nsIDOMKeyEvent::DOM_VK_NUMPAD7:
        case nsIDOMKeyEvent::DOM_VK_NUMPAD8:
        case nsIDOMKeyEvent::DOM_VK_NUMPAD9:
        case nsIDOMKeyEvent::DOM_VK_MULTIPLY:
        case nsIDOMKeyEvent::DOM_VK_ADD:
        case nsIDOMKeyEvent::DOM_VK_SEPARATOR:
        case nsIDOMKeyEvent::DOM_VK_SUBTRACT:
        case nsIDOMKeyEvent::DOM_VK_DECIMAL:
        case nsIDOMKeyEvent::DOM_VK_DIVIDE:
          event.mLocation = eKeyLocationNumpad;
          break;
        case nsIDOMKeyEvent::DOM_VK_SHIFT:
        case nsIDOMKeyEvent::DOM_VK_CONTROL:
        case nsIDOMKeyEvent::DOM_VK_ALT:
        case nsIDOMKeyEvent::DOM_VK_META:
          event.mLocation = eKeyLocationLeft;
          break;
        default:
          event.mLocation = eKeyLocationStandard;
          break;
      }
      break;
  }

  event.mRefPoint = LayoutDeviceIntPoint(0, 0);
  event.mTime = PR_IntervalNow();
  if (!(aAdditionalFlags & nsIDOMWindowUtils::KEY_FLAG_NOT_SYNTHESIZED_FOR_TESTS)) {
    event.mFlags.mIsSynthesizedForTests = true;
  }

  if (aAdditionalFlags & nsIDOMWindowUtils::KEY_FLAG_PREVENT_DEFAULT) {
    event.PreventDefaultBeforeDispatch();
  }

  nsEventStatus status;
  nsresult rv = aWidget->DispatchEvent(&event, status);
  NS_ENSURE_SUCCESS(rv, rv);

  *aDefaultActionTaken = (status != nsEventStatus_eConsumeNoDefault);

  return NS_OK;
}

nsresult
nsContentUtils::SendMouseEvent(const nsCOMPtr<nsIPresShell>& aPresShell,
                               const nsAString& aType,
                               float aX,
                               float aY,
                               int32_t aButton,
                               int32_t aButtons,
                               int32_t aClickCount,
                               int32_t aModifiers,
                               bool aIgnoreRootScrollFrame,
                               float aPressure,
                               unsigned short aInputSourceArg,
                               uint32_t aIdentifier,
                               bool aToWindow,
                               bool *aPreventDefault,
                               bool aIsDOMEventSynthesized,
                               bool aIsWidgetEventSynthesized)
{
  nsPoint offset;
  nsCOMPtr<nsIWidget> widget = GetWidget(aPresShell, &offset);
  if (!widget)
    return NS_ERROR_FAILURE;

  EventMessage msg;
  WidgetMouseEvent::ExitFrom exitFrom = WidgetMouseEvent::eChild;
  bool contextMenuKey = false;
  if (aType.EqualsLiteral("mousedown")) {
    msg = eMouseDown;
  } else if (aType.EqualsLiteral("mouseup")) {
    msg = eMouseUp;
  } else if (aType.EqualsLiteral("mousemove")) {
    msg = eMouseMove;
  } else if (aType.EqualsLiteral("mouseover")) {
    msg = eMouseEnterIntoWidget;
  } else if (aType.EqualsLiteral("mouseout")) {
    msg = eMouseExitFromWidget;
  } else if (aType.EqualsLiteral("mousecancel")) {
    msg = eMouseExitFromWidget;
    exitFrom = WidgetMouseEvent::eTopLevel;
  } else if (aType.EqualsLiteral("mouselongtap")) {
    msg = eMouseLongTap;
  } else if (aType.EqualsLiteral("contextmenu")) {
    msg = eContextMenu;
    contextMenuKey = (aButton == 0);
  } else if (aType.EqualsLiteral("MozMouseHittest")) {
    msg = eMouseHitTest;
  } else {
    return NS_ERROR_FAILURE;
  }

  if (aInputSourceArg == nsIDOMMouseEvent::MOZ_SOURCE_UNKNOWN) {
    aInputSourceArg = nsIDOMMouseEvent::MOZ_SOURCE_MOUSE;
  }

  WidgetMouseEvent event(true, msg, widget,
                         aIsWidgetEventSynthesized ?
                           WidgetMouseEvent::eSynthesized :
                           WidgetMouseEvent::eReal,
                         contextMenuKey ? WidgetMouseEvent::eContextMenuKey :
                                          WidgetMouseEvent::eNormal);
  event.pointerId = aIdentifier;
  event.mModifiers = GetWidgetModifiers(aModifiers);
  event.button = aButton;
  event.buttons = aButtons != nsIDOMWindowUtils::MOUSE_BUTTONS_NOT_SPECIFIED ?
                  aButtons :
                  msg == eMouseUp ? 0 : GetButtonsFlagForButton(aButton);
  event.pressure = aPressure;
  event.inputSource = aInputSourceArg;
  event.mClickCount = aClickCount;
  event.mTime = PR_IntervalNow();
  event.mFlags.mIsSynthesizedForTests = aIsDOMEventSynthesized;
  event.mExitFrom = exitFrom;

  nsPresContext* presContext = aPresShell->GetPresContext();
  if (!presContext)
    return NS_ERROR_FAILURE;

  event.mRefPoint = ToWidgetPoint(CSSPoint(aX, aY), offset, presContext);
  event.mIgnoreRootScrollFrame = aIgnoreRootScrollFrame;

  nsEventStatus status = nsEventStatus_eIgnore;
  if (aToWindow) {
    nsCOMPtr<nsIPresShell> presShell;
    nsView* view = GetViewToDispatchEvent(presContext, getter_AddRefs(presShell));
    if (!presShell || !view) {
      return NS_ERROR_FAILURE;
    }
    return presShell->HandleEvent(view->GetFrame(), &event, false, &status);
  }
  if (gfxPrefs::TestEventsAsyncEnabled()) {
    status = widget->DispatchInputEvent(&event);
  } else {
    nsresult rv = widget->DispatchEvent(&event, status);
    NS_ENSURE_SUCCESS(rv, rv);
  }
  if (aPreventDefault) {
    *aPreventDefault = (status == nsEventStatus_eConsumeNoDefault);
  }

  return NS_OK;
}

/* static */
void
nsContentUtils::FirePageHideEvent(nsIDocShellTreeItem* aItem,
                                  EventTarget* aChromeEventHandler)
{
  nsCOMPtr<nsIDocument> doc = aItem->GetDocument();
  NS_ASSERTION(doc, "What happened here?");
  doc->OnPageHide(true, aChromeEventHandler);

  int32_t childCount = 0;
  aItem->GetChildCount(&childCount);
  AutoTArray<nsCOMPtr<nsIDocShellTreeItem>, 8> kids;
  kids.AppendElements(childCount);
  for (int32_t i = 0; i < childCount; ++i) {
    aItem->GetChildAt(i, getter_AddRefs(kids[i]));
  }

  for (uint32_t i = 0; i < kids.Length(); ++i) {
    if (kids[i]) {
      FirePageHideEvent(kids[i], aChromeEventHandler);
    }
  }
}

// The pageshow event is fired for a given document only if IsShowing() returns
// the same thing as aFireIfShowing.  This gives us a way to fire pageshow only
// on documents that are still loading or only on documents that are already
// loaded.
/* static */
void
nsContentUtils::FirePageShowEvent(nsIDocShellTreeItem* aItem,
                                  EventTarget* aChromeEventHandler,
                                  bool aFireIfShowing)
{
  int32_t childCount = 0;
  aItem->GetChildCount(&childCount);
  AutoTArray<nsCOMPtr<nsIDocShellTreeItem>, 8> kids;
  kids.AppendElements(childCount);
  for (int32_t i = 0; i < childCount; ++i) {
    aItem->GetChildAt(i, getter_AddRefs(kids[i]));
  }

  for (uint32_t i = 0; i < kids.Length(); ++i) {
    if (kids[i]) {
      FirePageShowEvent(kids[i], aChromeEventHandler, aFireIfShowing);
    }
  }

  nsCOMPtr<nsIDocument> doc = aItem->GetDocument();
  NS_ASSERTION(doc, "What happened here?");
  if (doc->IsShowing() == aFireIfShowing) {
    doc->OnPageShow(true, aChromeEventHandler);
  }
}

/* static */
already_AddRefed<nsPIWindowRoot>
nsContentUtils::GetWindowRoot(nsIDocument* aDoc)
{
  if (aDoc) {
    if (nsPIDOMWindowOuter* win = aDoc->GetWindow()) {
      return win->GetTopWindowRoot();
    }
  }
  return nullptr;
}

/* static */
bool
nsContentUtils::IsPreloadType(nsContentPolicyType aType)
{
  return (aType == nsIContentPolicy::TYPE_INTERNAL_SCRIPT_PRELOAD ||
          aType == nsIContentPolicy::TYPE_INTERNAL_IMAGE_PRELOAD ||
          aType == nsIContentPolicy::TYPE_INTERNAL_STYLESHEET_PRELOAD);
}

nsresult
nsContentUtils::SetFetchReferrerURIWithPolicy(nsIPrincipal* aPrincipal,
                                              nsIDocument* aDoc,
                                              nsIHttpChannel* aChannel,
                                              mozilla::net::ReferrerPolicy aReferrerPolicy)
{
  NS_ENSURE_ARG_POINTER(aPrincipal);
  NS_ENSURE_ARG_POINTER(aChannel);

  nsCOMPtr<nsIURI> principalURI;

  if (IsSystemPrincipal(aPrincipal)) {
    return NS_OK;
  }

  aPrincipal->GetURI(getter_AddRefs(principalURI));

  if (!aDoc) {
    return aChannel->SetReferrerWithPolicy(principalURI, aReferrerPolicy);
  }

  // If it weren't for history.push/replaceState, we could just use the
  // principal's URI here.  But since we want changes to the URI effected
  // by push/replaceState to be reflected in the XHR referrer, we have to
  // be more clever.
  //
  // If the document's original URI (before any push/replaceStates) matches
  // our principal, then we use the document's current URI (after
  // push/replaceStates).  Otherwise (if the document is, say, a data:
  // URI), we just use the principal's URI.
  nsCOMPtr<nsIURI> docCurURI = aDoc->GetDocumentURI();
  nsCOMPtr<nsIURI> docOrigURI = aDoc->GetOriginalURI();

  nsCOMPtr<nsIURI> referrerURI;

  if (principalURI && docCurURI && docOrigURI) {
    bool equal = false;
    principalURI->Equals(docOrigURI, &equal);
    if (equal) {
      referrerURI = docCurURI;
    }
  }

  if (!referrerURI) {
    referrerURI = principalURI;
  }

  return aChannel->SetReferrerWithPolicy(referrerURI, aReferrerPolicy);
}

// static
net::ReferrerPolicy
nsContentUtils::GetReferrerPolicyFromHeader(const nsAString& aHeader)
{
  // Multiple headers could be concatenated into one comma-separated
  // list of policies. Need to tokenize the multiple headers.
  nsCharSeparatedTokenizer tokenizer(aHeader, ',');
  nsAutoString token;
  net::ReferrerPolicy referrerPolicy = mozilla::net::RP_Unset;
  while (tokenizer.hasMoreTokens()) {
    token = tokenizer.nextToken();
    if (token.IsEmpty()) {
      continue;
    }
    net::ReferrerPolicy policy = net::ReferrerPolicyFromString(token);
    if (policy != net::RP_Unset) {
      referrerPolicy = policy;
    }
  }
  return referrerPolicy;
}

// static
bool
nsContentUtils::PromiseRejectionEventsEnabled(JSContext* aCx, JSObject* aObj)
{
  if (NS_IsMainThread()) {
    return Preferences::GetBool("dom.promise_rejection_events.enabled", false);
  }

  using namespace workers;

  // Otherwise, check the pref via the WorkerPrivate
  WorkerPrivate* workerPrivate = GetWorkerPrivateFromContext(aCx);
  if (!workerPrivate) {
    return false;
  }

  return workerPrivate->PromiseRejectionEventsEnabled();
}

// static
bool
nsContentUtils::PushEnabled(JSContext* aCx, JSObject* aObj)
{
  if (NS_IsMainThread()) {
    return Preferences::GetBool("dom.push.enabled", false);
  }

  using namespace workers;

  // Otherwise, check the pref via the WorkerPrivate
  WorkerPrivate* workerPrivate = GetWorkerPrivateFromContext(aCx);
  if (!workerPrivate) {
    return false;
  }

  return workerPrivate->PushEnabled();
}

// static
bool
nsContentUtils::StreamsEnabled(JSContext* aCx, JSObject* aObj)
{
  if (NS_IsMainThread()) {
    return Preferences::GetBool("dom.streams.enabled", false);
  }

  using namespace workers;

  // Otherwise, check the pref via the WorkerPrivate
  WorkerPrivate* workerPrivate = GetWorkerPrivateFromContext(aCx);
  if (!workerPrivate) {
    return false;
  }

  return workerPrivate->StreamsEnabled();
}

// static
bool
nsContentUtils::IsNonSubresourceRequest(nsIChannel* aChannel)
{
  nsLoadFlags loadFlags = 0;
  aChannel->GetLoadFlags(&loadFlags);
  if (loadFlags & nsIChannel::LOAD_DOCUMENT_URI) {
    return true;
  }

  nsCOMPtr<nsILoadInfo> loadInfo = aChannel->GetLoadInfo();
  if (!loadInfo) {
    return false;
  }
  nsContentPolicyType type = loadInfo->InternalContentPolicyType();
  return type == nsIContentPolicy::TYPE_INTERNAL_WORKER ||
         type == nsIContentPolicy::TYPE_INTERNAL_SHARED_WORKER;
}

// static, public
nsContentUtils::StorageAccess
nsContentUtils::StorageAllowedForWindow(nsPIDOMWindowInner* aWindow)
{
  if (nsIDocument* document = aWindow->GetExtantDoc()) {
    nsCOMPtr<nsIPrincipal> principal = document->NodePrincipal();
    return InternalStorageAllowedForPrincipal(principal, aWindow);
  }

  return StorageAccess::eDeny;
}

// static, public
nsContentUtils::StorageAccess
nsContentUtils::StorageAllowedForDocument(nsIDocument* aDoc)
{
  MOZ_ASSERT(aDoc);

  if (nsPIDOMWindowInner* inner = aDoc->GetInnerWindow()) {
    nsCOMPtr<nsIPrincipal> principal = aDoc->NodePrincipal();
    return InternalStorageAllowedForPrincipal(principal, inner);
  }

  return StorageAccess::eDeny;
}

// static, public
nsContentUtils::StorageAccess
nsContentUtils::StorageAllowedForPrincipal(nsIPrincipal* aPrincipal)
{
  return InternalStorageAllowedForPrincipal(aPrincipal, nullptr);
}

// static, private
void
nsContentUtils::GetCookieBehaviorForPrincipal(nsIPrincipal* aPrincipal,
                                              uint32_t* aLifetimePolicy,
                                              uint32_t* aBehavior)
{
  *aLifetimePolicy = sCookiesLifetimePolicy;
  *aBehavior = sCookiesBehavior;

  // Any permissions set for the given principal will override our default
  // settings from preferences.
  nsCOMPtr<nsIPermissionManager> permissionManager =
    services::GetPermissionManager();
  if (!permissionManager) {
    return;
  }

  uint32_t perm;
  permissionManager->TestPermissionFromPrincipal(aPrincipal, "cookie", &perm);
  switch (perm) {
    case nsICookiePermission::ACCESS_ALLOW:
      *aBehavior = nsICookieService::BEHAVIOR_ACCEPT;
      *aLifetimePolicy = nsICookieService::ACCEPT_NORMALLY;
      break;
    case nsICookiePermission::ACCESS_DENY:
      *aBehavior = nsICookieService::BEHAVIOR_REJECT;
      *aLifetimePolicy = nsICookieService::ACCEPT_NORMALLY;
      break;
    case nsICookiePermission::ACCESS_SESSION:
      *aBehavior = nsICookieService::BEHAVIOR_ACCEPT;
      *aLifetimePolicy = nsICookieService::ACCEPT_SESSION;
      break;
    case nsICookiePermission::ACCESS_ALLOW_FIRST_PARTY_ONLY:
      *aBehavior = nsICookieService::BEHAVIOR_REJECT_FOREIGN;
      // NOTE: The decision was made here to override the lifetime policy to be
      // ACCEPT_NORMALLY for consistency with ACCESS_ALLOW, but this does
      // prevent us from expressing BEHAVIOR_REJECT_FOREIGN/ACCEPT_SESSION for a
      // specific domain. As BEHAVIOR_REJECT_FOREIGN isn't visible in our UI,
      // this is probably not an issue.
      *aLifetimePolicy = nsICookieService::ACCEPT_NORMALLY;
      break;
    case nsICookiePermission::ACCESS_LIMIT_THIRD_PARTY:
      *aBehavior = nsICookieService::BEHAVIOR_LIMIT_FOREIGN;
      // NOTE: The decision was made here to override the lifetime policy to be
      // ACCEPT_NORMALLY for consistency with ACCESS_ALLOW, but this does
      // prevent us from expressing BEHAVIOR_REJECT_FOREIGN/ACCEPT_SESSION for a
      // specific domain. As BEHAVIOR_LIMIT_FOREIGN isn't visible in our UI,
      // this is probably not an issue.
      *aLifetimePolicy = nsICookieService::ACCEPT_NORMALLY;
      break;
  }
}

// static, private
nsContentUtils::StorageAccess
nsContentUtils::InternalStorageAllowedForPrincipal(nsIPrincipal* aPrincipal,
                                                   nsPIDOMWindowInner* aWindow)
{
  MOZ_ASSERT(aPrincipal);

  StorageAccess access = StorageAccess::eAllow;

  // We don't allow storage on the null principal, in general. Even if the
  // calling context is chrome.
  if (aPrincipal->GetIsNullPrincipal()) {
    return StorageAccess::eDeny;
  }

  if (aWindow) {
    // If the document is sandboxed, then it is not permitted to use storage
    nsIDocument* document = aWindow->GetExtantDoc();
    if (document && document->GetSandboxFlags() & SANDBOXED_ORIGIN) {
      return StorageAccess::eDeny;
    }

    // Check if we are in private browsing, and record that fact
    if (IsInPrivateBrowsing(document)) {
      access = StorageAccess::ePrivateBrowsing;
    }
  }

  uint32_t lifetimePolicy;
  uint32_t behavior;

  // WebExtensions principals always get BEHAVIOR_ACCEPT as cookieBehavior
  // and ACCEPT_NORMALLY as lifetimePolicy (See Bug 1406675 for rationale).
  auto policy = BasePrincipal::Cast(aPrincipal)->AddonPolicy();

  if (policy) {
    behavior = nsICookieService::BEHAVIOR_ACCEPT;
    lifetimePolicy = nsICookieService::ACCEPT_NORMALLY;
  } else {
    GetCookieBehaviorForPrincipal(aPrincipal, &lifetimePolicy, &behavior);
  }

  // Check if we should only allow storage for the session, and record that fact
  if (lifetimePolicy == nsICookieService::ACCEPT_SESSION) {
    // Storage could be StorageAccess::ePrivateBrowsing or StorageAccess::eAllow
    // so perform a std::min comparison to make sure we preserve ePrivateBrowsing
    // if it has been set.
    access = std::min(StorageAccess::eSessionScoped, access);
  }

  // About URIs are allowed to access storage, even if they don't have chrome
  // privileges. If this is not desired, than the consumer will have to
  // implement their own restriction functionality.
  //
  // This is due to backwards-compatibility and the state of storage access before
  // the introducton of nsContentUtils::InternalStorageAllowedForPrincipal:
  //
  // BEFORE:
  // localStorage, caches: allowed in 3rd-party iframes always
  // IndexedDB: allowed in 3rd-party iframes only if 3rd party URI is an about:
  //   URI within a specific whitelist
  //
  // AFTER:
  // localStorage, caches: allowed in 3rd-party iframes by default. Preference
  //   can be set to disable in 3rd-party, which will not disallow in about: URIs.
  // IndexedDB: allowed in 3rd-party iframes by default. Preference can be set to
  //   disable in 3rd-party, which will disallow in about: URIs, unless they are
  //   within a specific whitelist.
  //
  // This means that behavior for storage with internal about: URIs should not be
  // affected, which is desireable due to the lack of automated testing for about:
  // URIs with these preferences set, and the importance of the correct functioning
  // of these URIs even with custom preferences.
  nsCOMPtr<nsIURI> uri;
  nsresult rv = aPrincipal->GetURI(getter_AddRefs(uri));
  if (NS_SUCCEEDED(rv) && uri) {
    bool isAbout = false;
    MOZ_ALWAYS_SUCCEEDS(uri->SchemeIs("about", &isAbout));
    if (isAbout) {
      return access;
    }
  }

  // We don't want to prompt for every attempt to access permissions.
  if (behavior == nsICookieService::BEHAVIOR_REJECT) {
    return StorageAccess::eDeny;
  }

  // In the absense of a window, we assume that we are first-party.
  if (aWindow && (behavior == nsICookieService::BEHAVIOR_REJECT_FOREIGN ||
                  behavior == nsICookieService::BEHAVIOR_LIMIT_FOREIGN)) {
    nsCOMPtr<mozIThirdPartyUtil> thirdPartyUtil =
      do_GetService(THIRDPARTYUTIL_CONTRACTID);
    MOZ_ASSERT(thirdPartyUtil);

    bool thirdPartyWindow = false;
    if (NS_SUCCEEDED(thirdPartyUtil->IsThirdPartyWindow(
          aWindow->GetOuterWindow(), nullptr, &thirdPartyWindow)) && thirdPartyWindow) {
      // XXX For non-cookie forms of storage, we handle BEHAVIOR_LIMIT_FOREIGN by
      // simply rejecting the request to use the storage. In the future, if we
      // change the meaning of BEHAVIOR_LIMIT_FOREIGN to be one which makes sense
      // for non-cookie storage types, this may change.

      return StorageAccess::eDeny;
    }
  }

  return access;
}

namespace {

// We put StringBuilder in the anonymous namespace to prevent anything outside
// this file from accidentally being linked against it.

class StringBuilder
{
private:
  // Try to keep the size of StringBuilder close to a jemalloc bucket size.
  static const uint32_t STRING_BUFFER_UNITS = 1020;
  class Unit
  {
  public:
    Unit() : mAtom(nullptr), mType(eUnknown), mLength(0)
    {
      MOZ_COUNT_CTOR(StringBuilder::Unit);
    }
    ~Unit()
    {
      if (mType == eString || mType == eStringWithEncode) {
        delete mString;
      }
      MOZ_COUNT_DTOR(StringBuilder::Unit);
    }

    enum Type
    {
      eUnknown,
      eAtom,
      eString,
      eStringWithEncode,
      eLiteral,
      eTextFragment,
      eTextFragmentWithEncode,
    };

    union
    {
      nsAtom*              mAtom;
      const char*           mLiteral;
      nsAutoString*         mString;
      const nsTextFragment* mTextFragment;
    };
    Type     mType;
    uint32_t mLength;
  };
public:
  StringBuilder() : mLast(this), mLength(0)
  {
    MOZ_COUNT_CTOR(StringBuilder);
  }

  ~StringBuilder()
  {
    MOZ_COUNT_DTOR(StringBuilder);
  }

  void Append(nsAtom* aAtom)
  {
    Unit* u = AddUnit();
    u->mAtom = aAtom;
    u->mType = Unit::eAtom;
    uint32_t len = aAtom->GetLength();
    u->mLength = len;
    mLength += len;
  }

  template<int N>
  void Append(const char (&aLiteral)[N])
  {
    Unit* u = AddUnit();
    u->mLiteral = aLiteral;
    u->mType = Unit::eLiteral;
    uint32_t len = N - 1;
    u->mLength = len;
    mLength += len;
  }

  template<int N>
  void Append(char (&aLiteral)[N])
  {
    Unit* u = AddUnit();
    u->mLiteral = aLiteral;
    u->mType = Unit::eLiteral;
    uint32_t len = N - 1;
    u->mLength = len;
    mLength += len;
  }

  void Append(const nsAString& aString)
  {
    Unit* u = AddUnit();
    u->mString = new nsAutoString(aString);
    u->mType = Unit::eString;
    uint32_t len = aString.Length();
    u->mLength = len;
    mLength += len;
  }

  void Append(nsAutoString* aString)
  {
    Unit* u = AddUnit();
    u->mString = aString;
    u->mType = Unit::eString;
    uint32_t len = aString->Length();
    u->mLength = len;
    mLength += len;
  }

  void AppendWithAttrEncode(nsAutoString* aString, uint32_t aLen)
  {
    Unit* u = AddUnit();
    u->mString = aString;
    u->mType = Unit::eStringWithEncode;
    u->mLength = aLen;
    mLength += aLen;
  }

  void Append(const nsTextFragment* aTextFragment)
  {
    Unit* u = AddUnit();
    u->mTextFragment = aTextFragment;
    u->mType = Unit::eTextFragment;
    uint32_t len = aTextFragment->GetLength();
    u->mLength = len;
    mLength += len;
  }

  void AppendWithEncode(const nsTextFragment* aTextFragment, uint32_t aLen)
  {
    Unit* u = AddUnit();
    u->mTextFragment = aTextFragment;
    u->mType = Unit::eTextFragmentWithEncode;
    u->mLength = aLen;
    mLength += aLen;
  }

  bool ToString(nsAString& aOut)
  {
    if (!aOut.SetCapacity(mLength, fallible)) {
      return false;
    }

    for (StringBuilder* current = this; current; current = current->mNext) {
      uint32_t len = current->mUnits.Length();
      for (uint32_t i = 0; i < len; ++i) {
        Unit& u = current->mUnits[i];
        switch (u.mType) {
          case Unit::eAtom:
            aOut.Append(nsDependentAtomString(u.mAtom));
            break;
          case Unit::eString:
            aOut.Append(*(u.mString));
            break;
          case Unit::eStringWithEncode:
            EncodeAttrString(*(u.mString), aOut);
            break;
          case Unit::eLiteral:
            aOut.AppendASCII(u.mLiteral, u.mLength);
            break;
          case Unit::eTextFragment:
            u.mTextFragment->AppendTo(aOut);
            break;
          case Unit::eTextFragmentWithEncode:
            EncodeTextFragment(u.mTextFragment, aOut);
            break;
          default:
            MOZ_CRASH("Unknown unit type?");
        }
      }
    }
    return true;
  }
private:
  Unit* AddUnit()
  {
    if (mLast->mUnits.Length() == STRING_BUFFER_UNITS) {
      new StringBuilder(this);
    }
    return mLast->mUnits.AppendElement();
  }

  explicit StringBuilder(StringBuilder* aFirst)
  : mLast(nullptr), mLength(0)
  {
    MOZ_COUNT_CTOR(StringBuilder);
    aFirst->mLast->mNext = this;
    aFirst->mLast = this;
  }

  void EncodeAttrString(const nsAutoString& aValue, nsAString& aOut)
  {
    const char16_t* c = aValue.BeginReading();
    const char16_t* end = aValue.EndReading();
    while (c < end) {
      switch (*c) {
      case '"':
        aOut.AppendLiteral("&quot;");
        break;
      case '&':
        aOut.AppendLiteral("&amp;");
        break;
      case 0x00A0:
        aOut.AppendLiteral("&nbsp;");
        break;
      default:
        aOut.Append(*c);
        break;
      }
      ++c;
    }
  }

  void EncodeTextFragment(const nsTextFragment* aValue, nsAString& aOut)
  {
    uint32_t len = aValue->GetLength();
    if (aValue->Is2b()) {
      const char16_t* data = aValue->Get2b();
      for (uint32_t i = 0; i < len; ++i) {
        const char16_t c = data[i];
        switch (c) {
          case '<':
            aOut.AppendLiteral("&lt;");
            break;
          case '>':
            aOut.AppendLiteral("&gt;");
            break;
          case '&':
            aOut.AppendLiteral("&amp;");
            break;
          case 0x00A0:
            aOut.AppendLiteral("&nbsp;");
            break;
          default:
            aOut.Append(c);
            break;
        }
      }
    } else {
      const char* data = aValue->Get1b();
      for (uint32_t i = 0; i < len; ++i) {
        const unsigned char c = data[i];
        switch (c) {
          case '<':
            aOut.AppendLiteral("&lt;");
            break;
          case '>':
            aOut.AppendLiteral("&gt;");
            break;
          case '&':
            aOut.AppendLiteral("&amp;");
            break;
          case 0x00A0:
            aOut.AppendLiteral("&nbsp;");
            break;
          default:
            aOut.Append(c);
            break;
        }
      }
    }
  }

  AutoTArray<Unit, STRING_BUFFER_UNITS> mUnits;
  nsAutoPtr<StringBuilder>                mNext;
  StringBuilder*                          mLast;
  // mLength is used only in the first StringBuilder object in the linked list.
  uint32_t                                mLength;
};

} // namespace

static void
AppendEncodedCharacters(const nsTextFragment* aText, StringBuilder& aBuilder)
{
  uint32_t extraSpaceNeeded = 0;
  uint32_t len = aText->GetLength();
  if (aText->Is2b()) {
    const char16_t* data = aText->Get2b();
    for (uint32_t i = 0; i < len; ++i) {
      const char16_t c = data[i];
      switch (c) {
        case '<':
          extraSpaceNeeded += ArrayLength("&lt;") - 2;
          break;
        case '>':
          extraSpaceNeeded += ArrayLength("&gt;") - 2;
          break;
        case '&':
          extraSpaceNeeded += ArrayLength("&amp;") - 2;
          break;
        case 0x00A0:
          extraSpaceNeeded += ArrayLength("&nbsp;") - 2;
          break;
        default:
          break;
      }
    }
  } else {
    const char* data = aText->Get1b();
    for (uint32_t i = 0; i < len; ++i) {
      const unsigned char c = data[i];
      switch (c) {
        case '<':
          extraSpaceNeeded += ArrayLength("&lt;") - 2;
          break;
        case '>':
          extraSpaceNeeded += ArrayLength("&gt;") - 2;
          break;
        case '&':
          extraSpaceNeeded += ArrayLength("&amp;") - 2;
          break;
        case 0x00A0:
          extraSpaceNeeded += ArrayLength("&nbsp;") - 2;
          break;
        default:
          break;
      }
    }
  }

  if (extraSpaceNeeded) {
    aBuilder.AppendWithEncode(aText, len + extraSpaceNeeded);
  } else {
    aBuilder.Append(aText);
  }
}

static void
AppendEncodedAttributeValue(nsAutoString* aValue, StringBuilder& aBuilder)
{
  const char16_t* c = aValue->BeginReading();
  const char16_t* end = aValue->EndReading();

  uint32_t extraSpaceNeeded = 0;
  while (c < end) {
    switch (*c) {
      case '"':
        extraSpaceNeeded += ArrayLength("&quot;") - 2;
        break;
      case '&':
        extraSpaceNeeded += ArrayLength("&amp;") - 2;
        break;
      case 0x00A0:
        extraSpaceNeeded += ArrayLength("&nbsp;") - 2;
        break;
      default:
        break;
    }
    ++c;
  }

  if (extraSpaceNeeded) {
    aBuilder.AppendWithAttrEncode(aValue, aValue->Length() + extraSpaceNeeded);
  } else {
    aBuilder.Append(aValue);
  }
}

static void
StartElement(Element* aContent, StringBuilder& aBuilder)
{
  nsAtom* localName = aContent->NodeInfo()->NameAtom();
  int32_t tagNS = aContent->GetNameSpaceID();

  aBuilder.Append("<");
  if (aContent->IsHTMLElement() || aContent->IsSVGElement() ||
      aContent->IsMathMLElement()) {
    aBuilder.Append(localName);
  } else {
    aBuilder.Append(aContent->NodeName());
  }

  int32_t count = aContent->GetAttrCount();
  for (int32_t i = 0; i < count; i++) {
    const nsAttrName* name = aContent->GetAttrNameAt(i);
    int32_t attNs = name->NamespaceID();
    nsAtom* attName = name->LocalName();

    // Filter out any attribute starting with [-|_]moz
    nsDependentAtomString attrNameStr(attName);
    if (StringBeginsWith(attrNameStr, NS_LITERAL_STRING("_moz")) ||
        StringBeginsWith(attrNameStr, NS_LITERAL_STRING("-moz"))) {
      continue;
    }

    auto* attValue = new nsAutoString();
    aContent->GetAttr(attNs, attName, *attValue);

    // Filter out special case of <br type="_moz*"> used by the editor.
    // Bug 16988.  Yuck.
    if (localName == nsGkAtoms::br && tagNS == kNameSpaceID_XHTML &&
        attName == nsGkAtoms::type && attNs == kNameSpaceID_None &&
        StringBeginsWith(*attValue, NS_LITERAL_STRING("_moz"))) {
      delete attValue;
      continue;
    }

    aBuilder.Append(" ");

    if (MOZ_LIKELY(attNs == kNameSpaceID_None) ||
        (attNs == kNameSpaceID_XMLNS &&
         attName == nsGkAtoms::xmlns)) {
      // Nothing else required
    } else if (attNs == kNameSpaceID_XML) {
      aBuilder.Append("xml:");
    } else if (attNs == kNameSpaceID_XMLNS) {
      aBuilder.Append("xmlns:");
    } else if (attNs == kNameSpaceID_XLink) {
      aBuilder.Append("xlink:");
    } else {
      nsAtom* prefix = name->GetPrefix();
      if (prefix) {
        aBuilder.Append(prefix);
        aBuilder.Append(":");
      }
    }

    aBuilder.Append(attName);
    aBuilder.Append(R"(=")");
    AppendEncodedAttributeValue(attValue, aBuilder);
    aBuilder.Append(R"(")");
  }

  aBuilder.Append(">");

  /*
  // Per HTML spec we should append one \n if the first child of
  // pre/textarea/listing is a textnode and starts with a \n.
  // But because browsers haven't traditionally had that behavior,
  // we're not changing our behavior either - yet.
  if (aContent->IsHTMLElement()) {
    if (localName == nsGkAtoms::pre || localName == nsGkAtoms::textarea ||
        localName == nsGkAtoms::listing) {
      nsIContent* fc = aContent->GetFirstChild();
      if (fc &&
          (fc->NodeType() == nsIDOMNode::TEXT_NODE ||
           fc->NodeType() == nsIDOMNode::CDATA_SECTION_NODE)) {
        const nsTextFragment* text = fc->GetText();
        if (text && text->GetLength() && text->CharAt(0) == char16_t('\n')) {
          aBuilder.Append("\n");
        }
      }
    }
  }*/
}

static inline bool
ShouldEscape(nsIContent* aParent)
{
  if (!aParent || !aParent->IsHTMLElement()) {
    return true;
  }

  static const nsAtom* nonEscapingElements[] = {
    nsGkAtoms::style, nsGkAtoms::script, nsGkAtoms::xmp,
    nsGkAtoms::iframe, nsGkAtoms::noembed, nsGkAtoms::noframes,
    nsGkAtoms::plaintext,
    // Per the current spec noscript should be escaped in case
    // scripts are disabled or if document doesn't have
    // browsing context. However the latter seems to be a spec bug
    // and Gecko hasn't traditionally done the former.
    nsGkAtoms::noscript
  };
  static mozilla::BloomFilter<12, nsAtom> sFilter;
  static bool sInitialized = false;
  if (!sInitialized) {
    sInitialized = true;
    for (auto& nonEscapingElement : nonEscapingElements) {
      sFilter.add(nonEscapingElement);
    }
  }

  nsAtom* tag = aParent->NodeInfo()->NameAtom();
  if (sFilter.mightContain(tag)) {
    for (auto& nonEscapingElement : nonEscapingElements) {
      if (tag == nonEscapingElement) {
        return false;
      }
    }
  }
  return true;
}

static inline bool
IsVoidTag(Element* aElement)
{
  if (!aElement->IsHTMLElement()) {
    return false;
  }
  return FragmentOrElement::IsHTMLVoid(aElement->NodeInfo()->NameAtom());
}

bool
nsContentUtils::SerializeNodeToMarkup(nsINode* aRoot,
                                      bool aDescendentsOnly,
                                      nsAString& aOut)
{
  // If you pass in a DOCUMENT_NODE, you must pass aDescendentsOnly as true
  MOZ_ASSERT(aDescendentsOnly ||
             aRoot->NodeType() != nsIDOMNode::DOCUMENT_NODE);

  nsINode* current = aDescendentsOnly ?
    nsNodeUtils::GetFirstChildOfTemplateOrNode(aRoot) : aRoot;

  if (!current) {
    return true;
  }

  StringBuilder builder;
  nsIContent* next;
  while (true) {
    bool isVoid = false;
    switch (current->NodeType()) {
      case nsIDOMNode::ELEMENT_NODE: {
        Element* elem = current->AsElement();
        StartElement(elem, builder);
        isVoid = IsVoidTag(elem);
        if (!isVoid &&
            (next = nsNodeUtils::GetFirstChildOfTemplateOrNode(current))) {
          current = next;
          continue;
        }
        break;
      }

      case nsIDOMNode::TEXT_NODE:
      case nsIDOMNode::CDATA_SECTION_NODE: {
        const nsTextFragment* text = static_cast<nsIContent*>(current)->GetText();
        nsIContent* parent = current->GetParent();
        if (ShouldEscape(parent)) {
          AppendEncodedCharacters(text, builder);
        } else {
          builder.Append(text);
        }
        break;
      }

      case nsIDOMNode::COMMENT_NODE: {
        builder.Append("<!--");
        builder.Append(static_cast<nsIContent*>(current)->GetText());
        builder.Append("-->");
        break;
      }

      case nsIDOMNode::DOCUMENT_TYPE_NODE: {
        builder.Append("<!DOCTYPE ");
        builder.Append(current->NodeName());
        builder.Append(">");
        break;
      }

      case nsIDOMNode::PROCESSING_INSTRUCTION_NODE: {
        builder.Append("<?");
        builder.Append(current->NodeName());
        builder.Append(" ");
        builder.Append(static_cast<nsIContent*>(current)->GetText());
        builder.Append(">");
        break;
      }
    }

    while (true) {
      if (!isVoid && current->NodeType() == nsIDOMNode::ELEMENT_NODE) {
        builder.Append("</");
        nsIContent* elem = static_cast<nsIContent*>(current);
        if (elem->IsHTMLElement() || elem->IsSVGElement() ||
            elem->IsMathMLElement()) {
          builder.Append(elem->NodeInfo()->NameAtom());
        } else {
          builder.Append(current->NodeName());
        }
        builder.Append(">");
      }
      isVoid = false;

      if (current == aRoot) {
        return builder.ToString(aOut);
      }

      if ((next = current->GetNextSibling())) {
        current = next;
        break;
      }

      current = current->GetParentNode();

      // Handle template element. If the parent is a template's content,
      // then adjust the parent to be the template element.
      if (current != aRoot &&
          current->NodeType() == nsIDOMNode::DOCUMENT_FRAGMENT_NODE) {
        DocumentFragment* frag = static_cast<DocumentFragment*>(current);
        nsIContent* fragHost = frag->GetHost();
        if (fragHost && nsNodeUtils::IsTemplateElement(fragHost)) {
          current = fragHost;
        }
      }

      if (aDescendentsOnly && current == aRoot) {
        return builder.ToString(aOut);
      }
    }
  }
}

bool
nsContentUtils::IsSpecificAboutPage(JSObject* aGlobal, const char* aUri)
{
  // aUri must start with about: or this isn't the right function to be using.
  MOZ_ASSERT(strncmp(aUri, "about:", 6) == 0);

  // Make sure the global is a window
  nsGlobalWindowInner* win = xpc::WindowGlobalOrNull(aGlobal);
  if (!win) {
    return false;
  }

  nsCOMPtr<nsIPrincipal> principal = win->GetPrincipal();
  NS_ENSURE_TRUE(principal, false);
  nsCOMPtr<nsIURI> uri;
  principal->GetURI(getter_AddRefs(uri));
  if (!uri) {
    return false;
  }

  // First check the scheme to avoid getting long specs in the common case.
  bool isAbout = false;
  uri->SchemeIs("about", &isAbout);
  if (!isAbout) {
    return false;
  }

  // Now check the spec itself
  nsAutoCString spec;
  uri->GetSpecIgnoringRef(spec);
  return spec.EqualsASCII(aUri);
}

/* static */ void
nsContentUtils::SetScrollbarsVisibility(nsIDocShell* aDocShell, bool aVisible)
{
  nsCOMPtr<nsIScrollable> scroller = do_QueryInterface(aDocShell);

  if (scroller) {
    int32_t prefValue;

    if (aVisible) {
      prefValue = nsIScrollable::Scrollbar_Auto;
    } else {
      prefValue = nsIScrollable::Scrollbar_Never;
    }

    scroller->SetDefaultScrollbarPreferences(
                nsIScrollable::ScrollOrientation_Y, prefValue);
    scroller->SetDefaultScrollbarPreferences(
                nsIScrollable::ScrollOrientation_X, prefValue);
  }
}

/* static */ void
nsContentUtils::GetPresentationURL(nsIDocShell* aDocShell, nsAString& aPresentationUrl)
{
  MOZ_ASSERT(aDocShell);

  // Simulate receiver context for web platform test
  if (Preferences::GetBool("dom.presentation.testing.simulate-receiver")) {
    nsCOMPtr<nsIDocument> doc;

    nsCOMPtr<nsPIDOMWindowOuter> docShellWin =
      do_QueryInterface(aDocShell->GetScriptGlobalObject());
    if (docShellWin) {
      doc = docShellWin->GetExtantDoc();
    }

    if (NS_WARN_IF(!doc)) {
      return;
    }

    nsCOMPtr<nsIURI> uri = doc->GetDocumentURI();
    if (NS_WARN_IF(!uri)) {
      return;
    }

    nsAutoCString uriStr;
    uri->GetSpec(uriStr);
    aPresentationUrl = NS_ConvertUTF8toUTF16(uriStr);
    return;
  }

  if (XRE_IsContentProcess()) {
    nsCOMPtr<nsIDocShellTreeItem> sameTypeRoot;
    aDocShell->GetSameTypeRootTreeItem(getter_AddRefs(sameTypeRoot));
    nsCOMPtr<nsIDocShellTreeItem> root;
    aDocShell->GetRootTreeItem(getter_AddRefs(root));
    if (sameTypeRoot.get() == root.get()) {
      // presentation URL is stored in TabChild for the top most
      // <iframe mozbrowser> in content process.
      TabChild* tabChild = TabChild::GetFrom(aDocShell);
      if (tabChild) {
        aPresentationUrl = tabChild->PresentationURL();
      }
      return;
    }
  }

  nsCOMPtr<nsILoadContext> loadContext(do_QueryInterface(aDocShell));
  nsCOMPtr<nsIDOMElement> topFrameElement;
  loadContext->GetTopFrameElement(getter_AddRefs(topFrameElement));
  if (!topFrameElement) {
    return;
  }

  topFrameElement->GetAttribute(NS_LITERAL_STRING("mozpresentation"), aPresentationUrl);
}

/* static */ nsIDocShell*
nsContentUtils::GetDocShellForEventTarget(EventTarget* aTarget)
{
  nsCOMPtr<nsPIDOMWindowInner> innerWindow;

  if (nsCOMPtr<nsINode> node = do_QueryInterface(aTarget)) {
    bool ignore;
    innerWindow =
      do_QueryInterface(node->OwnerDoc()->GetScriptHandlingObject(ignore));
  } else if ((innerWindow = do_QueryInterface(aTarget))) {
    // Nothing else to do
  } else {
    nsCOMPtr<DOMEventTargetHelper> helper = do_QueryInterface(aTarget);
    if (helper) {
      innerWindow = helper->GetOwner();
    }
  }

  if (innerWindow) {
    return innerWindow->GetDocShell();
  }

  return nullptr;
}

/*
 * Note: this function only relates to figuring out HTTPS state, which is an
 * input to the Secure Context algorithm.  We are not actually implementing any
 * part of the Secure Context algorithm itself here.
 *
 * This is a bit of a hack.  Ideally we'd propagate HTTPS state through
 * nsIChannel as described in the Fetch and HTML specs, but making channels
 * know about whether they should inherit HTTPS state, propagating information
 * about who the channel's "client" is, exposing GetHttpsState API on channels
 * and modifying the various cache implementations to store and retrieve HTTPS
 * state involves a huge amount of code (see bug 1220687).  We avoid that for
 * now using this function.
 *
 * This function takes advantage of the observation that we can return true if
 * nsIContentSecurityManager::IsOriginPotentiallyTrustworthy returns true for
 * the document's origin (e.g. the origin has a scheme of 'https' or host
 * 'localhost' etc.).  Since we generally propagate a creator document's origin
 * onto data:, blob:, etc. documents, this works for them too.
 *
 * The scenario where this observation breaks down is sandboxing without the
 * 'allow-same-origin' flag, since in this case a document is given a unique
 * origin (IsOriginPotentiallyTrustworthy would return false).  We handle that
 * by using the origin that the document would have had had it not been
 * sandboxed.
 *
 * DEFICIENCIES: Note that this function uses nsIScriptSecurityManager's
 * getChannelResultPrincipalIfNotSandboxed, and that method's ignoring of
 * sandboxing is limited to the immediate sandbox.  In the case that aDocument
 * should inherit its origin (e.g. data: URI) but its parent has ended up
 * with a unique origin due to sandboxing further up the parent chain we may
 * end up returning false when we would ideally return true (since we will
 * examine the parent's origin for 'https' and not finding it.)  This means
 * that we may restrict the privileges of some pages unnecessarily in this
 * edge case.
 */
/* static */ bool
nsContentUtils::HttpsStateIsModern(nsIDocument* aDocument)
{
  if (!aDocument) {
    return false;
  }

  nsCOMPtr<nsIPrincipal> principal = aDocument->NodePrincipal();

  if (principal->GetIsSystemPrincipal()) {
    return true;
  }

  // If aDocument is sandboxed, try and get the principal that it would have
  // been given had it not been sandboxed:
  if (principal->GetIsNullPrincipal() &&
      (aDocument->GetSandboxFlags() & SANDBOXED_ORIGIN)) {
    nsIChannel* channel = aDocument->GetChannel();
    if (channel) {
      nsCOMPtr<nsIScriptSecurityManager> ssm =
        nsContentUtils::GetSecurityManager();
      nsresult rv =
        ssm->GetChannelResultPrincipalIfNotSandboxed(channel,
                                                     getter_AddRefs(principal));
      if (NS_FAILED(rv)) {
        return false;
      }
      if (principal->GetIsSystemPrincipal()) {
        // If a document with the system principal is sandboxing a subdocument
        // that would normally inherit the embedding element's principal (e.g.
        // a srcdoc document) then the embedding document does not trust the
        // content that is written to the embedded document.  Unlike when the
        // embedding document is https, in this case we have no indication as
        // to whether the embedded document's contents are delivered securely
        // or not, and the sandboxing would possibly indicate that they were
        // not.  To play it safe we return false here.  (See bug 1162772
        // comment 73-80.)
        return false;
      }
    }
  }

  if (principal->GetIsNullPrincipal()) {
    return false;
  }

  MOZ_ASSERT(principal->GetIsCodebasePrincipal());

  nsCOMPtr<nsIContentSecurityManager> csm =
    do_GetService(NS_CONTENTSECURITYMANAGER_CONTRACTID);
  NS_WARNING_ASSERTION(csm, "csm is null");
  if (csm) {
    bool isTrustworthyOrigin = false;
    csm->IsOriginPotentiallyTrustworthy(principal, &isTrustworthyOrigin);
    if (isTrustworthyOrigin) {
      return true;
    }
  }

  return false;
}

/* static */ void
nsContentUtils::TryToUpgradeElement(Element* aElement)
{
  NodeInfo* nodeInfo = aElement->NodeInfo();
  RefPtr<nsAtom> typeAtom =
    aElement->GetCustomElementData()->GetCustomElementType();
  CustomElementDefinition* definition =
    nsContentUtils::LookupCustomElementDefinition(nodeInfo->GetDocument(),
                                                  nodeInfo->LocalName(),
                                                  nodeInfo->NamespaceID(),
                                                  typeAtom);
  if (definition) {
    nsContentUtils::EnqueueUpgradeReaction(aElement, definition);
  } else {
    // Add an unresolved custom element that is a candidate for
    // upgrade when a custom element is connected to the document.
    // We will make sure it's shadow-including tree order in bug 1326028.
    nsContentUtils::RegisterUnresolvedElement(aElement, typeAtom);
  }
}

static void
DoCustomElementCreate(Element** aElement, nsIDocument* aDoc, NodeInfo* aNodeInfo,
                      CustomElementConstructor* aConstructor, ErrorResult& aRv)
{
  RefPtr<Element> element =
    aConstructor->Construct("Custom Element Create", aRv);
  if (aRv.Failed()) {
    return;
  }

  if (aNodeInfo->NamespaceEquals(kNameSpaceID_XHTML)) {
    if (!element || !element->IsHTMLElement()) {
      aRv.ThrowTypeError<MSG_THIS_DOES_NOT_IMPLEMENT_INTERFACE>(NS_LITERAL_STRING("HTMLElement"));
      return;
    }
  } else {
    if (!element || !element->IsXULElement()) {
      aRv.ThrowTypeError<MSG_THIS_DOES_NOT_IMPLEMENT_INTERFACE>(NS_LITERAL_STRING("XULElement"));
      return;
    }
  }

  nsAtom* localName = aNodeInfo->NameAtom();

  if (aDoc != element->OwnerDoc() || element->GetParentNode() ||
      element->HasChildren() || element->GetAttrCount() ||
      element->NodeInfo()->NameAtom() != localName) {
    aRv.Throw(NS_ERROR_DOM_NOT_SUPPORTED_ERR);
    return;
  }

  element.forget(aElement);
}

/* static */ nsresult
nsContentUtils::NewXULOrHTMLElement(Element** aResult, mozilla::dom::NodeInfo* aNodeInfo,
                                    FromParser aFromParser, const nsAString* aIs,
                                    mozilla::dom::CustomElementDefinition* aDefinition)
{
  RefPtr<mozilla::dom::NodeInfo> nodeInfo = aNodeInfo;
  MOZ_ASSERT(nodeInfo->NamespaceEquals(kNameSpaceID_XHTML) ||
             nodeInfo->NamespaceEquals(kNameSpaceID_XUL),
             "Can only create XUL or XHTML elements.");

  nsAtom *name = nodeInfo->NameAtom();
  RefPtr<nsAtom> tagAtom = nodeInfo->NameAtom();
  RefPtr<nsAtom> typeAtom = aIs ? NS_Atomize(*aIs) : tagAtom;

  int32_t tag = eHTMLTag_unknown;
  bool isCustomElementName = false;
  if (nodeInfo->NamespaceEquals(kNameSpaceID_XHTML)) {
    tag = nsHTMLTags::CaseSensitiveAtomTagToId(name);
    isCustomElementName = (tag == eHTMLTag_userdefined &&
                           nsContentUtils::IsCustomElementName(name));
  } else {
    isCustomElementName = nsContentUtils::IsCustomElementName(name);
  }
  bool isCustomElement = isCustomElementName || aIs;
  MOZ_ASSERT_IF(aDefinition, isCustomElement);

  // https://dom.spec.whatwg.org/#concept-create-element
  // We only handle the "synchronous custom elements flag is set" now.
  // For the unset case (e.g. cloning a node), see bug 1319342 for that.
  // Step 4.
  CustomElementDefinition* definition = aDefinition;
  if (CustomElementRegistry::IsCustomElementEnabled() && isCustomElement &&
      !definition) {
    definition =
      nsContentUtils::LookupCustomElementDefinition(nodeInfo->GetDocument(),
                                                    nodeInfo->LocalName(),
                                                    nodeInfo->NamespaceID(),
                                                    typeAtom);
  }

  // It might be a problem that parser synchronously calls constructor, so filed
  // bug 1378079 to figure out what we should do for parser case.
  if (definition) {
    /*
      * Synchronous custom elements flag is determined by 3 places in spec,
      * 1) create an element for a token, the flag is determined by
      *    "will execute script" which is not originally created
      *    for the HTML fragment parsing algorithm.
      * 2) createElement and createElementNS, the flag is the same as
      *    NOT_FROM_PARSER.
      * 3) clone a node, our implementation will not go into this function.
      * For the unset case which is non-synchronous only applied for
      * inner/outerHTML.
      */
    bool synchronousCustomElements = aFromParser != dom::FROM_PARSER_FRAGMENT ||
                                     aFromParser == dom::NOT_FROM_PARSER;
    // Per discussion in https://github.com/w3c/webcomponents/issues/635,
    // use entry global in those places that are called from JS APIs and use the
    // node document's global object if it is called from parser.
    nsIGlobalObject* global;
    if (aFromParser == dom::NOT_FROM_PARSER) {
      global = GetEntryGlobal();
    } else {
      global = nodeInfo->GetDocument()->GetScopeObject();
    }
    if (!global) {
      // In browser chrome code, one may have access to a document which doesn't
      // have scope object anymore.
      return NS_ERROR_FAILURE;
    }

    AutoEntryScript aes(global, "create custom elements");
    JSContext* cx = aes.cx();
    ErrorResult rv;

    // Step 5.
    if (definition->IsCustomBuiltIn()) {
      // SetupCustomElement() should be called with an element that don't have
      // CustomElementData setup, if not we will hit the assertion in
      // SetCustomElementData().
      // Built-in element
      MOZ_ASSERT(nodeInfo->NamespaceEquals(kNameSpaceID_XHTML),
                 "Custom built-in XUL elements are not supported yet.");
      *aResult = CreateHTMLElement(tag, nodeInfo.forget(), aFromParser).take();
      (*aResult)->SetCustomElementData(new CustomElementData(typeAtom));
      if (synchronousCustomElements) {
        CustomElementRegistry::Upgrade(*aResult, definition, rv);
        if (rv.MaybeSetPendingException(cx)) {
          aes.ReportException();
        }
      } else {
        nsContentUtils::EnqueueUpgradeReaction(*aResult, definition);
      }

      return NS_OK;
    }

    // Step 6.1.
    if (synchronousCustomElements) {
      DoCustomElementCreate(aResult, nodeInfo->GetDocument(), nodeInfo,
                            definition->mConstructor, rv);
      if (rv.MaybeSetPendingException(cx)) {
        if (nodeInfo->NamespaceEquals(kNameSpaceID_XHTML)) {
          NS_IF_ADDREF(*aResult = NS_NewHTMLUnknownElement(nodeInfo.forget(), aFromParser));
        } else {
          NS_IF_ADDREF(*aResult = new nsXULElement(nodeInfo.forget()));
        }
      }
      return NS_OK;
    }

    // Step 6.2.
    if (nodeInfo->NamespaceEquals(kNameSpaceID_XHTML)) {
      NS_IF_ADDREF(*aResult = NS_NewHTMLElement(nodeInfo.forget(), aFromParser));
    } else {
      NS_IF_ADDREF(*aResult = new nsXULElement(nodeInfo.forget()));
    }
    (*aResult)->SetCustomElementData(new CustomElementData(definition->mType));
    nsContentUtils::EnqueueUpgradeReaction(*aResult, definition);
    return NS_OK;
  }

  if (nodeInfo->NamespaceEquals(kNameSpaceID_XHTML)) {
    // Per the Custom Element specification, unknown tags that are valid custom
    // element names should be HTMLElement instead of HTMLUnknownElement.
    if (isCustomElementName) {
      NS_IF_ADDREF(*aResult = NS_NewHTMLElement(nodeInfo.forget(), aFromParser));
    } else {
      *aResult = CreateHTMLElement(tag, nodeInfo.forget(), aFromParser).take();
    }
  } else {
    NS_IF_ADDREF(*aResult = new nsXULElement(nodeInfo.forget()));
  }

  if (!*aResult) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  if (CustomElementRegistry::IsCustomElementEnabled() && isCustomElement) {
    (*aResult)->SetCustomElementData(new CustomElementData(typeAtom));
  }

  return NS_OK;
}

/* static */ CustomElementDefinition*
nsContentUtils::LookupCustomElementDefinition(nsIDocument* aDoc,
                                              const nsAString& aLocalName,
                                              uint32_t aNameSpaceID,
                                              nsAtom* aTypeAtom)
{
  MOZ_ASSERT(aDoc);

  if ((aNameSpaceID != kNameSpaceID_XUL &&
       aNameSpaceID != kNameSpaceID_XHTML) ||
      !aDoc->GetDocShell()) {
    return nullptr;
  }

  nsCOMPtr<nsPIDOMWindowInner> window(aDoc->GetInnerWindow());
  if (!window) {
    return nullptr;
  }

  RefPtr<CustomElementRegistry> registry(window->CustomElements());
  if (!registry) {
    return nullptr;
  }

  return registry->LookupCustomElementDefinition(aLocalName, aTypeAtom);
}

/* static */ void
nsContentUtils::RegisterUnresolvedElement(Element* aElement, nsAtom* aTypeName)
{
  MOZ_ASSERT(aElement);

  nsIDocument* doc = aElement->OwnerDoc();
  nsPIDOMWindowInner* window(doc->GetInnerWindow());
  if (!window) {
    return;
  }

  RefPtr<CustomElementRegistry> registry(window->CustomElements());
  if (!registry) {
    return;
  }

  registry->RegisterUnresolvedElement(aElement, aTypeName);
}

/* static */ void
nsContentUtils::UnregisterUnresolvedElement(Element* aElement)
{
  MOZ_ASSERT(aElement);

  RefPtr<nsAtom> typeAtom =
    aElement->GetCustomElementData()->GetCustomElementType();
  nsIDocument* doc = aElement->OwnerDoc();
  nsPIDOMWindowInner* window(doc->GetInnerWindow());
  if (!window) {
    return;
  }

  RefPtr<CustomElementRegistry> registry(window->CustomElements());
  if (!registry) {
    return;
  }

  registry->UnregisterUnresolvedElement(aElement, typeAtom);
}

/* static */ void
nsContentUtils::EnqueueUpgradeReaction(Element* aElement,
                                       CustomElementDefinition* aDefinition)
{
  MOZ_ASSERT(aElement);

  nsIDocument* doc = aElement->OwnerDoc();

  // No DocGroup means no custom element reactions stack.
  if (!doc->GetDocGroup()) {
    return;
  }

  CustomElementReactionsStack* stack =
    doc->GetDocGroup()->CustomElementReactionsStack();
  stack->EnqueueUpgradeReaction(aElement, aDefinition);
}

/* static */ void
nsContentUtils::EnqueueLifecycleCallback(nsIDocument::ElementCallbackType aType,
                                         Element* aCustomElement,
                                         LifecycleCallbackArgs* aArgs,
                                         LifecycleAdoptedCallbackArgs* aAdoptedCallbackArgs,
                                         CustomElementDefinition* aDefinition)
{
  // No DocGroup means no custom element reactions stack.
  if (!aCustomElement->OwnerDoc()->GetDocGroup()) {
    return;
  }

  CustomElementRegistry::EnqueueLifecycleCallback(aType, aCustomElement, aArgs,
                                                  aAdoptedCallbackArgs,
                                                  aDefinition);
}

/* static */ bool
nsContentUtils::AttemptLargeAllocationLoad(nsIHttpChannel* aChannel)
{
  MOZ_ASSERT(aChannel);

  nsCOMPtr<nsILoadGroup> loadGroup;
  nsresult rv = aChannel->GetLoadGroup(getter_AddRefs(loadGroup));
  if (NS_WARN_IF(NS_FAILED(rv) || !loadGroup)) {
    return false;
  }

  nsCOMPtr<nsIInterfaceRequestor> callbacks;
  rv = loadGroup->GetNotificationCallbacks(getter_AddRefs(callbacks));
  if (NS_WARN_IF(NS_FAILED(rv) || !callbacks)) {
    return false;
  }

  nsCOMPtr<nsILoadContext> loadContext = do_GetInterface(callbacks);
  if (NS_WARN_IF(!loadContext)) {
    return false;
  }

  nsCOMPtr<mozIDOMWindowProxy> window;
  rv = loadContext->GetAssociatedWindow(getter_AddRefs(window));
  if (NS_WARN_IF(NS_FAILED(rv) || !window)) {
    return false;
  }

  nsPIDOMWindowOuter* outer = nsPIDOMWindowOuter::From(window);
  if (NS_WARN_IF(!outer)) {
    return false;
  }

  if (!XRE_IsContentProcess()) {
    outer->SetLargeAllocStatus(LargeAllocStatus::NON_E10S);
    return false;
  }

  nsIDocShell* docShell = outer->GetDocShell();
  if (!docShell->GetIsOnlyToplevelInTabGroup()) {
    outer->SetLargeAllocStatus(LargeAllocStatus::NOT_ONLY_TOPLEVEL_IN_TABGROUP);
    return false;
  }

  // Get the request method, and check if it is a GET request. If it is not GET,
  // then we cannot perform a large allocation load.
  nsAutoCString requestMethod;
  rv = aChannel->GetRequestMethod(requestMethod);
  NS_ENSURE_SUCCESS(rv, false);

  if (NS_WARN_IF(!requestMethod.LowerCaseEqualsLiteral("get"))) {
    outer->SetLargeAllocStatus(LargeAllocStatus::NON_GET);
    return false;
  }

  TabChild* tabChild = TabChild::GetFrom(outer->AsOuter());
  NS_ENSURE_TRUE(tabChild, false);

  if (tabChild->IsAwaitingLargeAlloc())  {
    NS_WARNING("In a Large-Allocation TabChild, ignoring Large-Allocation header!");
    tabChild->StopAwaitingLargeAlloc();
    outer->SetLargeAllocStatus(LargeAllocStatus::SUCCESS);
    return false;
  }

  // On Win32 systems, we want to behave differently, so set the isWin32 bool to
  // be true iff we are on win32.
#if defined(XP_WIN) && defined(_X86_)
  const bool isWin32 = true;
#else
  const bool isWin32 = false;
#endif

  static bool sLargeAllocForceEnable = false;
  static bool sCachedLargeAllocForceEnable = false;
  if (!sCachedLargeAllocForceEnable) {
    sCachedLargeAllocForceEnable = true;
    mozilla::Preferences::AddBoolVarCache(&sLargeAllocForceEnable,
                                          "dom.largeAllocation.forceEnable");
  }

  // We want to enable the large allocation header on 32-bit windows machines,
  // and disable it on other machines, while still printing diagnostic messages.
  // dom.largeAllocation.forceEnable can allow you to enable the process
  // switching behavior of the Large-Allocation header on non 32-bit windows
  // machines.
  bool largeAllocEnabled = isWin32 || sLargeAllocForceEnable;
  if (!largeAllocEnabled) {
    NS_WARNING("dom.largeAllocation.forceEnable not set - "
               "ignoring otherwise successful Large-Allocation header.");
    // On platforms which aren't WIN32, we don't activate the largeAllocation
    // header, instead we simply emit diagnostics into the console.
    outer->SetLargeAllocStatus(LargeAllocStatus::NON_WIN32);
    return false;
  }

  // At this point the fress process load should succeed! We just need to get
  // ourselves a nsIWebBrowserChrome3 to ask to perform the reload. We should
  // have one, as we have already confirmed that we are running in a content
  // process.
  nsCOMPtr<nsIDocShellTreeOwner> treeOwner;
  docShell->GetTreeOwner(getter_AddRefs(treeOwner));
  NS_ENSURE_TRUE(treeOwner, false);

  nsCOMPtr<nsIWebBrowserChrome3> wbc3 = do_GetInterface(treeOwner);
  NS_ENSURE_TRUE(wbc3, false);

  nsCOMPtr<nsIURI> uri;
  rv = aChannel->GetURI(getter_AddRefs(uri));
  NS_ENSURE_SUCCESS(rv, false);
  NS_ENSURE_TRUE(uri, false);

  nsCOMPtr<nsIURI> referrer;
  rv = aChannel->GetReferrer(getter_AddRefs(referrer));
  NS_ENSURE_SUCCESS(rv, false);

  nsCOMPtr<nsILoadInfo> loadInfo = aChannel->GetLoadInfo();
  if (!loadInfo) {
    return false;
  }
  nsCOMPtr<nsIPrincipal> triggeringPrincipal = loadInfo->TriggeringPrincipal();

  // Get the channel's load flags, and use them to generate nsIWebNavigation
  // load flags. We want to make sure to propagate the refresh and cache busting
  // flags.
  nsLoadFlags channelLoadFlags;
  aChannel->GetLoadFlags(&channelLoadFlags);

  uint32_t webnavLoadFlags = nsIWebNavigation::LOAD_FLAGS_NONE;
  if (channelLoadFlags & nsIRequest::LOAD_BYPASS_CACHE) {
    webnavLoadFlags |= nsIWebNavigation::LOAD_FLAGS_BYPASS_CACHE;
    webnavLoadFlags |= nsIWebNavigation::LOAD_FLAGS_BYPASS_PROXY;
  } else if (channelLoadFlags & nsIRequest::VALIDATE_ALWAYS) {
    webnavLoadFlags |= nsIWebNavigation::LOAD_FLAGS_IS_REFRESH;
  }

  // Actually perform the cross process load
  bool reloadSucceeded = false;
  rv = wbc3->ReloadInFreshProcess(docShell, uri, referrer,
                                  triggeringPrincipal, webnavLoadFlags,
                                  &reloadSucceeded);
  NS_ENSURE_SUCCESS(rv, false);

  return reloadSucceeded;
}

/* static */ void
nsContentUtils::AppendDocumentLevelNativeAnonymousContentTo(
    nsIDocument* aDocument,
    nsTArray<nsIContent*>& aElements)
{
  MOZ_ASSERT(aDocument);

  if (nsIPresShell* presShell = aDocument->GetShell()) {
    if (nsIFrame* scrollFrame = presShell->GetRootScrollFrame()) {
      nsIAnonymousContentCreator* creator = do_QueryFrame(scrollFrame);
      MOZ_ASSERT(creator,
                 "scroll frame should always implement nsIAnonymousContentCreator");
      creator->AppendAnonymousContentTo(aElements, 0);
    }

    if (nsCanvasFrame* canvasFrame = presShell->GetCanvasFrame()) {
      if (Element* container = canvasFrame->GetCustomContentContainer()) {
        aElements.AppendElement(container);
      }
    }
  }
}

static void
AppendNativeAnonymousChildrenFromFrame(
    nsIFrame* aFrame,
    nsTArray<nsIContent*>& aKids,
    uint32_t aFlags)
{
  if (nsIAnonymousContentCreator* ac = do_QueryFrame(aFrame)) {
    ac->AppendAnonymousContentTo(aKids, aFlags);
  }
}

/* static */ void
nsContentUtils::AppendNativeAnonymousChildren(
    const nsIContent* aContent,
    nsTArray<nsIContent*>& aKids,
    uint32_t aFlags)
{
  if (aContent->MayHaveAnonymousChildren()) {
    if (nsIFrame* primaryFrame = aContent->GetPrimaryFrame()) {
      // NAC created by the element's primary frame.
      AppendNativeAnonymousChildrenFromFrame(primaryFrame, aKids, aFlags);

      // NAC created by any other non-primary frames for the element.
      AutoTArray<nsIFrame::OwnedAnonBox, 8> ownedAnonBoxes;
      primaryFrame->AppendOwnedAnonBoxes(ownedAnonBoxes);
      for (nsIFrame::OwnedAnonBox& box : ownedAnonBoxes) {
        MOZ_ASSERT(box.mAnonBoxFrame->GetContent() == aContent);
        AppendNativeAnonymousChildrenFromFrame(box.mAnonBoxFrame, aKids, aFlags);
      }
    }

    // Get manually created NAC (editor resize handles, etc.).
    if (auto nac = static_cast<ManualNACArray*>(
          aContent->GetProperty(nsGkAtoms::manualNACProperty))) {
      aKids.AppendElements(*nac);
    }
  }

  // The root scroll frame is not the primary frame of the root element.
  // Detect and handle this case.
  if (!(aFlags & nsIContent::eSkipDocumentLevelNativeAnonymousContent) &&
      aContent == aContent->OwnerDoc()->GetRootElement()) {
    AppendDocumentLevelNativeAnonymousContentTo(aContent->OwnerDoc(), aKids);
  }
}

/* static */ bool
nsContentUtils::QueryTriggeringPrincipal(nsIContent* aLoadingNode,
                                         nsIPrincipal* aDefaultPrincipal,
                                         nsIPrincipal** aTriggeringPrincipal)
{
  MOZ_ASSERT(aLoadingNode);
  MOZ_ASSERT(aTriggeringPrincipal);

  bool result = false;
  nsCOMPtr<nsIPrincipal> loadingPrincipal = aDefaultPrincipal;
  if (!loadingPrincipal) {
    loadingPrincipal = aLoadingNode->NodePrincipal();
  }

  // If aLoadingNode is content, bail out early.
  if (!aLoadingNode->NodePrincipal()->GetIsSystemPrincipal()) {
    loadingPrincipal.forget(aTriggeringPrincipal);
    return result;
  }

  nsAutoString loadingStr;
  aLoadingNode->GetAttr(kNameSpaceID_None, nsGkAtoms::triggeringprincipal,
                        loadingStr);

  // Fall back if 'triggeringprincipal' isn't specified,
  if (loadingStr.IsEmpty()) {
    loadingPrincipal.forget(aTriggeringPrincipal);
    return result;
  }

  nsCOMPtr<nsISupports> serializedPrincipal;
  NS_DeserializeObject(NS_ConvertUTF16toUTF8(loadingStr),
                       getter_AddRefs(serializedPrincipal));
  nsCOMPtr<nsIPrincipal> serializedPrin = do_QueryInterface(serializedPrincipal);
  if (serializedPrin) {
    result = true;
    serializedPrin.forget(aTriggeringPrincipal);
  } else {
    // Fallback if the deserialization is failed.
    loadingPrincipal.forget(aTriggeringPrincipal);
  }

  return result;
}

/* static */ void
nsContentUtils::GetContentPolicyTypeForUIImageLoading(nsIContent* aLoadingNode,
                                                      nsIPrincipal** aTriggeringPrincipal,
                                                      nsContentPolicyType& aContentPolicyType,
                                                      uint64_t* aRequestContextID)
{
  MOZ_ASSERT(aRequestContextID);

  bool result = QueryTriggeringPrincipal(aLoadingNode, aTriggeringPrincipal);
  if (result) {
    // Set the content policy type to TYPE_INTERNAL_IMAGE_FAVICON for
    // indicating it's a favicon loading.
    aContentPolicyType = nsIContentPolicy::TYPE_INTERNAL_IMAGE_FAVICON;

    nsAutoString requestContextID;
    aLoadingNode->GetAttr(kNameSpaceID_None, nsGkAtoms::requestcontextid,
                          requestContextID);
    nsresult rv;
    int64_t val  = requestContextID.ToInteger64(&rv);
    *aRequestContextID = NS_SUCCEEDED(rv)
      ? val
      : 0;
  } else {
    aContentPolicyType = nsIContentPolicy::TYPE_INTERNAL_IMAGE;
  }
}

/* static */ nsresult
nsContentUtils::CreateJSValueFromSequenceOfObject(JSContext* aCx,
                                                  const Sequence<JSObject*>& aTransfer,
                                                  JS::MutableHandle<JS::Value> aValue)
{
  if (aTransfer.IsEmpty()) {
    return NS_OK;
  }

  JS::Rooted<JSObject*> array(aCx, JS_NewArrayObject(aCx, aTransfer.Length()));
  if (!array) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  for (uint32_t i = 0; i < aTransfer.Length(); ++i) {
    JS::Rooted<JSObject*> object(aCx, aTransfer[i]);
    if (!object) {
      continue;
    }

    if (NS_WARN_IF(!JS_DefineElement(aCx, array, i, object,
                                     JSPROP_ENUMERATE))) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
  }

  aValue.setObject(*array);
  return NS_OK;
}

/* static */
bool
nsContentUtils::ShouldBlockReservedKeys(WidgetKeyboardEvent* aKeyEvent)
{
  nsCOMPtr<nsIPrincipal> principal;
  nsCOMPtr<nsIRemoteBrowser> targetBrowser = do_QueryInterface(aKeyEvent->mOriginalTarget);
  if (targetBrowser) {
    targetBrowser->GetContentPrincipal(getter_AddRefs(principal));
  }
  else {
    // Get the top-level document.
    nsCOMPtr<nsIContent> content = do_QueryInterface(aKeyEvent->mOriginalTarget);
    if (content) {
      nsIDocument* doc = content->GetUncomposedDoc();
      if (doc) {
        nsCOMPtr<nsIDocShellTreeItem> docShell = doc->GetDocShell();
        if (docShell && docShell->ItemType() == nsIDocShellTreeItem::typeContent) {
          nsCOMPtr<nsIDocShellTreeItem> rootItem;
          docShell->GetSameTypeRootTreeItem(getter_AddRefs(rootItem));
          if (rootItem && rootItem->GetDocument()) {
            principal = rootItem->GetDocument()->NodePrincipal();
          }
        }
      }
    }
  }

  if (principal) {
    return nsContentUtils::IsSitePermDeny(principal, "shortcuts");
  }

  return false;
}

/* static */ Element*
nsContentUtils::GetClosestNonNativeAnonymousAncestor(Element* aElement)
{
  MOZ_ASSERT(aElement);
  MOZ_ASSERT(aElement->IsNativeAnonymous());

  Element* e = aElement;
  while (e && e->IsNativeAnonymous()) {
    e = e->GetParentElement();
  }
  return e;
}

/**
 * Checks whether the given type is a supported document type for
 * loading within the nsObjectLoadingContent specified by aContent.
 *
 * NOTE Helper method for nsContentUtils::HtmlObjectContentTypeForMIMEType.
 * NOTE Does not take content policy or capabilities into account
 */
static bool
HtmlObjectContentSupportsDocument(const nsCString& aMimeType,
                                  nsIContent* aContent)
{
  nsCOMPtr<nsIWebNavigationInfo> info(
    do_GetService(NS_WEBNAVIGATION_INFO_CONTRACTID));
  if (!info) {
    return false;
  }

  nsCOMPtr<nsIWebNavigation> webNav;
  if (aContent) {
    nsIDocument* currentDoc = aContent->GetComposedDoc();
    if (currentDoc) {
      webNav = do_GetInterface(currentDoc->GetWindow());
    }
  }

  uint32_t supported;
  nsresult rv = info->IsTypeSupported(aMimeType, webNav, &supported);

  if (NS_FAILED(rv)) {
    return false;
  }

  if (supported != nsIWebNavigationInfo::UNSUPPORTED) {
    // Don't want to support plugins as documents
    return supported != nsIWebNavigationInfo::PLUGIN;
  }

  // Try a stream converter
  // NOTE: We treat any type we can convert from as a supported type. If a
  // type is not actually supported, the URI loader will detect that and
  // return an error, and we'll fallback.
  nsCOMPtr<nsIStreamConverterService> convServ =
    do_GetService("@mozilla.org/streamConverters;1");
  bool canConvert = false;
  if (convServ) {
    rv = convServ->CanConvert(aMimeType.get(), "*/*", &canConvert);
  }
  return NS_SUCCEEDED(rv) && canConvert;
}

/* static */
already_AddRefed<nsIPluginTag>
nsContentUtils::PluginTagForType(const nsCString& aMIMEType, bool aNoFakePlugin)
{
  RefPtr<nsPluginHost> pluginHost = nsPluginHost::GetInst();
  nsCOMPtr<nsIPluginTag> tag;
  NS_ENSURE_TRUE(pluginHost, nullptr);

  // ShouldPlay will handle the case where the plugin is disabled
  pluginHost->GetPluginTagForType(aMIMEType,
                                  aNoFakePlugin ? nsPluginHost::eExcludeFake
                                                : nsPluginHost::eExcludeNone,
                                  getter_AddRefs(tag));

  return tag.forget();
}

/* static */ uint32_t
nsContentUtils::HtmlObjectContentTypeForMIMEType(const nsCString& aMIMEType,
                                                 bool aNoFakePlugin,
                                                 nsIContent* aContent)
{
  if (aMIMEType.IsEmpty()) {
    return nsIObjectLoadingContent::TYPE_NULL;
  }

  if (imgLoader::SupportImageWithMimeType(aMIMEType.get())) {
    return nsIObjectLoadingContent::TYPE_IMAGE;
  }

  // Faking support of the PDF content as a document for EMBED tags
  // when internal PDF viewer is enabled.
  if (aMIMEType.LowerCaseEqualsLiteral("application/pdf") &&
      IsPDFJSEnabled()) {
    return nsIObjectLoadingContent::TYPE_DOCUMENT;
  }

  if (HtmlObjectContentSupportsDocument(aMIMEType, aContent)) {
    return nsIObjectLoadingContent::TYPE_DOCUMENT;
  }

  RefPtr<nsPluginHost> pluginHost = nsPluginHost::GetInst();
  if (pluginHost) {
    nsCOMPtr<nsIPluginTag> tag = PluginTagForType(aMIMEType, aNoFakePlugin);
    if (tag) {
      if (!aNoFakePlugin &&
          nsCOMPtr<nsIFakePluginTag>(do_QueryInterface(tag))) {
        return nsIObjectLoadingContent::TYPE_FAKE_PLUGIN;
      }

      // ShouldPlay will handle checking for disabled plugins
      return nsIObjectLoadingContent::TYPE_PLUGIN;
    }
  }

  return nsIObjectLoadingContent::TYPE_NULL;
}

/* static */ already_AddRefed<nsISerialEventTarget>
nsContentUtils::GetEventTargetByLoadInfo(nsILoadInfo* aLoadInfo, TaskCategory aCategory)
{
  if (NS_WARN_IF(!aLoadInfo)) {
    return nullptr;
  }

  nsCOMPtr<nsIDOMDocument> domDoc;
  aLoadInfo->GetLoadingDocument(getter_AddRefs(domDoc));
  nsCOMPtr<nsIDocument> doc = do_QueryInterface(domDoc);
  nsCOMPtr<nsISerialEventTarget> target;
  if (doc) {
    if (DocGroup* group = doc->GetDocGroup()) {
      target = group->EventTargetFor(aCategory);
    }
  } else {
    // There's no document yet, but this might be a top-level load where we can
    // find a TabGroup.
    uint64_t outerWindowId;
    if (NS_FAILED(aLoadInfo->GetOuterWindowID(&outerWindowId))) {
      // No window. This might be an add-on XHR, a service worker request, or
      // something else.
      return nullptr;
    }
    RefPtr<nsGlobalWindowOuter> window =
      nsGlobalWindowOuter::GetOuterWindowWithId(outerWindowId);
    if (!window) {
      return nullptr;
    }

    target = window->TabGroup()->EventTargetFor(aCategory);
  }

  return target.forget();
}

namespace {
template<class T>
bool IsLocalRefURL(const T& aString) {
  // Find the first non-"C0 controls + space" character.
  const typename T::char_type* current = aString.BeginReading();
  for (; current != aString.EndReading(); current++) {
    if (*current > 0x20) {
      // if the first non-"C0 controls + space" character is '#', this is a
      // local-ref URL.
      return *current == '#';
    }
  }

  return false;
}
}

/* static */ bool
nsContentUtils::IsLocalRefURL(const nsString& aString)
{
  return ::IsLocalRefURL(aString);
}

/* static */ bool
nsContentUtils::IsLocalRefURL(const nsACString& aString)
{
  return ::IsLocalRefURL(aString);
}

// Tab ID is composed in a similar manner of Window ID.
static uint64_t gNextTabId = 0;
static const uint64_t kTabIdProcessBits = 32;
static const uint64_t kTabIdTabBits = 64 - kTabIdProcessBits;

/* static */ uint64_t
nsContentUtils::GenerateTabId()
{
  uint64_t processId = 0;
  if (XRE_IsContentProcess()) {
    ContentChild* cc = ContentChild::GetSingleton();
    processId = cc->GetID();
  }

  MOZ_RELEASE_ASSERT(processId < (uint64_t(1) << kTabIdProcessBits));
  uint64_t processBits = processId & ((uint64_t(1) << kTabIdProcessBits) - 1);

  uint64_t tabId = ++gNextTabId;
  MOZ_RELEASE_ASSERT(tabId < (uint64_t(1) << kTabIdTabBits));
  uint64_t tabBits = tabId & ((uint64_t(1) << kTabIdTabBits) - 1);

  return (processBits << kTabIdTabBits) | tabBits;
}

/* static */ bool
nsContentUtils::GetUserIsInteracting()
{
  return UserInteractionObserver::sUserActive;
}

/* static */ bool
nsContentUtils::GetSourceMapURL(nsIHttpChannel* aChannel, nsACString& aResult)
{
  nsresult rv = aChannel->GetResponseHeader(NS_LITERAL_CSTRING("SourceMap"), aResult);
  if (NS_FAILED(rv)) {
    rv = aChannel->GetResponseHeader(NS_LITERAL_CSTRING("X-SourceMap"), aResult);
  }
  return NS_SUCCEEDED(rv);
}

/* static */  bool
nsContentUtils::IsMessageInputEvent(const IPC::Message& aMsg)
{
  if ((aMsg.type() & mozilla::dom::PBrowser::PBrowserStart)
      == mozilla::dom::PBrowser::PBrowserStart) {
    switch (aMsg.type()) {
      case mozilla::dom::PBrowser::Msg_RealMouseMoveEvent__ID:
      case mozilla::dom::PBrowser::Msg_RealMouseButtonEvent__ID:
      case mozilla::dom::PBrowser::Msg_RealKeyEvent__ID:
      case mozilla::dom::PBrowser::Msg_MouseWheelEvent__ID:
      case mozilla::dom::PBrowser::Msg_RealTouchEvent__ID:
      case mozilla::dom::PBrowser::Msg_RealTouchMoveEvent__ID:
      case mozilla::dom::PBrowser::Msg_RealDragEvent__ID:
      case mozilla::dom::PBrowser::Msg_UpdateDimensions__ID:
      case mozilla::dom::PBrowser::Msg_MouseEvent__ID:
      case mozilla::dom::PBrowser::Msg_KeyEvent__ID:
      case mozilla::dom::PBrowser::Msg_SetDocShellIsActive__ID:
        return true;
    }
  }
  return false;
}

static const char* kUserInteractionInactive = "user-interaction-inactive";
static const char* kUserInteractionActive = "user-interaction-active";

void
nsContentUtils::UserInteractionObserver::Init()
{
  // Listen for the observer messages from EventStateManager which are telling
  // us whether or not the user is interacting.
  nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
  obs->AddObserver(this, kUserInteractionInactive, false);
  obs->AddObserver(this, kUserInteractionActive, false);

  // We can't register ourselves as an annotator yet, as the HangMonitor hasn't
  // started yet. It will have started by the time we have the chance to spin
  // the event loop.
  RefPtr<UserInteractionObserver> self = this;
  NS_DispatchToMainThread(
    NS_NewRunnableFunction("nsContentUtils::UserInteractionObserver::Init",
                           [=]() { HangMonitor::RegisterAnnotator(*self); }));
}

void
nsContentUtils::UserInteractionObserver::Shutdown()
{
  nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
  if (obs) {
    obs->RemoveObserver(this, kUserInteractionInactive);
    obs->RemoveObserver(this, kUserInteractionActive);
  }

  HangMonitor::UnregisterAnnotator(*this);
}

/**
 * NB: This function is always called by the HangMonitor thread.
 *     Plan accordingly
 */
void
nsContentUtils::UserInteractionObserver::AnnotateHang(HangMonitor::HangAnnotations& aAnnotations)
{
  // NOTE: Only annotate the hang report if the user is known to be interacting.
  if (sUserActive) {
    aAnnotations.AddAnnotation(NS_LITERAL_STRING("UserInteracting"), true);
  }
}

NS_IMETHODIMP
nsContentUtils::UserInteractionObserver::Observe(nsISupports* aSubject,
                                                 const char* aTopic,
                                                 const char16_t* aData)
{
  if (!strcmp(aTopic, kUserInteractionInactive)) {
    sUserActive = false;
  } else if (!strcmp(aTopic, kUserInteractionActive)) {
    sUserActive = true;
  } else {
    NS_WARNING("Unexpected observer notification");
  }
  return NS_OK;
}

Atomic<bool> nsContentUtils::UserInteractionObserver::sUserActive(false);
NS_IMPL_ISUPPORTS(nsContentUtils::UserInteractionObserver, nsIObserver)

/* static */ bool
nsContentUtils::IsOverridingWindowName(const nsAString& aName)
{
  return !aName.IsEmpty() &&
    !aName.LowerCaseEqualsLiteral("_blank") &&
    !aName.LowerCaseEqualsLiteral("_top") &&
    !aName.LowerCaseEqualsLiteral("_parent") &&
    !aName.LowerCaseEqualsLiteral("_self");
}

// Unfortunately, we can't unwrap an IDL object using only a concrete type.
// We need to calculate type data based on the IDL typename. Which means
// wrapping our templated function in a macro.
#define EXTRACT_EXN_VALUES(T, ...)                                \
  ExtractExceptionValues<mozilla::dom::prototypes::id::T,         \
                         T##Binding::NativeType, T>(__VA_ARGS__).isOk()

template <prototypes::ID PrototypeID, class NativeType, typename T>
static Result<Ok, nsresult>
ExtractExceptionValues(JSContext* aCx,
                       JS::HandleObject aObj,
                       nsACString& aSourceSpecOut,
                       uint32_t* aLineOut,
                       uint32_t* aColumnOut,
                       nsString& aMessageOut)
{
  RefPtr<T> exn;
  MOZ_TRY((UnwrapObject<PrototypeID, NativeType>(aObj, exn)));

  nsAutoString filename;
  exn->GetFilename(aCx, filename);
  if (!filename.IsEmpty()) {
    CopyUTF16toUTF8(filename, aSourceSpecOut);
    *aLineOut = exn->LineNumber(aCx);
    *aColumnOut = exn->ColumnNumber();
  }

  exn->GetName(aMessageOut);
  aMessageOut.AppendLiteral(": ");

  nsAutoString message;
  exn->GetMessageMoz(message);
  aMessageOut.Append(message);
  return Ok();
}

/* static */ void
nsContentUtils::ExtractErrorValues(JSContext* aCx,
                                   JS::Handle<JS::Value> aValue,
                                   nsACString& aSourceSpecOut,
                                   uint32_t* aLineOut,
                                   uint32_t* aColumnOut,
                                   nsString& aMessageOut)
{
  MOZ_ASSERT(aLineOut);
  MOZ_ASSERT(aColumnOut);

  if (aValue.isObject()) {
    JS::Rooted<JSObject*> obj(aCx, &aValue.toObject());

    // Try to process as an Error object.  Use the file/line/column values
    // from the Error as they will be more specific to the root cause of
    // the problem.
    JSErrorReport* err = obj ? JS_ErrorFromException(aCx, obj) : nullptr;
    if (err) {
      // Use xpc to extract the error message only.  We don't actually send
      // this report anywhere.
      RefPtr<xpc::ErrorReport> report = new xpc::ErrorReport();
      report->Init(err,
                   "<unknown>", // toString result
                   false,       // chrome
                   0);          // window ID

      if (!report->mFileName.IsEmpty()) {
        CopyUTF16toUTF8(report->mFileName, aSourceSpecOut);
        *aLineOut = report->mLineNumber;
        *aColumnOut = report->mColumn;
      }
      aMessageOut.Assign(report->mErrorMsg);
    }

    // Next, try to unwrap the rejection value as a DOMException.
    else if (EXTRACT_EXN_VALUES(DOMException, aCx, obj, aSourceSpecOut,
                                aLineOut, aColumnOut, aMessageOut)) {
      return;
    }

    // Next, try to unwrap the rejection value as an XPC Exception.
    else if (EXTRACT_EXN_VALUES(Exception, aCx, obj, aSourceSpecOut,
                                aLineOut, aColumnOut, aMessageOut)) {
      return;
    }
  }

  // If we could not unwrap a specific error type, then perform default safe
  // string conversions on primitives.  Objects will result in "[Object]"
  // unfortunately.
  if (aMessageOut.IsEmpty()) {
    nsAutoJSString jsString;
    if (jsString.init(aCx, aValue)) {
      aMessageOut = jsString;
    } else {
      JS_ClearPendingException(aCx);
    }
  }
}

#undef EXTRACT_EXN_VALUES

/* static */ bool
nsContentUtils::DevToolsEnabled(JSContext* aCx)
{
  if (NS_IsMainThread()) {
    return sDevToolsEnabled;
  }

  workers::WorkerPrivate* workerPrivate = workers::GetWorkerPrivateFromContext(aCx);
  if (!workerPrivate) {
    return false;
  }

  return workerPrivate->DevToolsEnabled();
}

/* static */ bool
nsContentUtils::ContentIsLink(nsIContent* aContent)
{
  return aContent && (aContent->IsHTMLElement(nsGkAtoms::a) ||
                      aContent->AttrValueIs(kNameSpaceID_XLink, nsGkAtoms::type,
                                            nsGkAtoms::simple, eCaseMatters));
}

/* static */ already_AddRefed<EventTarget>
nsContentUtils::TryGetTabChildGlobalAsEventTarget(nsISupports* aFrom)
{
  nsCOMPtr<nsIFrameLoaderOwner> frameLoaderOwner = do_QueryInterface(aFrom);
  if (!frameLoaderOwner) {
    return nullptr;
  }

  RefPtr<nsFrameLoader> frameLoader = frameLoaderOwner->GetFrameLoader();
  if (!frameLoader) {
    return nullptr;
  }

  nsCOMPtr<EventTarget> target = frameLoader->GetTabChildGlobalAsEventTarget();
  return target.forget();
}

/* static */ uint32_t
nsContentUtils::InnerOrOuterWindowCreated()
{
  MOZ_ASSERT(NS_IsMainThread());
  ++sInnerOrOuterWindowCount;
  return ++sInnerOrOuterWindowSerialCounter;
}

/* static */ void
nsContentUtils::InnerOrOuterWindowDestroyed()
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(sInnerOrOuterWindowCount > 0);
  --sInnerOrOuterWindowCount;
}
