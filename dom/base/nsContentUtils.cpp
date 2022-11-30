/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* A namespace class for static layout utilities. */

#include "nsContentUtils.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <new>
#include <utility>
#include "BrowserChild.h"
#include "DecoderTraits.h"
#include "ErrorList.h"
#include "ImageOps.h"
#include "InProcessBrowserChildMessageManager.h"
#include "Layers.h"
#include "MainThreadUtils.h"
#include "PLDHashTable.h"
#include "ReferrerInfo.h"
#include "ScopedNSSTypes.h"
#include "ThirdPartyUtil.h"
#include "Units.h"
#include "chrome/common/ipc_message.h"
#include "gfxDrawable.h"
#include "harfbuzz/hb.h"
#include "imgICache.h"
#include "imgIContainer.h"
#include "imgILoader.h"
#include "imgIRequest.h"
#include "imgLoader.h"
#include "js/Array.h"
#include "js/ArrayBuffer.h"
#include "js/BuildId.h"
#include "js/GCAPI.h"
#include "js/Id.h"
#include "js/JSON.h"
#include "js/PropertyAndElement.h"  // JS_DefineElement, JS_GetProperty
#include "js/PropertyDescriptor.h"
#include "js/Realm.h"
#include "js/RegExp.h"
#include "js/RegExpFlags.h"
#include "js/RootingAPI.h"
#include "js/TypeDecls.h"
#include "js/Value.h"
#include "js/Wrapper.h"
#include "jsapi.h"
#include "jsfriendapi.h"
#include "mozAutoDocUpdate.h"
#include "mozIDOMWindow.h"
#include "mozilla/AlreadyAddRefed.h"
#include "mozilla/ArrayIterator.h"
#include "mozilla/ArrayUtils.h"
#include "mozilla/AsyncEventDispatcher.h"
#include "mozilla/AtomArray.h"
#include "mozilla/Atomics.h"
#include "mozilla/Attributes.h"
#include "mozilla/AutoRestore.h"
#include "mozilla/AutoTimelineMarker.h"
#include "mozilla/BackgroundHangMonitor.h"
#include "mozilla/Base64.h"
#include "mozilla/BasePrincipal.h"
#include "mozilla/BasicEvents.h"
#include "mozilla/BloomFilter.h"
#include "mozilla/CORSMode.h"
#include "mozilla/CallState.h"
#include "mozilla/CheckedInt.h"
#include "mozilla/Components.h"
#include "mozilla/CycleCollectedJSContext.h"
#include "mozilla/DOMEventTargetHelper.h"
#include "mozilla/DebugOnly.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/EventDispatcher.h"
#include "mozilla/EventListenerManager.h"
#include "mozilla/EventQueue.h"
#include "mozilla/EventStateManager.h"
#include "mozilla/FlushType.h"
#include "mozilla/FOGIPC.h"
#include "mozilla/HTMLEditor.h"
#include "mozilla/HangAnnotations.h"
#include "mozilla/IMEStateManager.h"
#include "mozilla/InputEventOptions.h"
#include "mozilla/InternalMutationEvent.h"
#include "mozilla/Latin1.h"
#include "mozilla/Likely.h"
#include "mozilla/LoadInfo.h"
#include "mozilla/Logging.h"
#include "mozilla/MacroForEach.h"
#include "mozilla/ManualNAC.h"
#include "mozilla/Maybe.h"
#include "mozilla/MouseEvents.h"
#include "mozilla/NotNull.h"
#include "mozilla/NullPrincipal.h"
#include "mozilla/OriginAttributes.h"
#include "mozilla/Preferences.h"
#include "mozilla/PresShell.h"
#include "mozilla/ProfilerRunnable.h"
#include "mozilla/RangeBoundary.h"
#include "mozilla/RefPtr.h"
#include "mozilla/Result.h"
#include "mozilla/ResultExtensions.h"
#include "mozilla/ScrollbarPreferences.h"
#include "mozilla/Span.h"
#include "mozilla/StaticAnalysisFunctions.h"
#include "mozilla/StaticPrefs_dom.h"
#ifdef FUZZING
#  include "mozilla/StaticPrefs_fuzzing.h"
#endif
#include "mozilla/StaticPrefs_nglayout.h"
#include "mozilla/StaticPrefs_privacy.h"
#include "mozilla/StaticPrefs_test.h"
#include "mozilla/StaticPrefs_ui.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/TaskCategory.h"
#include "mozilla/TextControlState.h"
#include "mozilla/TextEditor.h"
#include "mozilla/TextEvents.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/Unused.h"
#include "mozilla/Variant.h"
#include "mozilla/ViewportUtils.h"
#include "mozilla/dom/AncestorIterator.h"
#include "mozilla/dom/AutoEntryScript.h"
#include "mozilla/dom/AutocompleteInfoBinding.h"
#include "mozilla/dom/AutoSuppressEventHandlingAndSuspend.h"
#include "mozilla/dom/BindingDeclarations.h"
#include "mozilla/dom/BindingUtils.h"
#include "mozilla/dom/BlobImpl.h"
#include "mozilla/dom/BlobURLProtocolHandler.h"
#include "mozilla/dom/BorrowedAttrInfo.h"
#include "mozilla/dom/BrowserBridgeParent.h"
#include "mozilla/dom/BrowserParent.h"
#include "mozilla/dom/BrowsingContext.h"
#include "mozilla/dom/BrowsingContextGroup.h"
#include "mozilla/dom/CallbackFunction.h"
#include "mozilla/dom/CallbackObject.h"
#include "mozilla/dom/ChromeMessageBroadcaster.h"
#include "mozilla/dom/ContentChild.h"
#include "mozilla/dom/ContentFrameMessageManager.h"
#include "mozilla/dom/ContentParent.h"
#include "mozilla/dom/CustomElementRegistry.h"
#include "mozilla/dom/CustomElementRegistryBinding.h"
#include "mozilla/dom/DOMArena.h"
#include "mozilla/dom/DOMException.h"
#include "mozilla/dom/DOMExceptionBinding.h"
#include "mozilla/dom/DOMSecurityMonitor.h"
#include "mozilla/dom/DOMTypes.h"
#include "mozilla/dom/DataTransfer.h"
#include "mozilla/dom/DocGroup.h"
#include "mozilla/dom/Document.h"
#include "mozilla/dom/DocumentFragment.h"
#include "mozilla/dom/DocumentInlines.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/ElementBinding.h"
#include "mozilla/dom/ElementInlines.h"
#include "mozilla/dom/Event.h"
#include "mozilla/dom/EventTarget.h"
#include "mozilla/dom/FileBlobImpl.h"
#include "mozilla/dom/FileSystemSecurity.h"
#include "mozilla/dom/FilteredNodeIterator.h"
#include "mozilla/dom/FontTableURIProtocolHandler.h"
#include "mozilla/dom/FragmentOrElement.h"
#include "mozilla/dom/FromParser.h"
#include "mozilla/dom/HTMLFormElement.h"
#include "mozilla/dom/HTMLInputElement.h"
#include "mozilla/dom/HTMLTextAreaElement.h"
#include "mozilla/dom/IPCBlob.h"
#include "mozilla/dom/IPCBlobUtils.h"
#include "mozilla/dom/MessageBroadcaster.h"
#include "mozilla/dom/MessageListenerManager.h"
#include "mozilla/dom/MessagePort.h"
#include "mozilla/dom/MouseEventBinding.h"
#include "mozilla/dom/NameSpaceConstants.h"
#include "mozilla/dom/NodeBinding.h"
#include "mozilla/dom/NodeInfo.h"
#include "mozilla/dom/PBrowser.h"
#include "mozilla/dom/PContentChild.h"
#include "mozilla/dom/PrototypeList.h"
#include "mozilla/dom/ReferrerPolicyBinding.h"
#include "mozilla/dom/ScriptSettings.h"
#include "mozilla/dom/Selection.h"
#include "mozilla/dom/ShadowRoot.h"
#include "mozilla/dom/Text.h"
#include "mozilla/dom/UserActivation.h"
#include "mozilla/dom/WindowContext.h"
#include "mozilla/dom/WorkerCommon.h"
#include "mozilla/dom/WorkerPrivate.h"
#include "mozilla/dom/XULCommandEvent.h"
#include "mozilla/fallible.h"
#include "mozilla/gfx/2D.h"
#include "mozilla/gfx/BaseMargin.h"
#include "mozilla/gfx/BasePoint.h"
#include "mozilla/gfx/BaseSize.h"
#include "mozilla/gfx/DataSurfaceHelpers.h"
#include "mozilla/gfx/Point.h"
#include "mozilla/gfx/Rect.h"
#include "mozilla/gfx/Types.h"
#include "mozilla/ipc/ProtocolUtils.h"
#include "mozilla/ipc/SharedMemory.h"
#include "mozilla/ipc/Shmem.h"
#include "mozilla/net/UrlClassifierCommon.h"
#include "mozilla/widget/IMEData.h"
#include "nsAboutProtocolUtils.h"
#include "nsAlgorithm.h"
#include "nsArrayUtils.h"
#include "nsAtom.h"
#include "nsAttrName.h"
#include "nsAttrValue.h"
#include "nsAttrValueInlines.h"
#include "nsBaseHashtable.h"
#include "nsCCUncollectableMarker.h"
#include "nsCOMPtr.h"
#include "nsCRT.h"
#include "nsCRTGlue.h"
#include "nsCanvasFrame.h"
#include "nsCaseTreatment.h"
#include "nsCharSeparatedTokenizer.h"
#include "nsCharTraits.h"
#include "nsCompatibility.h"
#include "nsComponentManagerUtils.h"
#include "nsContainerFrame.h"
#include "nsContentCreatorFunctions.h"
#include "nsContentDLF.h"
#include "nsContentList.h"
#include "nsContentListDeclarations.h"
#include "nsContentPolicyUtils.h"
#include "nsCoord.h"
#include "nsCycleCollectionNoteChild.h"
#include "nsDOMMutationObserver.h"
#include "nsDOMString.h"
#include "nsTHashMap.h"
#include "nsDebug.h"
#include "nsDocShell.h"
#include "nsDocShellCID.h"
#include "nsError.h"
#include "nsFocusManager.h"
#include "nsFrameList.h"
#include "nsFrameLoader.h"
#include "nsFrameLoaderOwner.h"
#include "nsGenericHTMLElement.h"
#include "nsGkAtoms.h"
#include "nsGlobalWindowInner.h"
#include "nsGlobalWindowOuter.h"
#include "nsHTMLDocument.h"
#include "nsHTMLTags.h"
#include "nsHashKeys.h"
#include "nsHtml5StringParser.h"
#include "nsIAboutModule.h"
#include "nsIAnonymousContentCreator.h"
#include "nsIArray.h"
#include "nsIAsyncVerifyRedirectCallback.h"
#include "nsIBidiKeyboard.h"
#include "nsIBrowser.h"
#include "nsICacheInfoChannel.h"
#include "nsICategoryManager.h"
#include "nsIChannel.h"
#include "nsIChannelEventSink.h"
#include "nsIClassifiedChannel.h"
#include "nsIConsoleService.h"
#include "nsIContent.h"
#include "nsIContentInlines.h"
#include "nsIContentPolicy.h"
#include "nsIContentSecurityPolicy.h"
#include "nsIContentSink.h"
#include "nsIContentViewer.h"
#include "nsIDOMWindowUtils.h"
#include "nsIDocShell.h"
#include "nsIDocShellTreeItem.h"
#include "nsIDocumentEncoder.h"
#include "nsIDocumentLoaderFactory.h"
#include "nsIDragService.h"
#include "nsIDragSession.h"
#include "nsIFile.h"
#include "nsIFocusManager.h"
#include "nsIFormControl.h"
#include "nsIFragmentContentSink.h"
#include "nsIFrame.h"
#include "nsIGlobalObject.h"
#include "nsIHttpChannel.h"
#include "nsIHttpChannelInternal.h"
#include "nsIIOService.h"
#include "nsIImageLoadingContent.h"
#include "nsIInputStream.h"
#include "nsIInterfaceRequestor.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsILoadContext.h"
#include "nsILoadGroup.h"
#include "nsILoadInfo.h"
#include "nsIMIMEService.h"
#include "nsIMemoryReporter.h"
#include "nsINetUtil.h"
#include "nsINode.h"
#include "nsIObjectLoadingContent.h"
#include "nsIObserver.h"
#include "nsIObserverService.h"
#include "nsIParserUtils.h"
#include "nsIPermissionManager.h"
#include "nsIPluginTag.h"
#include "nsIPrincipal.h"
#include "nsIProperties.h"
#include "nsIProtocolHandler.h"
#include "nsIRequest.h"
#include "nsIRunnable.h"
#include "nsIScreen.h"
#include "nsIScriptError.h"
#include "nsIScriptGlobalObject.h"
#include "nsIScriptObjectPrincipal.h"
#include "nsIScriptSecurityManager.h"
#include "nsISerialEventTarget.h"
#include "nsIStreamConverter.h"
#include "nsIStreamConverterService.h"
#include "nsIStringBundle.h"
#include "nsISupports.h"
#include "nsISupportsPrimitives.h"
#include "nsISupportsUtils.h"
#include "nsITransferable.h"
#include "nsIURI.h"
#include "nsIURIMutator.h"
#if defined(MOZ_THUNDERBIRD) || defined(MOZ_SUITE)
#  include "nsIURIWithSpecialOrigin.h"
#endif
#include "nsIUserIdleService.h"
#include "nsIWeakReferenceUtils.h"
#include "nsIWebNavigation.h"
#include "nsIWebNavigationInfo.h"
#include "nsIWidget.h"
#include "nsIWindowMediator.h"
#include "nsIXPConnect.h"
#include "nsJSPrincipals.h"
#include "nsJSUtils.h"
#include "nsLayoutUtils.h"
#include "nsLiteralString.h"
#include "nsMappedAttributes.h"
#include "nsMargin.h"
#include "nsMimeTypes.h"
#include "nsNameSpaceManager.h"
#include "nsNetCID.h"
#include "nsNetUtil.h"
#include "nsNodeInfoManager.h"
#include "nsPIDOMWindow.h"
#include "nsPIDOMWindowInlines.h"
#include "nsParser.h"
#include "nsParserConstants.h"
#include "nsPluginHost.h"
#include "nsPoint.h"
#include "nsPointerHashKeys.h"
#include "nsPresContext.h"
#include "nsQueryFrame.h"
#include "nsQueryObject.h"
#include "nsRFPService.h"
#include "nsRange.h"
#include "nsRefPtrHashtable.h"
#include "nsSandboxFlags.h"
#include "nsScriptSecurityManager.h"
#include "nsServiceManagerUtils.h"
#include "nsStreamUtils.h"
#include "nsString.h"
#include "nsStringBuffer.h"
#include "nsStringFlags.h"
#include "nsStringFwd.h"
#include "nsStringIterator.h"
#include "nsStringStream.h"
#include "nsTArray.h"
#include "nsTLiteralString.h"
#include "nsTPromiseFlatString.h"
#include "nsTStringRepr.h"
#include "nsTextFragment.h"
#include "nsTextNode.h"
#include "nsThreadManager.h"
#include "nsThreadUtils.h"
#include "nsTreeSanitizer.h"
#include "nsUGenCategory.h"
#include "nsURLHelper.h"
#include "nsUnicodeProperties.h"
#include "nsVariant.h"
#include "nsView.h"
#include "nsViewManager.h"
#include "nsXPCOM.h"
#include "nsXPCOMCID.h"
#include "nsXULAppAPI.h"
#include "nsXULElement.h"
#include "nsXULPopupManager.h"
#include "nscore.h"
#include "prinrval.h"
#include "xpcprivate.h"
#include "xpcpublic.h"

#if defined(XP_WIN)
// Undefine LoadImage to prevent naming conflict with Windows.
#  undef LoadImage
#endif

extern "C" int MOZ_XMLTranslateEntity(const char* ptr, const char* end,
                                      const char** next, char16_t* result);
extern "C" int MOZ_XMLCheckQName(const char* ptr, const char* end, int ns_aware,
                                 const char** colon);

using namespace mozilla::dom;
using namespace mozilla::ipc;
using namespace mozilla::gfx;
using namespace mozilla::layers;
using namespace mozilla::widget;
using namespace mozilla;

const char kLoadAsData[] = "loadAsData";

nsIXPConnect* nsContentUtils::sXPConnect;
nsIScriptSecurityManager* nsContentUtils::sSecurityManager;
nsIPrincipal* nsContentUtils::sSystemPrincipal;
nsIPrincipal* nsContentUtils::sNullSubjectPrincipal;
nsIIOService* nsContentUtils::sIOService;
nsIConsoleService* nsContentUtils::sConsoleService;
nsTHashMap<nsRefPtrHashKey<nsAtom>, EventNameMapping>*
    nsContentUtils::sAtomEventTable = nullptr;
nsTHashMap<nsStringHashKey, EventNameMapping>*
    nsContentUtils::sStringEventTable = nullptr;
nsTArray<RefPtr<nsAtom>>* nsContentUtils::sUserDefinedEvents = nullptr;
nsIStringBundleService* nsContentUtils::sStringBundleService;
nsIStringBundle* nsContentUtils::sStringBundles[PropertiesFile_COUNT];
nsIContentPolicy* nsContentUtils::sContentPolicyService;
bool nsContentUtils::sTriedToGetContentPolicy = false;
StaticRefPtr<nsIBidiKeyboard> nsContentUtils::sBidiKeyboard;
uint32_t nsContentUtils::sScriptBlockerCount = 0;
uint32_t nsContentUtils::sDOMNodeRemovedSuppressCount = 0;
AutoTArray<nsCOMPtr<nsIRunnable>, 8>* nsContentUtils::sBlockedScriptRunners =
    nullptr;
uint32_t nsContentUtils::sRunnersCountAtFirstBlocker = 0;
nsIInterfaceRequestor* nsContentUtils::sSameOriginChecker = nullptr;

bool nsContentUtils::sIsHandlingKeyBoardEvent = false;

nsString* nsContentUtils::sShiftText = nullptr;
nsString* nsContentUtils::sControlText = nullptr;
nsString* nsContentUtils::sMetaText = nullptr;
nsString* nsContentUtils::sOSText = nullptr;
nsString* nsContentUtils::sAltText = nullptr;
nsString* nsContentUtils::sModifierSeparator = nullptr;

bool nsContentUtils::sInitialized = false;
#ifndef RELEASE_OR_BETA
bool nsContentUtils::sBypassCSSOMOriginCheck = false;
#endif

nsCString* nsContentUtils::sJSScriptBytecodeMimeType = nullptr;
nsCString* nsContentUtils::sJSModuleBytecodeMimeType = nullptr;

nsContentUtils::UserInteractionObserver*
    nsContentUtils::sUserInteractionObserver = nullptr;

nsHtml5StringParser* nsContentUtils::sHTMLFragmentParser = nullptr;
nsParser* nsContentUtils::sXMLFragmentParser = nullptr;
nsIFragmentContentSink* nsContentUtils::sXMLFragmentSink = nullptr;
bool nsContentUtils::sFragmentParsingActive = false;

mozilla::LazyLogModule nsContentUtils::gResistFingerprintingLog(
    "nsResistFingerprinting");
mozilla::LazyLogModule nsContentUtils::sDOMDumpLog("Dump");

int32_t nsContentUtils::sInnerOrOuterWindowCount = 0;
uint32_t nsContentUtils::sInnerOrOuterWindowSerialCounter = 0;

template Maybe<int32_t> nsContentUtils::ComparePoints(
    const RangeBoundary& aFirstBoundary, const RangeBoundary& aSecondBoundary);
template Maybe<int32_t> nsContentUtils::ComparePoints(
    const RangeBoundary& aFirstBoundary,
    const RawRangeBoundary& aSecondBoundary);
template Maybe<int32_t> nsContentUtils::ComparePoints(
    const RawRangeBoundary& aFirstBoundary,
    const RangeBoundary& aSecondBoundary);
template Maybe<int32_t> nsContentUtils::ComparePoints(
    const RawRangeBoundary& aFirstBoundary,
    const RawRangeBoundary& aSecondBoundary);

template int32_t nsContentUtils::ComparePoints_Deprecated(
    const RangeBoundary& aFirstBoundary, const RangeBoundary& aSecondBoundary,
    bool* aDisconnected);
template int32_t nsContentUtils::ComparePoints_Deprecated(
    const RangeBoundary& aFirstBoundary,
    const RawRangeBoundary& aSecondBoundary, bool* aDisconnected);
template int32_t nsContentUtils::ComparePoints_Deprecated(
    const RawRangeBoundary& aFirstBoundary,
    const RangeBoundary& aSecondBoundary, bool* aDisconnected);
template int32_t nsContentUtils::ComparePoints_Deprecated(
    const RawRangeBoundary& aFirstBoundary,
    const RawRangeBoundary& aSecondBoundary, bool* aDisconnected);

// Subset of
// http://www.whatwg.org/specs/web-apps/current-work/#autofill-field-name
enum AutocompleteUnsupportedFieldName : uint8_t {
#define AUTOCOMPLETE_UNSUPPORTED_FIELD_NAME(name_, value_) \
  eAutocompleteUnsupportedFieldName_##name_,
#include "AutocompleteFieldList.h"
#undef AUTOCOMPLETE_UNSUPPORTED_FIELD_NAME
};

enum AutocompleteNoPersistFieldName : uint8_t {
#define AUTOCOMPLETE_NO_PERSIST_FIELD_NAME(name_, value_) \
  eAutocompleteNoPersistFieldName_##name_,
#include "AutocompleteFieldList.h"
#undef AUTOCOMPLETE_NO_PERSIST_FIELD_NAME
};

enum AutocompleteUnsupportFieldContactHint : uint8_t {
#define AUTOCOMPLETE_UNSUPPORTED_FIELD_CONTACT_HINT(name_, value_) \
  eAutocompleteUnsupportedFieldContactHint_##name_,
#include "AutocompleteFieldList.h"
#undef AUTOCOMPLETE_UNSUPPORTED_FIELD_CONTACT_HINT
};

enum AutocompleteFieldName : uint8_t {
#define AUTOCOMPLETE_FIELD_NAME(name_, value_) eAutocompleteFieldName_##name_,
#define AUTOCOMPLETE_CONTACT_FIELD_NAME(name_, value_) \
  AUTOCOMPLETE_FIELD_NAME(name_, value_)
#include "AutocompleteFieldList.h"
#undef AUTOCOMPLETE_FIELD_NAME
#undef AUTOCOMPLETE_CONTACT_FIELD_NAME
};

enum AutocompleteFieldHint : uint8_t {
#define AUTOCOMPLETE_FIELD_HINT(name_, value_) eAutocompleteFieldHint_##name_,
#include "AutocompleteFieldList.h"
#undef AUTOCOMPLETE_FIELD_HINT
};

enum AutocompleteFieldContactHint : uint8_t {
#define AUTOCOMPLETE_FIELD_CONTACT_HINT(name_, value_) \
  eAutocompleteFieldContactHint_##name_,
#include "AutocompleteFieldList.h"
#undef AUTOCOMPLETE_FIELD_CONTACT_HINT
};

enum AutocompleteCategory {
#define AUTOCOMPLETE_CATEGORY(name_, value_) eAutocompleteCategory_##name_,
#include "AutocompleteFieldList.h"
#undef AUTOCOMPLETE_CATEGORY
};

static const nsAttrValue::EnumTable kAutocompleteUnsupportedFieldNameTable[] = {
#define AUTOCOMPLETE_UNSUPPORTED_FIELD_NAME(name_, value_) \
  {value_, eAutocompleteUnsupportedFieldName_##name_},
#include "AutocompleteFieldList.h"
#undef AUTOCOMPLETE_UNSUPPORTED_FIELD_NAME
    {nullptr, 0}};

static const nsAttrValue::EnumTable kAutocompleteNoPersistFieldNameTable[] = {
#define AUTOCOMPLETE_NO_PERSIST_FIELD_NAME(name_, value_) \
  {value_, eAutocompleteNoPersistFieldName_##name_},
#include "AutocompleteFieldList.h"
#undef AUTOCOMPLETE_NO_PERSIST_FIELD_NAME
    {nullptr, 0}};

static const nsAttrValue::EnumTable
    kAutocompleteUnsupportedContactFieldHintTable[] = {
#define AUTOCOMPLETE_UNSUPPORTED_FIELD_CONTACT_HINT(name_, value_) \
  {value_, eAutocompleteUnsupportedFieldContactHint_##name_},
#include "AutocompleteFieldList.h"
#undef AUTOCOMPLETE_UNSUPPORTED_FIELD_CONTACT_HINT
        {nullptr, 0}};

static const nsAttrValue::EnumTable kAutocompleteFieldNameTable[] = {
#define AUTOCOMPLETE_FIELD_NAME(name_, value_) \
  {value_, eAutocompleteFieldName_##name_},
#include "AutocompleteFieldList.h"
#undef AUTOCOMPLETE_FIELD_NAME
    {nullptr, 0}};

static const nsAttrValue::EnumTable kAutocompleteContactFieldNameTable[] = {
#define AUTOCOMPLETE_CONTACT_FIELD_NAME(name_, value_) \
  {value_, eAutocompleteFieldName_##name_},
#include "AutocompleteFieldList.h"
#undef AUTOCOMPLETE_CONTACT_FIELD_NAME
    {nullptr, 0}};

static const nsAttrValue::EnumTable kAutocompleteFieldHintTable[] = {
#define AUTOCOMPLETE_FIELD_HINT(name_, value_) \
  {value_, eAutocompleteFieldHint_##name_},
#include "AutocompleteFieldList.h"
#undef AUTOCOMPLETE_FIELD_HINT
    {nullptr, 0}};

static const nsAttrValue::EnumTable kAutocompleteContactFieldHintTable[] = {
#define AUTOCOMPLETE_FIELD_CONTACT_HINT(name_, value_) \
  {value_, eAutocompleteFieldContactHint_##name_},
#include "AutocompleteFieldList.h"
#undef AUTOCOMPLETE_FIELD_CONTACT_HINT
    {nullptr, 0}};

namespace {

static PLDHashTable* sEventListenerManagersHash;

// A global hashtable to for keeping the arena alive for cross docGroup node
// adoption.
static nsRefPtrHashtable<nsPtrHashKey<const nsINode>, mozilla::dom::DOMArena>*
    sDOMArenaHashtable;

class DOMEventListenerManagersHashReporter final : public nsIMemoryReporter {
  MOZ_DEFINE_MALLOC_SIZE_OF(MallocSizeOf)

  ~DOMEventListenerManagersHashReporter() = default;

 public:
  NS_DECL_ISUPPORTS

  NS_IMETHOD CollectReports(nsIHandleReportCallback* aHandleReport,
                            nsISupports* aData, bool aAnonymize) override {
    // We don't measure the |EventListenerManager| objects pointed to by the
    // entries because those references are non-owning.
    int64_t amount =
        sEventListenerManagersHash
            ? sEventListenerManagersHash->ShallowSizeOfIncludingThis(
                  MallocSizeOf)
            : 0;

    MOZ_COLLECT_REPORT(
        "explicit/dom/event-listener-managers-hash", KIND_HEAP, UNITS_BYTES,
        amount, "Memory used by the event listener manager's hash table.");

    return NS_OK;
  }
};

NS_IMPL_ISUPPORTS(DOMEventListenerManagersHashReporter, nsIMemoryReporter)

class EventListenerManagerMapEntry : public PLDHashEntryHdr {
 public:
  explicit EventListenerManagerMapEntry(const void* aKey) : mKey(aKey) {}

  ~EventListenerManagerMapEntry() {
    NS_ASSERTION(!mListenerManager, "caller must release and disconnect ELM");
  }

 protected:          // declared protected to silence clang warnings
  const void* mKey;  // must be first, to look like PLDHashEntryStub

 public:
  RefPtr<EventListenerManager> mListenerManager;
};

static void EventListenerManagerHashInitEntry(PLDHashEntryHdr* entry,
                                              const void* key) {
  // Initialize the entry with placement new
  new (entry) EventListenerManagerMapEntry(key);
}

static void EventListenerManagerHashClearEntry(PLDHashTable* table,
                                               PLDHashEntryHdr* entry) {
  EventListenerManagerMapEntry* lm =
      static_cast<EventListenerManagerMapEntry*>(entry);

  // Let the EventListenerManagerMapEntry clean itself up...
  lm->~EventListenerManagerMapEntry();
}

class SameOriginCheckerImpl final : public nsIChannelEventSink,
                                    public nsIInterfaceRequestor {
  ~SameOriginCheckerImpl() = default;

  NS_DECL_ISUPPORTS
  NS_DECL_NSICHANNELEVENTSINK
  NS_DECL_NSIINTERFACEREQUESTOR
};

}  // namespace

void AutoSuppressEventHandling::SuppressDocument(Document* aDoc) {
  // Note: Document::SuppressEventHandling will also automatically suppress
  // event handling for any in-process sub-documents. However, since we need
  // to deal with cases where remote BrowsingContexts may be interleaved
  // with in-process ones, we still need to walk the entire tree ourselves.
  // This may be slightly redundant in some cases, but since event handling
  // suppressions maintain a count of current blockers, it does not cause
  // any problems.
  aDoc->SuppressEventHandling();
}

void AutoSuppressEventHandling::UnsuppressDocument(Document* aDoc) {
  aDoc->UnsuppressEventHandlingAndFireEvents(true);
}

AutoSuppressEventHandling::~AutoSuppressEventHandling() {
  UnsuppressDocuments();
}

void AutoSuppressEventHandlingAndSuspend::SuppressDocument(Document* aDoc) {
  AutoSuppressEventHandling::SuppressDocument(aDoc);
  if (nsCOMPtr<nsPIDOMWindowInner> win = aDoc->GetInnerWindow()) {
    win->Suspend();
    mWindows.AppendElement(win);
  }
}

AutoSuppressEventHandlingAndSuspend::~AutoSuppressEventHandlingAndSuspend() {
  for (const auto& win : mWindows) {
    win->Resume();
  }
}

/**
 * This class is used to determine whether or not the user is currently
 * interacting with the browser. It listens to observer events to toggle the
 * value of the sUserActive static.
 *
 * This class is an internal implementation detail.
 * nsContentUtils::GetUserIsInteracting() should be used to access current
 * user interaction status.
 */
class nsContentUtils::UserInteractionObserver final
    : public nsIObserver,
      public BackgroundHangAnnotator {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER

  void Init();
  void Shutdown();
  void AnnotateHang(BackgroundHangAnnotations& aAnnotations) override;

  static Atomic<bool> sUserActive;

 private:
  ~UserInteractionObserver() = default;
};

// static
nsresult nsContentUtils::Init() {
  if (sInitialized) {
    NS_WARNING("Init() called twice");

    return NS_OK;
  }

  nsHTMLTags::AddRefTable();

  sXPConnect = nsXPConnect::XPConnect();
  // We hold a strong ref to sXPConnect to ensure that it does not go away until
  // nsLayoutStatics::Shutdown is happening.  Otherwise ~nsXPConnect can be
  // triggered by xpcModuleDtor late in shutdown and cause crashes due to
  // various stuff already being torn down by then.  Note that this means that
  // we are effectively making sure that if we leak nsLayoutStatics then we also
  // leak nsXPConnect.
  NS_ADDREF(sXPConnect);

  sSecurityManager = nsScriptSecurityManager::GetScriptSecurityManager();
  if (!sSecurityManager) return NS_ERROR_FAILURE;
  NS_ADDREF(sSecurityManager);

  sSecurityManager->GetSystemPrincipal(&sSystemPrincipal);
  MOZ_ASSERT(sSystemPrincipal);

  RefPtr<NullPrincipal> nullPrincipal =
      NullPrincipal::CreateWithoutOriginAttributes();
  if (!nullPrincipal) {
    return NS_ERROR_FAILURE;
  }

  nullPrincipal.forget(&sNullSubjectPrincipal);

  nsresult rv = CallGetService(NS_IOSERVICE_CONTRACTID, &sIOService);
  if (NS_FAILED(rv)) {
    // This makes life easier, but we can live without it.

    sIOService = nullptr;
  }

  if (!InitializeEventTable()) return NS_ERROR_FAILURE;

  if (!sEventListenerManagersHash) {
    static const PLDHashTableOps hash_table_ops = {
        PLDHashTable::HashVoidPtrKeyStub, PLDHashTable::MatchEntryStub,
        PLDHashTable::MoveEntryStub, EventListenerManagerHashClearEntry,
        EventListenerManagerHashInitEntry};

    sEventListenerManagersHash =
        new PLDHashTable(&hash_table_ops, sizeof(EventListenerManagerMapEntry));

    RegisterStrongMemoryReporter(new DOMEventListenerManagersHashReporter());
  }

  sBlockedScriptRunners = new AutoTArray<nsCOMPtr<nsIRunnable>, 8>;

#ifndef RELEASE_OR_BETA
  sBypassCSSOMOriginCheck = getenv("MOZ_BYPASS_CSSOM_ORIGIN_CHECK");
#endif

  Element::InitCCCallbacks();

  Unused << nsRFPService::GetOrCreate();

  if (XRE_IsParentProcess()) {
    AsyncPrecreateStringBundles();
  }

  RefPtr<UserInteractionObserver> uio = new UserInteractionObserver();
  uio->Init();
  uio.forget(&sUserInteractionObserver);

  sInitialized = true;

  return NS_OK;
}

bool nsContentUtils::InitJSBytecodeMimeType() {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!sJSScriptBytecodeMimeType);
  MOZ_ASSERT(!sJSModuleBytecodeMimeType);

  JS::BuildIdCharVector jsBuildId;
  if (!JS::GetScriptTranscodingBuildId(&jsBuildId)) {
    return false;
  }

  nsDependentCSubstring jsBuildIdStr(jsBuildId.begin(), jsBuildId.length());
  sJSScriptBytecodeMimeType =
      new nsCString("javascript/moz-script-bytecode-"_ns + jsBuildIdStr);
  sJSModuleBytecodeMimeType =
      new nsCString("javascript/moz-module-bytecode-"_ns + jsBuildIdStr);
  return true;
}

void nsContentUtils::GetShiftText(nsAString& text) {
  if (!sShiftText) InitializeModifierStrings();
  text.Assign(*sShiftText);
}

void nsContentUtils::GetControlText(nsAString& text) {
  if (!sControlText) InitializeModifierStrings();
  text.Assign(*sControlText);
}

void nsContentUtils::GetMetaText(nsAString& text) {
  if (!sMetaText) InitializeModifierStrings();
  text.Assign(*sMetaText);
}

void nsContentUtils::GetOSText(nsAString& text) {
  if (!sOSText) {
    InitializeModifierStrings();
  }
  text.Assign(*sOSText);
}

void nsContentUtils::GetAltText(nsAString& text) {
  if (!sAltText) InitializeModifierStrings();
  text.Assign(*sAltText);
}

void nsContentUtils::GetModifierSeparatorText(nsAString& text) {
  if (!sModifierSeparator) InitializeModifierStrings();
  text.Assign(*sModifierSeparator);
}

void nsContentUtils::InitializeModifierStrings() {
  // load the display strings for the keyboard accelerators
  nsCOMPtr<nsIStringBundleService> bundleService =
      mozilla::components::StringBundle::Service();
  nsCOMPtr<nsIStringBundle> bundle;
  DebugOnly<nsresult> rv = NS_OK;
  if (bundleService) {
    rv = bundleService->CreateBundle(
        "chrome://global-platform/locale/platformKeys.properties",
        getter_AddRefs(bundle));
  }

  NS_ASSERTION(
      NS_SUCCEEDED(rv) && bundle,
      "chrome://global/locale/platformKeys.properties could not be loaded");
  nsAutoString shiftModifier;
  nsAutoString metaModifier;
  nsAutoString osModifier;
  nsAutoString altModifier;
  nsAutoString controlModifier;
  nsAutoString modifierSeparator;
  if (bundle) {
    // macs use symbols for each modifier key, so fetch each from the bundle,
    // which also covers i18n
    bundle->GetStringFromName("VK_SHIFT", shiftModifier);
    bundle->GetStringFromName("VK_META", metaModifier);
    bundle->GetStringFromName("VK_WIN", osModifier);
    bundle->GetStringFromName("VK_ALT", altModifier);
    bundle->GetStringFromName("VK_CONTROL", controlModifier);
    bundle->GetStringFromName("MODIFIER_SEPARATOR", modifierSeparator);
  }
  // if any of these don't exist, we get  an empty string
  sShiftText = new nsString(shiftModifier);
  sMetaText = new nsString(metaModifier);
  sOSText = new nsString(osModifier);
  sAltText = new nsString(altModifier);
  sControlText = new nsString(controlModifier);
  sModifierSeparator = new nsString(modifierSeparator);
}

mozilla::EventClassID nsContentUtils::GetEventClassIDFromMessage(
    EventMessage aEventMessage) {
  switch (aEventMessage) {
#define MESSAGE_TO_EVENT(name_, message_, type_, struct_) \
  case message_:                                          \
    return struct_;
#include "mozilla/EventNameList.h"
#undef MESSAGE_TO_EVENT
    default:
      MOZ_ASSERT_UNREACHABLE("Invalid event message?");
      return eBasicEventClass;
  }
}

static nsAtom* GetEventTypeFromMessage(EventMessage aEventMessage) {
  switch (aEventMessage) {
#define MESSAGE_TO_EVENT(name_, message_, type_, struct_) \
  case message_:                                          \
    return nsGkAtoms::on##name_;
#include "mozilla/EventNameList.h"
#undef MESSAGE_TO_EVENT
    default:
      return nullptr;
  }
}

// Because of SVG/SMIL we have several atoms mapped to the same
// id, but we can rely on MESSAGE_TO_EVENT to map id to only one atom.
static bool ShouldAddEventToStringEventTable(const EventNameMapping& aMapping) {
  MOZ_ASSERT(aMapping.mAtom);
  return GetEventTypeFromMessage(aMapping.mMessage) == aMapping.mAtom;
}

bool nsContentUtils::InitializeEventTable() {
  NS_ASSERTION(!sAtomEventTable, "EventTable already initialized!");
  NS_ASSERTION(!sStringEventTable, "EventTable already initialized!");

  static const EventNameMapping eventArray[] = {
#define EVENT(name_, _message, _type, _class) \
  {nsGkAtoms::on##name_, _type, _message, _class, false},
#define WINDOW_ONLY_EVENT EVENT
#define DOCUMENT_ONLY_EVENT EVENT
#define NON_IDL_EVENT EVENT
#include "mozilla/EventNameList.h"
#undef WINDOW_ONLY_EVENT
#undef NON_IDL_EVENT
#undef EVENT
      {nullptr}};

  sAtomEventTable = new nsTHashMap<nsRefPtrHashKey<nsAtom>, EventNameMapping>(
      ArrayLength(eventArray));
  sStringEventTable = new nsTHashMap<nsStringHashKey, EventNameMapping>(
      ArrayLength(eventArray));
  sUserDefinedEvents = new nsTArray<RefPtr<nsAtom>>(64);

  // Subtract one from the length because of the trailing null
  for (uint32_t i = 0; i < ArrayLength(eventArray) - 1; ++i) {
    MOZ_ASSERT(!sAtomEventTable->Contains(eventArray[i].mAtom),
               "Double-defining event name; fix your EventNameList.h");
    sAtomEventTable->InsertOrUpdate(eventArray[i].mAtom, eventArray[i]);
    if (ShouldAddEventToStringEventTable(eventArray[i])) {
      sStringEventTable->InsertOrUpdate(
          Substring(nsDependentAtomString(eventArray[i].mAtom), 2),
          eventArray[i]);
    }
  }

  return true;
}

void nsContentUtils::InitializeTouchEventTable() {
  static bool sEventTableInitialized = false;
  if (!sEventTableInitialized && sAtomEventTable && sStringEventTable) {
    sEventTableInitialized = true;
    static const EventNameMapping touchEventArray[] = {
#define EVENT(name_, _message, _type, _class)
#define TOUCH_EVENT(name_, _message, _type, _class) \
  {nsGkAtoms::on##name_, _type, _message, _class},
#include "mozilla/EventNameList.h"
#undef TOUCH_EVENT
#undef EVENT
        {nullptr}};
    // Subtract one from the length because of the trailing null
    for (uint32_t i = 0; i < ArrayLength(touchEventArray) - 1; ++i) {
      sAtomEventTable->InsertOrUpdate(touchEventArray[i].mAtom,
                                      touchEventArray[i]);
      sStringEventTable->InsertOrUpdate(
          Substring(nsDependentAtomString(touchEventArray[i].mAtom), 2),
          touchEventArray[i]);
    }
  }
}

static bool Is8bit(const nsAString& aString) {
  static const char16_t EIGHT_BIT = char16_t(~0x00FF);

  for (nsAString::const_char_iterator start = aString.BeginReading(),
                                      end = aString.EndReading();
       start != end; ++start) {
    if (*start & EIGHT_BIT) {
      return false;
    }
  }

  return true;
}

nsresult nsContentUtils::Btoa(const nsAString& aBinaryData,
                              nsAString& aAsciiBase64String) {
  if (!Is8bit(aBinaryData)) {
    aAsciiBase64String.Truncate();
    return NS_ERROR_DOM_INVALID_CHARACTER_ERR;
  }

  return Base64Encode(aBinaryData, aAsciiBase64String);
}

nsresult nsContentUtils::Atob(const nsAString& aAsciiBase64String,
                              nsAString& aBinaryData) {
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

bool nsContentUtils::IsAutocompleteEnabled(
    mozilla::dom::HTMLInputElement* aInput) {
  MOZ_ASSERT(aInput, "aInput should not be null!");

  nsAutoString autocomplete;
  aInput->GetAutocomplete(autocomplete);

  if (autocomplete.IsEmpty()) {
    auto* form = aInput->GetForm();
    if (!form) {
      return true;
    }

    form->GetAutocomplete(autocomplete);
  }

  return !autocomplete.EqualsLiteral("off");
}

nsContentUtils::AutocompleteAttrState
nsContentUtils::SerializeAutocompleteAttribute(
    const nsAttrValue* aAttr, nsAString& aResult,
    AutocompleteAttrState aCachedState) {
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
nsContentUtils::SerializeAutocompleteAttribute(
    const nsAttrValue* aAttr, mozilla::dom::AutocompleteInfo& aInfo,
    AutocompleteAttrState aCachedState, bool aGrantAllValidValue) {
  if (!aAttr ||
      aCachedState == nsContentUtils::eAutocompleteAttrState_Invalid) {
    return aCachedState;
  }

  return InternalSerializeAutocompleteAttribute(aAttr, aInfo,
                                                aGrantAllValidValue);
}

/**
 * Helper to validate the @autocomplete tokens.
 *
 * @return {AutocompleteAttrState} The state of the attribute (invalid/valid).
 */
nsContentUtils::AutocompleteAttrState
nsContentUtils::InternalSerializeAutocompleteAttribute(
    const nsAttrValue* aAttrVal, mozilla::dom::AutocompleteInfo& aInfo,
    bool aGrantAllValidValue) {
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
    unsupported = enumValue.ParseEnumValue(
        tokenString, kAutocompleteUnsupportedFieldNameTable, false);
    if (unsupported) {
      return eAutocompleteAttrState_Invalid;
    }
  }

  nsAutoString str;
  bool result =
      enumValue.ParseEnumValue(tokenString, kAutocompleteFieldNameTable, false);
  if (result) {
    // Off/Automatic/Normal categories.
    if (enumValue.Equals(u"off"_ns, eIgnoreCase) ||
        enumValue.Equals(u"on"_ns, eIgnoreCase)) {
      if (numTokens > 1) {
        return eAutocompleteAttrState_Invalid;
      }
      enumValue.ToString(str);
      ASCIIToLower(str);
      aInfo.mFieldName.Assign(str);
      aInfo.mCanAutomaticallyPersist =
          !enumValue.Equals(u"off"_ns, eIgnoreCase);
      return eAutocompleteAttrState_Valid;
    }

    // Only allow on/off if form autofill @autocomplete values aren't enabled
    // and it doesn't grant all valid values.
    if (!StaticPrefs::dom_forms_autocomplete_formautofill() &&
        !aGrantAllValidValue) {
      return eAutocompleteAttrState_Invalid;
    }

    // Normal category
    if (numTokens > 3) {
      return eAutocompleteAttrState_Invalid;
    }
    category = eAutocompleteCategory_NORMAL;
  } else {  // Check if the last token is of the contact category instead.
    // Only allow on/off if form autofill @autocomplete values aren't enabled
    // and it doesn't grant all valid values.
    if (!StaticPrefs::dom_forms_autocomplete_formautofill() &&
        !aGrantAllValidValue) {
      return eAutocompleteAttrState_Invalid;
    }

    result = enumValue.ParseEnumValue(
        tokenString, kAutocompleteContactFieldNameTable, false);
    if (!result || numTokens > 4) {
      return eAutocompleteAttrState_Invalid;
    }

    category = eAutocompleteCategory_CONTACT;
  }

  enumValue.ToString(str);
  ASCIIToLower(str);
  aInfo.mFieldName.Assign(str);

  aInfo.mCanAutomaticallyPersist = !enumValue.ParseEnumValue(
      tokenString, kAutocompleteNoPersistFieldNameTable, false);

  // We are done if this was the only token.
  if (numTokens == 1) {
    return eAutocompleteAttrState_Valid;
  }

  --index;
  tokenString = nsDependentAtomString(aAttrVal->AtomAt(index));

  if (category == eAutocompleteCategory_CONTACT) {
    if (!aGrantAllValidValue) {
      unsupported = enumValue.ParseEnumValue(
          tokenString, kAutocompleteUnsupportedContactFieldHintTable, false);
      if (unsupported) {
        return eAutocompleteAttrState_Invalid;
      }
    }

    nsAttrValue contactFieldHint;
    result = contactFieldHint.ParseEnumValue(
        tokenString, kAutocompleteContactFieldHintTable, false);
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
  if (fieldHint.ParseEnumValue(tokenString, kAutocompleteFieldHintTable,
                               false)) {
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
template <class CharT>
int32_t nsContentUtils::ParseHTMLIntegerImpl(
    const CharT* aStart, const CharT* aEnd,
    ParseHTMLIntegerResultFlags* aResult) {
  int result = eParseHTMLInteger_NoFlags;

  const CharT* iter = aStart;

  while (iter != aEnd && nsContentUtils::IsHTMLWhitespace(*iter)) {
    result |= eParseHTMLInteger_NonStandard;
    ++iter;
  }

  if (iter == aEnd) {
    result |= eParseHTMLInteger_Error | eParseHTMLInteger_ErrorNoValue;
    *aResult = (ParseHTMLIntegerResultFlags)result;
    return 0;
  }

  int sign = 1;
  if (*iter == CharT('-')) {
    sign = -1;
    result |= eParseHTMLInteger_Negative;
    ++iter;
  } else if (*iter == CharT('+')) {
    result |= eParseHTMLInteger_NonStandard;
    ++iter;
  }

  bool foundValue = false;
  CheckedInt32 value = 0;

  // Check for leading zeros first.
  uint64_t leadingZeros = 0;
  while (iter != aEnd) {
    if (*iter != CharT('0')) {
      break;
    }

    ++leadingZeros;
    foundValue = true;
    ++iter;
  }

  while (iter != aEnd) {
    if (*iter >= CharT('0') && *iter <= CharT('9')) {
      value = (value * 10) + (*iter - CharT('0')) * sign;
      ++iter;
      if (!value.isValid()) {
        result |= eParseHTMLInteger_Error | eParseHTMLInteger_ErrorOverflow;
        break;
      }
      foundValue = true;
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

  if (iter != aEnd) {
    result |= eParseHTMLInteger_DidNotConsumeAllInput;
  }

  *aResult = (ParseHTMLIntegerResultFlags)result;
  return value.isValid() ? value.value() : 0;
}

// Parse an integer according to HTML spec
int32_t nsContentUtils::ParseHTMLInteger(const char16_t* aStart,
                                         const char16_t* aEnd,
                                         ParseHTMLIntegerResultFlags* aResult) {
  return ParseHTMLIntegerImpl(aStart, aEnd, aResult);
}

int32_t nsContentUtils::ParseHTMLInteger(const char* aStart, const char* aEnd,
                                         ParseHTMLIntegerResultFlags* aResult) {
  return ParseHTMLIntegerImpl(aStart, aEnd, aResult);
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

bool nsContentUtils::GetPseudoAttributeValue(const nsString& aSource,
                                             nsAtom* aName, nsAString& aValue) {
  aValue.Truncate();

  const char16_t* start = aSource.get();
  const char16_t* end = start + aSource.Length();
  const char16_t* iter;

  while (start != end) {
    SKIP_WHITESPACE(start, end, false)
    iter = start;
    SKIP_ATTR_NAME(iter, end)

    if (start == iter) {
      return false;
    }

    // Remember the attr name.
    const nsDependentSubstring& attrName = Substring(start, iter);

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
      const char16_t* chunkEnd = start;
      while (chunkEnd != iter) {
        if (*chunkEnd == kLessThan) {
          aValue.Truncate();

          return false;
        }

        if (*chunkEnd == kAmpersand) {
          aValue.Append(start, chunkEnd - start);

          const char16_t* afterEntity = nullptr;
          char16_t result[2];
          uint32_t count = MOZ_XMLTranslateEntity(
              reinterpret_cast<const char*>(chunkEnd),
              reinterpret_cast<const char*>(iter),
              reinterpret_cast<const char**>(&afterEntity), result);
          if (count == 0) {
            aValue.Truncate();

            return false;
          }

          aValue.Append(result, count);

          // Advance to after the entity and begin a new chunk.
          start = chunkEnd = afterEntity;
        } else {
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

bool nsContentUtils::IsJavaScriptLanguage(const nsString& aName) {
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

void nsContentUtils::SplitMimeType(const nsAString& aValue, nsString& aType,
                                   nsString& aParams) {
  aType.Truncate();
  aParams.Truncate();
  int32_t semiIndex = aValue.FindChar(char16_t(';'));
  if (-1 != semiIndex) {
    aType = Substring(aValue, 0, semiIndex);
    aParams =
        Substring(aValue, semiIndex + 1, aValue.Length() - (semiIndex + 1));
    aParams.StripWhitespace();
  } else {
    aType = aValue;
  }
  aType.StripWhitespace();
}

nsresult nsContentUtils::IsUserIdle(uint32_t aRequestedIdleTimeInMS,
                                    bool* aUserIsIdle) {
  nsresult rv;
  nsCOMPtr<nsIUserIdleService> idleService =
      do_GetService("@mozilla.org/widget/useridleservice;1", &rv);
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
uint32_t nsContentUtils::ParseSandboxAttributeToFlags(
    const nsAttrValue* aSandboxAttr) {
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
bool nsContentUtils::IsValidSandboxFlag(const nsAString& aFlag) {
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
void nsContentUtils::SandboxFlagsToString(uint32_t aFlags, nsAString& aString) {
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

nsIBidiKeyboard* nsContentUtils::GetBidiKeyboard() {
  if (!sBidiKeyboard) {
    sBidiKeyboard = nsIWidget::CreateBidiKeyboard();
  }
  return sBidiKeyboard;
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
bool nsContentUtils::IsFirstLetterPunctuation(uint32_t aChar) {
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
bool nsContentUtils::IsAlphanumeric(uint32_t aChar) {
  nsUGenCategory cat = mozilla::unicode::GetGenCategory(aChar);

  return (cat == nsUGenCategory::kLetter || cat == nsUGenCategory::kNumber);
}

// static
bool nsContentUtils::IsAlphanumericOrSymbol(uint32_t aChar) {
  nsUGenCategory cat = mozilla::unicode::GetGenCategory(aChar);

  return cat == nsUGenCategory::kLetter || cat == nsUGenCategory::kNumber ||
         cat == nsUGenCategory::kSymbol;
}

/* static */
bool nsContentUtils::IsHTMLWhitespace(char16_t aChar) {
  return aChar == char16_t(0x0009) || aChar == char16_t(0x000A) ||
         aChar == char16_t(0x000C) || aChar == char16_t(0x000D) ||
         aChar == char16_t(0x0020);
}

/* static */
bool nsContentUtils::IsHTMLWhitespaceOrNBSP(char16_t aChar) {
  return IsHTMLWhitespace(aChar) || aChar == char16_t(0xA0);
}

/* static */
bool nsContentUtils::IsHTMLBlockLevelElement(nsIContent* aContent) {
  return aContent->IsAnyOfHTMLElements(
      nsGkAtoms::address, nsGkAtoms::article, nsGkAtoms::aside,
      nsGkAtoms::blockquote, nsGkAtoms::center, nsGkAtoms::dir, nsGkAtoms::div,
      nsGkAtoms::dl,  // XXX why not dt and dd?
      nsGkAtoms::fieldset,
      nsGkAtoms::figure,  // XXX shouldn't figcaption be on this list
      nsGkAtoms::footer, nsGkAtoms::form, nsGkAtoms::h1, nsGkAtoms::h2,
      nsGkAtoms::h3, nsGkAtoms::h4, nsGkAtoms::h5, nsGkAtoms::h6,
      nsGkAtoms::header, nsGkAtoms::hgroup, nsGkAtoms::hr, nsGkAtoms::li,
      nsGkAtoms::listing, nsGkAtoms::menu, nsGkAtoms::nav, nsGkAtoms::ol,
      nsGkAtoms::p, nsGkAtoms::pre, nsGkAtoms::section, nsGkAtoms::table,
      nsGkAtoms::ul, nsGkAtoms::xmp);
}

/* static */
bool nsContentUtils::ParseIntMarginValue(const nsAString& aString,
                                         nsIntMargin& result) {
  nsAutoString marginStr(aString);
  marginStr.CompressWhitespace(true, true);
  if (marginStr.IsEmpty()) {
    return false;
  }

  int32_t start = 0, end = 0;
  for (int count = 0; count < 4; count++) {
    if ((uint32_t)end >= marginStr.Length()) return false;

    // top, right, bottom, left
    if (count < 3)
      end = Substring(marginStr, start).FindChar(',');
    else
      end = Substring(marginStr, start).Length();

    if (end <= 0) return false;

    nsresult ec;
    int32_t val = nsString(Substring(marginStr, start, end)).ToInteger(&ec);
    if (NS_FAILED(ec)) return false;

    switch (count) {
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
int32_t nsContentUtils::ParseLegacyFontSize(const nsAString& aValue) {
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
    value = 10 * value + (*iter - char16_t('0'));
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
void nsContentUtils::GetOfflineAppManifest(Document* aDocument, nsIURI** aURI) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aDocument);
  *aURI = nullptr;

  if (aDocument->GetController().isSome()) {
    return;
  }

  Element* docElement = aDocument->GetRootElement();
  if (!docElement) {
    return;
  }

  nsAutoString manifestSpec;
  docElement->GetAttr(kNameSpaceID_None, nsGkAtoms::manifest, manifestSpec);

  // Manifest URIs can't have fragment identifiers.
  if (manifestSpec.IsEmpty() || manifestSpec.Contains('#')) {
    return;
  }

  nsContentUtils::NewURIWithDocumentCharset(aURI, manifestSpec, aDocument,
                                            aDocument->GetDocBaseURI());
}

/* static */
bool nsContentUtils::OfflineAppAllowed(nsIURI* aURI) { return false; }

/* static */
bool nsContentUtils::OfflineAppAllowed(nsIPrincipal* aPrincipal) {
  return false;
}
// Static
bool nsContentUtils::IsErrorPage(nsIURI* aURI) {
  if (!aURI) {
    return false;
  }

  if (!aURI->SchemeIs("about")) {
    return false;
  }

  nsAutoCString name;
  nsresult rv = NS_GetAboutModuleName(aURI, name);
  NS_ENSURE_SUCCESS(rv, false);

  return name.EqualsLiteral("certerror") || name.EqualsLiteral("neterror") ||
         name.EqualsLiteral("blocked");
}

// static
void nsContentUtils::Shutdown() {
  sInitialized = false;

  nsHTMLTags::ReleaseTable();

  NS_IF_RELEASE(sContentPolicyService);
  sTriedToGetContentPolicy = false;
  uint32_t i;
  for (i = 0; i < PropertiesFile_COUNT; ++i) NS_IF_RELEASE(sStringBundles[i]);

  NS_IF_RELEASE(sStringBundleService);
  NS_IF_RELEASE(sConsoleService);
  NS_IF_RELEASE(sXPConnect);
  NS_IF_RELEASE(sSecurityManager);
  NS_IF_RELEASE(sSystemPrincipal);
  NS_IF_RELEASE(sNullSubjectPrincipal);
  NS_IF_RELEASE(sIOService);

  sBidiKeyboard = nullptr;

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

  if (sDOMArenaHashtable) {
    MOZ_ASSERT(sDOMArenaHashtable->Count() == 0);
    MOZ_ASSERT(StaticPrefs::dom_arena_allocator_enabled_AtStartup());
    delete sDOMArenaHashtable;
    sDOMArenaHashtable = nullptr;
  }

  NS_ASSERTION(!sBlockedScriptRunners || sBlockedScriptRunners->Length() == 0,
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

  delete sJSScriptBytecodeMimeType;
  sJSScriptBytecodeMimeType = nullptr;

  delete sJSModuleBytecodeMimeType;
  sJSModuleBytecodeMimeType = nullptr;

  NS_IF_RELEASE(sSameOriginChecker);

  if (sUserInteractionObserver) {
    sUserInteractionObserver->Shutdown();
    NS_RELEASE(sUserInteractionObserver);
  }

  TextControlState::Shutdown();
  nsMappedAttributes::Shutdown();
}

/**
 * Checks whether two nodes come from the same origin. aTrustedNode is
 * considered 'safe' in that a user can operate on it.
 */
// static
nsresult nsContentUtils::CheckSameOrigin(const nsINode* aTrustedNode,
                                         const nsINode* unTrustedNode) {
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
bool nsContentUtils::CanCallerAccess(nsIPrincipal* aSubjectPrincipal,
                                     nsIPrincipal* aPrincipal) {
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
bool nsContentUtils::CanCallerAccess(const nsINode* aNode) {
  nsIPrincipal* subject = SubjectPrincipal();
  if (subject->IsSystemPrincipal()) {
    return true;
  }

  if (aNode->ChromeOnlyAccess()) {
    return false;
  }

  return CanCallerAccess(subject, aNode->NodePrincipal());
}

// static
bool nsContentUtils::CanCallerAccess(nsPIDOMWindowInner* aWindow) {
  nsCOMPtr<nsIScriptObjectPrincipal> scriptObject = do_QueryInterface(aWindow);
  NS_ENSURE_TRUE(scriptObject, false);

  return CanCallerAccess(SubjectPrincipal(), scriptObject->GetPrincipal());
}

// static
bool nsContentUtils::PrincipalHasPermission(nsIPrincipal& aPrincipal,
                                            const nsAtom* aPerm) {
  // Chrome gets access by default.
  if (aPrincipal.IsSystemPrincipal()) {
    return true;
  }

  // Otherwise, only allow if caller is an addon with the permission.
  return BasePrincipal::Cast(aPrincipal).AddonHasPermission(aPerm);
}

// static
bool nsContentUtils::CallerHasPermission(JSContext* aCx, const nsAtom* aPerm) {
  return PrincipalHasPermission(*SubjectPrincipal(aCx), aPerm);
}

// static
nsIPrincipal* nsContentUtils::GetAttrTriggeringPrincipal(
    nsIContent* aContent, const nsAString& aAttrValue,
    nsIPrincipal* aSubjectPrincipal) {
  nsIPrincipal* contentPrin = aContent ? aContent->NodePrincipal() : nullptr;

  // If the subject principal is the same as the content principal, or no
  // explicit subject principal was provided, we don't need to do any further
  // checks. Just return the content principal.
  if (contentPrin == aSubjectPrincipal || !aSubjectPrincipal) {
    return contentPrin;
  }

  // Only use the subject principal if the URL string we are going to end up
  // fetching is under the control of that principal, which is never the case
  // for relative URLs.
  if (aAttrValue.IsEmpty() ||
      !IsAbsoluteURL(NS_ConvertUTF16toUTF8(aAttrValue))) {
    return contentPrin;
  }

  // Only use the subject principal as the attr triggering principal if it
  // should override the CSP of the node's principal.
  if (BasePrincipal::Cast(aSubjectPrincipal)->OverridesCSP(contentPrin)) {
    return aSubjectPrincipal;
  }

  return contentPrin;
}

// static
bool nsContentUtils::IsAbsoluteURL(const nsACString& aURL) {
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

// static
bool nsContentUtils::InProlog(nsINode* aNode) {
  MOZ_ASSERT(aNode, "missing node to nsContentUtils::InProlog");

  nsINode* parent = aNode->GetParentNode();
  if (!parent || !parent->IsDocument()) {
    return false;
  }

  const Document* doc = parent->AsDocument();
  const nsIContent* root = doc->GetRootElement();
  if (!root) {
    return true;
  }
  const Maybe<uint32_t> indexOfNode = doc->ComputeIndexOf(aNode);
  const Maybe<uint32_t> indexOfRoot = doc->ComputeIndexOf(root);
  if (MOZ_LIKELY(indexOfNode.isSome() && indexOfRoot.isSome())) {
    return *indexOfNode < *indexOfRoot;
  }
  // XXX Keep the odd traditional behavior for now.
  return indexOfNode.isNothing() && indexOfRoot.isSome();
}

bool nsContentUtils::IsCallerChrome() {
  MOZ_ASSERT(NS_IsMainThread());
  return SubjectPrincipal() == sSystemPrincipal;
}

#ifdef FUZZING
bool nsContentUtils::IsFuzzingEnabled() {
  return StaticPrefs::fuzzing_enabled();
}
#endif

/* static */
bool nsContentUtils::IsCallerChromeOrElementTransformGettersEnabled(
    JSContext* aCx, JSObject*) {
  return ThreadsafeIsSystemCaller(aCx) ||
         StaticPrefs::dom_element_transform_getters_enabled();
}

/* static */
bool nsContentUtils::IsCallerChromeOrErrorPage(JSContext* aCx,
                                               JSObject* aObject) {
  if (ThreadsafeIsSystemCaller(aCx)) {
    return true;
  }
  nsGlobalWindowInner* win =
      xpc::WindowGlobalOrNull(js::UncheckedUnwrap(aObject));
  if (!win) {
    return false;
  }
  Document* doc = win->GetExtantDoc();
  return doc && IsErrorPage(doc->GetDocumentURI());
}

/* static */
bool nsContentUtils::ShouldResistFingerprinting() {
  return StaticPrefs::privacy_resistFingerprinting();
}

/* static */
bool nsContentUtils::ShouldResistFingerprinting(
    nsIGlobalObject* aGlobalObject) {
  if (!aGlobalObject) {
    return ShouldResistFingerprinting();
  }
  return aGlobalObject->ShouldResistFingerprinting();
}

bool nsContentUtils::ShouldResistFingerprinting(nsIDocShell* aDocShell) {
  if (!aDocShell) {
    return ShouldResistFingerprinting();
  }
  return ShouldResistFingerprinting(aDocShell->GetDocument());
}

/* static */
bool nsContentUtils::ShouldResistFingerprinting(const Document* aDoc) {
  if (!aDoc) {
    MOZ_LOG(nsContentUtils::ResistFingerprintingLog(), LogLevel::Info,
            ("Called nsContentUtils::ShouldResistFingerprinting(const "
             "Document* aDoc) with NULL document"));
    return ShouldResistFingerprinting();
  }
  bool isChrome = nsContentUtils::IsChromeDoc(aDoc);
  if (isChrome) {
    return false;
  }
  return ShouldResistFingerprinting(aDoc->GetChannel());
}

/* static */
bool nsContentUtils::ShouldResistFingerprinting(nsIPrincipal* aPrincipal) {
  if (!aPrincipal) {
    return ShouldResistFingerprinting();
  }
  bool isChrome = aPrincipal->IsSystemPrincipal();
  return !isChrome && ShouldResistFingerprinting();
}

/* static */
bool nsContentUtils::ShouldResistFingerprinting(const char* aJustification) {
  // See comment in header file for information about usage
  return ShouldResistFingerprinting();
}

inline void LogDomainAndPrefList(const char* exemptedDomainsPrefName,
                                 nsAutoCString& url, bool isExemptDomain) {
  nsAutoCString list;
  Preferences::GetCString(exemptedDomainsPrefName, list);
  MOZ_LOG(nsContentUtils::ResistFingerprintingLog(), LogLevel::Debug,
          ("Domain \"%s\" is %s the exempt list \"%s\"",
           PromiseFlatCString(url).get(), isExemptDomain ? "in" : "NOT in",
           PromiseFlatCString(list).get()));
}

// These constants are used for privacy.resistFingerprinting.testGranularityMask
const unsigned int sWebExtensionExemptMask = 0x01;
const unsigned int sNonPBMExemptMask = 0x02;
const unsigned int sSpecificDomainsExemptMask = 0x04;

/* static */
bool nsContentUtils::ShouldResistFingerprinting(nsIChannel* aChannel) {
  if (!ShouldResistFingerprinting("Legacy quick-check")) {
    return false;
  }

  if (!aChannel) {
    MOZ_LOG(nsContentUtils::ResistFingerprintingLog(), LogLevel::Info,
            ("Called nsContentUtils::ShouldResistFingerprinting(nsIChannel* "
             "aChannel) with NULL channel"));
    return true;
  }

  nsCOMPtr<nsILoadInfo> loadInfo = aChannel->LoadInfo();

  if (StaticPrefs::privacy_resistFingerprinting_testGranularityMask() &
      sNonPBMExemptMask) {
    // if non-PBM exempt mask is true, exempt non-PBM channels.
    if (loadInfo->GetOriginAttributes().mPrivateBrowsingId == 0) {
      return false;
    }
  }

  bool isExemptDomain = false;
  const char* exemptedDomainsPrefName =
      "privacy.resistFingerprinting.exemptedDomains";

  if (loadInfo->GetExternalContentPolicyType() ==
      ExtContentPolicy::TYPE_DOCUMENT) {
    // Case 1: Top Level Load

    nsCOMPtr<nsIURI> channelURI;
    nsresult rv = NS_GetFinalChannelURI(aChannel, getter_AddRefs(channelURI));
    MOZ_ASSERT(
        NS_SUCCEEDED(rv),
        "Failed to get URI in "
        "nsContentUtils::ShouldResistFingerprinting(nsIChannel* aChannel)");
    // this check is to ensure that we do not crash in non-debug builds.
    if (NS_FAILED(rv)) {
      return true;
    }

    // Exclude internal schemes
    if (channelURI->SchemeIs("about") || channelURI->SchemeIs("chrome") ||
        channelURI->SchemeIs("resource") ||
        channelURI->SchemeIs("view-source")) {
      return false;
    }

    if (StaticPrefs::privacy_resistFingerprinting_testGranularityMask() &
        sWebExtensionExemptMask) {
      if (channelURI->SchemeIs("web-extension")) {
        return false;
      }
    }

    if (StaticPrefs::privacy_resistFingerprinting_testGranularityMask() &
        sSpecificDomainsExemptMask) {
      nsAutoCString list;
      Preferences::GetCString(exemptedDomainsPrefName, list);
      ToLowerCase(list);
      isExemptDomain = IsURIInList(channelURI, list);

      if (MOZ_LOG_TEST(nsContentUtils::ResistFingerprintingLog(),
                       mozilla::LogLevel::Debug)) {
        nsAutoCString url;
        channelURI->GetHost(url);
        LogDomainAndPrefList(exemptedDomainsPrefName, url, isExemptDomain);
      }
    }

    return !isExemptDomain;
  }

  // Case 2: Subresource Load
  return ShouldResistFingerprinting(loadInfo->GetLoadingPrincipal(),
                                    loadInfo->GetOriginAttributes());
}

/* static */
bool nsContentUtils::ShouldResistFingerprinting(
    nsIPrincipal* aPrincipal,
    const mozilla::OriginAttributes& aOriginAttributes) {
  if (!ShouldResistFingerprinting("Legacy quick-check")) {
    return false;
  }

  if (StaticPrefs::privacy_resistFingerprinting_testGranularityMask() &
      sNonPBMExemptMask) {
    // if non-PBM exempt mask is true, exempt non-PBM channels.
    if (aOriginAttributes.mPrivateBrowsingId == 0) {
      return false;
    }
  }

  // Exclude internal schemes
  if (aPrincipal->SchemeIs("about") || aPrincipal->SchemeIs("chrome") ||
      aPrincipal->SchemeIs("resource") || aPrincipal->SchemeIs("view-source")) {
    return false;
  }

  if (StaticPrefs::privacy_resistFingerprinting_testGranularityMask() &
      sWebExtensionExemptMask) {
    if (BasePrincipal::Cast(aPrincipal)->AddonPolicy()) {
      return false;
    }
  }

  bool isExemptDomain = false;
  const char* exemptedDomainsPrefName =
      "privacy.resistFingerprinting.exemptedDomains";

  if (StaticPrefs::privacy_resistFingerprinting_testGranularityMask() &
      sSpecificDomainsExemptMask) {
    aPrincipal->IsURIInPrefList(exemptedDomainsPrefName, &isExemptDomain);

    if (MOZ_LOG_TEST(nsContentUtils::ResistFingerprintingLog(),
                     mozilla::LogLevel::Debug)) {
      nsAutoCString origin;
      aPrincipal->GetAsciiOrigin(origin);
      LogDomainAndPrefList(exemptedDomainsPrefName, origin, isExemptDomain);
    }
  }

  return !isExemptDomain;
}

/* static */
void nsContentUtils::CalcRoundedWindowSizeForResistingFingerprinting(
    int32_t aChromeWidth, int32_t aChromeHeight, int32_t aScreenWidth,
    int32_t aScreenHeight, int32_t aInputWidth, int32_t aInputHeight,
    bool aSetOuterWidth, bool aSetOuterHeight, int32_t* aOutputWidth,
    int32_t* aOutputHeight) {
  MOZ_ASSERT(aOutputWidth);
  MOZ_ASSERT(aOutputHeight);

  int32_t availContentWidth = 0;
  int32_t availContentHeight = 0;

  availContentWidth = std::min(StaticPrefs::privacy_window_maxInnerWidth(),
                               aScreenWidth - aChromeWidth);
#ifdef MOZ_WIDGET_GTK
  // In the GTK window, it will not report outside system decorations
  // when we get available window size, see Bug 581863. So, we leave a
  // 40 pixels space for them when calculating the available content
  // height. It is not necessary for the width since the content width
  // is usually pretty much the same as the chrome width.
  availContentHeight = std::min(StaticPrefs::privacy_window_maxInnerHeight(),
                                (-40 + aScreenHeight) - aChromeHeight);
#else
  availContentHeight = std::min(StaticPrefs::privacy_window_maxInnerHeight(),
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
    resultWidth = NSToIntCeil((aInputWidth - chromeOffsetWidth) / 200.0) * 200 +
                  chromeOffsetWidth;
  }

  if (aInputHeight > (availContentHeight + chromeOffsetHeight)) {
    resultHeight = availContentHeight + chromeOffsetHeight;
  } else if (aInputHeight < (100 + chromeOffsetHeight)) {
    resultHeight = 100 + chromeOffsetHeight;
  } else {
    resultHeight =
        NSToIntCeil((aInputHeight - chromeOffsetHeight) / 100.0) * 100 +
        chromeOffsetHeight;
  }

  *aOutputWidth = resultWidth;
  *aOutputHeight = resultHeight;
}

bool nsContentUtils::ThreadsafeIsCallerChrome() {
  return NS_IsMainThread() ? IsCallerChrome()
                           : IsCurrentThreadRunningChromeWorker();
}

bool nsContentUtils::IsCallerUAWidget() {
  JSContext* cx = GetCurrentJSContext();
  if (!cx) {
    return false;
  }

  JS::Realm* realm = JS::GetCurrentRealmOrNull(cx);
  if (!realm) {
    return false;
  }

  return xpc::IsUAWidgetScope(realm);
}

bool nsContentUtils::IsSystemCaller(JSContext* aCx) {
  // Note that SubjectPrincipal() assumes we are in a compartment here.
  return SubjectPrincipal(aCx) == sSystemPrincipal;
}

bool nsContentUtils::ThreadsafeIsSystemCaller(JSContext* aCx) {
  CycleCollectedJSContext* ccjscx = CycleCollectedJSContext::Get();
  MOZ_ASSERT(ccjscx->Context() == aCx);

  return ccjscx->IsSystemCaller();
}

// static
bool nsContentUtils::LookupBindingMember(
    JSContext* aCx, nsIContent* aContent, JS::Handle<jsid> aId,
    JS::MutableHandle<JS::PropertyDescriptor> aDesc) {
  return true;
}

nsINode* nsContentUtils::GetNearestInProcessCrossDocParentNode(
    nsINode* aChild) {
  if (aChild->IsDocument()) {
    for (BrowsingContext* bc = aChild->AsDocument()->GetBrowsingContext(); bc;
         bc = bc->GetParent()) {
      if (bc->GetEmbedderElement()) {
        return bc->GetEmbedderElement();
      }
    }
    return nullptr;
  }

  nsINode* parent = aChild->GetParentNode();
  if (parent && parent->IsContent() && aChild->IsContent()) {
    parent = aChild->AsContent()->GetFlattenedTreeParent();
  }

  return parent;
}

bool nsContentUtils::ContentIsHostIncludingDescendantOf(
    const nsINode* aPossibleDescendant, const nsINode* aPossibleAncestor) {
  MOZ_ASSERT(aPossibleDescendant, "The possible descendant is null!");
  MOZ_ASSERT(aPossibleAncestor, "The possible ancestor is null!");

  do {
    if (aPossibleDescendant == aPossibleAncestor) return true;
    if (aPossibleDescendant->IsDocumentFragment()) {
      aPossibleDescendant =
          aPossibleDescendant->AsDocumentFragment()->GetHost();
    } else {
      aPossibleDescendant = aPossibleDescendant->GetParentNode();
    }
  } while (aPossibleDescendant);

  return false;
}

// static
bool nsContentUtils::ContentIsCrossDocDescendantOf(nsINode* aPossibleDescendant,
                                                   nsINode* aPossibleAncestor) {
  MOZ_ASSERT(aPossibleDescendant, "The possible descendant is null!");
  MOZ_ASSERT(aPossibleAncestor, "The possible ancestor is null!");

  do {
    if (aPossibleDescendant == aPossibleAncestor) {
      return true;
    }

    aPossibleDescendant =
        GetNearestInProcessCrossDocParentNode(aPossibleDescendant);
  } while (aPossibleDescendant);

  return false;
}

// static
bool nsContentUtils::ContentIsFlattenedTreeDescendantOf(
    const nsINode* aPossibleDescendant, const nsINode* aPossibleAncestor) {
  MOZ_ASSERT(aPossibleDescendant, "The possible descendant is null!");
  MOZ_ASSERT(aPossibleAncestor, "The possible ancestor is null!");

  do {
    if (aPossibleDescendant == aPossibleAncestor) {
      return true;
    }
    aPossibleDescendant = aPossibleDescendant->GetFlattenedTreeParentNode();
  } while (aPossibleDescendant);

  return false;
}

// static
bool nsContentUtils::ContentIsFlattenedTreeDescendantOfForStyle(
    const nsINode* aPossibleDescendant, const nsINode* aPossibleAncestor) {
  MOZ_ASSERT(aPossibleDescendant, "The possible descendant is null!");
  MOZ_ASSERT(aPossibleAncestor, "The possible ancestor is null!");

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
nsINode* nsContentUtils::Retarget(nsINode* aTargetA, nsINode* aTargetB) {
  while (true && aTargetA) {
    // If A's root is not a shadow root...
    nsINode* root = aTargetA->SubtreeRoot();
    if (!root->IsShadowRoot()) {
      // ...then return A.
      return aTargetA;
    }

    // or A's root is a shadow-including inclusive ancestor of B...
    if (aTargetB->IsShadowIncludingInclusiveDescendantOf(root)) {
      // ...then return A.
      return aTargetA;
    }

    aTargetA = ShadowRoot::FromNode(root)->GetHost();
  }

  return nullptr;
}

// static
nsresult nsContentUtils::GetInclusiveAncestors(nsINode* aNode,
                                               nsTArray<nsINode*>& aArray) {
  while (aNode) {
    aArray.AppendElement(aNode);
    aNode = aNode->GetParentNode();
  }
  return NS_OK;
}

// static
nsresult nsContentUtils::GetInclusiveAncestorsAndOffsets(
    nsINode* aNode, uint32_t aOffset, nsTArray<nsIContent*>* aAncestorNodes,
    nsTArray<Maybe<uint32_t>>* aAncestorOffsets) {
  NS_ENSURE_ARG_POINTER(aNode);

  if (!aNode->IsContent()) {
    return NS_ERROR_FAILURE;
  }
  nsIContent* content = aNode->AsContent();

  if (!aAncestorNodes->IsEmpty()) {
    NS_WARNING("aAncestorNodes is not empty");
    aAncestorNodes->Clear();
  }

  if (!aAncestorOffsets->IsEmpty()) {
    NS_WARNING("aAncestorOffsets is not empty");
    aAncestorOffsets->Clear();
  }

  // insert the node itself
  aAncestorNodes->AppendElement(content);
  aAncestorOffsets->AppendElement(Some(aOffset));

  // insert all the ancestors
  nsIContent* child = content;
  nsIContent* parent = child->GetParent();
  while (parent) {
    aAncestorNodes->AppendElement(parent);
    aAncestorOffsets->AppendElement(parent->ComputeIndexOf(child));
    child = parent;
    parent = parent->GetParent();
  }

  return NS_OK;
}

template <typename Node, typename GetParentFunc>
static Node* GetCommonAncestorInternal(Node* aNode1, Node* aNode2,
                                       GetParentFunc aGetParentFunc) {
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
nsINode* nsContentUtils::GetCommonAncestorHelper(nsINode* aNode1,
                                                 nsINode* aNode2) {
  return GetCommonAncestorInternal(
      aNode1, aNode2, [](nsINode* aNode) { return aNode->GetParentNode(); });
}

/* static */
nsIContent* nsContentUtils::GetCommonFlattenedTreeAncestorHelper(
    nsIContent* aContent1, nsIContent* aContent2) {
  return GetCommonAncestorInternal(
      aContent1, aContent2,
      [](nsIContent* aContent) { return aContent->GetFlattenedTreeParent(); });
}

/* static */
Element* nsContentUtils::GetCommonFlattenedTreeAncestorForStyle(
    Element* aElement1, Element* aElement2) {
  return GetCommonAncestorInternal(aElement1, aElement2, [](Element* aElement) {
    return aElement->GetFlattenedTreeParentElementForStyle();
  });
}

/* static */
bool nsContentUtils::PositionIsBefore(nsINode* aNode1, nsINode* aNode2,
                                      Maybe<uint32_t>* aNode1Index,
                                      Maybe<uint32_t>* aNode2Index) {
  // Note, CompareDocumentPosition takes the latter params in different order.
  return (aNode2->CompareDocumentPosition(*aNode1, aNode2Index, aNode1Index) &
          (Node_Binding::DOCUMENT_POSITION_PRECEDING |
           Node_Binding::DOCUMENT_POSITION_DISCONNECTED)) ==
         Node_Binding::DOCUMENT_POSITION_PRECEDING;
}

/* static */
Maybe<int32_t> nsContentUtils::ComparePoints(
    const nsINode* aParent1, uint32_t aOffset1, const nsINode* aParent2,
    uint32_t aOffset2, ComparePointsCache* aParent1Cache) {
  bool disconnected{false};

  const int32_t order = ComparePoints_Deprecated(
      aParent1, aOffset1, aParent2, aOffset2, &disconnected, aParent1Cache);
  if (disconnected) {
    return Nothing();
  }

  return Some(order);
}

/* static */
int32_t nsContentUtils::ComparePoints_Deprecated(
    const nsINode* aParent1, uint32_t aOffset1, const nsINode* aParent2,
    uint32_t aOffset2, bool* aDisconnected, ComparePointsCache* aParent1Cache) {
  if (aParent1 == aParent2) {
    return aOffset1 < aOffset2 ? -1 : aOffset1 > aOffset2 ? 1 : 0;
  }

  AutoTArray<const nsINode*, 32> parents1, parents2;
  const nsINode* node1 = aParent1;
  const nsINode* node2 = aParent2;
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
  const nsINode* parent = parents1.ElementAt(pos1);
  uint32_t len;
  for (len = std::min(pos1, pos2); len > 0; --len) {
    const nsINode* child1 = parents1.ElementAt(--pos1);
    const nsINode* child2 = parents2.ElementAt(--pos2);
    if (child1 != child2) {
      const Maybe<uint32_t> child1Index =
          aParent1Cache ? aParent1Cache->ComputeIndexOf(parent, child1)
                        : parent->ComputeIndexOf(child1);
      const Maybe<uint32_t> child2Index = parent->ComputeIndexOf(child2);
      if (MOZ_LIKELY(child1Index.isSome() && child2Index.isSome())) {
        return *child1Index < *child2Index ? -1 : 1;
      }
      // XXX Keep the odd traditional behavior for now.
      return child1Index.isNothing() && child2Index.isSome() ? -1 : 1;
    }
    parent = child1;
  }

  // The parent chains never differed, so one of the nodes is an ancestor of
  // the other

  NS_ASSERTION(!pos1 || !pos2,
               "should have run out of parent chain for one of the nodes");

  if (!pos1) {
    const nsINode* child2 = parents2.ElementAt(--pos2);
    const Maybe<uint32_t> child2Index = parent->ComputeIndexOf(child2);
    if (MOZ_UNLIKELY(NS_WARN_IF(child2Index.isNothing()))) {
      return 1;
    }
    return aOffset1 <= *child2Index ? -1 : 1;
  }

  const nsINode* child1 = parents1.ElementAt(--pos1);
  const Maybe<uint32_t> child1Index =
      aParent1Cache ? aParent1Cache->ComputeIndexOf(parent, child1)
                    : parent->ComputeIndexOf(child1);
  if (MOZ_UNLIKELY(NS_WARN_IF(child1Index.isNothing()))) {
    return -1;
  }
  return *child1Index < aOffset2 ? -1 : 1;
}

// static
nsINode* nsContentUtils::GetCommonAncestorUnderInteractiveContent(
    nsINode* aNode1, nsINode* aNode2) {
  if (!aNode1 || !aNode2) {
    return nullptr;
  }

  if (aNode1 == aNode2) {
    return aNode1;
  }

  // Build the chain of parents
  AutoTArray<nsINode*, 30> parents1;
  do {
    parents1.AppendElement(aNode1);
    if (aNode1->IsElement() &&
        aNode1->AsElement()->IsInteractiveHTMLContent()) {
      break;
    }
    aNode1 = aNode1->GetFlattenedTreeParentNode();
  } while (aNode1);

  AutoTArray<nsINode*, 30> parents2;
  do {
    parents2.AppendElement(aNode2);
    if (aNode2->IsElement() &&
        aNode2->AsElement()->IsInteractiveHTMLContent()) {
      break;
    }
    aNode2 = aNode2->GetFlattenedTreeParentNode();
  } while (aNode2);

  // Find where the parent chain differs
  uint32_t pos1 = parents1.Length();
  uint32_t pos2 = parents2.Length();
  nsINode* parent = nullptr;
  for (uint32_t len = std::min(pos1, pos2); len > 0; --len) {
    nsINode* child1 = parents1.ElementAt(--pos1);
    nsINode* child2 = parents2.ElementAt(--pos2);
    if (child1 != child2) {
      break;
    }
    parent = child1;
  }

  return parent;
}

/* static */
BrowserParent* nsContentUtils::GetCommonBrowserParentAncestor(
    BrowserParent* aBrowserParent1, BrowserParent* aBrowserParent2) {
  return GetCommonAncestorInternal(
      aBrowserParent1, aBrowserParent2, [](BrowserParent* aBrowserParent) {
        return aBrowserParent->GetBrowserBridgeParent()
                   ? aBrowserParent->GetBrowserBridgeParent()->Manager()
                   : nullptr;
      });
}

/* static */
template <typename FPT, typename FRT, typename SPT, typename SRT>
Maybe<int32_t> nsContentUtils::ComparePoints(
    const RangeBoundaryBase<FPT, FRT>& aFirstBoundary,
    const RangeBoundaryBase<SPT, SRT>& aSecondBoundary) {
  if (!aFirstBoundary.IsSet() || !aSecondBoundary.IsSet()) {
    return Nothing{};
  }

  bool disconnected{false};
  const int32_t order =
      ComparePoints_Deprecated(aFirstBoundary, aSecondBoundary, &disconnected);

  if (disconnected) {
    return Nothing{};
  }

  return Some(order);
}

/* static */
template <typename FPT, typename FRT, typename SPT, typename SRT>
int32_t nsContentUtils::ComparePoints_Deprecated(
    const RangeBoundaryBase<FPT, FRT>& aFirstBoundary,
    const RangeBoundaryBase<SPT, SRT>& aSecondBoundary, bool* aDisconnected) {
  if (NS_WARN_IF(!aFirstBoundary.IsSet()) ||
      NS_WARN_IF(!aSecondBoundary.IsSet())) {
    return -1;
  }
  // XXX Re-implement this without calling `Offset()` as far as possible,
  //     and the other overload should be an alias of this.
  return ComparePoints_Deprecated(
      aFirstBoundary.Container(),
      *aFirstBoundary.Offset(
          RangeBoundaryBase<FPT, FRT>::OffsetFilter::kValidOrInvalidOffsets),
      aSecondBoundary.Container(),
      *aSecondBoundary.Offset(
          RangeBoundaryBase<SPT, SRT>::OffsetFilter::kValidOrInvalidOffsets),
      aDisconnected);
}

inline bool IsCharInSet(const char* aSet, const char16_t aChar) {
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
const nsDependentSubstring nsContentUtils::TrimCharsInSet(
    const char* aSet, const nsAString& aValue) {
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
    ++valueEnd;  // Step beyond the last character we want in the value.
  }

  // valueEnd should point to the char after the last to copy
  return Substring(valueCurrent, valueEnd);
}

/**
 * This method strips leading and trailing whitespace from a string.
 */

// static
template <bool IsWhitespace(char16_t)>
const nsDependentSubstring nsContentUtils::TrimWhitespace(const nsAString& aStr,
                                                          bool aTrimTrailing) {
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
template const nsDependentSubstring
nsContentUtils::TrimWhitespace<nsCRT::IsAsciiSpace>(const nsAString&, bool);
template const nsDependentSubstring nsContentUtils::TrimWhitespace<
    nsContentUtils::IsHTMLWhitespace>(const nsAString&, bool);
template const nsDependentSubstring nsContentUtils::TrimWhitespace<
    nsContentUtils::IsHTMLWhitespaceOrNBSP>(const nsAString&, bool);

static inline void KeyAppendSep(nsACString& aKey) {
  if (!aKey.IsEmpty()) {
    aKey.Append('>');
  }
}

static inline void KeyAppendString(const nsAString& aString, nsACString& aKey) {
  KeyAppendSep(aKey);

  // Could escape separator here if collisions happen.  > is not a legal char
  // for a name or type attribute, so we should be safe avoiding that extra
  // work.

  AppendUTF16toUTF8(aString, aKey);
}

static inline void KeyAppendString(const nsACString& aString,
                                   nsACString& aKey) {
  KeyAppendSep(aKey);

  // Could escape separator here if collisions happen.  > is not a legal char
  // for a name or type attribute, so we should be safe avoiding that extra
  // work.

  aKey.Append(aString);
}

static inline void KeyAppendInt(int32_t aInt, nsACString& aKey) {
  KeyAppendSep(aKey);

  aKey.AppendInt(aInt);
}

static inline bool IsAutocompleteOff(const nsIContent* aContent) {
  return aContent->IsElement() &&
         aContent->AsElement()->AttrValueIs(kNameSpaceID_None,
                                            nsGkAtoms::autocomplete, u"off"_ns,
                                            eIgnoreCase);
}

/*static*/
void nsContentUtils::GenerateStateKey(nsIContent* aContent, Document* aDocument,
                                      nsACString& aKey) {
  MOZ_ASSERT(aContent);

  aKey.Truncate();

  uint32_t partID = aDocument ? aDocument->GetPartID() : 0;

  // Don't capture state for anonymous content
  if (aContent->IsInNativeAnonymousSubtree()) {
    return;
  }

  if (IsAutocompleteOff(aContent)) {
    return;
  }

  RefPtr<Document> doc = aContent->GetUncomposedDoc();

  KeyAppendInt(partID, aKey);  // first append a partID
  bool generatedUniqueKey = false;

  if (doc && doc->IsHTMLOrXHTML()) {
    nsHTMLDocument* htmlDoc = doc->AsHTMLDocument();

    // If we have a form control and can calculate form information, use that
    // as the key - it is more reliable than just recording position in the
    // DOM.
    // XXXbz Is it, really?  We have bugs on this, I think...
    // Important to have a unique key, and tag/type/name may not be.
    //
    // The format of the key depends on whether the control has a form,
    // and whether the element was parser inserted:
    //
    // [Has Form, Parser Inserted]:
    //   fp>type>FormNum>IndOfControlInForm>FormName>name
    //
    // [No Form, Parser Inserted]:
    //   dp>type>ControlNum>name
    //
    // [Has Form, Not Parser Inserted]:
    //   fn>type>IndOfFormInDoc>IndOfControlInForm>FormName>name
    //
    // [No Form, Not Parser Inserted]:
    //   dn>type>IndOfControlInDoc>name
    //
    // XXX We don't need to use index if name is there
    // XXXbz We don't?  Why not?  I don't follow.
    //
    nsCOMPtr<nsIFormControl> control(do_QueryInterface(aContent));
    if (control) {
      // Get the control number if this was a parser inserted element from the
      // network.
      int32_t controlNumber =
          control->GetParserInsertedControlNumberForStateKey();
      bool parserInserted = controlNumber != -1;

      RefPtr<nsContentList> htmlForms;
      RefPtr<nsContentList> htmlFormControls;
      if (!parserInserted) {
        // Getting these lists is expensive, as we need to keep them up to date
        // as the document loads, so we avoid it if we don't need them.
        htmlDoc->GetFormsAndFormControls(getter_AddRefs(htmlForms),
                                         getter_AddRefs(htmlFormControls));
      }

      // Append the control type
      KeyAppendInt(int32_t(control->ControlType()), aKey);

      // If in a form, add form name / index of form / index in form
      HTMLFormElement* formElement = control->GetForm();
      if (formElement) {
        if (IsAutocompleteOff(formElement)) {
          aKey.Truncate();
          return;
        }

        // Append the form number, if this is a parser inserted control, or
        // the index of the form in the document otherwise.
        bool appendedForm = false;
        if (parserInserted) {
          MOZ_ASSERT(formElement->GetFormNumberForStateKey() != -1,
                     "when generating a state key for a parser inserted form "
                     "control we should have a parser inserted <form> element");
          KeyAppendString("fp"_ns, aKey);
          KeyAppendInt(formElement->GetFormNumberForStateKey(), aKey);
          appendedForm = true;
        } else {
          KeyAppendString("fn"_ns, aKey);
          int32_t index = htmlForms->IndexOf(formElement, false);
          if (index <= -1) {
            //
            // XXX HACK this uses some state that was dumped into the document
            // specifically to fix bug 138892.  What we are trying to do is
            // *guess* which form this control's state is found in, with the
            // highly likely guess that the highest form parsed so far is the
            // one. This code should not be on trunk, only branch.
            //
            index = htmlDoc->GetNumFormsSynchronous() - 1;
          }
          if (index > -1) {
            KeyAppendInt(index, aKey);
            appendedForm = true;
          }
        }

        if (appendedForm) {
          // Append the index of the control in the form
          int32_t index = formElement->IndexOfContent(aContent);

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
        // Not in a form.  Append the control number, if this is a parser
        // inserted control, or the index of the control in the document
        // otherwise.
        if (parserInserted) {
          KeyAppendString("dp"_ns, aKey);
          KeyAppendInt(control->GetParserInsertedControlNumberForStateKey(),
                       aKey);
          generatedUniqueKey = true;
        } else {
          KeyAppendString("dn"_ns, aKey);
          int32_t index = htmlFormControls->IndexOf(aContent, true);
          if (index > -1) {
            KeyAppendInt(index, aKey);
            generatedUniqueKey = true;
          }
        }

        // Append the control name
        nsAutoString name;
        aContent->AsElement()->GetAttr(kNameSpaceID_None, nsGkAtoms::name,
                                       name);
        KeyAppendString(name, aKey);
      }
    }
  }

  if (!generatedUniqueKey) {
    // Either we didn't have a form control or we aren't in an HTML document so
    // we can't figure out form info.  Append the tag name if it's an element
    // to avoid restoring state for one type of element on another type.
    if (aContent->IsElement()) {
      KeyAppendString(nsDependentAtomString(aContent->NodeInfo()->NameAtom()),
                      aKey);
    } else {
      // Append a character that is not "d" or "f" to disambiguate from
      // the case when we were a form control in an HTML document.
      KeyAppendString("o"_ns, aKey);
    }

    // Now start at aContent and append the indices of it and all its ancestors
    // in their containers.  That should at least pin down its position in the
    // DOM...
    nsINode* parent = aContent->GetParentNode();
    nsINode* content = aContent;
    while (parent) {
      KeyAppendInt(parent->ComputeIndexOf_Deprecated(content), aKey);
      content = parent;
      parent = content->GetParentNode();
    }
  }
}

// static
nsIPrincipal* nsContentUtils::SubjectPrincipal(JSContext* aCx) {
  MOZ_ASSERT(NS_IsMainThread());

  // As opposed to SubjectPrincipal(), we do in fact assume that
  // we're in a realm here; anyone who calls this function in
  // situations where that's not the case is doing it wrong.
  JS::Realm* realm = js::GetContextRealm(aCx);
  MOZ_ASSERT(realm);

  JSPrincipals* principals = JS::GetRealmPrincipals(realm);
  return nsJSPrincipals::get(principals);
}

// static
nsIPrincipal* nsContentUtils::SubjectPrincipal() {
  MOZ_ASSERT(IsInitialized());
  MOZ_ASSERT(NS_IsMainThread());
  JSContext* cx = GetCurrentJSContext();
  if (!cx) {
    MOZ_CRASH(
        "Accessing the Subject Principal without an AutoJSAPI on the stack is "
        "forbidden");
  }

  JS::Realm* realm = js::GetContextRealm(cx);

  // When an AutoJSAPI is instantiated, we are in a null realm until the
  // first JSAutoRealm, which is kind of a purgatory as far as permissions
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
  // release-mode assert during realm creation against using this principal on
  // an actual global.
  if (!realm) {
    return sNullSubjectPrincipal;
  }

  return SubjectPrincipal(cx);
}

// static
nsIPrincipal* nsContentUtils::ObjectPrincipal(JSObject* aObj) {
  MOZ_ASSERT(NS_IsMainThread());

#ifdef DEBUG
  JS::AssertObjectBelongsToCurrentThread(aObj);
#endif

  MOZ_DIAGNOSTIC_ASSERT(!js::IsCrossCompartmentWrapper(aObj));

  JS::Realm* realm = js::GetNonCCWObjectRealm(aObj);
  JSPrincipals* principals = JS::GetRealmPrincipals(realm);
  return nsJSPrincipals::get(principals);
}

// static
nsresult nsContentUtils::NewURIWithDocumentCharset(nsIURI** aResult,
                                                   const nsAString& aSpec,
                                                   Document* aDocument,
                                                   nsIURI* aBaseURI) {
  if (aDocument) {
    return NS_NewURI(aResult, aSpec, aDocument->GetDocumentCharacterSet(),
                     aBaseURI);
  }
  return NS_NewURI(aResult, aSpec, nullptr, aBaseURI);
}

// static
bool nsContentUtils::IsNameWithDash(nsAtom* aName) {
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
    if (i + 1 < len && NS_IS_SURROGATE_PAIR(name[i], name[i + 1])) {
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

      if (name[i] != '-' && name[i] != '.' && name[i] != '_' &&
          name[i] != 0xB7 && (name[i] < '0' || name[i] > '9') &&
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

  return hasDash;
}

// static
bool nsContentUtils::IsCustomElementName(nsAtom* aName, uint32_t aNameSpaceID) {
  // Allow non-dashed names in XUL for XBL to Custom Element migrations.
  if (aNameSpaceID == kNameSpaceID_XUL) {
    return true;
  }

  bool hasDash = IsNameWithDash(aName);
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
         aName != nsGkAtoms::colorProfile && aName != nsGkAtoms::font_face &&
         aName != nsGkAtoms::font_face_src &&
         aName != nsGkAtoms::font_face_uri &&
         aName != nsGkAtoms::font_face_format &&
         aName != nsGkAtoms::font_face_name && aName != nsGkAtoms::missingGlyph;
}

// static
nsresult nsContentUtils::CheckQName(const nsAString& aQualifiedName,
                                    bool aNamespaceAware,
                                    const char16_t** aColon) {
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

// static
nsresult nsContentUtils::SplitQName(const nsIContent* aNamespaceResolver,
                                    const nsString& aQName, int32_t* aNamespace,
                                    nsAtom** aLocalName) {
  const char16_t* colon;
  nsresult rv = nsContentUtils::CheckQName(aQName, true, &colon);
  NS_ENSURE_SUCCESS(rv, rv);

  if (colon) {
    const char16_t* end;
    aQName.EndReading(end);
    nsAutoString nameSpace;
    rv = aNamespaceResolver->LookupNamespaceURIInternal(
        Substring(aQName.get(), colon), nameSpace);
    NS_ENSURE_SUCCESS(rv, rv);

    *aNamespace = nsNameSpaceManager::GetInstance()->GetNameSpaceID(
        nameSpace, nsContentUtils::IsChromeDoc(aNamespaceResolver->OwnerDoc()));
    if (*aNamespace == kNameSpaceID_Unknown) return NS_ERROR_FAILURE;

    *aLocalName = NS_AtomizeMainThread(Substring(colon + 1, end)).take();
  } else {
    *aNamespace = kNameSpaceID_None;
    *aLocalName = NS_AtomizeMainThread(aQName).take();
  }
  NS_ENSURE_TRUE(aLocalName, NS_ERROR_OUT_OF_MEMORY);
  return NS_OK;
}

// static
nsresult nsContentUtils::GetNodeInfoFromQName(
    const nsAString& aNamespaceURI, const nsAString& aQualifiedName,
    nsNodeInfoManager* aNodeInfoManager, uint16_t aNodeType,
    mozilla::dom::NodeInfo** aNodeInfo) {
  const nsString& qName = PromiseFlatString(aQualifiedName);
  const char16_t* colon;
  nsresult rv = nsContentUtils::CheckQName(qName, true, &colon);
  NS_ENSURE_SUCCESS(rv, rv);

  int32_t nsID;
  nsNameSpaceManager::GetInstance()->RegisterNameSpace(aNamespaceURI, nsID);
  if (colon) {
    const char16_t* end;
    qName.EndReading(end);

    RefPtr<nsAtom> prefix = NS_AtomizeMainThread(Substring(qName.get(), colon));

    rv = aNodeInfoManager->GetNodeInfo(Substring(colon + 1, end), prefix, nsID,
                                       aNodeType, aNodeInfo);
  } else {
    rv = aNodeInfoManager->GetNodeInfo(aQualifiedName, nullptr, nsID, aNodeType,
                                       aNodeInfo);
  }
  NS_ENSURE_SUCCESS(rv, rv);

  return nsContentUtils::IsValidNodeName((*aNodeInfo)->NameAtom(),
                                         (*aNodeInfo)->GetPrefixAtom(),
                                         (*aNodeInfo)->NamespaceID())
             ? NS_OK
             : NS_ERROR_DOM_NAMESPACE_ERR;
}

// static
void nsContentUtils::SplitExpatName(const char16_t* aExpatName,
                                    nsAtom** aPrefix, nsAtom** aLocalName,
                                    int32_t* aNameSpaceID) {
  /**
   *  Expat can send the following:
   *    localName
   *    namespaceURI<separator>localName
   *    namespaceURI<separator>localName<separator>prefix
   *
   *  and we use 0xFFFF for the <separator>.
   *
   */

  const char16_t* uriEnd = nullptr;
  const char16_t* nameEnd = nullptr;
  const char16_t* pos;
  for (pos = aExpatName; *pos; ++pos) {
    if (*pos == 0xFFFF) {
      if (uriEnd) {
        nameEnd = pos;
      } else {
        uriEnd = pos;
      }
    }
  }

  const char16_t* nameStart;
  if (uriEnd) {
    nsNameSpaceManager::GetInstance()->RegisterNameSpace(
        nsDependentSubstring(aExpatName, uriEnd), *aNameSpaceID);

    nameStart = (uriEnd + 1);
    if (nameEnd) {
      const char16_t* prefixStart = nameEnd + 1;
      *aPrefix = NS_AtomizeMainThread(Substring(prefixStart, pos)).take();
    } else {
      nameEnd = pos;
      *aPrefix = nullptr;
    }
  } else {
    *aNameSpaceID = kNameSpaceID_None;
    nameStart = aExpatName;
    nameEnd = pos;
    *aPrefix = nullptr;
  }
  *aLocalName = NS_AtomizeMainThread(Substring(nameStart, nameEnd)).take();
}

// static
PresShell* nsContentUtils::GetPresShellForContent(const nsIContent* aContent) {
  Document* doc = aContent->GetComposedDoc();
  if (!doc) {
    return nullptr;
  }
  return doc->GetPresShell();
}

// static
nsPresContext* nsContentUtils::GetContextForContent(
    const nsIContent* aContent) {
  PresShell* presShell = GetPresShellForContent(aContent);
  if (!presShell) {
    return nullptr;
  }
  return presShell->GetPresContext();
}

// static
bool nsContentUtils::CanLoadImage(nsIURI* aURI, nsINode* aNode,
                                  Document* aLoadingDocument,
                                  nsIPrincipal* aLoadingPrincipal) {
  MOZ_ASSERT(aURI, "Must have a URI");
  MOZ_ASSERT(aLoadingDocument, "Must have a document");
  MOZ_ASSERT(aLoadingPrincipal, "Must have a loading principal");

  nsresult rv;

  auto appType = nsIDocShell::APP_TYPE_UNKNOWN;

  {
    nsCOMPtr<nsIDocShellTreeItem> docShellTreeItem =
        aLoadingDocument->GetDocShell();
    if (docShellTreeItem) {
      nsCOMPtr<nsIDocShellTreeItem> root;
      docShellTreeItem->GetInProcessRootTreeItem(getter_AddRefs(root));

      nsCOMPtr<nsIDocShell> docShell(do_QueryInterface(root));

      if (docShell) {
        appType = docShell->GetAppType();
      }
    }
  }

  if (appType != nsIDocShell::APP_TYPE_EDITOR) {
    // Editor apps get special treatment here, editors can load images
    // from anywhere.  This allows editor to insert images from file://
    // into documents that are being edited.
    rv = sSecurityManager->CheckLoadURIWithPrincipal(
        aLoadingPrincipal, aURI, nsIScriptSecurityManager::ALLOW_CHROME,
        aLoadingDocument->InnerWindowID());
    if (NS_FAILED(rv)) {
      return false;
    }
  }

  nsCOMPtr<nsILoadInfo> secCheckLoadInfo = new mozilla::net::LoadInfo(
      aLoadingPrincipal,
      aLoadingPrincipal,  // triggering principal
      aNode, nsILoadInfo::SEC_ONLY_FOR_EXPLICIT_CONTENTSEC_CHECK,
      nsIContentPolicy::TYPE_INTERNAL_IMAGE);

  int16_t decision = nsIContentPolicy::ACCEPT;

  rv = NS_CheckContentLoadPolicy(aURI, secCheckLoadInfo,
                                 ""_ns,  // mime guess
                                 &decision, GetContentPolicy());

  return NS_SUCCEEDED(rv) && NS_CP_ACCEPTED(decision);
}

// static
bool nsContentUtils::IsInPrivateBrowsing(Document* aDoc) {
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
bool nsContentUtils::IsInPrivateBrowsing(nsILoadGroup* aLoadGroup) {
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

// FIXME(emilio): This is (effectively) almost but not quite the same as
// Document::ShouldLoadImages(), which one is right?
bool nsContentUtils::DocumentInactiveForImageLoads(Document* aDocument) {
  if (!aDocument) {
    return false;
  }
  if (IsChromeDoc(aDocument) || aDocument->IsResourceDoc() ||
      aDocument->IsStaticDocument()) {
    return false;
  }
  nsCOMPtr<nsPIDOMWindowInner> win =
      do_QueryInterface(aDocument->GetScopeObject());
  return !win || !win->GetDocShell();
}

imgLoader* nsContentUtils::GetImgLoaderForDocument(Document* aDoc) {
  NS_ENSURE_TRUE(!DocumentInactiveForImageLoads(aDoc), nullptr);

  if (!aDoc) {
    return imgLoader::NormalLoader();
  }
  bool isPrivate = IsInPrivateBrowsing(aDoc);
  return isPrivate ? imgLoader::PrivateBrowsingLoader()
                   : imgLoader::NormalLoader();
}

// static
imgLoader* nsContentUtils::GetImgLoaderForChannel(nsIChannel* aChannel,
                                                  Document* aContext) {
  NS_ENSURE_TRUE(!DocumentInactiveForImageLoads(aContext), nullptr);

  if (!aChannel) {
    return imgLoader::NormalLoader();
  }
  nsCOMPtr<nsILoadContext> context;
  NS_QueryNotificationCallbacks(aChannel, context);
  return context && context->UsePrivateBrowsing()
             ? imgLoader::PrivateBrowsingLoader()
             : imgLoader::NormalLoader();
}

// static
bool nsContentUtils::IsImageInCache(nsIURI* aURI, Document* aDocument) {
  imgILoader* loader = GetImgLoaderForDocument(aDocument);
  nsCOMPtr<imgICache> cache = do_QueryInterface(loader);

  // If something unexpected happened we return false, otherwise if props
  // is set, the image is cached and we return true
  nsCOMPtr<nsIProperties> props;
  nsresult rv =
      cache->FindEntryProperties(aURI, aDocument, getter_AddRefs(props));
  return (NS_SUCCEEDED(rv) && props);
}

// static
int32_t nsContentUtils::CORSModeToLoadImageFlags(mozilla::CORSMode aMode) {
  switch (aMode) {
    case CORS_ANONYMOUS:
      return imgILoader::LOAD_CORS_ANONYMOUS;
    case CORS_USE_CREDENTIALS:
      return imgILoader::LOAD_CORS_USE_CREDENTIALS;
    default:
      return 0;
  }
}

// static
nsresult nsContentUtils::LoadImage(
    nsIURI* aURI, nsINode* aContext, Document* aLoadingDocument,
    nsIPrincipal* aLoadingPrincipal, uint64_t aRequestContextID,
    nsIReferrerInfo* aReferrerInfo, imgINotificationObserver* aObserver,
    int32_t aLoadFlags, const nsAString& initiatorType,
    imgRequestProxy** aRequest, nsContentPolicyType aContentPolicyType,
    bool aUseUrgentStartForChannel, bool aLinkPreload) {
  MOZ_ASSERT(aURI, "Must have a URI");
  MOZ_ASSERT(aContext, "Must have a context");
  MOZ_ASSERT(aLoadingDocument, "Must have a document");
  MOZ_ASSERT(aLoadingPrincipal, "Must have a principal");
  MOZ_ASSERT(aRequest, "Null out param");

  imgLoader* imgLoader = GetImgLoaderForDocument(aLoadingDocument);
  if (!imgLoader) {
    // nothing we can do here
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsILoadGroup> loadGroup = aLoadingDocument->GetDocumentLoadGroup();

  nsIURI* documentURI = aLoadingDocument->GetDocumentURI();

  NS_ASSERTION(loadGroup || IsFontTableURI(documentURI),
               "Could not get loadgroup; onload may fire too early");

  // XXXbz using "documentURI" for the initialDocumentURI is not quite
  // right, but the best we can do here...
  return imgLoader->LoadImage(aURI,               /* uri to load */
                              documentURI,        /* initialDocumentURI */
                              aReferrerInfo,      /* referrerInfo */
                              aLoadingPrincipal,  /* loading principal */
                              aRequestContextID,  /* request context ID */
                              loadGroup,          /* loadgroup */
                              aObserver,          /* imgINotificationObserver */
                              aContext,           /* loading context */
                              aLoadingDocument,   /* uniquification key */
                              aLoadFlags,         /* load flags */
                              nullptr,            /* cache key */
                              aContentPolicyType, /* content policy type */
                              initiatorType,      /* the load initiator */
                              aUseUrgentStartForChannel, /* urgent-start flag */
                              aLinkPreload, /* <link preload> initiator */
                              aRequest);
}

// static
already_AddRefed<imgIContainer> nsContentUtils::GetImageFromContent(
    nsIImageLoadingContent* aContent, imgIRequest** aRequest) {
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
    // If the consumer wants the request, verify it has actually loaded
    // successfully.
    uint32_t imgStatus;
    imgRequest->GetImageStatus(&imgStatus);
    if (imgStatus & imgIRequest::STATUS_FRAME_COMPLETE &&
        !(imgStatus & imgIRequest::STATUS_ERROR)) {
      imgRequest.swap(*aRequest);
    }
  }

  return imgContainer.forget();
}

static bool IsLinkWithURI(const nsIContent& aContent) {
  const auto* element = Element::FromNode(aContent);
  if (!element || !element->IsLink()) {
    return false;
  }
  nsCOMPtr<nsIURI> absURI = element->GetHrefURI();
  return !!absURI;
}

static bool HasImageRequest(nsIContent& aContent) {
  nsCOMPtr<nsIImageLoadingContent> imageContent(do_QueryInterface(&aContent));
  if (!imageContent) {
    return false;
  }

  nsCOMPtr<imgIRequest> imgRequest;
  imageContent->GetRequest(nsIImageLoadingContent::CURRENT_REQUEST,
                           getter_AddRefs(imgRequest));

  // XXXbz It may be draggable even if the request resulted in an error.  Why?
  // Not sure; that's what the old nsContentAreaDragDrop/nsFrame code did.
  return !!imgRequest;
}

static Maybe<bool> DraggableOverride(const nsIContent& aContent) {
  if (auto* el = nsGenericHTMLElement::FromNode(aContent)) {
    if (el->Draggable()) {
      return Some(true);
    }

    if (el->AttrValueIs(kNameSpaceID_None, nsGkAtoms::draggable,
                        nsGkAtoms::_false, eIgnoreCase)) {
      return Some(false);
    }
  }
  if (aContent.IsSVGElement()) {
    return Some(false);
  }
  return Nothing();
}

// static
bool nsContentUtils::ContentIsDraggable(nsIContent* aContent) {
  MOZ_ASSERT(aContent);

  if (auto draggable = DraggableOverride(*aContent)) {
    return *draggable;
  }

  // special handling for content area image and link dragging
  return HasImageRequest(*aContent) || IsLinkWithURI(*aContent);
}

// static
bool nsContentUtils::IsDraggableImage(nsIContent* aContent) {
  MOZ_ASSERT(aContent);
  return HasImageRequest(*aContent) &&
         DraggableOverride(*aContent).valueOr(true);
}

// static
bool nsContentUtils::IsDraggableLink(const nsIContent* aContent) {
  MOZ_ASSERT(aContent);
  return IsLinkWithURI(*aContent) && DraggableOverride(*aContent).valueOr(true);
}

// static
nsresult nsContentUtils::QNameChanged(mozilla::dom::NodeInfo* aNodeInfo,
                                      nsAtom* aName,
                                      mozilla::dom::NodeInfo** aResult) {
  nsNodeInfoManager* niMgr = aNodeInfo->NodeInfoManager();

  *aResult = niMgr
                 ->GetNodeInfo(aName, nullptr, aNodeInfo->NamespaceID(),
                               aNodeInfo->NodeType(), aNodeInfo->GetExtraName())
                 .take();
  return NS_OK;
}

static bool TestSitePerm(nsIPrincipal* aPrincipal, const nsACString& aType,
                         uint32_t aPerm, bool aExactHostMatch) {
  if (!aPrincipal) {
    // We always deny (i.e. don't allow) the permission if we don't have a
    // principal.
    return aPerm != nsIPermissionManager::ALLOW_ACTION;
  }

  nsCOMPtr<nsIPermissionManager> permMgr =
      components::PermissionManager::Service();
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

bool nsContentUtils::IsSitePermAllow(nsIPrincipal* aPrincipal,
                                     const nsACString& aType) {
  return TestSitePerm(aPrincipal, aType, nsIPermissionManager::ALLOW_ACTION,
                      false);
}

bool nsContentUtils::IsSitePermDeny(nsIPrincipal* aPrincipal,
                                    const nsACString& aType) {
  return TestSitePerm(aPrincipal, aType, nsIPermissionManager::DENY_ACTION,
                      false);
}

bool nsContentUtils::IsExactSitePermAllow(nsIPrincipal* aPrincipal,
                                          const nsACString& aType) {
  return TestSitePerm(aPrincipal, aType, nsIPermissionManager::ALLOW_ACTION,
                      true);
}

bool nsContentUtils::IsExactSitePermDeny(nsIPrincipal* aPrincipal,
                                         const nsACString& aType) {
  return TestSitePerm(aPrincipal, aType, nsIPermissionManager::DENY_ACTION,
                      true);
}

bool nsContentUtils::HasSitePerm(nsIPrincipal* aPrincipal,
                                 const nsACString& aType) {
  if (!aPrincipal) {
    return false;
  }

  nsCOMPtr<nsIPermissionManager> permMgr =
      components::PermissionManager::Service();
  NS_ENSURE_TRUE(permMgr, false);

  uint32_t perm;
  nsresult rv = permMgr->TestPermissionFromPrincipal(aPrincipal, aType, &perm);
  NS_ENSURE_SUCCESS(rv, false);

  return perm != nsIPermissionManager::UNKNOWN_ACTION;
}

static const char* gEventNames[] = {"event"};
static const char* gSVGEventNames[] = {"evt"};
// for b/w compat, the first name to onerror is still 'event', even though it
// is actually the error message
static const char* gOnErrorNames[] = {"event", "source", "lineno", "colno",
                                      "error"};

// static
void nsContentUtils::GetEventArgNames(int32_t aNameSpaceID, nsAtom* aEventName,
                                      bool aIsForWindow, uint32_t* aArgCount,
                                      const char*** aArgArray) {
#define SET_EVENT_ARG_NAMES(names)               \
  *aArgCount = sizeof(names) / sizeof(names[0]); \
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

// Note: The list of content bundles in nsStringBundle.cpp should be updated
// whenever entries are added or removed from this list.
static const char* gPropertiesFiles[nsContentUtils::PropertiesFile_COUNT] = {
    // Must line up with the enum values in |PropertiesFile| enum.
    "chrome://global/locale/css.properties",
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
    "chrome://necko/locale/necko.properties",
    "resource://gre/res/locale/layout/HtmlForm.properties",
    "resource://gre/res/locale/dom/dom.properties"};

/* static */
nsresult nsContentUtils::EnsureStringBundle(PropertiesFile aFile) {
  if (!sStringBundles[aFile]) {
    if (!sStringBundleService) {
      nsresult rv =
          CallGetService(NS_STRINGBUNDLE_CONTRACTID, &sStringBundleService);
      NS_ENSURE_SUCCESS(rv, rv);
    }
    nsIStringBundle* bundle;
    nsresult rv =
        sStringBundleService->CreateBundle(gPropertiesFiles[aFile], &bundle);
    NS_ENSURE_SUCCESS(rv, rv);
    sStringBundles[aFile] = bundle;  // transfer ownership
  }
  return NS_OK;
}

/* static */
void nsContentUtils::AsyncPrecreateStringBundles() {
  // We only ever want to pre-create bundles in the parent process.
  //
  // All nsContentUtils bundles are shared between the parent and child
  // precesses, and the shared memory regions that back them *must* be created
  // in the parent, and then sent to all children.
  //
  // If we attempt to create a bundle in the child before its memory region is
  // available, we need to create a temporary non-shared bundle, and later
  // replace that with the shared memory copy. So attempting to pre-load in the
  // child is wasteful and unnecessary.
  MOZ_ASSERT(XRE_IsParentProcess());

  for (uint32_t bundleIndex = 0; bundleIndex < PropertiesFile_COUNT;
       ++bundleIndex) {
    nsresult rv = NS_DispatchToCurrentThreadQueue(
        NS_NewRunnableFunction("AsyncPrecreateStringBundles",
                               [bundleIndex]() {
                                 PropertiesFile file =
                                     static_cast<PropertiesFile>(bundleIndex);
                                 EnsureStringBundle(file);
                                 nsIStringBundle* bundle = sStringBundles[file];
                                 bundle->AsyncPreload();
                               }),
        EventQueuePriority::Idle);
    Unused << NS_WARN_IF(NS_FAILED(rv));
  }
}

/* static */
bool nsContentUtils::SpoofLocaleEnglish() {
  // 0 - will prompt
  // 1 - don't spoof
  // 2 - spoof
  return StaticPrefs::privacy_spoof_english() == 2;
}

static nsContentUtils::PropertiesFile GetMaybeSpoofedPropertiesFile(
    nsContentUtils::PropertiesFile aFile, const char* aKey,
    Document* aDocument) {
  // When we spoof English, use en-US properties in strings that are accessible
  // by content.
  bool spoofLocale = nsContentUtils::SpoofLocaleEnglish() &&
                     (!aDocument || !aDocument->AllowsL10n());
  if (spoofLocale) {
    switch (aFile) {
      case nsContentUtils::eFORMS_PROPERTIES:
        return nsContentUtils::eFORMS_PROPERTIES_en_US;
      case nsContentUtils::eDOM_PROPERTIES:
        return nsContentUtils::eDOM_PROPERTIES_en_US;
      default:
        break;
    }
  }
  return aFile;
}

/* static */
nsresult nsContentUtils::GetMaybeLocalizedString(PropertiesFile aFile,
                                                 const char* aKey,
                                                 Document* aDocument,
                                                 nsAString& aResult) {
  return GetLocalizedString(
      GetMaybeSpoofedPropertiesFile(aFile, aKey, aDocument), aKey, aResult);
}

/* static */
nsresult nsContentUtils::GetLocalizedString(PropertiesFile aFile,
                                            const char* aKey,
                                            nsAString& aResult) {
  nsresult rv = EnsureStringBundle(aFile);
  NS_ENSURE_SUCCESS(rv, rv);
  nsIStringBundle* bundle = sStringBundles[aFile];
  return bundle->GetStringFromName(aKey, aResult);
}

/* static */
nsresult nsContentUtils::FormatMaybeLocalizedString(
    PropertiesFile aFile, const char* aKey, Document* aDocument,
    const nsTArray<nsString>& aParams, nsAString& aResult) {
  return FormatLocalizedString(
      GetMaybeSpoofedPropertiesFile(aFile, aKey, aDocument), aKey, aParams,
      aResult);
}

/* static */
nsresult nsContentUtils::FormatLocalizedString(
    PropertiesFile aFile, const char* aKey, const nsTArray<nsString>& aParams,
    nsAString& aResult) {
  nsresult rv = EnsureStringBundle(aFile);
  NS_ENSURE_SUCCESS(rv, rv);
  nsIStringBundle* bundle = sStringBundles[aFile];

  if (aParams.IsEmpty()) {
    return bundle->GetStringFromName(aKey, aResult);
  }

  return bundle->FormatStringFromName(aKey, aParams, aResult);
}

/* static */
void nsContentUtils::LogSimpleConsoleError(const nsAString& aErrorText,
                                           const char* aCategory,
                                           bool aFromPrivateWindow,
                                           bool aFromChromeContext,
                                           uint32_t aErrorFlags) {
  nsCOMPtr<nsIScriptError> scriptError =
      do_CreateInstance(NS_SCRIPTERROR_CONTRACTID);
  if (scriptError) {
    nsCOMPtr<nsIConsoleService> console =
        do_GetService(NS_CONSOLESERVICE_CONTRACTID);
    if (console && NS_SUCCEEDED(scriptError->Init(
                       aErrorText, u""_ns, u""_ns, 0, 0, aErrorFlags, aCategory,
                       aFromPrivateWindow, aFromChromeContext))) {
      console->LogMessage(scriptError);
    }
  }
}

/* static */
nsresult nsContentUtils::ReportToConsole(
    uint32_t aErrorFlags, const nsACString& aCategory,
    const Document* aDocument, PropertiesFile aFile, const char* aMessageName,
    const nsTArray<nsString>& aParams, nsIURI* aURI,
    const nsString& aSourceLine, uint32_t aLineNumber, uint32_t aColumnNumber) {
  nsresult rv;
  nsAutoString errorText;
  if (!aParams.IsEmpty()) {
    rv = FormatLocalizedString(aFile, aMessageName, aParams, errorText);
  } else {
    rv = GetLocalizedString(aFile, aMessageName, errorText);
  }
  NS_ENSURE_SUCCESS(rv, rv);

  return ReportToConsoleNonLocalized(errorText, aErrorFlags, aCategory,
                                     aDocument, aURI, aSourceLine, aLineNumber,
                                     aColumnNumber);
}

/* static */
void nsContentUtils::ReportEmptyGetElementByIdArg(const Document* aDoc) {
  ReportToConsole(nsIScriptError::warningFlag, "DOM"_ns, aDoc,
                  nsContentUtils::eDOM_PROPERTIES, "EmptyGetElementByIdParam");
}

/* static */
nsresult nsContentUtils::ReportToConsoleNonLocalized(
    const nsAString& aErrorText, uint32_t aErrorFlags,
    const nsACString& aCategory, const Document* aDocument, nsIURI* aURI,
    const nsString& aSourceLine, uint32_t aLineNumber, uint32_t aColumnNumber,
    MissingErrorLocationMode aLocationMode) {
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

/* static */
nsresult nsContentUtils::ReportToConsoleByWindowID(
    const nsAString& aErrorText, uint32_t aErrorFlags,
    const nsACString& aCategory, uint64_t aInnerWindowID, nsIURI* aURI,
    const nsString& aSourceLine, uint32_t aLineNumber, uint32_t aColumnNumber,
    MissingErrorLocationMode aLocationMode) {
  nsresult rv;
  if (!sConsoleService) {  // only need to bother null-checking here
    rv = CallGetService(NS_CONSOLESERVICE_CONTRACTID, &sConsoleService);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  nsAutoString spec;
  if (!aLineNumber && aLocationMode == eUSE_CALLING_LOCATION) {
    JSContext* cx = GetCurrentJSContext();
    if (cx) {
      nsJSUtils::GetCallingLocation(cx, spec, &aLineNumber, &aColumnNumber);
    }
  }

  nsCOMPtr<nsIScriptError> errorObject =
      do_CreateInstance(NS_SCRIPTERROR_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  if (!spec.IsEmpty()) {
    rv = errorObject->InitWithWindowID(aErrorText,
                                       spec,  // file name
                                       aSourceLine, aLineNumber, aColumnNumber,
                                       aErrorFlags, aCategory, aInnerWindowID);
  } else {
    rv = errorObject->InitWithSourceURI(aErrorText, aURI, aSourceLine,
                                        aLineNumber, aColumnNumber, aErrorFlags,
                                        aCategory, aInnerWindowID);
  }
  NS_ENSURE_SUCCESS(rv, rv);

  return sConsoleService->LogMessage(errorObject);
}

void nsContentUtils::LogMessageToConsole(const char* aMsg) {
  if (!sConsoleService) {  // only need to bother null-checking here
    CallGetService(NS_CONSOLESERVICE_CONTRACTID, &sConsoleService);
    if (!sConsoleService) {
      return;
    }
  }
  sConsoleService->LogStringMessage(NS_ConvertUTF8toUTF16(aMsg).get());
}

bool nsContentUtils::IsChromeDoc(const Document* aDocument) {
  return aDocument && aDocument->NodePrincipal() == sSystemPrincipal;
}

bool nsContentUtils::IsChildOfSameType(Document* aDoc) {
  if (BrowsingContext* bc = aDoc->GetBrowsingContext()) {
    return bc->GetParent();
  }
  return false;
}

bool nsContentUtils::IsPlainTextType(const nsACString& aContentType) {
  // NOTE: if you add a type here, add it to the CONTENTDLF_CATEGORIES
  // define in nsContentDLF.h as well.
  return aContentType.EqualsLiteral(TEXT_PLAIN) ||
         aContentType.EqualsLiteral(TEXT_CSS) ||
         aContentType.EqualsLiteral(TEXT_CACHE_MANIFEST) ||
         aContentType.EqualsLiteral(TEXT_VTT) ||
         aContentType.EqualsLiteral(APPLICATION_JAVASCRIPT) ||
         aContentType.EqualsLiteral(APPLICATION_XJAVASCRIPT) ||
         aContentType.EqualsLiteral(TEXT_ECMASCRIPT) ||
         aContentType.EqualsLiteral(APPLICATION_ECMASCRIPT) ||
         aContentType.EqualsLiteral(TEXT_JAVASCRIPT) ||
         aContentType.EqualsLiteral(APPLICATION_JSON) ||
         aContentType.EqualsLiteral(TEXT_JSON);
}

bool nsContentUtils::IsUtf8OnlyPlainTextType(const nsACString& aContentType) {
  // NOTE: This must be a subset of the list in IsPlainTextType().
  return aContentType.EqualsLiteral(TEXT_CACHE_MANIFEST) ||
         aContentType.EqualsLiteral(APPLICATION_JSON) ||
         aContentType.EqualsLiteral(TEXT_JSON) ||
         aContentType.EqualsLiteral(TEXT_VTT);
}

bool nsContentUtils::IsInChromeDocshell(const Document* aDocument) {
  return aDocument && aDocument->IsInChromeDocShell();
}

// static
nsIContentPolicy* nsContentUtils::GetContentPolicy() {
  if (!sTriedToGetContentPolicy) {
    CallGetService(NS_CONTENTPOLICY_CONTRACTID, &sContentPolicyService);
    // It's OK to not have a content policy service
    sTriedToGetContentPolicy = true;
  }

  return sContentPolicyService;
}

// static
bool nsContentUtils::IsEventAttributeName(nsAtom* aName, int32_t aType) {
  const char16_t* name = aName->GetUTF16String();
  if (name[0] != 'o' || name[1] != 'n') {
    return false;
  }

  EventNameMapping mapping;
  return (sAtomEventTable->Get(aName, &mapping) && mapping.mType & aType);
}

// static
EventMessage nsContentUtils::GetEventMessage(nsAtom* aName) {
  MOZ_ASSERT(NS_IsMainThread(), "sAtomEventTable is not threadsafe");
  if (aName) {
    EventNameMapping mapping;
    if (sAtomEventTable->Get(aName, &mapping)) {
      return mapping.mMessage;
    }
  }

  return eUnidentifiedEvent;
}

// static
mozilla::EventClassID nsContentUtils::GetEventClassID(const nsAString& aName) {
  EventNameMapping mapping;
  if (sStringEventTable->Get(aName, &mapping)) return mapping.mEventClassID;

  return eBasicEventClass;
}

nsAtom* nsContentUtils::GetEventMessageAndAtom(
    const nsAString& aName, mozilla::EventClassID aEventClassID,
    EventMessage* aEventMessage) {
  MOZ_ASSERT(NS_IsMainThread(), "Our hashtables are not threadsafe");
  EventNameMapping mapping;
  if (sStringEventTable->Get(aName, &mapping)) {
    *aEventMessage = mapping.mEventClassID == aEventClassID
                         ? mapping.mMessage
                         : eUnidentifiedEvent;
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
  RefPtr<nsAtom> atom = NS_AtomizeMainThread(u"on"_ns + aName);
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
  sStringEventTable->InsertOrUpdate(aName, mapping);
  return mapping.mAtom;
}

// static
EventMessage nsContentUtils::GetEventMessageAndAtomForListener(
    const nsAString& aName, nsAtom** aOnName) {
  MOZ_ASSERT(NS_IsMainThread(), "Our hashtables are not threadsafe");

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
      atom = NS_AtomizeMainThread(u"on"_ns + aName);
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

static nsresult GetEventAndTarget(Document* aDoc, nsISupports* aTarget,
                                  const nsAString& aEventName,
                                  CanBubble aCanBubble, Cancelable aCancelable,
                                  Composed aComposed, Trusted aTrusted,
                                  Event** aEvent, EventTarget** aTargetOut) {
  nsCOMPtr<EventTarget> target(do_QueryInterface(aTarget));
  NS_ENSURE_TRUE(aDoc && target, NS_ERROR_INVALID_ARG);

  ErrorResult err;
  RefPtr<Event> event =
      aDoc->CreateEvent(u"Events"_ns, CallerType::System, err);
  if (NS_WARN_IF(err.Failed())) {
    return err.StealNSResult();
  }

  event->InitEvent(aEventName, aCanBubble, aCancelable, aComposed);
  event->SetTrusted(aTrusted == Trusted::eYes);

  event->SetTarget(target);

  event.forget(aEvent);
  target.forget(aTargetOut);
  return NS_OK;
}

// static
nsresult nsContentUtils::DispatchTrustedEvent(
    Document* aDoc, nsISupports* aTarget, const nsAString& aEventName,
    CanBubble aCanBubble, Cancelable aCancelable, Composed aComposed,
    bool* aDefaultAction) {
  MOZ_ASSERT(!aEventName.EqualsLiteral("input") &&
                 !aEventName.EqualsLiteral("beforeinput"),
             "Use DispatchInputEvent() instead");
  return DispatchEvent(aDoc, aTarget, aEventName, aCanBubble, aCancelable,
                       aComposed, Trusted::eYes, aDefaultAction);
}

// static
nsresult nsContentUtils::DispatchUntrustedEvent(
    Document* aDoc, nsISupports* aTarget, const nsAString& aEventName,
    CanBubble aCanBubble, Cancelable aCancelable, bool* aDefaultAction) {
  return DispatchEvent(aDoc, aTarget, aEventName, aCanBubble, aCancelable,
                       Composed::eDefault, Trusted::eNo, aDefaultAction);
}

// static
nsresult nsContentUtils::DispatchEvent(Document* aDoc, nsISupports* aTarget,
                                       const nsAString& aEventName,
                                       CanBubble aCanBubble,
                                       Cancelable aCancelable,
                                       Composed aComposed, Trusted aTrusted,
                                       bool* aDefaultAction,
                                       ChromeOnlyDispatch aOnlyChromeDispatch) {
  RefPtr<Event> event;
  nsCOMPtr<EventTarget> target;
  nsresult rv = GetEventAndTarget(
      aDoc, aTarget, aEventName, aCanBubble, aCancelable, aComposed, aTrusted,
      getter_AddRefs(event), getter_AddRefs(target));
  NS_ENSURE_SUCCESS(rv, rv);
  event->WidgetEventPtr()->mFlags.mOnlyChromeDispatch =
      aOnlyChromeDispatch == ChromeOnlyDispatch::eYes;

  ErrorResult err;
  bool doDefault = target->DispatchEvent(*event, CallerType::System, err);
  if (aDefaultAction) {
    *aDefaultAction = doDefault;
  }
  return err.StealNSResult();
}

// static
nsresult nsContentUtils::DispatchEvent(Document* aDoc, nsISupports* aTarget,
                                       WidgetEvent& aEvent,
                                       EventMessage aEventMessage,
                                       CanBubble aCanBubble,
                                       Cancelable aCancelable, Trusted aTrusted,
                                       bool* aDefaultAction,
                                       ChromeOnlyDispatch aOnlyChromeDispatch) {
  MOZ_ASSERT_IF(aOnlyChromeDispatch == ChromeOnlyDispatch::eYes,
                aTrusted == Trusted::eYes);

  nsCOMPtr<EventTarget> target(do_QueryInterface(aTarget));

  aEvent.mTime = PR_Now();

  aEvent.mSpecifiedEventType = GetEventTypeFromMessage(aEventMessage);
  aEvent.SetDefaultComposed();
  aEvent.SetDefaultComposedInNativeAnonymousContent();

  aEvent.mFlags.mBubbles = aCanBubble == CanBubble::eYes;
  aEvent.mFlags.mCancelable = aCancelable == Cancelable::eYes;
  aEvent.mFlags.mOnlyChromeDispatch =
      aOnlyChromeDispatch == ChromeOnlyDispatch::eYes;

  aEvent.mTarget = target;

  nsEventStatus status = nsEventStatus_eIgnore;
  nsresult rv = EventDispatcher::DispatchDOMEvent(target, &aEvent, nullptr,
                                                  nullptr, &status);
  if (aDefaultAction) {
    *aDefaultAction = (status != nsEventStatus_eConsumeNoDefault);
  }
  return rv;
}

// static
nsresult nsContentUtils::DispatchInputEvent(Element* aEventTarget) {
  return DispatchInputEvent(aEventTarget, mozilla::eEditorInput,
                            mozilla::EditorInputType::eUnknown, nullptr,
                            InputEventOptions());
}

// static
nsresult nsContentUtils::DispatchInputEvent(
    Element* aEventTargetElement, EventMessage aEventMessage,
    EditorInputType aEditorInputType, EditorBase* aEditorBase,
    InputEventOptions&& aOptions, nsEventStatus* aEventStatus /* = nullptr */) {
  MOZ_ASSERT(aEventMessage == eEditorInput ||
             aEventMessage == eEditorBeforeInput);

  if (NS_WARN_IF(!aEventTargetElement)) {
    return NS_ERROR_INVALID_ARG;
  }

  // If this is called from editor, the instance should be set to aEditorBase.
  // Otherwise, we need to look for an editor for aEventTargetElement.
  // However, we don't need to do it for HTMLEditor since nobody shouldn't
  // dispatch "beforeinput" nor "input" event for HTMLEditor except HTMLEditor
  // itself.
  bool useInputEvent = false;
  if (aEditorBase) {
    useInputEvent = true;
  } else if (HTMLTextAreaElement* textAreaElement =
                 HTMLTextAreaElement::FromNode(aEventTargetElement)) {
    aEditorBase = textAreaElement->GetTextEditorWithoutCreation();
    useInputEvent = true;
  } else if (HTMLInputElement* inputElement =
                 HTMLInputElement::FromNode(aEventTargetElement)) {
    if (inputElement->IsInputEventTarget()) {
      aEditorBase = inputElement->GetTextEditorWithoutCreation();
      useInputEvent = true;
    }
  }
#ifdef DEBUG
  else {
    MOZ_ASSERT(!aEventTargetElement->IsTextControlElement(),
               "The event target may have editor, but we've not known it yet.");
  }
#endif  // #ifdef DEBUG

  if (!useInputEvent) {
    MOZ_ASSERT(aEventMessage == eEditorInput);
    MOZ_ASSERT(aEditorInputType == EditorInputType::eUnknown);
    MOZ_ASSERT(!aOptions.mNeverCancelable);
    // Dispatch "input" event with Event instance.
    WidgetEvent widgetEvent(true, eUnidentifiedEvent);
    widgetEvent.mSpecifiedEventType = nsGkAtoms::oninput;
    widgetEvent.mFlags.mCancelable = false;
    widgetEvent.mFlags.mComposed = true;
    // Using same time as nsContentUtils::DispatchEvent() for backward
    // compatibility.
    widgetEvent.mTime = PR_Now();
    (new AsyncEventDispatcher(aEventTargetElement, widgetEvent))
        ->RunDOMEventWhenSafe();
    return NS_OK;
  }

  MOZ_ASSERT_IF(aEventMessage != eEditorBeforeInput,
                !aOptions.mNeverCancelable);
  MOZ_ASSERT_IF(
      aEventMessage == eEditorBeforeInput && aOptions.mNeverCancelable,
      aEditorInputType == EditorInputType::eInsertReplacementText);

  nsCOMPtr<nsIWidget> widget;
  if (aEditorBase) {
    widget = aEditorBase->GetWidget();
    if (NS_WARN_IF(!widget)) {
      return NS_ERROR_FAILURE;
    }
  } else {
    Document* document = aEventTargetElement->OwnerDoc();
    if (NS_WARN_IF(!document)) {
      return NS_ERROR_FAILURE;
    }
    // If we're running xpcshell tests, we fail to get presShell here.
    // Even in such case, we need to dispatch "input" event without widget.
    PresShell* presShell = document->GetPresShell();
    if (presShell) {
      nsPresContext* presContext = presShell->GetPresContext();
      if (NS_WARN_IF(!presContext)) {
        return NS_ERROR_FAILURE;
      }
      widget = presContext->GetRootWidget();
      if (NS_WARN_IF(!widget)) {
        return NS_ERROR_FAILURE;
      }
    }
  }

  // Dispatch "input" event with InputEvent instance.
  InternalEditorInputEvent inputEvent(true, aEventMessage, widget);

  inputEvent.mFlags.mCancelable =
      !aOptions.mNeverCancelable && aEventMessage == eEditorBeforeInput &&
      IsCancelableBeforeInputEvent(aEditorInputType);
  MOZ_ASSERT(!inputEvent.mFlags.mCancelable || aEventStatus);

  // Using same time as old event dispatcher in EditorBase for backward
  // compatibility.
  inputEvent.mTime = static_cast<uint64_t>(PR_Now() / 1000);

  // If there is an editor, set isComposing to true when it has composition.
  // Note that EditorBase::IsIMEComposing() may return false even when we
  // need to set it to true.
  // Otherwise, i.e., editor hasn't been created for the element yet,
  // we should set isComposing to false since the element can never has
  // composition without editor.
  inputEvent.mIsComposing = aEditorBase && aEditorBase->GetComposition();

  if (!aEditorBase || aEditorBase->IsTextEditor()) {
    if (IsDataAvailableOnTextEditor(aEditorInputType)) {
      inputEvent.mData = std::move(aOptions.mData);
      MOZ_ASSERT(!inputEvent.mData.IsVoid(),
                 "inputEvent.mData shouldn't be void");
    }
#ifdef DEBUG
    else {
      MOZ_ASSERT(inputEvent.mData.IsVoid(), "inputEvent.mData should be void");
    }
#endif  // #ifdef DEBUG
    MOZ_ASSERT(
        aOptions.mTargetRanges.IsEmpty(),
        "Target ranges for <input> and <textarea> should always be empty");
  } else {
    MOZ_ASSERT(aEditorBase->IsHTMLEditor());
    if (IsDataAvailableOnHTMLEditor(aEditorInputType)) {
      inputEvent.mData = std::move(aOptions.mData);
      MOZ_ASSERT(!inputEvent.mData.IsVoid(),
                 "inputEvent.mData shouldn't be void");
    } else {
      MOZ_ASSERT(inputEvent.mData.IsVoid(), "inputEvent.mData should be void");
      if (IsDataTransferAvailableOnHTMLEditor(aEditorInputType)) {
        inputEvent.mDataTransfer = std::move(aOptions.mDataTransfer);
        MOZ_ASSERT(inputEvent.mDataTransfer,
                   "inputEvent.mDataTransfer shouldn't be nullptr");
        MOZ_ASSERT(inputEvent.mDataTransfer->IsReadOnly(),
                   "inputEvent.mDataTransfer should be read only");
      }
#ifdef DEBUG
      else {
        MOZ_ASSERT(!inputEvent.mDataTransfer,
                   "inputEvent.mDataTransfer should be nullptr");
      }
#endif  // #ifdef DEBUG
    }
    if (aEventMessage == eEditorBeforeInput &&
        MayHaveTargetRangesOnHTMLEditor(aEditorInputType)) {
      inputEvent.mTargetRanges = std::move(aOptions.mTargetRanges);
    }
#ifdef DEBUG
    else {
      MOZ_ASSERT(aOptions.mTargetRanges.IsEmpty(),
                 "Target ranges shouldn't be set for the dispatching event");
    }
#endif  // #ifdef DEBUG
  }

  inputEvent.mInputType = aEditorInputType;

  if (!IsSafeToRunScript()) {
    // If we cannot dispatch an event right now, we cannot make it cancelable.
    NS_ASSERTION(
        !inputEvent.mFlags.mCancelable,
        "Cancelable beforeinput event dispatcher should run when it's safe");
    inputEvent.mFlags.mCancelable = false;
    (new AsyncEventDispatcher(aEventTargetElement, inputEvent))
        ->RunDOMEventWhenSafe();
    return NS_OK;
  }

  // If we're running xpcshell tests, we fail to get presShell here.
  // Even in such case, we need to dispatch "input" event without widget.
  RefPtr<nsPresContext> presContext =
      aEventTargetElement->OwnerDoc()->GetPresContext();
  nsresult rv = EventDispatcher::Dispatch(aEventTargetElement, presContext,
                                          &inputEvent, nullptr, aEventStatus);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "Dispatching `beforeinput` or `input` event failed");
  return rv;
}

nsresult nsContentUtils::DispatchChromeEvent(
    Document* aDoc, nsISupports* aTarget, const nsAString& aEventName,
    CanBubble aCanBubble, Cancelable aCancelable, bool* aDefaultAction) {
  RefPtr<Event> event;
  nsCOMPtr<EventTarget> target;
  nsresult rv = GetEventAndTarget(
      aDoc, aTarget, aEventName, aCanBubble, aCancelable, Composed::eDefault,
      Trusted::eYes, getter_AddRefs(event), getter_AddRefs(target));
  NS_ENSURE_SUCCESS(rv, rv);

  NS_ASSERTION(aDoc, "GetEventAndTarget lied?");
  if (!aDoc->GetWindow()) return NS_ERROR_INVALID_ARG;

  EventTarget* piTarget = aDoc->GetWindow()->GetParentTarget();
  if (!piTarget) return NS_ERROR_INVALID_ARG;

  ErrorResult err;
  bool defaultActionEnabled =
      piTarget->DispatchEvent(*event, CallerType::System, err);
  if (aDefaultAction) {
    *aDefaultAction = defaultActionEnabled;
  }
  return err.StealNSResult();
}

void nsContentUtils::RequestFrameFocus(Element& aFrameElement, bool aCanRaise,
                                       CallerType aCallerType) {
  RefPtr<Element> target = &aFrameElement;
  bool defaultAction = true;
  if (aCanRaise) {
    DispatchEventOnlyToChrome(target->OwnerDoc(), target,
                              u"framefocusrequested"_ns, CanBubble::eYes,
                              Cancelable::eYes, &defaultAction);
  }
  if (!defaultAction) {
    return;
  }

  RefPtr<nsFocusManager> fm = nsFocusManager::GetFocusManager();
  if (!fm) {
    return;
  }

  uint32_t flags = nsIFocusManager::FLAG_NOSCROLL;
  if (aCanRaise) {
    flags |= nsIFocusManager::FLAG_RAISE;
  }

  if (aCallerType == CallerType::NonSystem) {
    flags |= nsIFocusManager::FLAG_NONSYSTEMCALLER;
  }

  fm->SetFocus(target, flags);
}

nsresult nsContentUtils::DispatchEventOnlyToChrome(
    Document* aDoc, nsISupports* aTarget, const nsAString& aEventName,
    CanBubble aCanBubble, Cancelable aCancelable, Composed aComposed,
    bool* aDefaultAction) {
  return DispatchEvent(aDoc, aTarget, aEventName, aCanBubble, aCancelable,
                       aComposed, Trusted::eYes, aDefaultAction,
                       ChromeOnlyDispatch::eYes);
}

/* static */
Element* nsContentUtils::MatchElementId(nsIContent* aContent,
                                        const nsAtom* aId) {
  for (nsIContent* cur = aContent; cur; cur = cur->GetNextNode(aContent)) {
    if (aId == cur->GetID()) {
      return cur->AsElement();
    }
  }

  return nullptr;
}

/* static */
Element* nsContentUtils::MatchElementId(nsIContent* aContent,
                                        const nsAString& aId) {
  MOZ_ASSERT(!aId.IsEmpty(), "Will match random elements");

  // ID attrs are generally stored as atoms, so just atomize this up front
  RefPtr<nsAtom> id(NS_Atomize(aId));
  if (!id) {
    // OOM, so just bail
    return nullptr;
  }

  return MatchElementId(aContent, id);
}

/* static */
void nsContentUtils::RegisterShutdownObserver(nsIObserver* aObserver) {
  nsCOMPtr<nsIObserverService> observerService =
      mozilla::services::GetObserverService();
  if (observerService) {
    observerService->AddObserver(aObserver, NS_XPCOM_SHUTDOWN_OBSERVER_ID,
                                 false);
  }
}

/* static */
void nsContentUtils::UnregisterShutdownObserver(nsIObserver* aObserver) {
  nsCOMPtr<nsIObserverService> observerService =
      mozilla::services::GetObserverService();
  if (observerService) {
    observerService->RemoveObserver(aObserver, NS_XPCOM_SHUTDOWN_OBSERVER_ID);
  }
}

/* static */
bool nsContentUtils::HasNonEmptyAttr(const nsIContent* aContent,
                                     int32_t aNameSpaceID, nsAtom* aName) {
  static Element::AttrValuesArray strings[] = {nsGkAtoms::_empty, nullptr};
  return aContent->IsElement() &&
         aContent->AsElement()->FindAttrValueIn(aNameSpaceID, aName, strings,
                                                eCaseMatters) ==
             Element::ATTR_VALUE_NO_MATCH;
}

/* static */
bool nsContentUtils::HasMutationListeners(nsINode* aNode, uint32_t aType,
                                          nsINode* aTargetForSubtreeModified) {
  Document* doc = aNode->OwnerDoc();

  // global object will be null for documents that don't have windows.
  nsPIDOMWindowInner* window = doc->GetInnerWindow();
  // This relies on EventListenerManager::AddEventListener, which sets
  // all mutation bits when there is a listener for DOMSubtreeModified event.
  if (window && !window->HasMutationListeners(aType)) {
    return false;
  }

  if (aNode->ChromeOnlyAccess() || aNode->IsInShadowTree()) {
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

    aNode = aNode->GetParentNode();
  }

  return false;
}

/* static */
bool nsContentUtils::HasMutationListeners(Document* aDocument, uint32_t aType) {
  nsPIDOMWindowInner* window =
      aDocument ? aDocument->GetInnerWindow() : nullptr;

  // This relies on EventListenerManager::AddEventListener, which sets
  // all mutation bits when there is a listener for DOMSubtreeModified event.
  return !window || window->HasMutationListeners(aType);
}

void nsContentUtils::MaybeFireNodeRemoved(nsINode* aChild, nsINode* aParent) {
  MOZ_ASSERT(aChild, "Missing child");
  MOZ_ASSERT(aChild->GetParentNode() == aParent, "Wrong parent");
  MOZ_ASSERT(aChild->OwnerDoc() == aParent->OwnerDoc(), "Wrong owner-doc");

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
    if (!aChild->IsInNativeAnonymousSubtree() &&
        !sDOMNodeRemovedSuppressCount) {
      NS_ERROR("Want to fire DOMNodeRemoved event, but it's not safe");
      WarnScriptWasIgnored(aChild->OwnerDoc());
    }
    return;
  }

  {
    Document* doc = aParent->OwnerDoc();
    if (MOZ_UNLIKELY(doc->DevToolsWatchingDOMMutations()) &&
        aChild->IsInComposedDoc() && !aChild->ChromeOnlyAccess()) {
      DispatchChromeEvent(doc, aChild, u"devtoolschildremoved"_ns,
                          CanBubble::eNo, Cancelable::eNo);
    }
  }

  if (HasMutationListeners(aChild, NS_EVENT_BITS_MUTATION_NODEREMOVED,
                           aParent)) {
    InternalMutationEvent mutation(true, eLegacyNodeRemoved);
    mutation.mRelatedNode = aParent;

    mozAutoSubtreeModified subtree(aParent->OwnerDoc(), aParent);
    EventDispatcher::Dispatch(aChild, nullptr, &mutation);
  }
}

void nsContentUtils::UnmarkGrayJSListenersInCCGenerationDocuments() {
  if (!sEventListenerManagersHash) {
    return;
  }

  for (auto i = sEventListenerManagersHash->Iter(); !i.Done(); i.Next()) {
    auto entry = static_cast<EventListenerManagerMapEntry*>(i.Get());
    nsINode* n = static_cast<nsINode*>(entry->mListenerManager->GetTarget());
    if (n && n->IsInComposedDoc() &&
        nsCCUncollectableMarker::InGeneration(
            n->OwnerDoc()->GetMarkedCCGeneration())) {
      entry->mListenerManager->MarkForCC();
    }
  }
}

/* static */
void nsContentUtils::TraverseListenerManager(
    nsINode* aNode, nsCycleCollectionTraversalCallback& cb) {
  if (!sEventListenerManagersHash) {
    // We're already shut down, just return.
    return;
  }

  auto entry = static_cast<EventListenerManagerMapEntry*>(
      sEventListenerManagersHash->Search(aNode));
  if (entry) {
    CycleCollectionNoteChild(cb, entry->mListenerManager.get(),
                             "[via hash] mListenerManager");
  }
}

EventListenerManager* nsContentUtils::GetListenerManagerForNode(
    nsINode* aNode) {
  if (!sEventListenerManagersHash) {
    // We're already shut down, don't bother creating an event listener
    // manager.

    return nullptr;
  }

  auto entry = static_cast<EventListenerManagerMapEntry*>(
      sEventListenerManagersHash->Add(aNode, fallible));

  if (!entry) {
    return nullptr;
  }

  if (!entry->mListenerManager) {
    entry->mListenerManager = new EventListenerManager(aNode);

    aNode->SetFlags(NODE_HAS_LISTENERMANAGER);
  }

  return entry->mListenerManager;
}

EventListenerManager* nsContentUtils::GetExistingListenerManagerForNode(
    const nsINode* aNode) {
  if (!aNode->HasFlag(NODE_HAS_LISTENERMANAGER)) {
    return nullptr;
  }

  if (!sEventListenerManagersHash) {
    // We're already shut down, don't bother creating an event listener
    // manager.

    return nullptr;
  }

  auto entry = static_cast<EventListenerManagerMapEntry*>(
      sEventListenerManagersHash->Search(aNode));
  if (entry) {
    return entry->mListenerManager;
  }

  return nullptr;
}

void nsContentUtils::AddEntryToDOMArenaTable(nsINode* aNode,
                                             DOMArena* aDOMArena) {
  MOZ_ASSERT(StaticPrefs::dom_arena_allocator_enabled_AtStartup());
  MOZ_ASSERT_IF(sDOMArenaHashtable, !sDOMArenaHashtable->Contains(aNode));
  MOZ_ASSERT(!aNode->HasFlag(NODE_KEEPS_DOMARENA));
  if (!sDOMArenaHashtable) {
    sDOMArenaHashtable =
        new nsRefPtrHashtable<nsPtrHashKey<const nsINode>, dom::DOMArena>();
  }
  aNode->SetFlags(NODE_KEEPS_DOMARENA);
  sDOMArenaHashtable->InsertOrUpdate(aNode, RefPtr<DOMArena>(aDOMArena));
}

already_AddRefed<DOMArena> nsContentUtils::TakeEntryFromDOMArenaTable(
    const nsINode* aNode) {
  MOZ_ASSERT(sDOMArenaHashtable->Contains(aNode));
  MOZ_ASSERT(StaticPrefs::dom_arena_allocator_enabled_AtStartup());
  RefPtr<DOMArena> arena;
  sDOMArenaHashtable->Remove(aNode, getter_AddRefs(arena));
  return arena.forget();
}

/* static */
void nsContentUtils::RemoveListenerManager(nsINode* aNode) {
  if (sEventListenerManagersHash) {
    auto entry = static_cast<EventListenerManagerMapEntry*>(
        sEventListenerManagersHash->Search(aNode));
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
bool nsContentUtils::IsValidNodeName(nsAtom* aLocalName, nsAtom* aPrefix,
                                     int32_t aNamespaceID) {
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

already_AddRefed<DocumentFragment> nsContentUtils::CreateContextualFragment(
    nsINode* aContextNode, const nsAString& aFragment,
    bool aPreventScriptExecution, ErrorResult& aRv) {
  if (!aContextNode) {
    aRv.Throw(NS_ERROR_INVALID_ARG);
    return nullptr;
  }

  // If we don't have a document here, we can't get the right security context
  // for compiling event handlers... so just bail out.
  RefPtr<Document> document = aContextNode->OwnerDoc();
  bool isHTML = document->IsHTMLDocument();

  if (isHTML) {
    RefPtr<DocumentFragment> frag = new (document->NodeInfoManager())
        DocumentFragment(document->NodeInfoManager());

    Element* element = aContextNode->GetAsElementOrParentElement();
    if (element && !element->IsHTMLElement(nsGkAtoms::html)) {
      aRv = ParseFragmentHTML(
          aFragment, frag, element->NodeInfo()->NameAtom(),
          element->GetNameSpaceID(),
          (document->GetCompatibilityMode() == eCompatibility_NavQuirks),
          aPreventScriptExecution);
    } else {
      aRv = ParseFragmentHTML(
          aFragment, frag, nsGkAtoms::body, kNameSpaceID_XHTML,
          (document->GetCompatibilityMode() == eCompatibility_NavQuirks),
          aPreventScriptExecution);
    }

    return frag.forget();
  }

  AutoTArray<nsString, 32> tagStack;
  nsAutoString uriStr, nameStr;
  for (Element* element : aContextNode->InclusiveAncestorsOfType<Element>()) {
    nsString& tagName = *tagStack.AppendElement();
    // It mostly doesn't actually matter what tag name we use here: XML doesn't
    // have parsing that depends on the open tag stack, apart from namespace
    // declarations.  So this whole tagStack bit is just there to get the right
    // namespace declarations to the XML parser.  That said, the parser _is_
    // going to create elements with the tag names we provide here, so we need
    // to make sure they are not names that can trigger custom element
    // constructors.  Just make up a name that is never going to be a valid
    // custom element name.
    //
    // The principled way to do this would probably be to add a new FromParser
    // value and make sure we use it when creating the context elements, then
    // make sure we teach all FromParser consumers (and in particular the custom
    // element code) about it as needed.  But right now the XML parser never
    // actually uses FromParser values other than NOT_FROM_PARSER, and changing
    // that is pretty complicated.
    tagName.AssignLiteral("notacustomelement");

    // see if we need to add xmlns declarations
    uint32_t count = element->GetAttrCount();
    bool setDefaultNamespace = false;
    if (count > 0) {
      uint32_t index;

      for (index = 0; index < count; index++) {
        const BorrowedAttrInfo info = element->GetAttrInfoAt(index);
        const nsAttrName* name = info.mName;
        if (name->NamespaceEquals(kNameSpaceID_XMLNS)) {
          info.mValue->ToString(uriStr);

          // really want something like nsXMLContentSerializer::SerializeAttr
          tagName.AppendLiteral(" xmlns");  // space important
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
      mozilla::dom::NodeInfo* info = element->NodeInfo();
      if (!info->GetPrefixAtom() && info->NamespaceID() != kNameSpaceID_None) {
        // We have no namespace prefix, but have a namespace ID.  Push
        // default namespace attr in, so that our kids will be in our
        // namespace.
        info->GetNamespaceURI(uriStr);
        tagName.AppendLiteral(R"( xmlns=")");
        tagName.Append(uriStr);
        tagName.Append('"');
      }
    }
  }

  RefPtr<DocumentFragment> frag;
  aRv = ParseFragmentXML(aFragment, document, tagStack, aPreventScriptExecution,
                         -1, getter_AddRefs(frag));
  return frag.forget();
}

/* static */
void nsContentUtils::DropFragmentParsers() {
  NS_IF_RELEASE(sHTMLFragmentParser);
  NS_IF_RELEASE(sXMLFragmentParser);
  NS_IF_RELEASE(sXMLFragmentSink);
}

/* static */
void nsContentUtils::XPCOMShutdown() { nsContentUtils::DropFragmentParsers(); }

/* Helper function to compuate Sanitization Flags for ParseFramentHTML/XML */
uint32_t computeSanitizationFlags(nsIPrincipal* aPrincipal, int32_t aFlags) {
  uint32_t sanitizationFlags = 0;
  if (aPrincipal->IsSystemPrincipal()) {
    if (aFlags < 0) {
      // if this is a chrome-privileged document and no explicit flags
      // were passed, then use this sanitization flags.
      sanitizationFlags = nsIParserUtils::SanitizerAllowStyle |
                          nsIParserUtils::SanitizerAllowComments |
                          nsIParserUtils::SanitizerDropForms |
                          nsIParserUtils::SanitizerLogRemovals;
    } else {
      // if the caller explicitly passes flags, then we use those
      // flags but additionally drop forms.
      sanitizationFlags = aFlags | nsIParserUtils::SanitizerDropForms;
    }
  } else if (aFlags >= 0) {
    // aFlags by default is -1 and is only ever non equal to -1 if the
    // caller of ParseFragmentHTML/ParseFragmentXML is
    // ParserUtils::ParseFragment(). Only in that case we should use
    // the sanitization flags passed within aFlags.
    sanitizationFlags = aFlags;
  }
  return sanitizationFlags;
}

/* static */
bool AllowsUnsanitizedContentForAboutNewTab(nsIPrincipal* aPrincipal) {
  if (StaticPrefs::dom_about_newtab_sanitization_enabled() ||
      !aPrincipal->SchemeIs("about")) {
    return false;
  }
  uint32_t aboutModuleFlags = 0;
  aPrincipal->GetAboutModuleFlags(&aboutModuleFlags);
  return aboutModuleFlags & nsIAboutModule::ALLOW_UNSANITIZED_CONTENT;
}

/* static */
nsresult nsContentUtils::ParseFragmentHTML(
    const nsAString& aSourceBuffer, nsIContent* aTargetNode,
    nsAtom* aContextLocalName, int32_t aContextNamespace, bool aQuirks,
    bool aPreventScriptExecution, int32_t aFlags) {
  AutoTimelineMarker m(aTargetNode->OwnerDoc()->GetDocShell(), "Parse HTML");

  if (nsContentUtils::sFragmentParsingActive) {
    MOZ_ASSERT_UNREACHABLE("Re-entrant fragment parsing attempted.");
    return NS_ERROR_DOM_INVALID_STATE_ERR;
  }
  mozilla::AutoRestore<bool> guard(nsContentUtils::sFragmentParsingActive);
  nsContentUtils::sFragmentParsingActive = true;
  if (!sHTMLFragmentParser) {
    NS_ADDREF(sHTMLFragmentParser = new nsHtml5StringParser());
    // Now sHTMLFragmentParser owns the object
  }

  nsCOMPtr<nsIPrincipal> nodePrincipal = aTargetNode->NodePrincipal();

#ifdef DEBUG
  // aFlags should always be -1 unless the caller of ParseFragmentHTML
  // is ParserUtils::ParseFragment() which is the only caller that intends
  // sanitization. For all other callers we need to ensure to call
  // AuditParsingOfHTMLXMLFragments.
  if (aFlags < 0) {
    DOMSecurityMonitor::AuditParsingOfHTMLXMLFragments(nodePrincipal,
                                                       aSourceBuffer);
  }
#endif

  nsIContent* target = aTargetNode;

  RefPtr<Document> doc = aTargetNode->OwnerDoc();
  RefPtr<DocumentFragment> fragment;
  // We sanitize if the fragment occurs in a system privileged
  // context, an about: page, or if there are explicit sanitization flags.
  // Please note that about:blank and about:srcdoc inherit the security
  // context from the embedding context and hence are not loaded using
  // an about: scheme principal.
  bool shouldSanitize = nodePrincipal->IsSystemPrincipal() ||
                        nodePrincipal->SchemeIs("about") || aFlags >= 0;
  if (shouldSanitize &&
      !AllowsUnsanitizedContentForAboutNewTab(nodePrincipal)) {
    if (!doc->IsLoadedAsData()) {
      doc = nsContentUtils::CreateInertHTMLDocument(doc);
      if (!doc) {
        return NS_ERROR_FAILURE;
      }
    }
    fragment =
        new (doc->NodeInfoManager()) DocumentFragment(doc->NodeInfoManager());
    target = fragment;
  }

  nsresult rv = sHTMLFragmentParser->ParseFragment(
      aSourceBuffer, target, aContextLocalName, aContextNamespace, aQuirks,
      aPreventScriptExecution);
  NS_ENSURE_SUCCESS(rv, rv);

  if (fragment) {
    uint32_t sanitizationFlags =
        computeSanitizationFlags(nodePrincipal, aFlags);
    // Don't fire mutation events for nodes removed by the sanitizer.
    nsAutoScriptBlockerSuppressNodeRemoved scriptBlocker;
    nsTreeSanitizer sanitizer(sanitizationFlags);
    sanitizer.Sanitize(fragment);

    ErrorResult error;
    aTargetNode->AppendChild(*fragment, error);
    rv = error.StealNSResult();
  }

  return rv;
}

/* static */
nsresult nsContentUtils::ParseDocumentHTML(
    const nsAString& aSourceBuffer, Document* aTargetDocument,
    bool aScriptingEnabledForNoscriptParsing) {
  AutoTimelineMarker m(aTargetDocument->GetDocShell(), "Parse HTML");

  if (nsContentUtils::sFragmentParsingActive) {
    MOZ_ASSERT_UNREACHABLE("Re-entrant fragment parsing attempted.");
    return NS_ERROR_DOM_INVALID_STATE_ERR;
  }
  mozilla::AutoRestore<bool> guard(nsContentUtils::sFragmentParsingActive);
  nsContentUtils::sFragmentParsingActive = true;
  if (!sHTMLFragmentParser) {
    NS_ADDREF(sHTMLFragmentParser = new nsHtml5StringParser());
    // Now sHTMLFragmentParser owns the object
  }
  nsresult rv = sHTMLFragmentParser->ParseDocument(
      aSourceBuffer, aTargetDocument, aScriptingEnabledForNoscriptParsing);
  return rv;
}

/* static */
nsresult nsContentUtils::ParseFragmentXML(const nsAString& aSourceBuffer,
                                          Document* aDocument,
                                          nsTArray<nsString>& aTagStack,
                                          bool aPreventScriptExecution,
                                          int32_t aFlags,
                                          DocumentFragment** aReturn) {
  AutoTimelineMarker m(aDocument->GetDocShell(), "Parse XML");

  if (nsContentUtils::sFragmentParsingActive) {
    MOZ_ASSERT_UNREACHABLE("Re-entrant fragment parsing attempted.");
    return NS_ERROR_DOM_INVALID_STATE_ERR;
  }
  mozilla::AutoRestore<bool> guard(nsContentUtils::sFragmentParsingActive);
  nsContentUtils::sFragmentParsingActive = true;
  if (!sXMLFragmentParser) {
    RefPtr<nsParser> parser = new nsParser();
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

  RefPtr<Document> doc;
  nsCOMPtr<nsIPrincipal> nodePrincipal = aDocument->NodePrincipal();

#ifdef DEBUG
  // aFlags should always be -1 unless the caller of ParseFragmentXML
  // is ParserUtils::ParseFragment() which is the only caller that intends
  // sanitization. For all other callers we need to ensure to call
  // AuditParsingOfHTMLXMLFragments.
  if (aFlags < 0) {
    DOMSecurityMonitor::AuditParsingOfHTMLXMLFragments(nodePrincipal,
                                                       aSourceBuffer);
  }
#endif

  // We sanitize if the fragment occurs in a system privileged
  // context, an about: page, or if there are explicit sanitization flags.
  // Please note that about:blank and about:srcdoc inherit the security
  // context from the embedding context and hence are not loaded using
  // an about: scheme principal.
  bool shouldSanitize = nodePrincipal->IsSystemPrincipal() ||
                        nodePrincipal->SchemeIs("about") || aFlags >= 0;
  if (shouldSanitize && !aDocument->IsLoadedAsData()) {
    doc = nsContentUtils::CreateInertXMLDocument(aDocument);
  } else {
    doc = aDocument;
  }

  sXMLFragmentSink->SetTargetDocument(doc);
  sXMLFragmentSink->SetPreventScriptExecution(aPreventScriptExecution);

  nsresult rv = sXMLFragmentParser->ParseFragment(aSourceBuffer, aTagStack);
  if (NS_FAILED(rv)) {
    // Drop the fragment parser and sink that might be in an inconsistent state
    NS_IF_RELEASE(sXMLFragmentParser);
    NS_IF_RELEASE(sXMLFragmentSink);
    return rv;
  }

  rv = sXMLFragmentSink->FinishFragmentParsing(aReturn);

  sXMLFragmentParser->Reset();
  NS_ENSURE_SUCCESS(rv, rv);

  if (shouldSanitize) {
    uint32_t sanitizationFlags =
        computeSanitizationFlags(nodePrincipal, aFlags);
    // Don't fire mutation events for nodes removed by the sanitizer.
    nsAutoScriptBlockerSuppressNodeRemoved scriptBlocker;
    nsTreeSanitizer sanitizer(sanitizationFlags);
    sanitizer.Sanitize(*aReturn);
  }

  return rv;
}

/* static */
nsresult nsContentUtils::ConvertToPlainText(const nsAString& aSourceBuffer,
                                            nsAString& aResultBuffer,
                                            uint32_t aFlags,
                                            uint32_t aWrapCol) {
  RefPtr<Document> document = nsContentUtils::CreateInertHTMLDocument(nullptr);
  if (!document) {
    return NS_ERROR_FAILURE;
  }

  nsresult rv = nsContentUtils::ParseDocumentHTML(
      aSourceBuffer, document,
      !(aFlags & nsIDocumentEncoder::OutputNoScriptContent));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIDocumentEncoder> encoder = do_createDocumentEncoder("text/plain");

  rv = encoder->Init(document, u"text/plain"_ns, aFlags);
  NS_ENSURE_SUCCESS(rv, rv);

  encoder->SetWrapColumn(aWrapCol);

  return encoder->EncodeToString(aResultBuffer);
}

static already_AddRefed<Document> CreateInertDocument(const Document* aTemplate,
                                                      DocumentFlavor aFlavor) {
  if (aTemplate) {
    bool hasHad = true;
    nsIScriptGlobalObject* sgo = aTemplate->GetScriptHandlingObject(hasHad);
    NS_ENSURE_TRUE(sgo || !hasHad, nullptr);

    nsCOMPtr<Document> doc;
    nsresult rv = NS_NewDOMDocument(
        getter_AddRefs(doc), u""_ns, u""_ns, nullptr,
        aTemplate->GetDocumentURI(), aTemplate->GetDocBaseURI(),
        aTemplate->NodePrincipal(), true, sgo, aFlavor);
    if (NS_FAILED(rv)) {
      return nullptr;
    }
    return doc.forget();
  }
  nsCOMPtr<nsIURI> uri;
  NS_NewURI(getter_AddRefs(uri), "about:blank"_ns);
  if (!uri) {
    return nullptr;
  }

  RefPtr<NullPrincipal> nullPrincipal =
      NullPrincipal::CreateWithoutOriginAttributes();
  if (!nullPrincipal) {
    return nullptr;
  }

  nsCOMPtr<Document> doc;
  nsresult rv =
      NS_NewDOMDocument(getter_AddRefs(doc), u""_ns, u""_ns, nullptr, uri, uri,
                        nullPrincipal, true, nullptr, aFlavor);
  if (NS_FAILED(rv)) {
    return nullptr;
  }
  return doc.forget();
}

/* static */
already_AddRefed<Document> nsContentUtils::CreateInertXMLDocument(
    const Document* aTemplate) {
  return CreateInertDocument(aTemplate, DocumentFlavorXML);
}

/* static */
already_AddRefed<Document> nsContentUtils::CreateInertHTMLDocument(
    const Document* aTemplate) {
  return CreateInertDocument(aTemplate, DocumentFlavorHTML);
}

/* static */
nsresult nsContentUtils::SetNodeTextContent(nsIContent* aContent,
                                            const nsAString& aValue,
                                            bool aTryReuse) {
  // Fire DOMNodeRemoved mutation events before we do anything else.
  nsCOMPtr<nsIContent> owningContent;

  // Batch possible DOMSubtreeModified events.
  mozAutoSubtreeModified subtree(nullptr, nullptr);

  // Scope firing mutation events so that we don't carry any state that
  // might be stale
  {
    // We're relying on mozAutoSubtreeModified to keep a strong reference if
    // needed.
    Document* doc = aContent->OwnerDoc();

    // Optimize the common case of there being no observers
    if (HasMutationListeners(doc, NS_EVENT_BITS_MUTATION_NODEREMOVED)) {
      subtree.UpdateTarget(doc, nullptr);
      owningContent = aContent;
      nsCOMPtr<nsINode> child;
      bool skipFirst = aTryReuse;
      for (child = aContent->GetFirstChild();
           child && child->GetParentNode() == aContent;
           child = child->GetNextSibling()) {
        if (skipFirst && child->IsText()) {
          skipFirst = false;
          continue;
        }
        nsContentUtils::MaybeFireNodeRemoved(child, aContent);
      }
    }
  }

  // Might as well stick a batch around this since we're performing several
  // mutations.
  mozAutoDocUpdate updateBatch(aContent->GetComposedDoc(), true);
  nsAutoMutationBatch mb;

  if (aTryReuse && !aValue.IsEmpty()) {
    // Let's remove nodes until we find a eTEXT.
    while (aContent->HasChildren()) {
      nsIContent* child = aContent->GetFirstChild();
      if (child->IsText()) {
        break;
      }
      aContent->RemoveChildNode(child, true);
    }

    // If we have a node, it must be a eTEXT and we reuse it.
    if (aContent->HasChildren()) {
      nsIContent* child = aContent->GetFirstChild();
      nsresult rv = child->AsText()->SetText(aValue, true);
      NS_ENSURE_SUCCESS(rv, rv);

      // All the following nodes, if they exist, must be deleted.
      while (nsIContent* nextChild = child->GetNextSibling()) {
        aContent->RemoveChildNode(nextChild, true);
      }
    }

    if (aContent->HasChildren()) {
      return NS_OK;
    }
  } else {
    mb.Init(aContent, true, false);
    while (aContent->HasChildren()) {
      aContent->RemoveChildNode(aContent->GetFirstChild(), true);
    }
  }
  mb.RemovalDone();

  if (aValue.IsEmpty()) {
    return NS_OK;
  }

  RefPtr<nsTextNode> textContent = new (aContent->NodeInfo()->NodeInfoManager())
      nsTextNode(aContent->NodeInfo()->NodeInfoManager());

  textContent->SetText(aValue, true);

  ErrorResult rv;
  aContent->AppendChildTo(textContent, true, rv);
  mb.NodesAdded();
  return rv.StealNSResult();
}

static bool AppendNodeTextContentsRecurse(nsINode* aNode, nsAString& aResult,
                                          const fallible_t& aFallible) {
  for (nsIContent* child = aNode->GetFirstChild(); child;
       child = child->GetNextSibling()) {
    if (child->IsElement()) {
      bool ok = AppendNodeTextContentsRecurse(child, aResult, aFallible);
      if (!ok) {
        return false;
      }
    } else if (Text* text = child->GetAsText()) {
      bool ok = text->AppendTextTo(aResult, aFallible);
      if (!ok) {
        return false;
      }
    }
  }

  return true;
}

/* static */
bool nsContentUtils::AppendNodeTextContent(nsINode* aNode, bool aDeep,
                                           nsAString& aResult,
                                           const fallible_t& aFallible) {
  if (Text* text = aNode->GetAsText()) {
    return text->AppendTextTo(aResult, aFallible);
  }
  if (aDeep) {
    return AppendNodeTextContentsRecurse(aNode, aResult, aFallible);
  }

  for (nsIContent* child = aNode->GetFirstChild(); child;
       child = child->GetNextSibling()) {
    if (Text* text = child->GetAsText()) {
      bool ok = text->AppendTextTo(aResult, fallible);
      if (!ok) {
        return false;
      }
    }
  }
  return true;
}

bool nsContentUtils::HasNonEmptyTextContent(
    nsINode* aNode, TextContentDiscoverMode aDiscoverMode) {
  for (nsIContent* child = aNode->GetFirstChild(); child;
       child = child->GetNextSibling()) {
    if (child->IsText() && child->TextLength() > 0) {
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
bool nsContentUtils::IsInSameAnonymousTree(const nsINode* aNode,
                                           const nsIContent* aContent) {
  MOZ_ASSERT(aNode, "Must have a node to work with");
  MOZ_ASSERT(aContent, "Must have a content to work with");

  if (aNode->IsInNativeAnonymousSubtree() !=
      aContent->IsInNativeAnonymousSubtree()) {
    return false;
  }

  if (aNode->IsInNativeAnonymousSubtree()) {
    return aContent->GetClosestNativeAnonymousSubtreeRoot() ==
           aNode->GetClosestNativeAnonymousSubtreeRoot();
  }

  // FIXME: This doesn't deal with disconnected nodes whatsoever, but it didn't
  // use to either. Maybe that's fine.
  return aNode->GetContainingShadow() == aContent->GetContainingShadow();
}

/* static */
bool nsContentUtils::IsInInteractiveHTMLContent(const Element* aElement,
                                                const Element* aStop) {
  const Element* element = aElement;
  while (element && element != aStop) {
    if (element->IsInteractiveHTMLContent()) {
      return true;
    }
    element = element->GetFlattenedTreeParentElement();
  }
  return false;
}

/* static */
void nsContentUtils::NotifyInstalledMenuKeyboardListener(bool aInstalling) {
  IMEStateManager::OnInstalledMenuKeyboardListener(aInstalling);
}

/* static */
bool nsContentUtils::SchemeIs(nsIURI* aURI, const char* aScheme) {
  nsCOMPtr<nsIURI> baseURI = NS_GetInnermostURI(aURI);
  NS_ENSURE_TRUE(baseURI, false);
  return baseURI->SchemeIs(aScheme);
}

bool nsContentUtils::IsExpandedPrincipal(nsIPrincipal* aPrincipal) {
  nsCOMPtr<nsIExpandedPrincipal> ep = do_QueryInterface(aPrincipal);
  return !!ep;
}

bool nsContentUtils::IsSystemOrExpandedPrincipal(nsIPrincipal* aPrincipal) {
  return (aPrincipal && aPrincipal->IsSystemPrincipal()) ||
         IsExpandedPrincipal(aPrincipal);
}

nsIPrincipal* nsContentUtils::GetSystemPrincipal() {
  MOZ_ASSERT(IsInitialized());
  return sSystemPrincipal;
}

bool nsContentUtils::CombineResourcePrincipals(
    nsCOMPtr<nsIPrincipal>* aResourcePrincipal, nsIPrincipal* aExtraPrincipal) {
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
  if (NS_SUCCEEDED(
          (*aResourcePrincipal)->Subsumes(aExtraPrincipal, &subsumes)) &&
      subsumes) {
    return false;
  }
  *aResourcePrincipal = sSystemPrincipal;
  return true;
}

/* static */
void nsContentUtils::TriggerLink(nsIContent* aContent, nsIURI* aLinkURI,
                                 const nsString& aTargetSpec, bool aClick,
                                 bool aIsTrusted) {
  MOZ_ASSERT(aLinkURI, "No link URI");

  if (aContent->IsEditable() || !aContent->OwnerDoc()->LinkHandlingEnabled()) {
    return;
  }

  nsCOMPtr<nsIDocShell> docShell = aContent->OwnerDoc()->GetDocShell();
  if (!docShell) {
    return;
  }

  if (!aClick) {
    nsDocShell::Cast(docShell)->OnOverLink(aContent, aLinkURI, aTargetSpec);
    return;
  }

  // Check that this page is allowed to load this URI.
  nsresult proceed = NS_OK;

  if (sSecurityManager) {
    uint32_t flag = static_cast<uint32_t>(nsIScriptSecurityManager::STANDARD);
    proceed = sSecurityManager->CheckLoadURIWithPrincipal(
        aContent->NodePrincipal(), aLinkURI, flag,
        aContent->OwnerDoc()->InnerWindowID());
  }

  // Only pass off the click event if the script security manager says it's ok.
  // We need to rest aTargetSpec for forced downloads.
  if (NS_SUCCEEDED(proceed)) {
    // A link/area element with a download attribute is allowed to set
    // a pseudo Content-Disposition header.
    // For security reasons we only allow websites to declare same-origin
    // resources as downloadable. If this check fails we will just do the normal
    // thing (i.e. navigate to the resource).
    nsAutoString fileName;
    if ((!aContent->IsHTMLElement(nsGkAtoms::a) &&
         !aContent->IsHTMLElement(nsGkAtoms::area) &&
         !aContent->IsSVGElement(nsGkAtoms::a)) ||
        !aContent->AsElement()->GetAttr(kNameSpaceID_None, nsGkAtoms::download,
                                        fileName) ||
        NS_FAILED(aContent->NodePrincipal()->CheckMayLoad(aLinkURI, true))) {
      fileName.SetIsVoid(true);  // No actionable download attribute was found.
    }

    nsCOMPtr<nsIPrincipal> triggeringPrincipal = aContent->NodePrincipal();
    nsCOMPtr<nsIContentSecurityPolicy> csp = aContent->GetCsp();

    // Sanitize fileNames containing null characters by replacing them with
    // underscores.
    if (!fileName.IsVoid()) {
      fileName.ReplaceChar(char16_t(0), '_');
    }
    nsDocShell::Cast(docShell)->OnLinkClick(
        aContent, aLinkURI, fileName.IsVoid() ? aTargetSpec : u""_ns, fileName,
        nullptr, nullptr, UserActivation::IsHandlingUserInput(), aIsTrusted,
        triggeringPrincipal, csp);
  }
}

/* static */
void nsContentUtils::GetLinkLocation(Element* aElement,
                                     nsString& aLocationString) {
  nsCOMPtr<nsIURI> hrefURI = aElement->GetHrefURI();
  if (hrefURI) {
    nsAutoCString specUTF8;
    nsresult rv = hrefURI->GetSpec(specUTF8);
    if (NS_SUCCEEDED(rv)) CopyUTF8toUTF16(specUTF8, aLocationString);
  }
}

/* static */
nsIWidget* nsContentUtils::GetTopLevelWidget(nsIWidget* aWidget) {
  if (!aWidget) return nullptr;

  return aWidget->GetTopLevelWidget();
}

/* static */
const nsDependentString nsContentUtils::GetLocalizedEllipsis() {
  static char16_t sBuf[4] = {0, 0, 0, 0};
  if (!sBuf[0]) {
    if (!SpoofLocaleEnglish()) {
      nsAutoString tmp;
      Preferences::GetLocalizedString("intl.ellipsis", tmp);
      uint32_t len =
          std::min(uint32_t(tmp.Length()), uint32_t(ArrayLength(sBuf) - 1));
      CopyUnicodeTo(tmp, 0, sBuf, len);
    }
    if (!sBuf[0]) sBuf[0] = char16_t(0x2026);
  }
  return nsDependentString(sBuf);
}

/* static */
void nsContentUtils::AddScriptBlocker() {
  MOZ_ASSERT(NS_IsMainThread());
  if (!sScriptBlockerCount) {
    MOZ_ASSERT(sRunnersCountAtFirstBlocker == 0,
               "Should not already have a count");
    sRunnersCountAtFirstBlocker =
        sBlockedScriptRunners ? sBlockedScriptRunners->Length() : 0;
  }
  ++sScriptBlockerCount;
}

#ifdef DEBUG
static bool sRemovingScriptBlockers = false;
#endif

/* static */
void nsContentUtils::RemoveScriptBlocker() {
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
  NS_ASSERTION(firstBlocker <= lastBlocker, "bad sRunnersCountAtFirstBlocker");

  while (firstBlocker < lastBlocker) {
    nsCOMPtr<nsIRunnable> runnable;
    runnable.swap((*sBlockedScriptRunners)[firstBlocker]);
    ++firstBlocker;

    // Calling the runnable can reenter us
    {
      AUTO_PROFILE_FOLLOWING_RUNNABLE(runnable);
      runnable->Run();
    }
    // So can dropping the reference to the runnable
    runnable = nullptr;

    NS_ASSERTION(sRunnersCountAtFirstBlocker == 0, "Bad count");
    NS_ASSERTION(!sScriptBlockerCount, "This is really bad");
  }
#ifdef DEBUG
  AutoRestore<bool> removingScriptBlockers(sRemovingScriptBlockers);
  sRemovingScriptBlockers = true;
#endif
  sBlockedScriptRunners->RemoveElementsAt(originalFirstBlocker, blockersCount);
}

/* static */
already_AddRefed<nsPIDOMWindowOuter>
nsContentUtils::GetMostRecentNonPBWindow() {
  nsCOMPtr<nsIWindowMediator> wm = do_GetService(NS_WINDOWMEDIATOR_CONTRACTID);

  nsCOMPtr<mozIDOMWindowProxy> window;
  wm->GetMostRecentNonPBWindow(u"navigator:browser", getter_AddRefs(window));
  nsCOMPtr<nsPIDOMWindowOuter> pwindow;
  pwindow = do_QueryInterface(window);

  return pwindow.forget();
}

/* static */
void nsContentUtils::WarnScriptWasIgnored(Document* aDocument) {
  nsAutoString msg;
  bool privateBrowsing = false;
  bool chromeContext = false;

  if (aDocument) {
    nsCOMPtr<nsIURI> uri = aDocument->GetDocumentURI();
    if (uri) {
      msg.Append(NS_ConvertUTF8toUTF16(uri->GetSpecOrDefault()));
      msg.AppendLiteral(" : ");
    }
    privateBrowsing =
        !!aDocument->NodePrincipal()->OriginAttributesRef().mPrivateBrowsingId;
    chromeContext = aDocument->NodePrincipal()->IsSystemPrincipal();
  }

  msg.AppendLiteral(
      "Unable to run script because scripts are blocked internally.");
  LogSimpleConsoleError(msg, "DOM", privateBrowsing, chromeContext);
}

/* static */
void nsContentUtils::AddScriptRunner(already_AddRefed<nsIRunnable> aRunnable) {
  nsCOMPtr<nsIRunnable> runnable = aRunnable;
  if (!runnable) {
    return;
  }

  if (sScriptBlockerCount) {
    sBlockedScriptRunners->AppendElement(runnable.forget());
    return;
  }

  AUTO_PROFILE_FOLLOWING_RUNNABLE(runnable);
  runnable->Run();
}

/* static */
void nsContentUtils::AddScriptRunner(nsIRunnable* aRunnable) {
  nsCOMPtr<nsIRunnable> runnable = aRunnable;
  AddScriptRunner(runnable.forget());
}

/* static */ bool nsContentUtils::IsSafeToRunScript() {
  MOZ_ASSERT(NS_IsMainThread(),
             "This static variable only makes sense on the main thread!");
  return sScriptBlockerCount == 0;
}

/* static */
void nsContentUtils::RunInStableState(already_AddRefed<nsIRunnable> aRunnable) {
  MOZ_ASSERT(CycleCollectedJSContext::Get(), "Must be on a script thread!");
  CycleCollectedJSContext::Get()->RunInStableState(std::move(aRunnable));
}

/* static */
void nsContentUtils::AddPendingIDBTransaction(
    already_AddRefed<nsIRunnable> aTransaction) {
  MOZ_ASSERT(CycleCollectedJSContext::Get(), "Must be on a script thread!");
  CycleCollectedJSContext::Get()->AddPendingIDBTransaction(
      std::move(aTransaction));
}

/* static */
bool nsContentUtils::IsInStableOrMetaStableState() {
  MOZ_ASSERT(CycleCollectedJSContext::Get(), "Must be on a script thread!");
  return CycleCollectedJSContext::Get()->IsInStableOrMetaStableState();
}

/* static */
void nsContentUtils::HidePopupsInDocument(Document* aDocument) {
  nsXULPopupManager* pm = nsXULPopupManager::GetInstance();
  if (pm && aDocument) {
    nsCOMPtr<nsIDocShellTreeItem> docShellToHide = aDocument->GetDocShell();
    if (docShellToHide) pm->HidePopupsInDocShell(docShellToHide);
  }
}

/* static */
already_AddRefed<nsIDragSession> nsContentUtils::GetDragSession() {
  nsCOMPtr<nsIDragSession> dragSession;
  nsCOMPtr<nsIDragService> dragService =
      do_GetService("@mozilla.org/widget/dragservice;1");
  if (dragService) dragService->GetCurrentSession(getter_AddRefs(dragSession));
  return dragSession.forget();
}

/* static */
nsresult nsContentUtils::SetDataTransferInEvent(WidgetDragEvent* aDragEvent) {
  if (aDragEvent->mDataTransfer || !aDragEvent->IsTrusted()) {
    return NS_OK;
  }

  // For dragstart events, the data transfer object is
  // created before the event fires, so it should already be set. For other
  // drag events, get the object from the drag session.
  NS_ASSERTION(aDragEvent->mMessage != eDragStart,
               "draggesture event created without a dataTransfer");

  nsCOMPtr<nsIDragSession> dragSession = GetDragSession();
  NS_ENSURE_TRUE(dragSession, NS_OK);  // no drag in progress

  RefPtr<DataTransfer> initialDataTransfer = dragSession->GetDataTransfer();
  if (!initialDataTransfer) {
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
  initialDataTransfer->Clone(
      aDragEvent->mTarget, aDragEvent->mMessage, aDragEvent->mUserCancelled,
      isCrossDomainSubFrameDrop, getter_AddRefs(aDragEvent->mDataTransfer));
  if (NS_WARN_IF(!aDragEvent->mDataTransfer)) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  // for the dragenter and dragover events, initialize the drop effect
  // from the drop action, which platform specific widget code sets before
  // the event is fired based on the keyboard state.
  if (aDragEvent->mMessage == eDragEnter || aDragEvent->mMessage == eDragOver) {
    uint32_t action;
    dragSession->GetDragAction(&action);
    uint32_t effectAllowed = aDragEvent->mDataTransfer->EffectAllowedInt();
    aDragEvent->mDataTransfer->SetDropEffectInt(
        FilterDropEffect(action, effectAllowed));
  } else if (aDragEvent->mMessage == eDrop ||
             aDragEvent->mMessage == eDragEnd) {
    // For the drop and dragend events, set the drop effect based on the
    // last value that the dropEffect had. This will have been set in
    // EventStateManager::PostHandleEvent for the last dragenter or
    // dragover event.
    aDragEvent->mDataTransfer->SetDropEffectInt(
        initialDataTransfer->DropEffectInt());
  }

  return NS_OK;
}

/* static */
uint32_t nsContentUtils::FilterDropEffect(uint32_t aAction,
                                          uint32_t aEffectAllowed) {
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
bool nsContentUtils::CheckForSubFrameDrop(nsIDragSession* aDragSession,
                                          WidgetDragEvent* aDropEvent) {
  nsCOMPtr<nsIContent> target =
      nsIContent::FromEventTargetOrNull(aDropEvent->mOriginalTarget);
  if (!target) {
    return true;
  }

  // Always allow dropping onto chrome shells.
  BrowsingContext* targetBC = target->OwnerDoc()->GetBrowsingContext();
  if (targetBC->IsChrome()) {
    return false;
  }

  WindowContext* targetWC = target->OwnerDoc()->GetWindowContext();

  // If there is no source browsing context, then this is a drag from another
  // application, which should be allowed.
  RefPtr<WindowContext> sourceWC;
  aDragSession->GetSourceWindowContext(getter_AddRefs(sourceWC));
  if (sourceWC) {
    // Get each successive parent of the source document and compare it to
    // the drop document. If they match, then this is a drag from a child frame.
    for (sourceWC = sourceWC->GetParentWindowContext(); sourceWC;
         sourceWC = sourceWC->GetParentWindowContext()) {
      // If the source and the target match, then the drag started in a
      // descendant frame. If the source is discarded, err on the side of
      // caution and treat it as a subframe drag.
      if (sourceWC == targetWC || sourceWC->IsDiscarded()) {
        return true;
      }
    }
  }

  return false;
}

/* static */
bool nsContentUtils::URIIsLocalFile(nsIURI* aURI) {
  bool isFile;
  nsCOMPtr<nsINetUtil> util = do_QueryInterface(sIOService);

  // Important: we do NOT test the entire URI chain here!
  return util &&
         NS_SUCCEEDED(util->ProtocolHasFlags(
             aURI, nsIProtocolHandler::URI_IS_LOCAL_FILE, &isFile)) &&
         isFile;
}

/* static */
JSContext* nsContentUtils::GetCurrentJSContext() {
  MOZ_ASSERT(IsInitialized());
  if (!IsJSAPIActive()) {
    return nullptr;
  }
  return danger::GetJSContext();
}

template <typename StringType, typename CharType>
void _ASCIIToLowerInSitu(StringType& aStr) {
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
void nsContentUtils::ASCIIToLower(nsAString& aStr) {
  return _ASCIIToLowerInSitu<nsAString, char16_t>(aStr);
}

/* static */
void nsContentUtils::ASCIIToLower(nsACString& aStr) {
  return _ASCIIToLowerInSitu<nsACString, char>(aStr);
}

template <typename StringType, typename CharType>
void _ASCIIToLowerCopy(const StringType& aSource, StringType& aDest) {
  uint32_t len = aSource.Length();
  aDest.SetLength(len);
  MOZ_ASSERT(aDest.Length() == len);

  CharType* dest = aDest.BeginWriting();
  MOZ_ASSERT(dest);

  const CharType* iter = aSource.BeginReading();
  const CharType* end = aSource.EndReading();
  while (iter != end) {
    CharType c = *iter;
    *dest = (c >= 'A' && c <= 'Z') ? c + ('a' - 'A') : c;
    ++iter;
    ++dest;
  }
}

/* static */
void nsContentUtils::ASCIIToLower(const nsAString& aSource, nsAString& aDest) {
  return _ASCIIToLowerCopy<nsAString, char16_t>(aSource, aDest);
}

/* static */
void nsContentUtils::ASCIIToLower(const nsACString& aSource,
                                  nsACString& aDest) {
  return _ASCIIToLowerCopy<nsACString, char>(aSource, aDest);
}

template <typename StringType, typename CharType>
void _ASCIIToUpperInSitu(StringType& aStr) {
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
void nsContentUtils::ASCIIToUpper(nsAString& aStr) {
  return _ASCIIToUpperInSitu<nsAString, char16_t>(aStr);
}

/* static */
void nsContentUtils::ASCIIToUpper(nsACString& aStr) {
  return _ASCIIToUpperInSitu<nsACString, char>(aStr);
}

template <typename StringType, typename CharType>
void _ASCIIToUpperCopy(const StringType& aSource, StringType& aDest) {
  uint32_t len = aSource.Length();
  aDest.SetLength(len);
  MOZ_ASSERT(aDest.Length() == len);

  CharType* dest = aDest.BeginWriting();
  MOZ_ASSERT(dest);

  const CharType* iter = aSource.BeginReading();
  const CharType* end = aSource.EndReading();
  while (iter != end) {
    CharType c = *iter;
    *dest = (c >= 'a' && c <= 'z') ? c + ('A' - 'a') : c;
    ++iter;
    ++dest;
  }
}

/* static */
void nsContentUtils::ASCIIToUpper(const nsAString& aSource, nsAString& aDest) {
  return _ASCIIToUpperCopy<nsAString, char16_t>(aSource, aDest);
}

/* static */
void nsContentUtils::ASCIIToUpper(const nsACString& aSource,
                                  nsACString& aDest) {
  return _ASCIIToUpperCopy<nsACString, char>(aSource, aDest);
}

/* static */
bool nsContentUtils::EqualsIgnoreASCIICase(nsAtom* aAtom1, nsAtom* aAtom2) {
  if (aAtom1 == aAtom2) {
    return true;
  }

  // If both are ascii lowercase already, we know that the slow comparison
  // below is going to return false.
  if (aAtom1->IsAsciiLowercase() && aAtom2->IsAsciiLowercase()) {
    return false;
  }

  return EqualsIgnoreASCIICase(nsDependentAtomString(aAtom1),
                               nsDependentAtomString(aAtom2));
}

/* static */
bool nsContentUtils::EqualsIgnoreASCIICase(const nsAString& aStr1,
                                           const nsAString& aStr2) {
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
bool nsContentUtils::StringContainsASCIIUpper(const nsAString& aStr) {
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
nsIInterfaceRequestor* nsContentUtils::SameOriginChecker() {
  if (!sSameOriginChecker) {
    sSameOriginChecker = new SameOriginCheckerImpl();
    NS_ADDREF(sSameOriginChecker);
  }
  return sSameOriginChecker;
}

/* static */
nsresult nsContentUtils::CheckSameOrigin(nsIChannel* aOldChannel,
                                         nsIChannel* aNewChannel) {
  if (!nsContentUtils::GetSecurityManager()) return NS_ERROR_NOT_AVAILABLE;

  nsCOMPtr<nsIPrincipal> oldPrincipal;
  nsContentUtils::GetSecurityManager()->GetChannelResultPrincipal(
      aOldChannel, getter_AddRefs(oldPrincipal));

  nsCOMPtr<nsIURI> newURI;
  aNewChannel->GetURI(getter_AddRefs(newURI));
  nsCOMPtr<nsIURI> newOriginalURI;
  aNewChannel->GetOriginalURI(getter_AddRefs(newOriginalURI));

  NS_ENSURE_STATE(oldPrincipal && newURI && newOriginalURI);

  nsresult rv = oldPrincipal->CheckMayLoad(newURI, false);
  if (NS_SUCCEEDED(rv) && newOriginalURI != newURI) {
    rv = oldPrincipal->CheckMayLoad(newOriginalURI, false);
  }

  return rv;
}

NS_IMPL_ISUPPORTS(SameOriginCheckerImpl, nsIChannelEventSink,
                  nsIInterfaceRequestor)

NS_IMETHODIMP
SameOriginCheckerImpl::AsyncOnChannelRedirect(
    nsIChannel* aOldChannel, nsIChannel* aNewChannel, uint32_t aFlags,
    nsIAsyncVerifyRedirectCallback* cb) {
  MOZ_ASSERT(aNewChannel, "Redirecting to null channel?");

  nsresult rv = nsContentUtils::CheckSameOrigin(aOldChannel, aNewChannel);
  if (NS_SUCCEEDED(rv)) {
    cb->OnRedirectVerifyCallback(NS_OK);
  }

  return rv;
}

NS_IMETHODIMP
SameOriginCheckerImpl::GetInterface(const nsIID& aIID, void** aResult) {
  return QueryInterface(aIID, aResult);
}

/* static */
nsresult nsContentUtils::GetASCIIOrigin(nsIURI* aURI, nsACString& aOrigin) {
  MOZ_ASSERT(aURI, "missing uri");

  // For Blob URI, the path is the URL of the owning page.
  if (aURI->SchemeIs(BLOBURI_SCHEME)) {
    nsAutoCString path;
    nsresult rv = aURI->GetPathQueryRef(path);
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIURI> uri;
    rv = NS_NewURI(getter_AddRefs(uri), path);
    if (NS_FAILED(rv)) {
      aOrigin.AssignLiteral("null");
      return NS_OK;
    }

    return GetASCIIOrigin(uri, aOrigin);
  }

  aOrigin.Truncate();

  nsCOMPtr<nsIURI> uri = NS_GetInnermostURI(aURI);
  NS_ENSURE_TRUE(uri, NS_ERROR_UNEXPECTED);

  nsAutoCString host;
  nsresult rv = uri->GetAsciiHost(host);

  if (NS_SUCCEEDED(rv) && !host.IsEmpty()) {
    nsAutoCString userPass;
    uri->GetUserPass(userPass);

    nsAutoCString prePath;
    if (!userPass.IsEmpty()) {
      rv = NS_MutateURI(uri).SetUserPass(""_ns).Finalize(uri);
      NS_ENSURE_SUCCESS(rv, rv);
    }

    rv = uri->GetPrePath(prePath);
    NS_ENSURE_SUCCESS(rv, rv);

    aOrigin = prePath;
  } else {
    aOrigin.AssignLiteral("null");
  }

  return NS_OK;
}

/* static */
nsresult nsContentUtils::GetUTFOrigin(nsIPrincipal* aPrincipal,
                                      nsAString& aOrigin) {
  MOZ_ASSERT(aPrincipal, "missing principal");

  aOrigin.Truncate();
  nsAutoCString asciiOrigin;

  nsresult rv = aPrincipal->GetAsciiOrigin(asciiOrigin);
  if (NS_FAILED(rv)) {
    asciiOrigin.AssignLiteral("null");
  }

  CopyUTF8toUTF16(asciiOrigin, aOrigin);
  return NS_OK;
}

/* static */
nsresult nsContentUtils::GetUTFOrigin(nsIURI* aURI, nsAString& aOrigin) {
  MOZ_ASSERT(aURI, "missing uri");
  nsresult rv;

#if defined(MOZ_THUNDERBIRD) || defined(MOZ_SUITE)
  // Check if either URI has a special origin.
  nsCOMPtr<nsIURIWithSpecialOrigin> uriWithSpecialOrigin =
      do_QueryInterface(aURI);
  if (uriWithSpecialOrigin) {
    nsCOMPtr<nsIURI> origin;
    rv = uriWithSpecialOrigin->GetOrigin(getter_AddRefs(origin));
    NS_ENSURE_SUCCESS(rv, rv);

    return GetUTFOrigin(origin, aOrigin);
  }
#endif

  nsAutoCString asciiOrigin;
  rv = GetASCIIOrigin(aURI, asciiOrigin);
  NS_ENSURE_SUCCESS(rv, rv);

  CopyUTF8toUTF16(asciiOrigin, aOrigin);
  return NS_OK;
}

/* static */
bool nsContentUtils::CheckMayLoad(nsIPrincipal* aPrincipal,
                                  nsIChannel* aChannel,
                                  bool aAllowIfInheritsPrincipal) {
  nsCOMPtr<nsIURI> channelURI;
  nsresult rv = NS_GetFinalChannelURI(aChannel, getter_AddRefs(channelURI));
  NS_ENSURE_SUCCESS(rv, false);

  return NS_SUCCEEDED(
      aPrincipal->CheckMayLoad(channelURI, aAllowIfInheritsPrincipal));
}

/* static */
bool nsContentUtils::CanAccessNativeAnon() {
  return LegacyIsCallerChromeOrNativeCode();
}

/* static */
nsresult nsContentUtils::DispatchXULCommand(nsIContent* aTarget, bool aTrusted,
                                            Event* aSourceEvent,
                                            PresShell* aPresShell, bool aCtrl,
                                            bool aAlt, bool aShift, bool aMeta,
                                            uint16_t aInputSource,
                                            int16_t aButton) {
  NS_ENSURE_STATE(aTarget);
  Document* doc = aTarget->OwnerDoc();
  nsPresContext* presContext = doc->GetPresContext();

  RefPtr<XULCommandEvent> xulCommand =
      new XULCommandEvent(doc, presContext, nullptr);
  xulCommand->InitCommandEvent(u"command"_ns, true, true,
                               nsGlobalWindowInner::Cast(doc->GetInnerWindow()),
                               0, aCtrl, aAlt, aShift, aMeta, aButton,
                               aSourceEvent, aInputSource, IgnoreErrors());

  if (aPresShell) {
    nsEventStatus status = nsEventStatus_eIgnore;
    return aPresShell->HandleDOMEventWithTarget(aTarget, xulCommand, &status);
  }

  ErrorResult rv;
  aTarget->DispatchEvent(*xulCommand, rv);
  return rv.StealNSResult();
}

// static
nsresult nsContentUtils::WrapNative(JSContext* cx, nsISupports* native,
                                    nsWrapperCache* cache, const nsIID* aIID,
                                    JS::MutableHandle<JS::Value> vp,
                                    bool aAllowWrapping) {
  MOZ_ASSERT(cx == GetCurrentJSContext());

  if (!native) {
    vp.setNull();

    return NS_OK;
  }

  JSObject* wrapper = xpc_FastGetCachedWrapper(cx, cache, vp);
  if (wrapper) {
    return NS_OK;
  }

  NS_ENSURE_TRUE(sXPConnect, NS_ERROR_UNEXPECTED);

  if (!NS_IsMainThread()) {
    MOZ_CRASH();
  }

  JS::Rooted<JSObject*> scope(cx, JS::CurrentGlobalOrNull(cx));
  nsresult rv = sXPConnect->WrapNativeToJSVal(cx, scope, native, cache, aIID,
                                              aAllowWrapping, vp);
  return rv;
}

nsresult nsContentUtils::CreateArrayBuffer(JSContext* aCx,
                                           const nsACString& aData,
                                           JSObject** aResult) {
  if (!aCx) {
    return NS_ERROR_FAILURE;
  }

  size_t dataLen = aData.Length();
  *aResult = JS::NewArrayBuffer(aCx, dataLen);
  if (!*aResult) {
    return NS_ERROR_FAILURE;
  }

  if (dataLen > 0) {
    NS_ASSERTION(JS::IsArrayBufferObject(*aResult), "What happened?");
    JS::AutoCheckCannotGC nogc;
    bool isShared;
    memcpy(JS::GetArrayBufferData(*aResult, &isShared, nogc),
           aData.BeginReading(), dataLen);
    MOZ_ASSERT(!isShared);
  }

  return NS_OK;
}

void nsContentUtils::StripNullChars(const nsAString& aInStr,
                                    nsAString& aOutStr) {
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
    if (*start != '\0') aOutStr.Append(*start);
    ++start;
  }
}

struct ClassMatchingInfo {
  AtomArray mClasses;
  nsCaseTreatment mCaseTreatment;
};

// static
bool nsContentUtils::MatchClassNames(Element* aElement, int32_t aNamespaceID,
                                     nsAtom* aAtom, void* aData) {
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
    if (!classAttr->Contains(info->mClasses[i], info->mCaseTreatment)) {
      return false;
    }
  }

  return true;
}

// static
void nsContentUtils::DestroyClassNameArray(void* aData) {
  ClassMatchingInfo* info = static_cast<ClassMatchingInfo*>(aData);
  delete info;
}

// static
void* nsContentUtils::AllocClassMatchingInfo(nsINode* aRootNode,
                                             const nsString* aClasses) {
  nsAttrValue attrValue;
  attrValue.ParseAtomArray(*aClasses);
  // nsAttrValue::Equals is sensitive to order, so we'll send an array
  auto* info = new ClassMatchingInfo;
  if (attrValue.Type() == nsAttrValue::eAtomArray) {
    info->mClasses = std::move(attrValue.GetAtomArrayValue()->mArray);
  } else if (attrValue.Type() == nsAttrValue::eAtom) {
    info->mClasses.AppendElement(attrValue.GetAtomValue());
  }

  info->mCaseTreatment =
      aRootNode->OwnerDoc()->GetCompatibilityMode() == eCompatibility_NavQuirks
          ? eIgnoreCase
          : eCaseMatters;
  return info;
}

// static
bool nsContentUtils::IsFocusedContent(const nsIContent* aContent) {
  nsFocusManager* fm = nsFocusManager::GetFocusManager();

  return fm && fm->GetFocusedElement() == aContent;
}

bool nsContentUtils::IsSubDocumentTabbable(nsIContent* aContent) {
  Document* doc = aContent->GetComposedDoc();
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
  Document* subDoc = doc->GetSubDocumentFor(aContent);
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

  // If there are 2 viewers for the current docshell, that
  // means the current document may be a zombie document.
  // While load and pageshow events are dispatched, zombie viewer is the old,
  // to be hidden document.
  if (contentViewer->GetPreviousViewer()) {
    bool inOnLoad = false;
    docShell->GetIsExecutingOnLoadHandler(&inOnLoad);
    return inOnLoad;
  }

  return true;
}

bool nsContentUtils::HasScrollgrab(nsIContent* aContent) {
  // If we ever standardize this feature we'll want to hook this up properly
  // again. For now we're removing all the DOM-side code related to it but
  // leaving the layout and APZ handling for it in place.
  return false;
}

void nsContentUtils::FlushLayoutForTree(nsPIDOMWindowOuter* aWindow) {
  if (!aWindow) {
    return;
  }

  // Note that because FlushPendingNotifications flushes parents, this
  // is O(N^2) in docshell tree depth.  However, the docshell tree is
  // usually pretty shallow.

  if (RefPtr<Document> doc = aWindow->GetDoc()) {
    doc->FlushPendingNotifications(FlushType::Layout);
  }

  if (nsCOMPtr<nsIDocShell> docShell = aWindow->GetDocShell()) {
    int32_t i = 0, i_end;
    docShell->GetInProcessChildCount(&i_end);
    for (; i < i_end; ++i) {
      nsCOMPtr<nsIDocShellTreeItem> item;
      if (docShell->GetInProcessChildAt(i, getter_AddRefs(item)) == NS_OK &&
          item) {
        if (nsCOMPtr<nsPIDOMWindowOuter> win = item->GetWindow()) {
          FlushLayoutForTree(win);
        }
      }
    }
  }
}

void nsContentUtils::RemoveNewlines(nsString& aString) { aString.StripCRLF(); }

void nsContentUtils::PlatformToDOMLineBreaks(nsString& aString) {
  if (!PlatformToDOMLineBreaks(aString, fallible)) {
    aString.AllocFailed(aString.Length());
  }
}

bool nsContentUtils::PlatformToDOMLineBreaks(nsString& aString,
                                             const fallible_t& aFallible) {
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

void nsContentUtils::PopulateStringFromStringBuffer(nsStringBuffer* aBuf,
                                                    nsAString& aResultString) {
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

already_AddRefed<nsContentList> nsContentUtils::GetElementsByClassName(
    nsINode* aRootNode, const nsAString& aClasses) {
  MOZ_ASSERT(aRootNode, "Must have root node");

  return GetFuncStringContentList<nsCacheableFuncStringHTMLCollection>(
      aRootNode, MatchClassNames, DestroyClassNameArray, AllocClassMatchingInfo,
      aClasses);
}

PresShell* nsContentUtils::FindPresShellForDocument(const Document* aDocument) {
  const Document* doc = aDocument;
  Document* displayDoc = doc->GetDisplayDocument();
  if (displayDoc) {
    doc = displayDoc;
  }

  PresShell* presShell = doc->GetPresShell();
  if (presShell) {
    return presShell;
  }

  nsCOMPtr<nsIDocShellTreeItem> docShellTreeItem = doc->GetDocShell();
  while (docShellTreeItem) {
    // We may be in a display:none subdocument, or we may not have a presshell
    // created yet.
    // Walk the docshell tree to find the nearest container that has a
    // presshell, and return that.
    nsCOMPtr<nsIDocShell> docShell = do_QueryInterface(docShellTreeItem);
    if (PresShell* presShell = docShell->GetPresShell()) {
      return presShell;
    }
    nsCOMPtr<nsIDocShellTreeItem> parent;
    docShellTreeItem->GetInProcessParent(getter_AddRefs(parent));
    docShellTreeItem = parent;
  }

  return nullptr;
}

/* static */
nsPresContext* nsContentUtils::FindPresContextForDocument(
    const Document* aDocument) {
  if (PresShell* presShell = FindPresShellForDocument(aDocument)) {
    return presShell->GetPresContext();
  }
  return nullptr;
}

nsIWidget* nsContentUtils::WidgetForDocument(const Document* aDocument) {
  PresShell* presShell = FindPresShellForDocument(aDocument);
  if (!presShell) {
    return nullptr;
  }
  nsViewManager* vm = presShell->GetViewManager();
  if (!vm) {
    return nullptr;
  }
  nsView* rootView = vm->GetRootView();
  if (!rootView) {
    return nullptr;
  }
  nsView* displayRoot = nsViewManager::GetDisplayRootFor(rootView);
  if (!displayRoot) {
    return nullptr;
  }
  return displayRoot->GetNearestWidget(nullptr);
}

nsIWidget* nsContentUtils::WidgetForContent(const nsIContent* aContent) {
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

WindowRenderer* nsContentUtils::WindowRendererForContent(
    const nsIContent* aContent) {
  nsIWidget* widget = nsContentUtils::WidgetForContent(aContent);
  if (widget) {
    return widget->GetWindowRenderer();
  }

  return nullptr;
}

WindowRenderer* nsContentUtils::WindowRendererForDocument(
    const Document* aDoc) {
  nsIWidget* widget = nsContentUtils::WidgetForDocument(aDoc);
  if (widget) {
    return widget->GetWindowRenderer();
  }

  return nullptr;
}

bool nsContentUtils::AllowXULXBLForPrincipal(nsIPrincipal* aPrincipal) {
  if (!aPrincipal) {
    return false;
  }

  if (aPrincipal->IsSystemPrincipal()) {
    return true;
  }

  return (StaticPrefs::dom_allow_XUL_XBL_for_file() &&
          aPrincipal->SchemeIs("file")) ||
         IsSitePermAllow(aPrincipal, "allowXULXBL"_ns);
}

bool nsContentUtils::IsPDFJSEnabled() {
  nsCOMPtr<nsIStreamConverter> conv = do_CreateInstance(
      "@mozilla.org/streamconv;1?from=application/pdf&to=text/html");
  return conv;
}

bool nsContentUtils::IsPDFJS(nsIPrincipal* aPrincipal) {
  if (!aPrincipal) {
    return false;
  }
  nsAutoCString spec;
  nsresult rv = aPrincipal->GetAsciiSpec(spec);
  NS_ENSURE_SUCCESS(rv, false);
  return spec.EqualsLiteral("resource://pdf.js/web/viewer.html");
}

bool nsContentUtils::IsPDFJS(JSContext* aCx, JSObject*) {
  return IsPDFJS(SubjectPrincipal(aCx));
}

already_AddRefed<nsIDocumentLoaderFactory>
nsContentUtils::FindInternalContentViewer(const nsACString& aType,
                                          ContentViewerType* aLoaderType) {
  if (aLoaderType) {
    *aLoaderType = TYPE_UNSUPPORTED;
  }

  // one helper factory, please
  nsCOMPtr<nsICategoryManager> catMan(
      do_GetService(NS_CATEGORYMANAGER_CONTRACTID));
  if (!catMan) return nullptr;

  nsCOMPtr<nsIDocumentLoaderFactory> docFactory;

  nsCString contractID;
  nsresult rv =
      catMan->GetCategoryEntry("Gecko-Content-Viewers", aType, contractID);
  if (NS_SUCCEEDED(rv)) {
    docFactory = do_GetService(contractID.get());
    if (docFactory && aLoaderType) {
      if (contractID.EqualsLiteral(CONTENT_DLF_CONTRACTID))
        *aLoaderType = TYPE_CONTENT;
      else if (contractID.EqualsLiteral(PLUGIN_DLF_CONTRACTID))
        *aLoaderType = TYPE_FALLBACK;
      else
        *aLoaderType = TYPE_UNKNOWN;
    }
    return docFactory.forget();
  }

  if (DecoderTraits::IsSupportedInVideoDocument(aType)) {
    docFactory =
        do_GetService("@mozilla.org/content/document-loader-factory;1");
    if (docFactory && aLoaderType) {
      *aLoaderType = TYPE_CONTENT;
    }
    return docFactory.forget();
  }

  return nullptr;
}

static void ReportPatternCompileFailure(nsAString& aPattern,
                                        const Document* aDocument,
                                        JS::MutableHandle<JS::Value> error,
                                        JSContext* cx) {
  JS::AutoSaveExceptionState savedExc(cx);
  JS::RootedObject exnObj(cx, &error.toObject());
  JS::RootedValue messageVal(cx);
  if (!JS_GetProperty(cx, exnObj, "message", &messageVal)) {
    return;
  }
  JS::RootedString messageStr(cx, messageVal.toString());
  MOZ_ASSERT(messageStr);

  AutoTArray<nsString, 2> strings;
  strings.AppendElement(aPattern);
  if (!AssignJSString(cx, *strings.AppendElement(), messageStr)) {
    return;
  }

  nsContentUtils::ReportToConsole(nsIScriptError::errorFlag, "DOM"_ns,
                                  aDocument, nsContentUtils::eDOM_PROPERTIES,
                                  "PatternAttributeCompileFailure", strings);
  savedExc.drop();
}

// static
Maybe<bool> nsContentUtils::IsPatternMatching(nsAString& aValue,
                                              nsAString& aPattern,
                                              const Document* aDocument) {
  NS_ASSERTION(aDocument, "aDocument should be a valid pointer (not null)");

  // The fact that we're using a JS regexp under the hood should not be visible
  // to things like window onerror handlers, so we don't initialize our JSAPI
  // with the document's window (which may not exist anyway).
  AutoJSAPI jsapi;
  jsapi.Init();
  JSContext* cx = jsapi.cx();
  AutoDisableJSInterruptCallback disabler(cx);

  // We can use the junk scope here, because we're just using it for regexp
  // evaluation, not actual script execution, and we disable statics so that the
  // evaluation does not interact with the execution global.
  JSAutoRealm ar(cx, xpc::PrivilegedJunkScope());

  // Check if the pattern by itself is valid first, and not that it only becomes
  // valid once we add ^(?: and )$.
  JS::RootedValue error(cx);
  if (!JS::CheckRegExpSyntax(
          cx, static_cast<char16_t*>(aPattern.BeginWriting()),
          aPattern.Length(), JS::RegExpFlag::Unicode, &error)) {
    return Nothing();
  }

  if (!error.isUndefined()) {
    ReportPatternCompileFailure(aPattern, aDocument, &error, cx);
    return Some(true);
  }

  // The pattern has to match the entire value.
  aPattern.InsertLiteral(u"^(?:", 0);
  aPattern.AppendLiteral(")$");

  JS::Rooted<JSObject*> re(
      cx,
      JS::NewUCRegExpObject(cx, static_cast<char16_t*>(aPattern.BeginWriting()),
                            aPattern.Length(), JS::RegExpFlag::Unicode));
  if (!re) {
    return Nothing();
  }

  JS::Rooted<JS::Value> rval(cx, JS::NullValue());
  size_t idx = 0;
  if (!JS::ExecuteRegExpNoStatics(cx, re,
                                  static_cast<char16_t*>(aValue.BeginWriting()),
                                  aValue.Length(), &idx, true, &rval)) {
    return Nothing();
  }

  return Some(!rval.isNull());
}

// static
nsresult nsContentUtils::URIInheritsSecurityContext(nsIURI* aURI,
                                                    bool* aResult) {
  // Note: about:blank URIs do NOT inherit the security context from the
  // current document, which is what this function tests for...
  return NS_URIChainHasFlags(
      aURI, nsIProtocolHandler::URI_INHERITS_SECURITY_CONTEXT, aResult);
}

// static
bool nsContentUtils::ChannelShouldInheritPrincipal(
    nsIPrincipal* aLoadingPrincipal, nsIURI* aURI, bool aInheritForAboutBlank,
    bool aForceInherit) {
  MOZ_ASSERT(aLoadingPrincipal,
             "Can not check inheritance without a principal");

  // Only tell the channel to inherit if it can't provide its own security
  // context.
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
         NS_SUCCEEDED(aLoadingPrincipal->CheckMayLoad(aURI, false)) &&
         // One more check here.  CheckMayLoad will always return true for the
         // system principal, but we do NOT want to inherit in that case.
         !aLoadingPrincipal->IsSystemPrincipal());
  }
  return inherit;
}

/* static */
bool nsContentUtils::IsCutCopyAllowed(Document* aDocument,
                                      nsIPrincipal& aSubjectPrincipal) {
  if (StaticPrefs::dom_allow_cut_copy() && aDocument &&
      aDocument->HasValidTransientUserGestureActivation()) {
    return true;
  }

  return PrincipalHasPermission(aSubjectPrincipal, nsGkAtoms::clipboardWrite);
}

/* static */
bool nsContentUtils::HaveEqualPrincipals(Document* aDoc1, Document* aDoc2) {
  if (!aDoc1 || !aDoc2) {
    return false;
  }
  bool principalsEqual = false;
  aDoc1->NodePrincipal()->Equals(aDoc2->NodePrincipal(), &principalsEqual);
  return principalsEqual;
}

/* static */
bool nsContentUtils::HasPluginWithUncontrolledEventDispatch(
    nsIContent* aContent) {
  return false;
}

/* static */
void nsContentUtils::FireMutationEventsForDirectParsing(
    Document* aDoc, nsIContent* aDest, int32_t aOldChildCount) {
  // Fire mutation events. Optimize for the case when there are no listeners
  int32_t newChildCount = aDest->GetChildCount();
  if (newChildCount && nsContentUtils::HasMutationListeners(
                           aDoc, NS_EVENT_BITS_MUTATION_NODEINSERTED)) {
    AutoTArray<nsCOMPtr<nsIContent>, 50> childNodes;
    NS_ASSERTION(newChildCount - aOldChildCount >= 0,
                 "What, some unexpected dom mutation has happened?");
    childNodes.SetCapacity(newChildCount - aOldChildCount);
    for (nsIContent* child = aDest->GetFirstChild(); child;
         child = child->GetNextSibling()) {
      childNodes.AppendElement(child);
    }
    FragmentOrElement::FireNodeInserted(aDoc, aDest, childNodes);
  }
}

/* static */
const Document* nsContentUtils::GetInProcessSubtreeRootDocument(const Document* aDoc) {
  if (!aDoc) {
    return nullptr;
  }
  const Document* doc = aDoc;
  while (doc->GetInProcessParentDocument()) {
    doc = doc->GetInProcessParentDocument();
  }
  return doc;
}

// static
int32_t nsContentUtils::GetAdjustedOffsetInTextControl(nsIFrame* aOffsetFrame,
                                                       int32_t aOffset) {
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

  if (aOffsetFrame->GetPrevSibling() && !aOffsetFrame->GetNextSibling()) {
    // In this case, we're actually within the last frame, which is a br
    // frame. Our offset should therefore be the length of the first child of
    // our parent.
    int32_t aOutOffset = aOffsetFrame->GetParent()
                             ->PrincipalChildList()
                             .FirstChild()
                             ->GetContent()
                             ->Length();
    return aOutOffset;
  }

  // Otherwise, we're within one of the text frames, in which case our offset
  // has already been correctly calculated.
  return aOffset;
}

// static
void nsContentUtils::GetSelectionInTextControl(Selection* aSelection,
                                               Element* aRoot,
                                               uint32_t& aOutStartOffset,
                                               uint32_t& aOutEndOffset) {
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
                   startContainer == lastChild,
               "Unexpected startContainer");
  NS_ASSERTION(endContainer == aRoot || endContainer == firstChild ||
                   endContainer == lastChild,
               "Unexpected endContainer");
  // firstChild is either text or a <br> (hence an element).
  MOZ_ASSERT_IF(firstChild, firstChild->IsText() || firstChild->IsElement());
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

// static
HTMLEditor* nsContentUtils::GetHTMLEditor(nsPresContext* aPresContext) {
  if (!aPresContext) {
    return nullptr;
  }
  return GetHTMLEditor(aPresContext->GetDocShell());
}

// static
HTMLEditor* nsContentUtils::GetHTMLEditor(nsDocShell* aDocShell) {
  bool isEditable;
  if (!aDocShell || NS_FAILED(aDocShell->GetEditable(&isEditable)) ||
      !isEditable) {
    return nullptr;
  }
  return aDocShell->GetHTMLEditor();
}

// static
EditorBase* nsContentUtils::GetActiveEditor(nsPresContext* aPresContext) {
  if (!aPresContext) {
    return nullptr;
  }

  return GetActiveEditor(aPresContext->Document()->GetWindow());
}

// static
EditorBase* nsContentUtils::GetActiveEditor(nsPIDOMWindowOuter* aWindow) {
  if (!aWindow || !aWindow->GetExtantDoc()) {
    return nullptr;
  }

  // If it's in designMode, nobody can have focus.  Therefore, the HTMLEditor
  // handles all events.  I.e., it's focused editor in this case.
  if (aWindow->GetExtantDoc()->IsInDesignMode()) {
    return GetHTMLEditor(nsDocShell::Cast(aWindow->GetDocShell()));
  }

  // If focused element is associated with TextEditor, it must be <input>
  // element or <textarea> element.  Let's return it even if it's in a
  // contenteditable element.
  nsCOMPtr<nsPIDOMWindowOuter> focusedWindow;
  if (Element* focusedElement = nsFocusManager::GetFocusedDescendant(
          aWindow, nsFocusManager::SearchRange::eOnlyCurrentWindow,
          getter_AddRefs(focusedWindow))) {
    if (TextEditor* textEditor = focusedElement->GetTextEditorInternal()) {
      return textEditor;
    }
  }

  // Otherwise, HTMLEditor may handle inputs even non-editable element has
  // focus or nobody has focus.
  return GetHTMLEditor(nsDocShell::Cast(aWindow->GetDocShell()));
}

// static
TextEditor* nsContentUtils::GetTextEditorFromAnonymousNodeWithoutCreation(
    const nsIContent* aAnonymousContent) {
  if (!aAnonymousContent) {
    return nullptr;
  }
  nsIContent* parent = aAnonymousContent->FindFirstNonChromeOnlyAccessContent();
  if (!parent || parent == aAnonymousContent) {
    return nullptr;
  }
  if (HTMLInputElement* inputElement =
          HTMLInputElement::FromNodeOrNull(parent)) {
    return inputElement->GetTextEditorWithoutCreation();
  }
  if (HTMLTextAreaElement* textareaElement =
          HTMLTextAreaElement::FromNodeOrNull(parent)) {
    return textareaElement->GetTextEditorWithoutCreation();
  }
  return nullptr;
}

// static
bool nsContentUtils::IsNodeInEditableRegion(nsINode* aNode) {
  while (aNode) {
    if (aNode->IsEditable()) {
      return true;
    }
    aNode = aNode->GetParent();
  }
  return false;
}

// static
bool nsContentUtils::IsForbiddenRequestHeader(const nsACString& aHeader,
                                              const nsACString& aValue) {
  if (IsForbiddenSystemRequestHeader(aHeader)) {
    return true;
  }

  if ((nsContentUtils::IsOverrideMethodHeader(aHeader) &&
       nsContentUtils::ContainsForbiddenMethod(aValue))) {
    return true;
  }

  if (StringBeginsWith(aHeader, "proxy-"_ns,
                       nsCaseInsensitiveCStringComparator) ||
      StringBeginsWith(aHeader, "sec-"_ns,
                       nsCaseInsensitiveCStringComparator)) {
    return true;
  }

  return false;
}

// static
bool nsContentUtils::IsForbiddenSystemRequestHeader(const nsACString& aHeader) {
  static const char* kInvalidHeaders[] = {"accept-charset",
                                          "accept-encoding",
                                          "access-control-request-headers",
                                          "access-control-request-method",
                                          "connection",
                                          "content-length",
                                          "cookie",
                                          "cookie2",
                                          "date",
                                          "dnt",
                                          "expect",
                                          "host",
                                          "keep-alive",
                                          "origin",
                                          "referer",
                                          "te",
                                          "trailer",
                                          "transfer-encoding",
                                          "upgrade",
                                          "via"};
  for (auto& kInvalidHeader : kInvalidHeaders) {
    if (aHeader.LowerCaseEqualsASCII(kInvalidHeader)) {
      return true;
    }
  }
  return false;
}

// static
bool nsContentUtils::IsForbiddenResponseHeader(const nsACString& aHeader) {
  return (aHeader.LowerCaseEqualsASCII("set-cookie") ||
          aHeader.LowerCaseEqualsASCII("set-cookie2"));
}

// static
bool nsContentUtils::IsOverrideMethodHeader(const nsACString& headerName) {
  return headerName.EqualsIgnoreCase("x-http-method-override") ||
         headerName.EqualsIgnoreCase("x-http-method") ||
         headerName.EqualsIgnoreCase("x-method-override");
}

// static
bool nsContentUtils::ContainsForbiddenMethod(const nsACString& headerValue) {
  bool hasInsecureMethod = false;
  nsCCharSeparatedTokenizer tokenizer(headerValue, ',');

  while (tokenizer.hasMoreTokens()) {
    const nsDependentCSubstring& value = tokenizer.nextToken();

    if (value.EqualsIgnoreCase("connect") || value.EqualsIgnoreCase("trace") ||
        value.EqualsIgnoreCase("track")) {
      hasInsecureMethod = true;
      break;
    }
  }

  return hasInsecureMethod;
}

// static
bool nsContentUtils::IsCorsUnsafeRequestHeaderValue(
    const nsACString& aHeaderValue) {
  const char* cur = aHeaderValue.BeginReading();
  const char* end = aHeaderValue.EndReading();

  while (cur != end) {
    // Implementation of
    // https://fetch.spec.whatwg.org/#cors-unsafe-request-header-byte Is less
    // than a space but not a horizontal tab
    if ((*cur < ' ' && *cur != '\t') || *cur == '"' || *cur == '(' ||
        *cur == ')' || *cur == ':' || *cur == '<' || *cur == '>' ||
        *cur == '?' || *cur == '@' || *cur == '[' || *cur == '\\' ||
        *cur == ']' || *cur == '{' || *cur == '}' ||
        *cur == 0x7F) {  // 0x75 is DEL
      return true;
    }
    cur++;
  }
  return false;
}

// static
bool nsContentUtils::IsAllowedNonCorsAccept(const nsACString& aHeaderValue) {
  if (IsCorsUnsafeRequestHeaderValue(aHeaderValue)) {
    return false;
  }
  return true;
}

// static
bool nsContentUtils::IsAllowedNonCorsContentType(
    const nsACString& aHeaderValue) {
  nsAutoCString contentType;
  nsAutoCString unused;

  if (IsCorsUnsafeRequestHeaderValue(aHeaderValue)) {
    return false;
  }

  nsresult rv = NS_ParseRequestContentType(aHeaderValue, contentType, unused);
  if (NS_FAILED(rv)) {
    return false;
  }

  return contentType.LowerCaseEqualsLiteral("text/plain") ||
         contentType.LowerCaseEqualsLiteral(
             "application/x-www-form-urlencoded") ||
         contentType.LowerCaseEqualsLiteral("multipart/form-data");
}

// static
bool nsContentUtils::IsAllowedNonCorsLanguage(const nsACString& aHeaderValue) {
  const char* cur = aHeaderValue.BeginReading();
  const char* end = aHeaderValue.EndReading();

  while (cur != end) {
    if ((*cur >= '0' && *cur <= '9') || (*cur >= 'A' && *cur <= 'Z') ||
        (*cur >= 'a' && *cur <= 'z') || *cur == ' ' || *cur == '*' ||
        *cur == ',' || *cur == '-' || *cur == '.' || *cur == ';' ||
        *cur == '=') {
      cur++;
      continue;
    }
    return false;
  }
  return true;
}

// static
bool nsContentUtils::IsCORSSafelistedRequestHeader(const nsACString& aName,
                                                   const nsACString& aValue) {
  // see https://fetch.spec.whatwg.org/#cors-safelisted-request-header
  if (aValue.Length() > 128) {
    return false;
  }
  return (aName.LowerCaseEqualsLiteral("accept") &&
          nsContentUtils::IsAllowedNonCorsAccept(aValue)) ||
         (aName.LowerCaseEqualsLiteral("accept-language") &&
          nsContentUtils::IsAllowedNonCorsLanguage(aValue)) ||
         (aName.LowerCaseEqualsLiteral("content-language") &&
          nsContentUtils::IsAllowedNonCorsLanguage(aValue)) ||
         (aName.LowerCaseEqualsLiteral("content-type") &&
          nsContentUtils::IsAllowedNonCorsContentType(aValue));
}

mozilla::LogModule* nsContentUtils::ResistFingerprintingLog() {
  return gResistFingerprintingLog;
}
mozilla::LogModule* nsContentUtils::DOMDumpLog() { return sDOMDumpLog; }

bool nsContentUtils::GetNodeTextContent(nsINode* aNode, bool aDeep,
                                        nsAString& aResult,
                                        const fallible_t& aFallible) {
  aResult.Truncate();
  return AppendNodeTextContent(aNode, aDeep, aResult, aFallible);
}

void nsContentUtils::GetNodeTextContent(nsINode* aNode, bool aDeep,
                                        nsAString& aResult) {
  if (!GetNodeTextContent(aNode, aDeep, aResult, fallible)) {
    NS_ABORT_OOM(0);  // Unfortunately we don't know the allocation size
  }
}

void nsContentUtils::DestroyMatchString(void* aData) {
  if (aData) {
    nsString* matchString = static_cast<nsString*>(aData);
    delete matchString;
  }
}

bool nsContentUtils::IsJavascriptMIMEType(const nsAString& aMIMEType) {
  // Table ordered from most to least likely JS MIME types.
  static const char* jsTypes[] = {"text/javascript",
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
                                  nullptr};

  for (uint32_t i = 0; jsTypes[i]; ++i) {
    if (aMIMEType.LowerCaseEqualsASCII(jsTypes[i])) {
      return true;
    }
  }

  return false;
}

bool nsContentUtils::PrefetchPreloadEnabled(nsIDocShell* aDocShell) {
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
    auto appType = docshell->GetAppType();
    if (appType == nsIDocShell::APP_TYPE_MAIL) {
      return false;  // do not prefetch, preload, preconnect from mailnews
    }

    docshell->GetInProcessParent(getter_AddRefs(parentItem));
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

uint64_t nsContentUtils::GetInnerWindowID(nsIRequest* aRequest) {
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

uint64_t nsContentUtils::GetInnerWindowID(nsILoadGroup* aLoadGroup) {
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

// static
void nsContentUtils::MaybeFixIPv6Host(nsACString& aHost) {
  if (aHost.FindChar(':') != -1) {  // Escape IPv6 address
    MOZ_ASSERT(!aHost.Length() ||
               (aHost[0] != '[' && aHost[aHost.Length() - 1] != ']'));
    aHost.Insert('[', 0);
    aHost.Append(']');
  }
}

nsresult nsContentUtils::GetHostOrIPv6WithBrackets(nsIURI* aURI,
                                                   nsACString& aHost) {
  aHost.Truncate();
  nsresult rv = aURI->GetHost(aHost);
  if (NS_FAILED(rv)) {  // Some URIs do not have a host
    return rv;
  }

  MaybeFixIPv6Host(aHost);

  return NS_OK;
}

nsresult nsContentUtils::GetHostOrIPv6WithBrackets(nsIURI* aURI,
                                                   nsAString& aHost) {
  nsAutoCString hostname;
  nsresult rv = GetHostOrIPv6WithBrackets(aURI, hostname);
  if (NS_FAILED(rv)) {
    return rv;
  }
  CopyUTF8toUTF16(hostname, aHost);
  return NS_OK;
}

nsresult nsContentUtils::GetHostOrIPv6WithBrackets(nsIPrincipal* aPrincipal,
                                                   nsACString& aHost) {
  nsresult rv = aPrincipal->GetAsciiHost(aHost);
  if (NS_FAILED(rv)) {  // Some URIs do not have a host
    return rv;
  }

  MaybeFixIPv6Host(aHost);
  return NS_OK;
}

CallState nsContentUtils::CallOnAllRemoteChildren(
    MessageBroadcaster* aManager,
    const std::function<CallState(BrowserParent*)>& aCallback) {
  uint32_t browserChildCount = aManager->ChildCount();
  for (uint32_t j = 0; j < browserChildCount; ++j) {
    RefPtr<MessageListenerManager> childMM = aManager->GetChildAt(j);
    if (!childMM) {
      continue;
    }

    RefPtr<MessageBroadcaster> nonLeafMM = MessageBroadcaster::From(childMM);
    if (nonLeafMM) {
      if (CallOnAllRemoteChildren(nonLeafMM, aCallback) == CallState::Stop) {
        return CallState::Stop;
      }
      continue;
    }

    mozilla::dom::ipc::MessageManagerCallback* cb = childMM->GetCallback();
    if (cb) {
      nsFrameLoader* fl = static_cast<nsFrameLoader*>(cb);
      BrowserParent* remote = BrowserParent::GetFrom(fl);
      if (remote && aCallback) {
        if (aCallback(remote) == CallState::Stop) {
          return CallState::Stop;
        }
      }
    }
  }

  return CallState::Continue;
}

void nsContentUtils::CallOnAllRemoteChildren(
    nsPIDOMWindowOuter* aWindow,
    const std::function<CallState(BrowserParent*)>& aCallback) {
  nsGlobalWindowOuter* window = nsGlobalWindowOuter::Cast(aWindow);
  if (window->IsChromeWindow()) {
    RefPtr<MessageBroadcaster> windowMM = window->GetMessageManager();
    if (windowMM) {
      CallOnAllRemoteChildren(windowMM, aCallback);
    }
  }
}

struct UIStateChangeInfo {
  UIStateChangeType mShowFocusRings;

  explicit UIStateChangeInfo(UIStateChangeType aShowFocusRings)
      : mShowFocusRings(aShowFocusRings) {}
};

void nsContentUtils::SetKeyboardIndicatorsOnRemoteChildren(
    nsPIDOMWindowOuter* aWindow, UIStateChangeType aShowFocusRings) {
  UIStateChangeInfo stateInfo(aShowFocusRings);
  CallOnAllRemoteChildren(aWindow, [&stateInfo](BrowserParent* aBrowserParent) {
    Unused << aBrowserParent->SendSetKeyboardIndicators(
        stateInfo.mShowFocusRings);
    return CallState::Continue;
  });
}

bool nsContentUtils::IPCDataTransferItemHasKnownFlavor(
    const IPCDataTransferItem& aItem) {
  // Unknown types are converted to kCustomTypesMime.
  // FIXME(bug 1776879) text/plain is converted to text/unicode still.
  if (aItem.flavor().EqualsASCII(kCustomTypesMime) ||
      aItem.flavor().EqualsASCII(kUnicodeMime)) {
    return true;
  }

  for (const char* format : DataTransfer::kKnownFormats) {
    if (aItem.flavor().EqualsASCII(format)) {
      return true;
    }
  }

  return false;
}

nsresult nsContentUtils::IPCTransferableToTransferable(
    const IPCDataTransfer& aDataTransfer, bool aAddDataFlavor,
    nsITransferable* aTransferable, IShmemAllocator* aAllocator, bool aFilterUnknownFlavors) {
  auto release = MakeScopeExit([&] {
    const nsTArray<IPCDataTransferItem>& items = aDataTransfer.items();
    for (const auto& item : items) {
      if (item.data().type() == IPCDataTransferData::TShmem) {
        Unused << aAllocator->DeallocShmem(item.data().get_Shmem());
      }
    }
  });

  nsresult rv;
  const nsTArray<IPCDataTransferItem>& items = aDataTransfer.items();
  for (const auto& item : items) {
    if (aFilterUnknownFlavors && !IPCDataTransferItemHasKnownFlavor(item)) {
      NS_WARNING(
          "Ignoring unknown flavor in "
          "nsContentUtils::IPCTransferableToTransferable");
      continue;
    }

    if (aAddDataFlavor) {
      aTransferable->AddDataFlavor(item.flavor().get());
    }

    if (item.dataType() == TransferableDataType::String) {
      nsCOMPtr<nsISupportsString> dataWrapper =
          do_CreateInstance(NS_SUPPORTS_STRING_CONTRACTID, &rv);
      NS_ENSURE_SUCCESS(rv, rv);

      if (item.data().type() == IPCDataTransferData::TShmem) {
        Shmem itemData = item.data().get_Shmem();
        const nsDependentSubstring text(itemData.get<char16_t>(),
                                        itemData.Size<char16_t>());
        rv = dataWrapper->SetData(text);
        NS_ENSURE_SUCCESS(rv, rv);
      } else {
        const nsString& text = item.data().get_nsString();
        rv = dataWrapper->SetData(text);
        NS_ENSURE_SUCCESS(rv, rv);
      }

      rv = aTransferable->SetTransferData(item.flavor().get(), dataWrapper);
      NS_ENSURE_SUCCESS(rv, rv);
      continue;
    }

    if (item.dataType() == TransferableDataType::ImageContainer) {
      nsCOMPtr<imgIContainer> imageContainer;
      rv = nsContentUtils::DataTransferItemToImage(
          item, getter_AddRefs(imageContainer));
      NS_ENSURE_SUCCESS(rv, rv);

      rv = aTransferable->SetTransferData(item.flavor().get(), imageContainer);
      NS_ENSURE_SUCCESS(rv, rv);
      continue;
    }

    if (item.dataType() == TransferableDataType::InputStream) {
      Shmem data = item.data().get_Shmem();
      nsCOMPtr<nsIInputStream> stream;
      rv = NS_NewCStringInputStream(
          getter_AddRefs(stream),
          nsDependentCSubstring(data.get<char>(), data.Size<char>()));
      NS_ENSURE_SUCCESS(rv, rv);

      rv = aTransferable->SetTransferData(item.flavor().get(), stream);
      NS_ENSURE_SUCCESS(rv, rv);
      continue;
    }

    if (item.dataType() == TransferableDataType::CString) {
      nsCOMPtr<nsISupportsCString> dataWrapper =
          do_CreateInstance(NS_SUPPORTS_CSTRING_CONTRACTID, &rv);
      NS_ENSURE_SUCCESS(rv, rv);

      // The buffer contains the terminating null.
      Shmem itemData = item.data().get_Shmem();
      const nsDependentCSubstring text(itemData.get<char>(),
                                       itemData.Size<char>());
      rv = dataWrapper->SetData(text);
      NS_ENSURE_SUCCESS(rv, rv);

      rv = aTransferable->SetTransferData(item.flavor().get(), dataWrapper);
      NS_ENSURE_SUCCESS(rv, rv);
      continue;
    }

    MOZ_ASSERT_UNREACHABLE("Unknown transferable data type");
  }
  return NS_OK;
}

nsresult nsContentUtils::IPCTransferableToTransferable(
    const IPCDataTransfer& aDataTransfer, const bool& aIsPrivateData,
    nsIPrincipal* aRequestingPrincipal,
    const nsContentPolicyType& aContentPolicyType, bool aAddDataFlavor,
    nsITransferable* aTransferable, IShmemAllocator* aAllocator, bool aFilterUnknownFlavors) {
  aTransferable->SetIsPrivateData(aIsPrivateData);

  nsresult rv = IPCTransferableToTransferable(aDataTransfer, aAddDataFlavor,
                                              aTransferable, aAllocator, aFilterUnknownFlavors);
  NS_ENSURE_SUCCESS(rv, rv);

  aTransferable->SetRequestingPrincipal(aRequestingPrincipal);
  aTransferable->SetContentPolicyType(aContentPolicyType);
  return NS_OK;
}

nsresult nsContentUtils::IPCTransferableItemToVariant(
    const IPCDataTransferItem& aDataTransferItem, nsIWritableVariant* aVariant,
    IProtocol* aActor) {
  MOZ_ASSERT(aVariant);
  MOZ_ASSERT(aActor);

  auto release = MakeScopeExit([&] {
    if (aDataTransferItem.data().type() == IPCDataTransferData::TShmem) {
      aActor->DeallocShmem(aDataTransferItem.data().get_Shmem());
    }
  });

  if (aDataTransferItem.dataType() == TransferableDataType::String) {
    if (aDataTransferItem.data().type() == IPCDataTransferData::TShmem) {
      Shmem data = aDataTransferItem.data().get_Shmem();
      aVariant->SetAsAString(
          nsDependentSubstring(data.get<char16_t>(), data.Size<char16_t>()));
      return NS_OK;
    }

    const nsString& data = aDataTransferItem.data().get_nsString();
    aVariant->SetAsAString(data);
    return NS_OK;
  }

  if (aDataTransferItem.dataType() == TransferableDataType::ImageContainer) {
    // An image! Get the imgIContainer for it and set it in the
    // variant.
    nsCOMPtr<imgIContainer> imageContainer;
    nsresult rv = nsContentUtils::DataTransferItemToImage(
        aDataTransferItem, getter_AddRefs(imageContainer));
    if (NS_FAILED(rv)) {
      return rv;
    }
    aVariant->SetAsISupports(imageContainer);
    return NS_OK;
  }

  if (aDataTransferItem.dataType() == TransferableDataType::CString) {
    Shmem data = aDataTransferItem.data().get_Shmem();
    aVariant->SetAsACString(
        nsDependentCSubstring(data.get<char>(), data.Size<char>()));
    return NS_OK;
  }

  if (aDataTransferItem.dataType() == TransferableDataType::Blob) {
    RefPtr<BlobImpl> blobImpl =
        IPCBlobUtils::Deserialize(aDataTransferItem.data().get_IPCBlob());
    aVariant->SetAsISupports(blobImpl);
    return NS_OK;
  }

  MOZ_ASSERT_UNREACHABLE("Unknown transferable data type");
  return NS_ERROR_UNEXPECTED;
}

void nsContentUtils::TransferablesToIPCTransferables(
    nsIArray* aTransferables, nsTArray<IPCDataTransfer>& aIPC,
    bool aInSyncMessage, mozilla::dom::ContentChild* aChild,
    mozilla::dom::ContentParent* aParent) {
  aIPC.Clear();
  if (aTransferables) {
    uint32_t transferableCount = 0;
    aTransferables->GetLength(&transferableCount);
    for (uint32_t i = 0; i < transferableCount; ++i) {
      IPCDataTransfer* dt = aIPC.AppendElement();
      nsCOMPtr<nsITransferable> transferable =
          do_QueryElementAt(aTransferables, i);
      TransferableToIPCTransferable(transferable, dt, aInSyncMessage, aChild,
                                    aParent);
    }
  }
}

nsresult nsContentUtils::SlurpFileToString(nsIFile* aFile,
                                           nsACString& aString) {
  aString.Truncate();

  nsCOMPtr<nsIURI> fileURI;
  nsresult rv = NS_NewFileURI(getter_AddRefs(fileURI), aFile);
  if (NS_FAILED(rv)) {
    return rv;
  }

  nsCOMPtr<nsIChannel> channel;
  rv = NS_NewChannel(getter_AddRefs(channel), fileURI,
                     nsContentUtils::GetSystemPrincipal(),
                     nsILoadInfo::SEC_ALLOW_CROSS_ORIGIN_SEC_CONTEXT_IS_NULL,
                     nsIContentPolicy::TYPE_OTHER);
  if (NS_FAILED(rv)) {
    return rv;
  }

  nsCOMPtr<nsIInputStream> stream;
  rv = channel->Open(getter_AddRefs(stream));
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

bool nsContentUtils::IsFileImage(nsIFile* aFile, nsACString& aType) {
  nsCOMPtr<nsIMIMEService> mime = do_GetService("@mozilla.org/mime;1");
  if (!mime) {
    return false;
  }

  nsresult rv = mime->GetTypeFromFile(aFile, aType);
  if (NS_FAILED(rv)) {
    return false;
  }

  return StringBeginsWith(aType, "image/"_ns);
}

nsresult nsContentUtils::CalculateBufferSizeForImage(
    const uint32_t& aStride, const IntSize& aImageSize,
    const SurfaceFormat& aFormat, size_t* aMaxBufferSize,
    size_t* aUsedBufferSize) {
  CheckedInt32 requiredBytes =
      CheckedInt32(aStride) * CheckedInt32(aImageSize.height);

  CheckedInt32 usedBytes =
      requiredBytes - aStride +
      (CheckedInt32(aImageSize.width) * BytesPerPixel(aFormat));
  if (!usedBytes.isValid()) {
    return NS_ERROR_FAILURE;
  }

  MOZ_ASSERT(requiredBytes.isValid(), "usedBytes valid but not required?");
  *aMaxBufferSize = requiredBytes.value();
  *aUsedBufferSize = usedBytes.value();
  return NS_OK;
}

static already_AddRefed<DataSourceSurface> ShmemToDataSurface(
    Shmem& aData, uint32_t aStride, const IntSize& aImageSize,
    SurfaceFormat aFormat) {
  if (!aData.IsReadable() || !aImageSize.width || !aImageSize.height) {
    return nullptr;
  }

  // Validate shared memory buffer size
  size_t imageBufLen = 0;
  size_t maxBufLen = 0;
  if (NS_FAILED(nsContentUtils::CalculateBufferSizeForImage(
          aStride, aImageSize, aFormat, &maxBufLen, &imageBufLen))) {
    return nullptr;
  }
  if (imageBufLen > aData.Size<uint8_t>()) {
    return nullptr;
  }
  return CreateDataSourceSurfaceFromData(aImageSize, aFormat,
                                         aData.get<uint8_t>(), aStride);
}

nsresult nsContentUtils::DataTransferItemToImage(
    const IPCDataTransferItem& aItem, imgIContainer** aContainer) {
  MOZ_ASSERT(aItem.data().type() == IPCDataTransferData::TShmem);
  MOZ_ASSERT(aItem.dataType() == TransferableDataType::ImageContainer);
  MOZ_ASSERT(aItem.imageDetails().isSome());

  const IPCDataTransferImage& imageDetails = aItem.imageDetails().value();
  const IntSize size(imageDetails.width(), imageDetails.height());
  RefPtr<DataSourceSurface> image =
      ShmemToDataSurface(aItem.data().get_Shmem(), imageDetails.stride(), size,
                         imageDetails.format());
  if (!image) {
    return NS_ERROR_FAILURE;
  }

  RefPtr<gfxDrawable> drawable = new gfxSurfaceDrawable(image, size);
  nsCOMPtr<imgIContainer> imageContainer =
      image::ImageOps::CreateFromDrawable(drawable);
  imageContainer.forget(aContainer);

  return NS_OK;
}

bool nsContentUtils::IsFlavorImage(const nsACString& aFlavor) {
  return aFlavor.EqualsLiteral(kNativeImageMime) ||
         aFlavor.EqualsLiteral(kJPEGImageMime) ||
         aFlavor.EqualsLiteral(kJPGImageMime) ||
         aFlavor.EqualsLiteral(kPNGImageMime) ||
         aFlavor.EqualsLiteral(kGIFImageMime);
}

static bool AllocateShmem(mozilla::dom::ContentChild* aChild,
                          mozilla::dom::ContentParent* aParent, size_t aSize,
                          mozilla::ipc::Shmem* aShmem) {
  MOZ_ASSERT((aChild && !aParent) || (!aChild && aParent));
  MOZ_ASSERT(aShmem);

  IShmemAllocator* allocator = aChild ? static_cast<IShmemAllocator*>(aChild)
                                      : static_cast<IShmemAllocator*>(aParent);

  return allocator->AllocShmem(aSize, SharedMemory::TYPE_BASIC, aShmem);
}

static Shmem ConvertToShmem(mozilla::dom::ContentChild* aChild,
                            mozilla::dom::ContentParent* aParent,
                            const nsACString& aInput) {
  Shmem result;
  if (!AllocateShmem(aChild, aParent, aInput.Length(), &result)) {
    return result;
  }

  memcpy(result.get<char>(), aInput.BeginReading(), aInput.Length());

  return result;
}

static Shmem ConvertToShmem(mozilla::dom::ContentChild* aChild,
                            mozilla::dom::ContentParent* aParent,
                            const nsAString& aInput) {
  Shmem result;
  uint32_t size = aInput.Length() * sizeof(char16_t);
  if (!AllocateShmem(aChild, aParent, size, &result)) {
    return result;
  }

  memcpy(result.get<char>(), aInput.BeginReading(), size);

  return result;
}

void nsContentUtils::TransferableToIPCTransferable(
    nsITransferable* aTransferable, IPCDataTransfer* aIPCDataTransfer,
    bool aInSyncMessage, mozilla::dom::ContentChild* aChild,
    mozilla::dom::ContentParent* aParent) {
  MOZ_ASSERT((aChild && !aParent) || (!aChild && aParent));

  if (aTransferable) {
    nsTArray<nsCString> flavorList;
    aTransferable->FlavorsTransferableCanExport(flavorList);

    for (uint32_t j = 0; j < flavorList.Length(); ++j) {
      nsCString& flavorStr = flavorList[j];
      if (!flavorStr.Length()) {
        continue;
      }

      nsCOMPtr<nsISupports> data;
      nsresult rv =
          aTransferable->GetTransferData(flavorStr.get(), getter_AddRefs(data));

      if (NS_FAILED(rv) || !data) {
        if (aInSyncMessage) {
          // Can't do anything.
          continue;
        }

        // This is a hack to support kFilePromiseMime.
        // On Windows there just needs to be an entry for it,
        // and for OSX we need to create
        // nsContentAreaDragDropDataProvider as nsIFlavorDataProvider.
        if (flavorStr.EqualsLiteral(kFilePromiseMime)) {
          IPCDataTransferItem* item = aIPCDataTransfer->items().AppendElement();
          item->flavor() = flavorStr;
          item->data() = NS_ConvertUTF8toUTF16(flavorStr);
          item->dataType() = TransferableDataType::String;
          continue;
        }

        // Empty element, transfer only the flavor
        IPCDataTransferItem* item = aIPCDataTransfer->items().AppendElement();
        item->flavor() = flavorStr;
        item->data() = nsString();
        item->dataType() = TransferableDataType::String;
        continue;
      }

      if (nsCOMPtr<nsISupportsString> text = do_QueryInterface(data)) {
        nsAutoString dataAsString;
        text->GetData(dataAsString);

        Maybe<Shmem> dataAsShmem;
        uint32_t size = dataAsString.Length() * sizeof(char16_t);
        // XXX IPCDataTransfer could contain multiple items, we give each item
        // same bucket size. The IPC message includes more than data payload, so
        // subtract 10 KB to make the total size within the bucket size. It
        // would be nice if we could have a smarter way to decide when to use
        // Shmem.
        uint32_t threshold =
            (IPC::Channel::kMaximumMessageSize / flavorList.Length()) -
            (10 * 1024);
        if (size > threshold) {
          dataAsShmem.emplace(ConvertToShmem(aChild, aParent, dataAsString));
          if (!dataAsShmem->IsReadable() || !dataAsShmem->Size<char16_t>()) {
            continue;
          }
        }

        IPCDataTransferItem* item = aIPCDataTransfer->items().AppendElement();
        item->flavor() = flavorStr;
        if (dataAsShmem) {
          item->data() = dataAsShmem.value();
        } else {
          item->data() = dataAsString;
        }
        item->dataType() = TransferableDataType::String;
        continue;
      }

      // We need to handle nsIInputStream before nsISupportsCString, otherwise
      // nsStringInputStream would be coverted into a wrong type.
      if (nsCOMPtr<nsIInputStream> stream = do_QueryInterface(data)) {
        // Images to be pasted on the clipboard are nsIInputStreams
        nsCString imageData;
        NS_ConsumeStream(stream, UINT32_MAX, imageData);

        Shmem imageDataShmem = ConvertToShmem(aChild, aParent, imageData);
        if (!imageDataShmem.IsReadable() || !imageDataShmem.Size<char>()) {
          continue;
        }

        IPCDataTransferItem* item = aIPCDataTransfer->items().AppendElement();
        item->flavor() = flavorStr;
        item->data() = std::move(imageDataShmem);
        item->dataType() = TransferableDataType::InputStream;
        continue;
      }

      if (nsCOMPtr<nsISupportsCString> ctext = do_QueryInterface(data)) {
        nsAutoCString dataAsString;
        ctext->GetData(dataAsString);

        Shmem dataAsShmem = ConvertToShmem(aChild, aParent, dataAsString);
        if (!dataAsShmem.IsReadable() || !dataAsShmem.Size<char>()) {
          continue;
        }

        IPCDataTransferItem* item = aIPCDataTransfer->items().AppendElement();
        item->flavor() = flavorStr;
        item->data() = std::move(dataAsShmem);
        item->dataType() = TransferableDataType::CString;
        continue;
      }

      if (nsCOMPtr<imgIContainer> image = do_QueryInterface(data)) {
        // Images to be placed on the clipboard are imgIContainers.
        RefPtr<mozilla::gfx::SourceSurface> surface = image->GetFrame(
            imgIContainer::FRAME_CURRENT,
            imgIContainer::FLAG_SYNC_DECODE | imgIContainer::FLAG_ASYNC_NOTIFY);
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
        IShmemAllocator* allocator =
            aChild ? static_cast<IShmemAllocator*>(aChild)
                   : static_cast<IShmemAllocator*>(aParent);
        Maybe<Shmem> surfaceData =
            GetSurfaceData(*dataSurface, &length, &stride, allocator);

        if (surfaceData.isNothing()) {
          continue;
        }

        IPCDataTransferItem* item = aIPCDataTransfer->items().AppendElement();
        item->flavor() = flavorStr;
        // Turn item->data() into an nsCString prior to accessing it.
        item->data() = std::move(surfaceData.ref());
        item->dataType() = TransferableDataType::ImageContainer;

        mozilla::gfx::IntSize size = dataSurface->GetSize();
        item->imageDetails().emplace(size.width, size.height, stride,
                                     dataSurface->GetFormat());
        continue;
      }

      // Otherwise, handle this as a file.
      nsCOMPtr<BlobImpl> blobImpl;
      if (nsCOMPtr<nsIFile> file = do_QueryInterface(data)) {
        // If we can send this over as a blob, do so. Otherwise, we're
        // responding to a sync message and the child can't process the blob
        // constructor before processing our response, which would crash. In
        // that case, hope that the caller is nsClipboardProxy::GetData,
        // called from editor and send over images as raw data.
        if (aInSyncMessage) {
          nsAutoCString type;
          if (IsFileImage(file, type)) {
            nsAutoCString data;
            SlurpFileToString(file, data);

            Shmem dataAsShmem = ConvertToShmem(aChild, aParent, data);
            if (!dataAsShmem.IsReadable() || !dataAsShmem.Size<char>()) {
              continue;
            }

            IPCDataTransferItem* item =
                aIPCDataTransfer->items().AppendElement();
            item->flavor() = type;
            item->data() = std::move(dataAsShmem);
            item->dataType() = TransferableDataType::InputStream;
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
        item->dataType() = TransferableDataType::Blob;
      }
    }
  }
}

namespace {
// The default type used for calling GetSurfaceData(). Gets surface data as
// raw buffer.
struct GetSurfaceDataRawBuffer {
  using ReturnType = mozilla::UniquePtr<char[]>;
  using BufferType = char*;

  ReturnType Allocate(size_t aSize) { return ReturnType(new char[aSize]); }

  static BufferType GetBuffer(const ReturnType& aReturnValue) {
    return aReturnValue.get();
  }

  static ReturnType NullValue() { return ReturnType(); }
};

// The type used for calling GetSurfaceData() that allocates and writes to
// a shared memory buffer.
struct GetSurfaceDataShmem {
  using ReturnType = Maybe<Shmem>;
  using BufferType = char*;

  explicit GetSurfaceDataShmem(IShmemAllocator* aAllocator)
      : mAllocator(aAllocator) {}

  ReturnType Allocate(size_t aSize) {
    Shmem shmem;
    if (!mAllocator->AllocShmem(aSize, SharedMemory::TYPE_BASIC, &shmem)) {
      return Nothing();
    }

    return Some(shmem);
  }

  static BufferType GetBuffer(const ReturnType& aReturnValue) {
    return aReturnValue.isSome() ? aReturnValue.ref().get<char>() : nullptr;
  }

  static ReturnType NullValue() { return ReturnType(); }

 private:
  IShmemAllocator* mAllocator;
};

/*
 * Get the pixel data from the given source surface and return it as a buffer.
 * The length and stride will be assigned from the surface.
 */
template <typename GetSurfaceDataContext = GetSurfaceDataRawBuffer>
typename GetSurfaceDataContext::ReturnType GetSurfaceDataImpl(
    DataSourceSurface& aSurface, size_t* aLength, int32_t* aStride,
    GetSurfaceDataContext aContext = GetSurfaceDataContext()) {
  mozilla::gfx::DataSourceSurface::MappedSurface map;
  if (!aSurface.Map(mozilla::gfx::DataSourceSurface::MapType::READ, &map)) {
    return GetSurfaceDataContext::NullValue();
  }

  size_t bufLen = 0;
  size_t maxBufLen = 0;
  nsresult rv = nsContentUtils::CalculateBufferSizeForImage(
      map.mStride, aSurface.GetSize(), aSurface.GetFormat(), &maxBufLen,
      &bufLen);
  if (NS_FAILED(rv)) {
    aSurface.Unmap();
    return GetSurfaceDataContext::NullValue();
  }

  // nsDependentCString wants null-terminated string.
  typename GetSurfaceDataContext::ReturnType surfaceData =
      aContext.Allocate(maxBufLen + 1);
  if (GetSurfaceDataContext::GetBuffer(surfaceData)) {
    memcpy(GetSurfaceDataContext::GetBuffer(surfaceData),
           reinterpret_cast<char*>(map.mData), bufLen);
    memset(GetSurfaceDataContext::GetBuffer(surfaceData) + bufLen, 0,
           maxBufLen - bufLen + 1);
  }

  *aLength = maxBufLen;
  *aStride = map.mStride;

  aSurface.Unmap();
  return surfaceData;
}
}  // Anonymous namespace.

UniquePtr<char[]> nsContentUtils::GetSurfaceData(DataSourceSurface& aSurface,
                                                 size_t* aLength,
                                                 int32_t* aStride) {
  return GetSurfaceDataImpl(aSurface, aLength, aStride);
}

Maybe<Shmem> nsContentUtils::GetSurfaceData(DataSourceSurface& aSurface,
                                            size_t* aLength, int32_t* aStride,
                                            IShmemAllocator* aAllocator) {
  return GetSurfaceDataImpl(aSurface, aLength, aStride,
                            GetSurfaceDataShmem(aAllocator));
}

Maybe<ShmemImage> nsContentUtils::SurfaceToIPCImage(
    DataSourceSurface& aSurface, IShmemAllocator* aAllocator) {
  size_t len = 0;
  int32_t stride = 0;
  auto mem = GetSurfaceData(aSurface, &len, &stride, aAllocator);
  if (!mem) {
    return Nothing();
  }
  return Some(ShmemImage{*mem, uint32_t(stride), aSurface.GetFormat(),
                         ImageIntSize::FromUnknownSize(aSurface.GetSize())});
}

already_AddRefed<DataSourceSurface> nsContentUtils::IPCImageToSurface(
    ShmemImage&& aImage, IShmemAllocator* aAllocator) {
  auto release =
      MakeScopeExit([&] { aAllocator->DeallocShmem(aImage.data()); });
  return ShmemToDataSurface(aImage.data(), aImage.stride(),
                            aImage.size().ToUnknownSize(), aImage.format());
}

Modifiers nsContentUtils::GetWidgetModifiers(int32_t aModifiers) {
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

nsIWidget* nsContentUtils::GetWidget(PresShell* aPresShell, nsPoint* aOffset) {
  if (!aPresShell) {
    return nullptr;
  }
  nsIFrame* frame = aPresShell->GetRootFrame();
  if (!frame) {
    return nullptr;
  }
  return frame->GetView()->GetNearestWidget(aOffset);
}

int16_t nsContentUtils::GetButtonsFlagForButton(int32_t aButton) {
  switch (aButton) {
    case -1:
      return MouseButtonsFlag::eNoButtons;
    case MouseButton::ePrimary:
      return MouseButtonsFlag::ePrimaryFlag;
    case MouseButton::eMiddle:
      return MouseButtonsFlag::eMiddleFlag;
    case MouseButton::eSecondary:
      return MouseButtonsFlag::eSecondaryFlag;
    case 3:
      return MouseButtonsFlag::e4thFlag;
    case 4:
      return MouseButtonsFlag::e5thFlag;
    case MouseButton::eEraser:
      return MouseButtonsFlag::eEraserFlag;
    default:
      NS_ERROR("Button not known.");
      return 0;
  }
}

LayoutDeviceIntPoint nsContentUtils::ToWidgetPoint(
    const CSSPoint& aPoint, const nsPoint& aOffset,
    nsPresContext* aPresContext) {
  nsPoint layoutRelative = CSSPoint::ToAppUnits(aPoint) + aOffset;
  nsPoint visualRelative =
      ViewportUtils::LayoutToVisual(layoutRelative, aPresContext->PresShell());
  return LayoutDeviceIntPoint::FromAppUnitsRounded(
      visualRelative, aPresContext->AppUnitsPerDevPixel());
}

nsView* nsContentUtils::GetViewToDispatchEvent(nsPresContext* aPresContext,
                                               PresShell** aPresShell) {
  if (!aPresContext || !aPresShell) {
    return nullptr;
  }
  RefPtr<PresShell> presShell = aPresContext->PresShell();
  if (NS_WARN_IF(!presShell)) {
    *aPresShell = nullptr;
    return nullptr;
  }
  nsViewManager* viewManager = presShell->GetViewManager();
  if (!viewManager) {
    presShell.forget(aPresShell);  // XXX Is this intentional?
    return nullptr;
  }
  presShell.forget(aPresShell);
  return viewManager->GetRootView();
}

nsresult nsContentUtils::SendMouseEvent(
    mozilla::PresShell* aPresShell, const nsAString& aType, float aX, float aY,
    int32_t aButton, int32_t aButtons, int32_t aClickCount, int32_t aModifiers,
    bool aIgnoreRootScrollFrame, float aPressure,
    unsigned short aInputSourceArg, uint32_t aIdentifier, bool aToWindow,
    PreventDefaultResult* aPreventDefault, bool aIsDOMEventSynthesized,
    bool aIsWidgetEventSynthesized) {
  nsPoint offset;
  nsCOMPtr<nsIWidget> widget = GetWidget(aPresShell, &offset);
  if (!widget) return NS_ERROR_FAILURE;

  EventMessage msg;
  Maybe<WidgetMouseEvent::ExitFrom> exitFrom;
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
    exitFrom = Some(WidgetMouseEvent::ePlatformChild);
  } else if (aType.EqualsLiteral("mousecancel")) {
    msg = eMouseExitFromWidget;
    exitFrom = Some(XRE_IsParentProcess() ? WidgetMouseEvent::ePlatformTopLevel
                                          : WidgetMouseEvent::ePuppet);
  } else if (aType.EqualsLiteral("mouselongtap")) {
    msg = eMouseLongTap;
  } else if (aType.EqualsLiteral("contextmenu")) {
    msg = eContextMenu;
    contextMenuKey = (aButton == 0);
  } else if (aType.EqualsLiteral("MozMouseHittest")) {
    msg = eMouseHitTest;
  } else if (aType.EqualsLiteral("MozMouseExploreByTouch")) {
    msg = eMouseExploreByTouch;
  } else {
    return NS_ERROR_FAILURE;
  }

  if (aInputSourceArg == MouseEvent_Binding::MOZ_SOURCE_UNKNOWN) {
    aInputSourceArg = MouseEvent_Binding::MOZ_SOURCE_MOUSE;
  }

  WidgetMouseEvent event(true, msg, widget,
                         aIsWidgetEventSynthesized
                             ? WidgetMouseEvent::eSynthesized
                             : WidgetMouseEvent::eReal,
                         contextMenuKey ? WidgetMouseEvent::eContextMenuKey
                                        : WidgetMouseEvent::eNormal);
  event.pointerId = aIdentifier;
  event.mModifiers = GetWidgetModifiers(aModifiers);
  event.mButton = aButton;
  event.mButtons = aButtons != nsIDOMWindowUtils::MOUSE_BUTTONS_NOT_SPECIFIED
                       ? aButtons
                   : msg == eMouseUp ? 0
                                     : GetButtonsFlagForButton(aButton);
  event.mPressure = aPressure;
  event.mInputSource = aInputSourceArg;
  event.mClickCount = aClickCount;
  event.mTime = PR_IntervalNow();
  event.mFlags.mIsSynthesizedForTests = aIsDOMEventSynthesized;
  event.mExitFrom = exitFrom;

  nsPresContext* presContext = aPresShell->GetPresContext();
  if (!presContext) return NS_ERROR_FAILURE;

  event.mRefPoint = ToWidgetPoint(CSSPoint(aX, aY), offset, presContext);
  event.mIgnoreRootScrollFrame = aIgnoreRootScrollFrame;

  nsEventStatus status = nsEventStatus_eIgnore;
  if (aToWindow) {
    RefPtr<PresShell> presShell;
    nsView* view =
        GetViewToDispatchEvent(presContext, getter_AddRefs(presShell));
    if (!presShell || !view) {
      return NS_ERROR_FAILURE;
    }
    return presShell->HandleEvent(view->GetFrame(), &event, false, &status);
  }
  if (StaticPrefs::test_events_async_enabled()) {
    status = widget->DispatchInputEvent(&event).mContentStatus;
  } else {
    nsresult rv = widget->DispatchEvent(&event, status);
    NS_ENSURE_SUCCESS(rv, rv);
  }
  if (aPreventDefault) {
    if (status == nsEventStatus_eConsumeNoDefault) {
      if (event.mFlags.mDefaultPreventedByContent) {
        *aPreventDefault = PreventDefaultResult::ByContent;
      } else {
        *aPreventDefault = PreventDefaultResult::ByChrome;
      }
    } else {
      *aPreventDefault = PreventDefaultResult::No;
    }
  }

  return NS_OK;
}

/* static */
void nsContentUtils::FirePageHideEventForFrameLoaderSwap(
    nsIDocShellTreeItem* aItem, EventTarget* aChromeEventHandler,
    bool aOnlySystemGroup) {
  MOZ_DIAGNOSTIC_ASSERT(aItem);
  MOZ_DIAGNOSTIC_ASSERT(aChromeEventHandler);

  RefPtr<Document> doc = aItem->GetDocument();
  NS_ASSERTION(doc, "What happened here?");
  doc->OnPageHide(true, aChromeEventHandler, aOnlySystemGroup);

  int32_t childCount = 0;
  aItem->GetInProcessChildCount(&childCount);
  AutoTArray<nsCOMPtr<nsIDocShellTreeItem>, 8> kids;
  kids.AppendElements(childCount);
  for (int32_t i = 0; i < childCount; ++i) {
    aItem->GetInProcessChildAt(i, getter_AddRefs(kids[i]));
  }

  for (uint32_t i = 0; i < kids.Length(); ++i) {
    if (kids[i]) {
      FirePageHideEventForFrameLoaderSwap(kids[i], aChromeEventHandler,
                                          aOnlySystemGroup);
    }
  }
}

// The pageshow event is fired for a given document only if IsShowing() returns
// the same thing as aFireIfShowing.  This gives us a way to fire pageshow only
// on documents that are still loading or only on documents that are already
// loaded.
/* static */
void nsContentUtils::FirePageShowEventForFrameLoaderSwap(
    nsIDocShellTreeItem* aItem, EventTarget* aChromeEventHandler,
    bool aFireIfShowing, bool aOnlySystemGroup) {
  int32_t childCount = 0;
  aItem->GetInProcessChildCount(&childCount);
  AutoTArray<nsCOMPtr<nsIDocShellTreeItem>, 8> kids;
  kids.AppendElements(childCount);
  for (int32_t i = 0; i < childCount; ++i) {
    aItem->GetInProcessChildAt(i, getter_AddRefs(kids[i]));
  }

  for (uint32_t i = 0; i < kids.Length(); ++i) {
    if (kids[i]) {
      FirePageShowEventForFrameLoaderSwap(kids[i], aChromeEventHandler,
                                          aFireIfShowing, aOnlySystemGroup);
    }
  }

  RefPtr<Document> doc = aItem->GetDocument();
  NS_ASSERTION(doc, "What happened here?");
  if (doc->IsShowing() == aFireIfShowing) {
    doc->OnPageShow(true, aChromeEventHandler, aOnlySystemGroup);
  }
}

/* static */
already_AddRefed<nsPIWindowRoot> nsContentUtils::GetWindowRoot(Document* aDoc) {
  if (aDoc) {
    if (nsPIDOMWindowOuter* win = aDoc->GetWindow()) {
      return win->GetTopWindowRoot();
    }
  }
  return nullptr;
}

/* static */
bool nsContentUtils::IsPreloadType(nsContentPolicyType aType) {
  return (aType == nsIContentPolicy::TYPE_INTERNAL_SCRIPT_PRELOAD ||
          aType == nsIContentPolicy::TYPE_INTERNAL_MODULE_PRELOAD ||
          aType == nsIContentPolicy::TYPE_INTERNAL_IMAGE_PRELOAD ||
          aType == nsIContentPolicy::TYPE_INTERNAL_STYLESHEET_PRELOAD ||
          aType == nsIContentPolicy::TYPE_INTERNAL_FONT_PRELOAD ||
          aType == nsIContentPolicy::TYPE_INTERNAL_FETCH_PRELOAD);
}

/* static */
bool nsContentUtils::IsUpgradableDisplayType(ExtContentPolicyType aType) {
  MOZ_ASSERT(NS_IsMainThread());
  return (aType == ExtContentPolicy::TYPE_IMAGE ||
          aType == ExtContentPolicy::TYPE_MEDIA);
}

// static
ReferrerPolicy nsContentUtils::GetReferrerPolicyFromChannel(
    nsIChannel* aChannel) {
  nsCOMPtr<nsIHttpChannel> httpChannel = do_QueryInterface(aChannel);
  if (!httpChannel) {
    return ReferrerPolicy::_empty;
  }

  nsresult rv;
  nsAutoCString headerValue;
  rv = httpChannel->GetResponseHeader("referrer-policy"_ns, headerValue);
  if (NS_FAILED(rv) || headerValue.IsEmpty()) {
    return ReferrerPolicy::_empty;
  }

  return ReferrerInfo::ReferrerPolicyFromHeaderString(
      NS_ConvertUTF8toUTF16(headerValue));
}

// static
bool nsContentUtils::IsNonSubresourceRequest(nsIChannel* aChannel) {
  nsLoadFlags loadFlags = 0;
  aChannel->GetLoadFlags(&loadFlags);
  if (loadFlags & nsIChannel::LOAD_DOCUMENT_URI) {
    return true;
  }

  nsCOMPtr<nsILoadInfo> loadInfo = aChannel->LoadInfo();
  nsContentPolicyType type = loadInfo->InternalContentPolicyType();
  return IsNonSubresourceInternalPolicyType(type);
}

// static
bool nsContentUtils::IsNonSubresourceInternalPolicyType(
    nsContentPolicyType aType) {
  return aType == nsIContentPolicy::TYPE_DOCUMENT ||
         aType == nsIContentPolicy::TYPE_INTERNAL_IFRAME ||
         aType == nsIContentPolicy::TYPE_INTERNAL_FRAME ||
         aType == nsIContentPolicy::TYPE_INTERNAL_WORKER ||
         aType == nsIContentPolicy::TYPE_INTERNAL_SHARED_WORKER;
}

// static public
bool nsContentUtils::IsThirdPartyTrackingResourceWindow(
    nsPIDOMWindowInner* aWindow) {
  MOZ_ASSERT(aWindow);

  Document* document = aWindow->GetExtantDoc();
  if (!document) {
    return false;
  }

  nsCOMPtr<nsIClassifiedChannel> classifiedChannel =
      do_QueryInterface(document->GetChannel());
  if (!classifiedChannel) {
    return false;
  }

  return classifiedChannel->IsThirdPartyTrackingResource();
}

// static public
bool nsContentUtils::IsFirstPartyTrackingResourceWindow(
    nsPIDOMWindowInner* aWindow) {
  MOZ_ASSERT(aWindow);

  Document* document = aWindow->GetExtantDoc();
  if (!document) {
    return false;
  }

  nsCOMPtr<nsIClassifiedChannel> classifiedChannel =
      do_QueryInterface(document->GetChannel());
  if (!classifiedChannel) {
    return false;
  }

  uint32_t classificationFlags =
      classifiedChannel->GetFirstPartyClassificationFlags();

  return mozilla::net::UrlClassifierCommon::IsTrackingClassificationFlag(
      classificationFlags);
}

namespace {

// We put StringBuilder in the anonymous namespace to prevent anything outside
// this file from accidentally being linked against it.
class BulkAppender {
  using size_type = typename nsAString::size_type;

 public:
  explicit BulkAppender(BulkWriteHandle<char16_t>&& aHandle)
      : mHandle(std::move(aHandle)), mPosition(0) {}
  ~BulkAppender() = default;

  template <int N>
  void AppendLiteral(const char16_t (&aStr)[N]) {
    size_t len = N - 1;
    MOZ_ASSERT(mPosition + len <= mHandle.Length());
    memcpy(mHandle.Elements() + mPosition, aStr, len * sizeof(char16_t));
    mPosition += len;
  }

  void Append(Span<const char16_t> aStr) {
    size_t len = aStr.Length();
    MOZ_ASSERT(mPosition + len <= mHandle.Length());
    // Both mHandle.Elements() and aStr.Elements() are guaranteed
    // to be non-null (by the string implementation and by Span,
    // respectively), so not checking the pointers for null before
    // memcpy does not lead to UB even if len was zero.
    memcpy(mHandle.Elements() + mPosition, aStr.Elements(),
           len * sizeof(char16_t));
    mPosition += len;
  }

  void Append(Span<const char> aStr) {
    size_t len = aStr.Length();
    MOZ_ASSERT(mPosition + len <= mHandle.Length());
    ConvertLatin1toUtf16(aStr, mHandle.AsSpan().From(mPosition));
    mPosition += len;
  }

  void Finish() { mHandle.Finish(mPosition, false); }

 private:
  mozilla::BulkWriteHandle<char16_t> mHandle;
  size_type mPosition;
};

class StringBuilder {
 private:
  // Try to keep the size of StringBuilder close to a jemalloc bucket size.
  static const uint32_t STRING_BUFFER_UNITS = 1020;
  class Unit {
   public:
    Unit() : mAtom(nullptr), mType(eUnknown), mLength(0) {
      MOZ_COUNT_CTOR(StringBuilder::Unit);
    }
    ~Unit() {
      if (mType == eString || mType == eStringWithEncode) {
        delete mString;
      }
      MOZ_COUNT_DTOR(StringBuilder::Unit);
    }

    enum Type {
      eUnknown,
      eAtom,
      eString,
      eStringWithEncode,
      eLiteral,
      eTextFragment,
      eTextFragmentWithEncode,
    };

    union {
      nsAtom* mAtom;
      const char16_t* mLiteral;
      nsAutoString* mString;
      const nsTextFragment* mTextFragment;
    };
    Type mType;
    uint32_t mLength;
  };

 public:
  StringBuilder() : mLast(this), mLength(0) { MOZ_COUNT_CTOR(StringBuilder); }

  MOZ_COUNTED_DTOR(StringBuilder)

  void Append(nsAtom* aAtom) {
    Unit* u = AddUnit();
    u->mAtom = aAtom;
    u->mType = Unit::eAtom;
    uint32_t len = aAtom->GetLength();
    u->mLength = len;
    mLength += len;
  }

  template <int N>
  void Append(const char16_t (&aLiteral)[N]) {
    Unit* u = AddUnit();
    u->mLiteral = aLiteral;
    u->mType = Unit::eLiteral;
    uint32_t len = N - 1;
    u->mLength = len;
    mLength += len;
  }

  void Append(const nsAString& aString) {
    Unit* u = AddUnit();
    u->mString = new nsAutoString(aString);
    u->mType = Unit::eString;
    uint32_t len = aString.Length();
    u->mLength = len;
    mLength += len;
  }

  void Append(nsAutoString* aString) {
    Unit* u = AddUnit();
    u->mString = aString;
    u->mType = Unit::eString;
    uint32_t len = aString->Length();
    u->mLength = len;
    mLength += len;
  }

  void AppendWithAttrEncode(nsAutoString* aString, uint32_t aLen) {
    Unit* u = AddUnit();
    u->mString = aString;
    u->mType = Unit::eStringWithEncode;
    u->mLength = aLen;
    mLength += aLen;
  }

  void Append(const nsTextFragment* aTextFragment) {
    Unit* u = AddUnit();
    u->mTextFragment = aTextFragment;
    u->mType = Unit::eTextFragment;
    uint32_t len = aTextFragment->GetLength();
    u->mLength = len;
    mLength += len;
  }

  void AppendWithEncode(const nsTextFragment* aTextFragment, uint32_t aLen) {
    Unit* u = AddUnit();
    u->mTextFragment = aTextFragment;
    u->mType = Unit::eTextFragmentWithEncode;
    u->mLength = aLen;
    mLength += aLen;
  }

  bool ToString(nsAString& aOut) {
    if (!mLength.isValid()) {
      return false;
    }
    auto appenderOrErr = aOut.BulkWrite(mLength.value(), 0, true);
    if (appenderOrErr.isErr()) {
      return false;
    }

    BulkAppender appender{appenderOrErr.unwrap()};

    for (StringBuilder* current = this; current;
         current = current->mNext.get()) {
      uint32_t len = current->mUnits.Length();
      for (uint32_t i = 0; i < len; ++i) {
        Unit& u = current->mUnits[i];
        switch (u.mType) {
          case Unit::eAtom:
            appender.Append(*(u.mAtom));
            break;
          case Unit::eString:
            appender.Append(*(u.mString));
            break;
          case Unit::eStringWithEncode:
            EncodeAttrString(*(u.mString), appender);
            break;
          case Unit::eLiteral:
            appender.Append(Span(u.mLiteral, u.mLength));
            break;
          case Unit::eTextFragment:
            if (u.mTextFragment->Is2b()) {
              appender.Append(
                  Span(u.mTextFragment->Get2b(), u.mTextFragment->GetLength()));
            } else {
              appender.Append(
                  Span(u.mTextFragment->Get1b(), u.mTextFragment->GetLength()));
            }
            break;
          case Unit::eTextFragmentWithEncode:
            if (u.mTextFragment->Is2b()) {
              EncodeTextFragment(
                  Span(u.mTextFragment->Get2b(), u.mTextFragment->GetLength()),
                  appender);
            } else {
              EncodeTextFragment(
                  Span(u.mTextFragment->Get1b(), u.mTextFragment->GetLength()),
                  appender);
            }
            break;
          default:
            MOZ_CRASH("Unknown unit type?");
        }
      }
    }
    appender.Finish();
    return true;
  }

 private:
  Unit* AddUnit() {
    if (mLast->mUnits.Length() == STRING_BUFFER_UNITS) {
      new StringBuilder(this);
    }
    return mLast->mUnits.AppendElement();
  }

  explicit StringBuilder(StringBuilder* aFirst) : mLast(nullptr), mLength(0) {
    MOZ_COUNT_CTOR(StringBuilder);
    aFirst->mLast->mNext = WrapUnique(this);
    aFirst->mLast = this;
  }

  void EncodeAttrString(Span<const char16_t> aStr, BulkAppender& aAppender) {
    size_t flushedUntil = 0;
    size_t currentPosition = 0;
    for (char16_t c : aStr) {
      switch (c) {
        case '"':
          aAppender.Append(aStr.FromTo(flushedUntil, currentPosition));
          aAppender.AppendLiteral(u"&quot;");
          flushedUntil = currentPosition + 1;
          break;
        case '&':
          aAppender.Append(aStr.FromTo(flushedUntil, currentPosition));
          aAppender.AppendLiteral(u"&amp;");
          flushedUntil = currentPosition + 1;
          break;
        case 0x00A0:
          aAppender.Append(aStr.FromTo(flushedUntil, currentPosition));
          aAppender.AppendLiteral(u"&nbsp;");
          flushedUntil = currentPosition + 1;
          break;
        default:
          break;
      }
      currentPosition++;
    }
    if (currentPosition > flushedUntil) {
      aAppender.Append(aStr.FromTo(flushedUntil, currentPosition));
    }
  }

  template <class T>
  void EncodeTextFragment(Span<const T> aStr, BulkAppender& aAppender) {
    size_t flushedUntil = 0;
    size_t currentPosition = 0;
    for (T c : aStr) {
      switch (c) {
        case '<':
          aAppender.Append(aStr.FromTo(flushedUntil, currentPosition));
          aAppender.AppendLiteral(u"&lt;");
          flushedUntil = currentPosition + 1;
          break;
        case '>':
          aAppender.Append(aStr.FromTo(flushedUntil, currentPosition));
          aAppender.AppendLiteral(u"&gt;");
          flushedUntil = currentPosition + 1;
          break;
        case '&':
          aAppender.Append(aStr.FromTo(flushedUntil, currentPosition));
          aAppender.AppendLiteral(u"&amp;");
          flushedUntil = currentPosition + 1;
          break;
        case T(0xA0):
          aAppender.Append(aStr.FromTo(flushedUntil, currentPosition));
          aAppender.AppendLiteral(u"&nbsp;");
          flushedUntil = currentPosition + 1;
          break;
        default:
          break;
      }
      currentPosition++;
    }
    if (currentPosition > flushedUntil) {
      aAppender.Append(aStr.FromTo(flushedUntil, currentPosition));
    }
  }

  AutoTArray<Unit, STRING_BUFFER_UNITS> mUnits;
  mozilla::UniquePtr<StringBuilder> mNext;
  StringBuilder* mLast;
  // mLength is used only in the first StringBuilder object in the linked list.
  CheckedInt<uint32_t> mLength;
};

}  // namespace

static void AppendEncodedCharacters(const nsTextFragment* aText,
                                    StringBuilder& aBuilder) {
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

static void AppendEncodedAttributeValue(nsAutoString* aValue,
                                        StringBuilder& aBuilder) {
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

static void StartElement(Element* aContent, StringBuilder& aBuilder) {
  nsAtom* localName = aContent->NodeInfo()->NameAtom();
  int32_t tagNS = aContent->GetNameSpaceID();

  aBuilder.Append(u"<");
  if (aContent->IsHTMLElement() || aContent->IsSVGElement() ||
      aContent->IsMathMLElement()) {
    aBuilder.Append(localName);
  } else {
    aBuilder.Append(aContent->NodeName());
  }

  CustomElementData* ceData = aContent->GetCustomElementData();
  if (ceData) {
    nsAtom* isAttr = ceData->GetIs(aContent);
    if (isAttr && !aContent->HasAttr(kNameSpaceID_None, nsGkAtoms::is)) {
      aBuilder.Append(uR"( is=")");
      aBuilder.Append(nsDependentAtomString(isAttr));
      aBuilder.Append(uR"(")");
    }
  }

  int32_t count = aContent->GetAttrCount();
  for (int32_t i = 0; i < count; i++) {
    const nsAttrName* name = aContent->GetAttrNameAt(i);
    int32_t attNs = name->NamespaceID();
    nsAtom* attName = name->LocalName();

    // Filter out any attribute starting with [-|_]moz
    nsDependentAtomString attrNameStr(attName);
    if (StringBeginsWith(attrNameStr, u"_moz"_ns) ||
        StringBeginsWith(attrNameStr, u"-moz"_ns)) {
      continue;
    }

    auto* attValue = new nsAutoString();
    aContent->GetAttr(attNs, attName, *attValue);

    // Filter out special case of <br type="_moz*"> used by the editor.
    // Bug 16988.  Yuck.
    if (localName == nsGkAtoms::br && tagNS == kNameSpaceID_XHTML &&
        attName == nsGkAtoms::type && attNs == kNameSpaceID_None &&
        StringBeginsWith(*attValue, u"_moz"_ns)) {
      delete attValue;
      continue;
    }

    aBuilder.Append(u" ");

    if (MOZ_LIKELY(attNs == kNameSpaceID_None) ||
        (attNs == kNameSpaceID_XMLNS && attName == nsGkAtoms::xmlns)) {
      // Nothing else required
    } else if (attNs == kNameSpaceID_XML) {
      aBuilder.Append(u"xml:");
    } else if (attNs == kNameSpaceID_XMLNS) {
      aBuilder.Append(u"xmlns:");
    } else if (attNs == kNameSpaceID_XLink) {
      aBuilder.Append(u"xlink:");
    } else {
      nsAtom* prefix = name->GetPrefix();
      if (prefix) {
        aBuilder.Append(prefix);
        aBuilder.Append(u":");
      }
    }

    aBuilder.Append(attName);
    aBuilder.Append(uR"(=")");
    AppendEncodedAttributeValue(attValue, aBuilder);
    aBuilder.Append(uR"(")");
  }

  aBuilder.Append(u">");

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
          (fc->NodeType() == nsINode::TEXT_NODE ||
           fc->NodeType() == nsINode::CDATA_SECTION_NODE)) {
        const nsTextFragment* text = fc->GetText();
        if (text && text->GetLength() && text->CharAt(0) == char16_t('\n')) {
          aBuilder.Append("\n");
        }
      }
    }
  }*/
}

static inline bool ShouldEscape(nsIContent* aParent) {
  if (!aParent || !aParent->IsHTMLElement()) {
    return true;
  }

  static const nsAtom* nonEscapingElements[] = {
      nsGkAtoms::style,     nsGkAtoms::script,  nsGkAtoms::xmp,
      nsGkAtoms::iframe,    nsGkAtoms::noembed, nsGkAtoms::noframes,
      nsGkAtoms::plaintext, nsGkAtoms::noscript};
  static mozilla::BitBloomFilter<12, nsAtom> sFilter;
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
        if (MOZ_UNLIKELY(tag == nsGkAtoms::noscript) &&
            MOZ_UNLIKELY(!aParent->OwnerDoc()->IsScriptEnabled())) {
          return true;
        }
        return false;
      }
    }
  }
  return true;
}

static inline bool IsVoidTag(Element* aElement) {
  if (!aElement->IsHTMLElement()) {
    return false;
  }
  return FragmentOrElement::IsHTMLVoid(aElement->NodeInfo()->NameAtom());
}

bool nsContentUtils::SerializeNodeToMarkup(nsINode* aRoot,
                                           bool aDescendentsOnly,
                                           nsAString& aOut) {
  // If you pass in a DOCUMENT_NODE, you must pass aDescendentsOnly as true
  MOZ_ASSERT(aDescendentsOnly || aRoot->NodeType() != nsINode::DOCUMENT_NODE);

  nsINode* current =
      aDescendentsOnly ? aRoot->GetFirstChildOfTemplateOrNode() : aRoot;

  if (!current) {
    return true;
  }

  StringBuilder builder;
  nsIContent* next;
  while (true) {
    bool isVoid = false;
    switch (current->NodeType()) {
      case nsINode::ELEMENT_NODE: {
        Element* elem = current->AsElement();
        StartElement(elem, builder);
        isVoid = IsVoidTag(elem);
        if (!isVoid && (next = current->GetFirstChildOfTemplateOrNode())) {
          current = next;
          continue;
        }
        break;
      }

      case nsINode::TEXT_NODE:
      case nsINode::CDATA_SECTION_NODE: {
        const nsTextFragment* text = &current->AsText()->TextFragment();
        nsIContent* parent = current->GetParent();
        if (ShouldEscape(parent)) {
          AppendEncodedCharacters(text, builder);
        } else {
          builder.Append(text);
        }
        break;
      }

      case nsINode::COMMENT_NODE: {
        builder.Append(u"<!--");
        builder.Append(static_cast<nsIContent*>(current)->GetText());
        builder.Append(u"-->");
        break;
      }

      case nsINode::DOCUMENT_TYPE_NODE: {
        builder.Append(u"<!DOCTYPE ");
        builder.Append(current->NodeName());
        builder.Append(u">");
        break;
      }

      case nsINode::PROCESSING_INSTRUCTION_NODE: {
        builder.Append(u"<?");
        builder.Append(current->NodeName());
        builder.Append(u" ");
        builder.Append(static_cast<nsIContent*>(current)->GetText());
        builder.Append(u">");
        break;
      }
    }

    while (true) {
      if (!isVoid && current->NodeType() == nsINode::ELEMENT_NODE) {
        builder.Append(u"</");
        nsIContent* elem = static_cast<nsIContent*>(current);
        if (elem->IsHTMLElement() || elem->IsSVGElement() ||
            elem->IsMathMLElement()) {
          builder.Append(elem->NodeInfo()->NameAtom());
        } else {
          builder.Append(current->NodeName());
        }
        builder.Append(u">");
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
          current->NodeType() == nsINode::DOCUMENT_FRAGMENT_NODE) {
        DocumentFragment* frag = static_cast<DocumentFragment*>(current);
        nsIContent* fragHost = frag->GetHost();
        if (fragHost && fragHost->IsTemplateElement()) {
          current = fragHost;
        }
      }

      if (aDescendentsOnly && current == aRoot) {
        return builder.ToString(aOut);
      }
    }
  }
}

bool nsContentUtils::IsSpecificAboutPage(JSObject* aGlobal, const char* aUri) {
  // aUri must start with about: or this isn't the right function to be using.
  MOZ_ASSERT(strncmp(aUri, "about:", 6) == 0);

  // Make sure the global is a window
  MOZ_DIAGNOSTIC_ASSERT(JS_IsGlobalObject(aGlobal));
  nsGlobalWindowInner* win = xpc::WindowOrNull(aGlobal);
  if (!win) {
    return false;
  }

  nsCOMPtr<nsIPrincipal> principal = win->GetPrincipal();
  NS_ENSURE_TRUE(principal, false);

  // First check the scheme to avoid getting long specs in the common case.
  if (!principal->SchemeIs("about")) {
    return false;
  }

  nsAutoCString spec;
  principal->GetAsciiSpec(spec);

  return spec.EqualsASCII(aUri);
}

/* static */
void nsContentUtils::SetScrollbarsVisibility(nsIDocShell* aDocShell,
                                             bool aVisible) {
  if (!aDocShell) {
    return;
  }
  auto pref = aVisible ? ScrollbarPreference::Auto : ScrollbarPreference::Never;
  nsDocShell::Cast(aDocShell)->SetScrollbarPreference(pref);
}

/* static */
nsIDocShell* nsContentUtils::GetDocShellForEventTarget(EventTarget* aTarget) {
  if (!aTarget) {
    return nullptr;
  }

  nsCOMPtr<nsPIDOMWindowInner> innerWindow;
  if (nsCOMPtr<nsINode> node = nsINode::FromEventTarget(aTarget)) {
    bool ignore;
    innerWindow =
        do_QueryInterface(node->OwnerDoc()->GetScriptHandlingObject(ignore));
  } else if ((innerWindow = nsPIDOMWindowInner::FromEventTarget(aTarget))) {
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
/* static */
bool nsContentUtils::HttpsStateIsModern(Document* aDocument) {
  if (!aDocument) {
    return false;
  }

  nsCOMPtr<nsIPrincipal> principal = aDocument->NodePrincipal();

  if (principal->IsSystemPrincipal()) {
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
      nsresult rv = ssm->GetChannelResultPrincipalIfNotSandboxed(
          channel, getter_AddRefs(principal));
      if (NS_FAILED(rv)) {
        return false;
      }
      if (principal->IsSystemPrincipal()) {
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

  MOZ_ASSERT(principal->GetIsContentPrincipal());

  return principal->GetIsOriginPotentiallyTrustworthy();
}

/* static */
bool nsContentUtils::ComputeIsSecureContext(nsIChannel* aChannel) {
  MOZ_ASSERT(aChannel);

  nsCOMPtr<nsIScriptSecurityManager> ssm = nsContentUtils::GetSecurityManager();
  nsCOMPtr<nsIPrincipal> principal;
  nsresult rv = ssm->GetChannelResultPrincipalIfNotSandboxed(
      aChannel, getter_AddRefs(principal));
  if (NS_FAILED(rv)) {
    return false;
  }

  const RefPtr<nsILoadInfo> loadInfo = aChannel->LoadInfo();

  if (principal->IsSystemPrincipal()) {
    // If the load would've been sandboxed, treat this load as an untrusted
    // load, as system code considers sandboxed resources insecure.
    return !loadInfo->GetLoadingSandboxed();
  }

  if (principal->GetIsNullPrincipal()) {
    return false;
  }

  if (const RefPtr<WindowContext> windowContext =
          WindowContext::GetById(loadInfo->GetInnerWindowID())) {
    if (!windowContext->GetIsSecureContext()) {
      return false;
    }
  }

  return principal->GetIsOriginPotentiallyTrustworthy();
}

/* static */
void nsContentUtils::TryToUpgradeElement(Element* aElement) {
  NodeInfo* nodeInfo = aElement->NodeInfo();
  RefPtr<nsAtom> typeAtom =
      aElement->GetCustomElementData()->GetCustomElementType();

  MOZ_ASSERT(nodeInfo->NameAtom()->Equals(nodeInfo->LocalName()));
  CustomElementDefinition* definition =
      nsContentUtils::LookupCustomElementDefinition(
          nodeInfo->GetDocument(), nodeInfo->NameAtom(),
          nodeInfo->NamespaceID(), typeAtom);
  if (definition) {
    nsContentUtils::EnqueueUpgradeReaction(aElement, definition);
  } else {
    // Add an unresolved custom element that is a candidate for upgrade when a
    // custom element is connected to the document.
    nsContentUtils::RegisterUnresolvedElement(aElement, typeAtom);
  }
}

MOZ_CAN_RUN_SCRIPT
static void DoCustomElementCreate(Element** aElement, JSContext* aCx,
                                  Document* aDoc, NodeInfo* aNodeInfo,
                                  CustomElementConstructor* aConstructor,
                                  ErrorResult& aRv) {
  JS::Rooted<JS::Value> constructResult(aCx);
  aConstructor->Construct(&constructResult, aRv, "Custom Element Create",
                          CallbackFunction::eRethrowExceptions);
  if (aRv.Failed()) {
    return;
  }

  RefPtr<Element> element;
  // constructResult is an ObjectValue because construction with a callback
  // always forms the return value from a JSObject.
  UNWRAP_OBJECT(Element, &constructResult, element);
  if (aNodeInfo->NamespaceEquals(kNameSpaceID_XHTML)) {
    if (!element || !element->IsHTMLElement()) {
      aRv.ThrowTypeError<MSG_DOES_NOT_IMPLEMENT_INTERFACE>("\"this\"",
                                                           "HTMLElement");
      return;
    }
  } else {
    if (!element || !element->IsXULElement()) {
      aRv.ThrowTypeError<MSG_DOES_NOT_IMPLEMENT_INTERFACE>("\"this\"",
                                                           "XULElement");
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

/* static */
nsresult nsContentUtils::NewXULOrHTMLElement(
    Element** aResult, mozilla::dom::NodeInfo* aNodeInfo,
    FromParser aFromParser, nsAtom* aIsAtom,
    mozilla::dom::CustomElementDefinition* aDefinition) {
  RefPtr<mozilla::dom::NodeInfo> nodeInfo = aNodeInfo;
  MOZ_ASSERT(nodeInfo->NamespaceEquals(kNameSpaceID_XHTML) ||
                 nodeInfo->NamespaceEquals(kNameSpaceID_XUL),
             "Can only create XUL or XHTML elements.");

  nsAtom* name = nodeInfo->NameAtom();
  int32_t tag = eHTMLTag_unknown;
  bool isCustomElementName = false;
  if (nodeInfo->NamespaceEquals(kNameSpaceID_XHTML)) {
    tag = nsHTMLTags::CaseSensitiveAtomTagToId(name);
    isCustomElementName =
        (tag == eHTMLTag_userdefined &&
         nsContentUtils::IsCustomElementName(name, kNameSpaceID_XHTML));
  } else {  // kNameSpaceID_XUL
    if (aIsAtom) {
      // Make sure the customized built-in element to be constructed confirms
      // to our naming requirement, i.e. [is] must be a dashed name and
      // the tag name must not.
      // if so, set isCustomElementName to false to kick off all the logics
      // that pick up aIsAtom.
      if (nsContentUtils::IsNameWithDash(aIsAtom) &&
          !nsContentUtils::IsNameWithDash(name)) {
        isCustomElementName = false;
      } else {
        isCustomElementName =
            nsContentUtils::IsCustomElementName(name, kNameSpaceID_XUL);
      }
    } else {
      isCustomElementName =
          nsContentUtils::IsCustomElementName(name, kNameSpaceID_XUL);
    }
  }

  nsAtom* tagAtom = nodeInfo->NameAtom();
  nsAtom* typeAtom = nullptr;
  bool isCustomElement = isCustomElementName || aIsAtom;
  if (isCustomElement) {
    typeAtom = isCustomElementName ? tagAtom : aIsAtom;
  }

  MOZ_ASSERT_IF(aDefinition, isCustomElement);

  // https://dom.spec.whatwg.org/#concept-create-element
  // We only handle the "synchronous custom elements flag is set" now.
  // For the unset case (e.g. cloning a node), see bug 1319342 for that.
  // Step 4.
  RefPtr<CustomElementDefinition> definition = aDefinition;
  if (isCustomElement && !definition) {
    MOZ_ASSERT(nodeInfo->NameAtom()->Equals(nodeInfo->LocalName()));
    definition = nsContentUtils::LookupCustomElementDefinition(
        nodeInfo->GetDocument(), nodeInfo->NameAtom(), nodeInfo->NamespaceID(),
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
    bool synchronousCustomElements = aFromParser != dom::FROM_PARSER_FRAGMENT;
    // Per discussion in https://github.com/w3c/webcomponents/issues/635,
    // use entry global in those places that are called from JS APIs and use the
    // node document's global object if it is called from parser.
    nsIGlobalObject* global;
    if (aFromParser == dom::NOT_FROM_PARSER) {
      global = GetEntryGlobal();

      // Documents created from the PrototypeDocumentSink always use
      // NOT_FROM_PARSER for non-XUL elements. We can get the global from the
      // document in that case.
      if (!global) {
        Document* doc = nodeInfo->GetDocument();
        if (doc && doc->LoadedFromPrototype()) {
          global = doc->GetScopeObject();
        }
      }
    } else {
      global = nodeInfo->GetDocument()->GetScopeObject();
    }
    if (!global) {
      // In browser chrome code, one may have access to a document which doesn't
      // have scope object anymore.
      return NS_ERROR_FAILURE;
    }

    AutoAllowLegacyScriptExecution exemption;
    AutoEntryScript aes(global, "create custom elements");
    JSContext* cx = aes.cx();
    ErrorResult rv;

    // Step 5.
    if (definition->IsCustomBuiltIn()) {
      // SetupCustomElement() should be called with an element that don't have
      // CustomElementData setup, if not we will hit the assertion in
      // SetCustomElementData().
      // Built-in element
      if (nodeInfo->NamespaceEquals(kNameSpaceID_XHTML)) {
        *aResult =
            CreateHTMLElement(tag, nodeInfo.forget(), aFromParser).take();
      } else {
        NS_IF_ADDREF(*aResult = nsXULElement::Construct(nodeInfo.forget()));
      }
      (*aResult)->SetCustomElementData(MakeUnique<CustomElementData>(typeAtom));
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
      definition->mPrefixStack.AppendElement(nodeInfo->GetPrefixAtom());
      RefPtr<Document> doc = nodeInfo->GetDocument();
      DoCustomElementCreate(aResult, cx, doc, nodeInfo,
                            MOZ_KnownLive(definition->mConstructor), rv);
      if (rv.MaybeSetPendingException(cx)) {
        if (nodeInfo->NamespaceEquals(kNameSpaceID_XHTML)) {
          NS_IF_ADDREF(*aResult = NS_NewHTMLUnknownElement(nodeInfo.forget(),
                                                           aFromParser));
        } else {
          NS_IF_ADDREF(*aResult = nsXULElement::Construct(nodeInfo.forget()));
        }
        (*aResult)->SetDefined(false);
      }
      definition->mPrefixStack.RemoveLastElement();
      return NS_OK;
    }

    // Step 6.2.
    if (nodeInfo->NamespaceEquals(kNameSpaceID_XHTML)) {
      NS_IF_ADDREF(*aResult =
                       NS_NewHTMLElement(nodeInfo.forget(), aFromParser));
    } else {
      NS_IF_ADDREF(*aResult = nsXULElement::Construct(nodeInfo.forget()));
    }
    (*aResult)->SetCustomElementData(
        MakeUnique<CustomElementData>(definition->mType));
    nsContentUtils::EnqueueUpgradeReaction(*aResult, definition);
    return NS_OK;
  }

  if (nodeInfo->NamespaceEquals(kNameSpaceID_XHTML)) {
    // Per the Custom Element specification, unknown tags that are valid custom
    // element names should be HTMLElement instead of HTMLUnknownElement.
    if (isCustomElementName) {
      NS_IF_ADDREF(*aResult =
                       NS_NewHTMLElement(nodeInfo.forget(), aFromParser));
    } else {
      *aResult = CreateHTMLElement(tag, nodeInfo.forget(), aFromParser).take();
    }
  } else {
    NS_IF_ADDREF(*aResult = nsXULElement::Construct(nodeInfo.forget()));
  }

  if (!*aResult) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  if (isCustomElement) {
    (*aResult)->SetCustomElementData(MakeUnique<CustomElementData>(typeAtom));
    nsContentUtils::RegisterCallbackUpgradeElement(*aResult, typeAtom);
  }

  return NS_OK;
}

CustomElementRegistry* nsContentUtils::GetCustomElementRegistry(
    Document* aDoc) {
  MOZ_ASSERT(aDoc);

  if (!aDoc->GetDocShell()) {
    return nullptr;
  }

  nsPIDOMWindowInner* window = aDoc->GetInnerWindow();
  if (!window) {
    return nullptr;
  }

  return window->CustomElements();
}

/* static */
CustomElementDefinition* nsContentUtils::LookupCustomElementDefinition(
    Document* aDoc, nsAtom* aNameAtom, uint32_t aNameSpaceID,
    nsAtom* aTypeAtom) {
  if (aNameSpaceID != kNameSpaceID_XUL && aNameSpaceID != kNameSpaceID_XHTML) {
    return nullptr;
  }

  RefPtr<CustomElementRegistry> registry = GetCustomElementRegistry(aDoc);
  if (!registry) {
    return nullptr;
  }

  return registry->LookupCustomElementDefinition(aNameAtom, aNameSpaceID,
                                                 aTypeAtom);
}

/* static */
void nsContentUtils::RegisterCallbackUpgradeElement(Element* aElement,
                                                    nsAtom* aTypeName) {
  MOZ_ASSERT(aElement);

  Document* doc = aElement->OwnerDoc();
  CustomElementRegistry* registry = GetCustomElementRegistry(doc);
  if (registry) {
    registry->RegisterCallbackUpgradeElement(aElement, aTypeName);
  }
}

/* static */
void nsContentUtils::RegisterUnresolvedElement(Element* aElement,
                                               nsAtom* aTypeName) {
  MOZ_ASSERT(aElement);

  Document* doc = aElement->OwnerDoc();
  CustomElementRegistry* registry = GetCustomElementRegistry(doc);
  if (registry) {
    registry->RegisterUnresolvedElement(aElement, aTypeName);
  }
}

/* static */
void nsContentUtils::UnregisterUnresolvedElement(Element* aElement) {
  MOZ_ASSERT(aElement);

  nsAtom* typeAtom = aElement->GetCustomElementData()->GetCustomElementType();
  Document* doc = aElement->OwnerDoc();
  CustomElementRegistry* registry = GetCustomElementRegistry(doc);
  if (registry) {
    registry->UnregisterUnresolvedElement(aElement, typeAtom);
  }
}

/* static */
void nsContentUtils::EnqueueUpgradeReaction(
    Element* aElement, CustomElementDefinition* aDefinition) {
  MOZ_ASSERT(aElement);

  Document* doc = aElement->OwnerDoc();

  // No DocGroup means no custom element reactions stack.
  if (!doc->GetDocGroup()) {
    return;
  }

  CustomElementReactionsStack* stack =
      doc->GetDocGroup()->CustomElementReactionsStack();
  stack->EnqueueUpgradeReaction(aElement, aDefinition);
}

/* static */
void nsContentUtils::EnqueueLifecycleCallback(
    ElementCallbackType aType, Element* aCustomElement,
    const LifecycleCallbackArgs& aArgs, CustomElementDefinition* aDefinition) {
  // No DocGroup means no custom element reactions stack.
  if (!aCustomElement->OwnerDoc()->GetDocGroup()) {
    return;
  }

  CustomElementRegistry::EnqueueLifecycleCallback(aType, aCustomElement, aArgs,
                                                  aDefinition);
}

/* static */
void nsContentUtils::AppendDocumentLevelNativeAnonymousContentTo(
    Document* aDocument, nsTArray<nsIContent*>& aElements) {
  MOZ_ASSERT(aDocument);
#ifdef DEBUG
  size_t oldLength = aElements.Length();
#endif

  if (PresShell* presShell = aDocument->GetPresShell()) {
    if (nsIFrame* scrollFrame = presShell->GetRootScrollFrame()) {
      nsIAnonymousContentCreator* creator = do_QueryFrame(scrollFrame);
      MOZ_ASSERT(
          creator,
          "scroll frame should always implement nsIAnonymousContentCreator");
      creator->AppendAnonymousContentTo(aElements, 0);
    }
    if (nsCanvasFrame* canvasFrame = presShell->GetCanvasFrame()) {
      canvasFrame->AppendAnonymousContentTo(aElements, 0);
    }
  }

#ifdef DEBUG
  for (size_t i = oldLength; i < aElements.Length(); i++) {
    MOZ_ASSERT(
        aElements[i]->GetProperty(nsGkAtoms::docLevelNativeAnonymousContent),
        "Someone here has lied, or missed to flag the node");
  }
#endif
}

static void AppendNativeAnonymousChildrenFromFrame(nsIFrame* aFrame,
                                                   nsTArray<nsIContent*>& aKids,
                                                   uint32_t aFlags) {
  if (nsIAnonymousContentCreator* ac = do_QueryFrame(aFrame)) {
    ac->AppendAnonymousContentTo(aKids, aFlags);
  }
}

/* static */
void nsContentUtils::AppendNativeAnonymousChildren(const nsIContent* aContent,
                                                   nsTArray<nsIContent*>& aKids,
                                                   uint32_t aFlags) {
  if (aContent->MayHaveAnonymousChildren()) {
    if (nsIFrame* primaryFrame = aContent->GetPrimaryFrame()) {
      // NAC created by the element's primary frame.
      AppendNativeAnonymousChildrenFromFrame(primaryFrame, aKids, aFlags);

      // NAC created by any other non-primary frames for the element.
      AutoTArray<nsIFrame::OwnedAnonBox, 8> ownedAnonBoxes;
      primaryFrame->AppendOwnedAnonBoxes(ownedAnonBoxes);
      for (nsIFrame::OwnedAnonBox& box : ownedAnonBoxes) {
        MOZ_ASSERT(box.mAnonBoxFrame->GetContent() == aContent);
        AppendNativeAnonymousChildrenFromFrame(box.mAnonBoxFrame, aKids,
                                               aFlags);
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

bool nsContentUtils::IsImageAvailable(nsIContent* aLoadingNode, nsIURI* aURI,
                                      nsIPrincipal* aDefaultTriggeringPrincipal,
                                      CORSMode aCORSMode) {
  nsCOMPtr<nsIPrincipal> triggeringPrincipal;
  QueryTriggeringPrincipal(aLoadingNode, aDefaultTriggeringPrincipal,
                           getter_AddRefs(triggeringPrincipal));
  MOZ_ASSERT(triggeringPrincipal);

  Document* doc = aLoadingNode->OwnerDoc();
  imgLoader* imgLoader = GetImgLoaderForDocument(doc);
  return imgLoader->IsImageAvailable(aURI, triggeringPrincipal, aCORSMode, doc);
}

/* static */
bool nsContentUtils::QueryTriggeringPrincipal(
    nsIContent* aLoadingNode, nsIPrincipal* aDefaultPrincipal,
    nsIPrincipal** aTriggeringPrincipal) {
  MOZ_ASSERT(aLoadingNode);
  MOZ_ASSERT(aTriggeringPrincipal);

  bool result = false;
  nsCOMPtr<nsIPrincipal> loadingPrincipal = aDefaultPrincipal;
  if (!loadingPrincipal) {
    loadingPrincipal = aLoadingNode->NodePrincipal();
  }

  // If aLoadingNode is content, bail out early.
  if (!aLoadingNode->NodePrincipal()->IsSystemPrincipal()) {
    loadingPrincipal.forget(aTriggeringPrincipal);
    return result;
  }

  nsAutoString loadingStr;
  if (aLoadingNode->IsElement()) {
    aLoadingNode->AsElement()->GetAttr(
        kNameSpaceID_None, nsGkAtoms::triggeringprincipal, loadingStr);
  }

  // Fall back if 'triggeringprincipal' isn't specified,
  if (loadingStr.IsEmpty()) {
    loadingPrincipal.forget(aTriggeringPrincipal);
    return result;
  }

  nsCString binary;
  nsresult rv = Base64Decode(NS_ConvertUTF16toUTF8(loadingStr), binary);
  if (NS_SUCCEEDED(rv)) {
    nsCOMPtr<nsIPrincipal> serializedPrin = BasePrincipal::FromJSON(binary);
    if (serializedPrin) {
      result = true;
      serializedPrin.forget(aTriggeringPrincipal);
    }
  } else {
    MOZ_ASSERT(false, "Unable to deserialize base64 principal");
  }

  if (!result) {
    // Fallback if the deserialization is failed.
    loadingPrincipal.forget(aTriggeringPrincipal);
  }

  return result;
}

/* static */
void nsContentUtils::GetContentPolicyTypeForUIImageLoading(
    nsIContent* aLoadingNode, nsIPrincipal** aTriggeringPrincipal,
    nsContentPolicyType& aContentPolicyType, uint64_t* aRequestContextID) {
  MOZ_ASSERT(aRequestContextID);

  bool result = QueryTriggeringPrincipal(aLoadingNode, aTriggeringPrincipal);
  if (result) {
    // Set the content policy type to TYPE_INTERNAL_IMAGE_FAVICON for
    // indicating it's a favicon loading.
    aContentPolicyType = nsIContentPolicy::TYPE_INTERNAL_IMAGE_FAVICON;

    nsAutoString requestContextID;
    if (aLoadingNode->IsElement()) {
      aLoadingNode->AsElement()->GetAttr(
          kNameSpaceID_None, nsGkAtoms::requestcontextid, requestContextID);
    }
    nsresult rv;
    int64_t val = requestContextID.ToInteger64(&rv);
    *aRequestContextID = NS_SUCCEEDED(rv) ? val : 0;
  } else {
    aContentPolicyType = nsIContentPolicy::TYPE_INTERNAL_IMAGE;
  }
}

/* static */
nsresult nsContentUtils::CreateJSValueFromSequenceOfObject(
    JSContext* aCx, const Sequence<JSObject*>& aTransfer,
    JS::MutableHandle<JS::Value> aValue) {
  if (aTransfer.IsEmpty()) {
    return NS_OK;
  }

  JS::Rooted<JSObject*> array(aCx, JS::NewArrayObject(aCx, aTransfer.Length()));
  if (!array) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  for (uint32_t i = 0; i < aTransfer.Length(); ++i) {
    JS::Rooted<JSObject*> object(aCx, aTransfer[i]);
    if (!object) {
      continue;
    }

    if (NS_WARN_IF(
            !JS_DefineElement(aCx, array, i, object, JSPROP_ENUMERATE))) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
  }

  aValue.setObject(*array);
  return NS_OK;
}

/* static */
void nsContentUtils::StructuredClone(JSContext* aCx, nsIGlobalObject* aGlobal,
                                     JS::Handle<JS::Value> aValue,
                                     const StructuredSerializeOptions& aOptions,
                                     JS::MutableHandle<JS::Value> aRetval,
                                     ErrorResult& aError) {
  JS::Rooted<JS::Value> transferArray(aCx, JS::UndefinedValue());
  aError = nsContentUtils::CreateJSValueFromSequenceOfObject(
      aCx, aOptions.mTransfer, &transferArray);
  if (NS_WARN_IF(aError.Failed())) {
    return;
  }

  JS::CloneDataPolicy clonePolicy;
  clonePolicy.allowIntraClusterClonableSharedObjects();
  clonePolicy.allowSharedMemoryObjects();

  StructuredCloneHolder holder(StructuredCloneHolder::CloningSupported,
                               StructuredCloneHolder::TransferringSupported,
                               JS::StructuredCloneScope::SameProcess);
  holder.Write(aCx, aValue, transferArray, clonePolicy, aError);
  if (NS_WARN_IF(aError.Failed())) {
    return;
  }

  holder.Read(aGlobal, aCx, aRetval, clonePolicy, aError);
  if (NS_WARN_IF(aError.Failed())) {
    return;
  }

  nsTArray<RefPtr<MessagePort>> ports = holder.TakeTransferredPorts();
  Unused << ports;
}

/* static */
bool nsContentUtils::ShouldBlockReservedKeys(WidgetKeyboardEvent* aKeyEvent) {
  nsCOMPtr<nsIPrincipal> principal;
  RefPtr<Element> targetElement =
      Element::FromEventTargetOrNull(aKeyEvent->mOriginalTarget);
  nsCOMPtr<nsIBrowser> targetBrowser;
  if (targetElement) {
    targetBrowser = targetElement->AsBrowser();
  }
  bool isRemoteBrowser = false;
  if (targetBrowser) {
    targetBrowser->GetIsRemoteBrowser(&isRemoteBrowser);
  }

  if (isRemoteBrowser) {
    targetBrowser->GetContentPrincipal(getter_AddRefs(principal));
    return principal ? nsContentUtils::IsSitePermDeny(principal, "shortcuts"_ns)
                     : false;
  }

  if (targetElement) {
    Document* doc = targetElement->GetUncomposedDoc();
    if (doc) {
      RefPtr<WindowContext> wc = doc->GetWindowContext();
      if (wc) {
        return wc->TopWindowContext()->GetShortcutsPermission() ==
               nsIPermissionManager::DENY_ACTION;
      }
    }
  }

  return false;
}

/**
 * Checks whether the given type is a supported document type for
 * loading within the nsObjectLoadingContent specified by aContent.
 *
 * NOTE Helper method for nsContentUtils::HtmlObjectContentTypeForMIMEType.
 * NOTE Does not take content policy or capabilities into account
 */
static bool HtmlObjectContentSupportsDocument(const nsCString& aMimeType) {
  nsCOMPtr<nsIWebNavigationInfo> info(
      do_GetService(NS_WEBNAVIGATION_INFO_CONTRACTID));
  if (!info) {
    return false;
  }

  uint32_t supported;
  nsresult rv = info->IsTypeSupported(aMimeType, &supported);

  if (NS_FAILED(rv)) {
    return false;
  }

  if (supported != nsIWebNavigationInfo::UNSUPPORTED) {
    // Don't want to support plugins as documents
    return supported != nsIWebNavigationInfo::FALLBACK;
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
already_AddRefed<nsIPluginTag> nsContentUtils::PluginTagForType(
    const nsCString& aMIMEType, bool aNoFakePlugin) {
  RefPtr<nsPluginHost> pluginHost = nsPluginHost::GetInst();
  nsCOMPtr<nsIPluginTag> tag;
  NS_ENSURE_TRUE(pluginHost, nullptr);

  // ShouldPlay will handle the case where the plugin is disabled
  pluginHost->GetPluginTagForType(
      aMIMEType,
      aNoFakePlugin ? nsPluginHost::eExcludeFake : nsPluginHost::eExcludeNone,
      getter_AddRefs(tag));

  return tag.forget();
}

/* static */
uint32_t nsContentUtils::HtmlObjectContentTypeForMIMEType(
    const nsCString& aMIMEType, bool aNoFakePlugin) {
  if (aMIMEType.IsEmpty()) {
    return nsIObjectLoadingContent::TYPE_NULL;
  }

  if (imgLoader::SupportImageWithMimeType(aMIMEType)) {
    return nsIObjectLoadingContent::TYPE_IMAGE;
  }

  // Faking support of the PDF content as a document for EMBED tags
  // when internal PDF viewer is enabled.
  if (aMIMEType.LowerCaseEqualsLiteral("application/pdf") && IsPDFJSEnabled()) {
    return nsIObjectLoadingContent::TYPE_DOCUMENT;
  }

  if (HtmlObjectContentSupportsDocument(aMIMEType)) {
    return nsIObjectLoadingContent::TYPE_DOCUMENT;
  }

  bool isSpecialPlugin = nsPluginHost::GetSpecialType(aMIMEType) !=
                         nsPluginHost::eSpecialType_None;
  if (isSpecialPlugin) {
    return nsIObjectLoadingContent::TYPE_FALLBACK;
  }

  return nsIObjectLoadingContent::TYPE_NULL;
}

/* static */
already_AddRefed<nsISerialEventTarget> nsContentUtils::GetEventTargetByLoadInfo(
    nsILoadInfo* aLoadInfo, TaskCategory aCategory) {
  if (NS_WARN_IF(!aLoadInfo)) {
    return nullptr;
  }

  RefPtr<Document> doc;
  aLoadInfo->GetLoadingDocument(getter_AddRefs(doc));
  nsCOMPtr<nsISerialEventTarget> target;
  if (doc) {
    if (DocGroup* group = doc->GetDocGroup()) {
      target = group->EventTargetFor(aCategory);
    }
  } else {
    target = GetMainThreadSerialEventTarget();
  }

  return target.forget();
}

/* static */
bool nsContentUtils::IsLocalRefURL(const nsAString& aString) {
  return !aString.IsEmpty() && aString[0] == '#';
}

// We use only 53 bits for the ID so that it can be converted to and from a JS
// value without loss of precision. The upper bits of the ID hold the process
// ID. The lower bits identify the object itself.
static constexpr uint64_t kIdTotalBits = 53;
static constexpr uint64_t kIdProcessBits = 22;
static constexpr uint64_t kIdBits = kIdTotalBits - kIdProcessBits;

/* static */
uint64_t nsContentUtils::GenerateProcessSpecificId(uint64_t aId) {
  uint64_t processId = 0;
  if (XRE_IsContentProcess()) {
    ContentChild* cc = ContentChild::GetSingleton();
    processId = cc->GetID();
  }

  MOZ_RELEASE_ASSERT(processId < (uint64_t(1) << kIdProcessBits));
  uint64_t processBits = processId & ((uint64_t(1) << kIdProcessBits) - 1);

  uint64_t id = aId;
  MOZ_RELEASE_ASSERT(id < (uint64_t(1) << kIdBits));
  uint64_t bits = id & ((uint64_t(1) << kIdBits) - 1);

  return (processBits << kIdBits) | bits;
}

/* static */
std::tuple<uint64_t, uint64_t> nsContentUtils::SplitProcessSpecificId(
    uint64_t aId) {
  return {aId >> kIdBits, aId & ((uint64_t(1) << kIdBits) - 1)};
}

// Next process-local Tab ID.
static uint64_t gNextTabId = 0;

/* static */
uint64_t nsContentUtils::GenerateTabId() {
  return GenerateProcessSpecificId(++gNextTabId);
}

// Next process-local Browser ID.
static uint64_t gNextBrowserId = 0;

/* static */
uint64_t nsContentUtils::GenerateBrowserId() {
  return GenerateProcessSpecificId(++gNextBrowserId);
}

// Next process-local Browsing Context ID.
static uint64_t gNextBrowsingContextId = 0;

/* static */
uint64_t nsContentUtils::GenerateBrowsingContextId() {
  return GenerateProcessSpecificId(++gNextBrowsingContextId);
}

// Next process-local Window ID.
static uint64_t gNextWindowId = 0;

/* static */
uint64_t nsContentUtils::GenerateWindowId() {
  return GenerateProcessSpecificId(++gNextWindowId);
}

// Next process-local load.
static Atomic<uint64_t> gNextLoadIdentifier(0);

/* static */
uint64_t nsContentUtils::GenerateLoadIdentifier() {
  return GenerateProcessSpecificId(++gNextLoadIdentifier);
}

/* static */
bool nsContentUtils::GetUserIsInteracting() {
  return UserInteractionObserver::sUserActive;
}

/* static */
bool nsContentUtils::GetSourceMapURL(nsIHttpChannel* aChannel,
                                     nsACString& aResult) {
  nsresult rv = aChannel->GetResponseHeader("SourceMap"_ns, aResult);
  if (NS_FAILED(rv)) {
    rv = aChannel->GetResponseHeader("X-SourceMap"_ns, aResult);
  }
  return NS_SUCCEEDED(rv);
}

/* static */
bool nsContentUtils::IsMessageInputEvent(const IPC::Message& aMsg) {
  if ((aMsg.type() & mozilla::dom::PBrowser::PBrowserStart) ==
      mozilla::dom::PBrowser::PBrowserStart) {
    switch (aMsg.type()) {
      case mozilla::dom::PBrowser::Msg_RealMouseMoveEvent__ID:
      case mozilla::dom::PBrowser::Msg_RealMouseButtonEvent__ID:
      case mozilla::dom::PBrowser::Msg_RealMouseEnterExitWidgetEvent__ID:
      case mozilla::dom::PBrowser::Msg_RealKeyEvent__ID:
      case mozilla::dom::PBrowser::Msg_MouseWheelEvent__ID:
      case mozilla::dom::PBrowser::Msg_RealTouchEvent__ID:
      case mozilla::dom::PBrowser::Msg_RealTouchMoveEvent__ID:
      case mozilla::dom::PBrowser::Msg_RealDragEvent__ID:
      case mozilla::dom::PBrowser::Msg_UpdateDimensions__ID:
        return true;
    }
  }
  return false;
}

/* static */
bool nsContentUtils::IsMessageCriticalInputEvent(const IPC::Message& aMsg) {
  if ((aMsg.type() & mozilla::dom::PBrowser::PBrowserStart) ==
      mozilla::dom::PBrowser::PBrowserStart) {
    switch (aMsg.type()) {
      case mozilla::dom::PBrowser::Msg_RealMouseButtonEvent__ID:
      case mozilla::dom::PBrowser::Msg_RealKeyEvent__ID:
      case mozilla::dom::PBrowser::Msg_MouseWheelEvent__ID:
      case mozilla::dom::PBrowser::Msg_RealTouchEvent__ID:
      case mozilla::dom::PBrowser::Msg_RealDragEvent__ID:
        return true;
    }
  }
  return false;
}

static const char* kUserInteractionInactive = "user-interaction-inactive";
static const char* kUserInteractionActive = "user-interaction-active";

void nsContentUtils::UserInteractionObserver::Init() {
  // Listen for the observer messages from EventStateManager which are telling
  // us whether or not the user is interacting.
  nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
  obs->AddObserver(this, kUserInteractionInactive, false);
  obs->AddObserver(this, kUserInteractionActive, false);

  // We can't register ourselves as an annotator yet, as the
  // BackgroundHangMonitor hasn't started yet. It will have started by the
  // time we have the chance to spin the event loop.
  RefPtr<UserInteractionObserver> self = this;
  NS_DispatchToMainThread(NS_NewRunnableFunction(
      "nsContentUtils::UserInteractionObserver::Init",
      [=]() { BackgroundHangMonitor::RegisterAnnotator(*self); }));
}

void nsContentUtils::UserInteractionObserver::Shutdown() {
  nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
  if (obs) {
    obs->RemoveObserver(this, kUserInteractionInactive);
    obs->RemoveObserver(this, kUserInteractionActive);
  }

  BackgroundHangMonitor::UnregisterAnnotator(*this);
}

/**
 * NB: This function is always called by the BackgroundHangMonitor thread.
 *     Plan accordingly
 */
void nsContentUtils::UserInteractionObserver::AnnotateHang(
    BackgroundHangAnnotations& aAnnotations) {
  // NOTE: Only annotate the hang report if the user is known to be interacting.
  if (sUserActive) {
    aAnnotations.AddAnnotation(u"UserInteracting"_ns, true);
  }
}

NS_IMETHODIMP
nsContentUtils::UserInteractionObserver::Observe(nsISupports* aSubject,
                                                 const char* aTopic,
                                                 const char16_t* aData) {
  if (!strcmp(aTopic, kUserInteractionInactive)) {
    if (sUserActive && XRE_IsParentProcess()) {
      glean::RecordPowerMetrics();
    }
    sUserActive = false;
  } else if (!strcmp(aTopic, kUserInteractionActive)) {
    if (!sUserActive && XRE_IsParentProcess()) {
      glean::RecordPowerMetrics();
    }
    sUserActive = true;
  } else {
    NS_WARNING("Unexpected observer notification");
  }
  return NS_OK;
}

Atomic<bool> nsContentUtils::UserInteractionObserver::sUserActive(false);
NS_IMPL_ISUPPORTS(nsContentUtils::UserInteractionObserver, nsIObserver)

/* static */
bool nsContentUtils::IsSpecialName(const nsAString& aName) {
  return aName.LowerCaseEqualsLiteral("_blank") ||
         aName.LowerCaseEqualsLiteral("_top") ||
         aName.LowerCaseEqualsLiteral("_parent") ||
         aName.LowerCaseEqualsLiteral("_self");
}

/* static */
bool nsContentUtils::IsOverridingWindowName(const nsAString& aName) {
  return !aName.IsEmpty() && !IsSpecialName(aName);
}

// Unfortunately, we can't unwrap an IDL object using only a concrete type.
// We need to calculate type data based on the IDL typename. Which means
// wrapping our templated function in a macro.
#define EXTRACT_EXN_VALUES(T, ...)                                \
  ExtractExceptionValues<mozilla::dom::prototypes::id::T,         \
                         T##_Binding::NativeType, T>(__VA_ARGS__) \
      .isOk()

template <prototypes::ID PrototypeID, class NativeType, typename T>
static Result<Ok, nsresult> ExtractExceptionValues(
    JSContext* aCx, JS::HandleObject aObj, nsAString& aSourceSpecOut,
    uint32_t* aLineOut, uint32_t* aColumnOut, nsString& aMessageOut) {
  AssertStaticUnwrapOK<PrototypeID>();
  RefPtr<T> exn;
  MOZ_TRY((UnwrapObject<PrototypeID, NativeType>(aObj, exn, nullptr)));

  exn->GetFilename(aCx, aSourceSpecOut);
  if (!aSourceSpecOut.IsEmpty()) {
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

/* static */
void nsContentUtils::ExtractErrorValues(
    JSContext* aCx, JS::Handle<JS::Value> aValue, nsACString& aSourceSpecOut,
    uint32_t* aLineOut, uint32_t* aColumnOut, nsString& aMessageOut) {
  nsAutoString sourceSpec;
  ExtractErrorValues(aCx, aValue, sourceSpec, aLineOut, aColumnOut,
                     aMessageOut);
  CopyUTF16toUTF8(sourceSpec, aSourceSpecOut);
}

/* static */
void nsContentUtils::ExtractErrorValues(
    JSContext* aCx, JS::Handle<JS::Value> aValue, nsAString& aSourceSpecOut,
    uint32_t* aLineOut, uint32_t* aColumnOut, nsString& aMessageOut) {
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
                   nullptr,  // toString result
                   false,    // chrome
                   0);       // window ID

      if (!report->mFileName.IsEmpty()) {
        aSourceSpecOut = report->mFileName;
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
    else if (EXTRACT_EXN_VALUES(Exception, aCx, obj, aSourceSpecOut, aLineOut,
                                aColumnOut, aMessageOut)) {
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

/* static */
bool nsContentUtils::ContentIsLink(nsIContent* aContent) {
  if (!aContent || !aContent->IsElement()) {
    return false;
  }

  if (aContent->IsHTMLElement(nsGkAtoms::a)) {
    return true;
  }

  return aContent->AsElement()->AttrValueIs(kNameSpaceID_XLink, nsGkAtoms::type,
                                            nsGkAtoms::simple, eCaseMatters);
}

/* static */
already_AddRefed<ContentFrameMessageManager>
nsContentUtils::TryGetBrowserChildGlobal(nsISupports* aFrom) {
  RefPtr<nsFrameLoaderOwner> frameLoaderOwner = do_QueryObject(aFrom);
  if (!frameLoaderOwner) {
    return nullptr;
  }

  RefPtr<nsFrameLoader> frameLoader = frameLoaderOwner->GetFrameLoader();
  if (!frameLoader) {
    return nullptr;
  }

  RefPtr<ContentFrameMessageManager> manager =
      frameLoader->GetBrowserChildMessageManager();
  return manager.forget();
}

/* static */
uint32_t nsContentUtils::InnerOrOuterWindowCreated() {
  MOZ_ASSERT(NS_IsMainThread());
  ++sInnerOrOuterWindowCount;
  return ++sInnerOrOuterWindowSerialCounter;
}

/* static */
void nsContentUtils::InnerOrOuterWindowDestroyed() {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(sInnerOrOuterWindowCount > 0);
  --sInnerOrOuterWindowCount;
}

static bool JSONCreator(const char16_t* aBuf, uint32_t aLen, void* aData) {
  nsAString* result = static_cast<nsAString*>(aData);
  result->Append(static_cast<const char16_t*>(aBuf),
                 static_cast<uint32_t>(aLen));
  return true;
}

/* static */
bool nsContentUtils::StringifyJSON(JSContext* aCx,
                                   JS::MutableHandle<JS::Value> aValue,
                                   nsAString& aOutStr) {
  MOZ_ASSERT(aCx);
  aOutStr.Truncate();
  JS::RootedValue value(aCx, aValue.get());
  nsAutoString serializedValue;
  NS_ENSURE_TRUE(JS_Stringify(aCx, &value, nullptr, JS::NullHandleValue,
                              JSONCreator, &serializedValue),
                 false);
  aOutStr = serializedValue;
  return true;
}

/* static */
bool nsContentUtils::
    HighPriorityEventPendingForTopLevelDocumentBeforeContentfulPaint(
        Document* aDocument) {
  MOZ_ASSERT(XRE_IsContentProcess(),
             "This function only makes sense in content processes");

  if (aDocument && !aDocument->IsLoadedAsData()) {
    if (nsPresContext* presContext = FindPresContextForDocument(aDocument)) {
      MOZ_ASSERT(!presContext->IsChrome(),
                 "Should never have a chrome PresContext in a content process");

      return !presContext->GetInProcessRootContentDocumentPresContext()
                  ->HadContentfulPaint() &&
             nsThreadManager::MainThreadHasPendingHighPriorityEvents();
    }
  }
  return false;
}

/* static */
nsGlobalWindowInner* nsContentUtils::CallerInnerWindow() {
  nsIGlobalObject* global = GetIncumbentGlobal();
  NS_ENSURE_TRUE(global, nullptr);

  if (auto* window = global->AsInnerWindow()) {
    return nsGlobalWindowInner::Cast(window);
  }

  // When Extensions run content scripts inside a sandbox, it uses
  // sandboxPrototype to make them appear as though they're running in the
  // scope of the page. So when a content script invokes postMessage, it expects
  // the |source| of the received message to be the window set as the
  // sandboxPrototype. This used to work incidentally for unrelated reasons, but
  // now we need to do some special handling to support it.
  JS::Rooted<JSObject*> scope(RootingCx(), global->GetGlobalJSObject());
  NS_ENSURE_TRUE(scope, nullptr);

  if (xpc::IsSandbox(scope)) {
    AutoJSAPI jsapi;
    MOZ_ALWAYS_TRUE(jsapi.Init(scope));
    JSContext* cx = jsapi.cx();
    // Our current Realm on aCx is the sandbox.  Using that for unwrapping
    // makes sense: if the sandbox can unwrap the window, we can use it.
    return xpc::SandboxWindowOrNull(scope, cx);
  }

  // The calling window must be holding a reference, so we can return a weak
  // pointer.
  return nsGlobalWindowInner::Cast(global->AsInnerWindow());
}

/* static */
bool nsContentUtils::IsURIInPrefList(nsIURI* aURI, const char* aPrefName) {
  MOZ_ASSERT(aPrefName);

  nsAutoCString list;
  Preferences::GetCString(aPrefName, list);
  ToLowerCase(list);
  return IsURIInList(aURI, list);
}

/* static */
bool nsContentUtils::IsURIInList(nsIURI* aURI, const nsCString& aList) {
#ifdef DEBUG
  nsAutoCString listLowerCase(aList);
  ToLowerCase(listLowerCase);
  MOZ_ASSERT(listLowerCase.Equals(aList),
             "The aList argument should be lower-case");
#endif

  if (!aURI) {
    return false;
  }

  nsAutoCString scheme;
  aURI->GetScheme(scheme);
  if (!scheme.EqualsLiteral("http") && !scheme.EqualsLiteral("https")) {
    return false;
  }

  if (aList.IsEmpty()) {
    return false;
  }

  // The list is comma separated domain list.  Each item may start with "*.".
  // If starts with "*.", it matches any sub-domains.

  nsCCharSeparatedTokenizer tokenizer(aList, ',');
  while (tokenizer.hasMoreTokens()) {
    const nsCString token(tokenizer.nextToken());

    nsAutoCString host;
    aURI->GetHost(host);
    if (host.IsEmpty()) {
      return false;
    }
    ToLowerCase(host);

    for (;;) {
      int32_t index = token.Find(host, false);
      if (index >= 0 &&
          static_cast<uint32_t>(index) + host.Length() <= token.Length()) {
        // If we found a full match, return true.
        size_t indexAfterHost = index + host.Length();
        if (index == 0 && indexAfterHost == token.Length()) {
          return true;
        }
        // If next character is '/', we need to check the path too.
        // We assume the path in the list means "/foo" + "*".
        if (token[indexAfterHost] == '/') {
          nsDependentCSubstring pathInList(
              token, indexAfterHost,
              static_cast<nsDependentCSubstring::size_type>(-1));
          nsAutoCString filePath;
          aURI->GetFilePath(filePath);
          ToLowerCase(filePath);
          if (StringBeginsWith(filePath, pathInList) &&
              (filePath.Length() == pathInList.Length() ||
               pathInList.EqualsLiteral("/") ||
               filePath[pathInList.Length() - 1] == '/' ||
               filePath[pathInList.Length() - 1] == '?' ||
               filePath[pathInList.Length() - 1] == '#')) {
            return true;
          }
        }
      }
      int32_t startIndexOfCurrentLevel = host[0] == '*' ? 1 : 0;
      int32_t startIndexOfNextLevel =
          host.Find(".", false, startIndexOfCurrentLevel + 1);
      if (startIndexOfNextLevel <= 0) {
        break;
      }
      host = "*"_ns + nsDependentCSubstring(host, startIndexOfNextLevel);
    }
  }

  return false;
}

/* static */
ScreenIntMargin nsContentUtils::GetWindowSafeAreaInsets(
    nsIScreen* aScreen, const ScreenIntMargin& aSafeAreaInsets,
    const LayoutDeviceIntRect& aWindowRect) {
  // This calculates safe area insets of window from screen rectangle, window
  // rectangle and safe area insets of screen.
  //
  // +----------------------------------------+ <-- screen
  // |  +-------------------------------+  <------- window
  // |  | window's safe area inset top) |     |
  // +--+-------------------------------+--+  |
  // |  |                               |  |<------ safe area rectangle of
  // |  |                               |  |  |     screen
  // +--+-------------------------------+--+  |
  // |  |window's safe area inset bottom|     |
  // |  +-------------------------------+     |
  // +----------------------------------------+
  //
  ScreenIntMargin windowSafeAreaInsets;

  if (windowSafeAreaInsets == aSafeAreaInsets) {
    // no safe area insets.
    return windowSafeAreaInsets;
  }

  int32_t screenLeft, screenTop, screenWidth, screenHeight;
  nsresult rv =
      aScreen->GetRect(&screenLeft, &screenTop, &screenWidth, &screenHeight);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return windowSafeAreaInsets;
  }

  const ScreenIntRect screenRect(screenLeft, screenTop, screenWidth,
                                 screenHeight);

  ScreenIntRect safeAreaRect = screenRect;
  safeAreaRect.Deflate(aSafeAreaInsets);

  ScreenIntRect windowRect = ViewAs<ScreenPixel>(
      aWindowRect, PixelCastJustification::LayoutDeviceIsScreenForTabDims);

  // FIXME(bug 1754323): This can trigger because the screen rect is not
  // orientation-aware.
  // MOZ_ASSERT(screenRect.Contains(windowRect),
  //            "Screen doesn't contain window rect? Something seems off");

  // window's rect of safe area
  safeAreaRect = safeAreaRect.Intersect(windowRect);

  windowSafeAreaInsets.top = safeAreaRect.y - aWindowRect.y;
  windowSafeAreaInsets.left = safeAreaRect.x - aWindowRect.x;
  windowSafeAreaInsets.right =
      aWindowRect.x + aWindowRect.width - (safeAreaRect.x + safeAreaRect.width);
  windowSafeAreaInsets.bottom = aWindowRect.y + aWindowRect.height -
                                (safeAreaRect.y + safeAreaRect.height);

  windowSafeAreaInsets.EnsureAtLeast(ScreenIntMargin());
  // This shouldn't be needed, but it wallpapers orientation issues, see bug
  // 1754323.
  windowSafeAreaInsets.EnsureAtMost(aSafeAreaInsets);

  return windowSafeAreaInsets;
}

/* static */
nsContentUtils::SubresourceCacheValidationInfo
nsContentUtils::GetSubresourceCacheValidationInfo(nsIRequest* aRequest,
                                                  nsIURI* aURI) {
  SubresourceCacheValidationInfo info;
  if (nsCOMPtr<nsICacheInfoChannel> cache = do_QueryInterface(aRequest)) {
    uint32_t value = 0;
    if (NS_SUCCEEDED(cache->GetCacheTokenExpirationTime(&value))) {
      info.mExpirationTime.emplace(value);
    }
  }

  // Determine whether the cache entry must be revalidated when we try to use
  // it. Currently, only HTTP specifies this information...
  if (nsCOMPtr<nsIHttpChannel> httpChannel = do_QueryInterface(aRequest)) {
    Unused << httpChannel->IsNoStoreResponse(&info.mMustRevalidate);

    if (!info.mMustRevalidate) {
      Unused << httpChannel->IsNoCacheResponse(&info.mMustRevalidate);
    }
  }

  // data: URIs are safe to cache across documents under any circumstance, so we
  // special-case them here even though the channel itself doesn't have any
  // caching policy. Same for chrome:// uris.
  //
  // TODO(emilio): Figure out which other schemes that don't have caching
  // policies are safe to cache. Blobs should be...
  const bool knownCacheable = [&] {
    if (!aURI) {
      return false;
    }
    if (aURI->SchemeIs("data") || aURI->SchemeIs("moz-page-thumb") ||
        aURI->SchemeIs("moz-extension")) {
      return true;
    }
    if (dom::IsChromeURI(aURI)) {
      return !StaticPrefs::nglayout_debug_disable_xul_cache();
    }
    return false;
  }();

  if (knownCacheable) {
    MOZ_ASSERT(!info.mExpirationTime);
    MOZ_ASSERT(!info.mMustRevalidate);
    info.mExpirationTime = Some(0);  // 0 means "doesn't expire".
  }

  return info;
}

nsCString nsContentUtils::TruncatedURLForDisplay(nsIURI* aURL, size_t aMaxLen) {
  nsCString spec;
  if (aURL) {
    aURL->GetSpec(spec);
    spec.Truncate(std::min(aMaxLen, spec.Length()));
  }
  return spec;
}

/* static */
nsresult nsContentUtils::AnonymizeId(nsAString& aId,
                                     const nsACString& aOriginKey,
                                     OriginFormat aFormat) {
  MOZ_ASSERT(NS_IsMainThread());

  nsresult rv;
  nsCString rawKey;
  if (aFormat == OriginFormat::Base64) {
    rv = Base64Decode(aOriginKey, rawKey);
    NS_ENSURE_SUCCESS(rv, rv);
  } else {
    rawKey = aOriginKey;
  }

  HMAC hmac;
  rv = hmac.Begin(
      SEC_OID_SHA256,
      Span(reinterpret_cast<const uint8_t*>(rawKey.get()), rawKey.Length()));
  NS_ENSURE_SUCCESS(rv, rv);

  NS_ConvertUTF16toUTF8 id(aId);
  rv = hmac.Update(reinterpret_cast<const uint8_t*>(id.get()), id.Length());
  NS_ENSURE_SUCCESS(rv, rv);

  nsTArray<uint8_t> macBytes;
  rv = hmac.End(macBytes);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCString macBase64;
  rv = Base64Encode(
      nsDependentCSubstring(reinterpret_cast<const char*>(macBytes.Elements()),
                            macBytes.Length()),
      macBase64);
  NS_ENSURE_SUCCESS(rv, rv);

  CopyUTF8toUTF16(macBase64, aId);
  return NS_OK;
}

namespace mozilla {
std::ostream& operator<<(std::ostream& aOut,
                         const PreventDefaultResult aPreventDefaultResult) {
  switch (aPreventDefaultResult) {
    case PreventDefaultResult::No:
      aOut << "unhandled";
      break;
    case PreventDefaultResult::ByContent:
      aOut << "handled-by-content";
      break;
    case PreventDefaultResult::ByChrome:
      aOut << "handled-by-chrome";
      break;
  }
  return aOut;
}
}  // namespace mozilla
