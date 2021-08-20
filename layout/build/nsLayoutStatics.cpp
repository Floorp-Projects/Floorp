/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "base/basictypes.h"

#include "nsLayoutStatics.h"
#include "nscore.h"

#include "DateTimeFormat.h"
#include "MediaManager.h"
#include "mozilla/dom/ServiceWorkerRegistrar.h"
#include "nsAttrValue.h"
#include "nsColorNames.h"
#include "nsComputedDOMStyle.h"
#include "nsContentDLF.h"
#include "nsContentUtils.h"
#include "nsCSSAnonBoxes.h"
#include "mozilla/css/ErrorReporter.h"
#include "nsCSSProps.h"
#include "nsCSSPseudoElements.h"
#include "nsCSSRendering.h"
#include "nsGenericHTMLFrameElement.h"
#include "mozilla/dom/Attr.h"
#include "mozilla/dom/PopupBlocker.h"
#include "nsIFrame.h"
#include "nsFrameState.h"
#include "nsGlobalWindow.h"
#include "nsGkAtoms.h"
#include "nsImageFrame.h"
#include "mozilla/GlobalStyleSheetCache.h"
#include "nsRegion.h"
#include "nsRepeatService.h"
#include "nsFloatManager.h"
#include "nsSprocketLayout.h"
#include "nsStackLayout.h"
#include "nsTextControlFrame.h"
#include "txMozillaXSLTProcessor.h"
#include "nsTreeSanitizer.h"
#include "nsCellMap.h"
#include "nsTextFrame.h"
#include "nsCCUncollectableMarker.h"
#include "nsTextFragment.h"
#include "nsCORSListenerProxy.h"
#include "nsHtml5Module.h"
#include "nsHTMLTags.h"
#include "nsFocusManager.h"
#include "nsListControlFrame.h"
#include "mozilla/dom/HTMLDNSPrefetch.h"
#include "mozilla/dom/HTMLInputElement.h"
#include "mozilla/dom/SVGElementFactory.h"
#include "nsMathMLAtoms.h"
#include "nsMathMLOperators.h"
#include "Navigator.h"
#include "StorageObserver.h"
#include "CacheObserver.h"
#include "DisplayItemClip.h"
#include "HitTestInfo.h"
#include "ActiveLayerTracker.h"
#include "AnimationCommon.h"
#include "LayerAnimationInfo.h"
#include "mozilla/TimelineConsumers.h"

#include "AudioChannelService.h"
#include "mozilla/dom/PromiseDebugging.h"
#include "mozilla/dom/nsMixedContentBlocker.h"

#ifdef MOZ_XUL
#  include "nsXULPopupManager.h"
#  include "nsXULContentUtils.h"
#  include "nsXULPrototypeCache.h"
#  include "nsXULTooltipListener.h"

#endif

#include "mozilla/dom/UIDirectionManager.h"

#include "CubebUtils.h"
#include "WebAudioUtils.h"

#include "nsError.h"

#include "nsJSEnvironment.h"
#include "nsContentSink.h"
#include "nsFrameMessageManager.h"
#include "nsDOMMutationObserver.h"
#include "nsHyphenationManager.h"
#include "nsWindowMemoryReporter.h"
#include "mozilla/dom/ContentParent.h"
#include "mozilla/ProcessPriorityManager.h"
#include "mozilla/PermissionManager.h"
#include "mozilla/dom/CustomElementRegistry.h"
#include "mozilla/EventDispatcher.h"
#include "mozilla/IMEStateManager.h"
#include "mozilla/dom/HTMLVideoElement.h"
#include "ThirdPartyUtil.h"
#include "TouchManager.h"
#include "DecoderDoctorLogger.h"
#include "MediaDecoder.h"
#include "mozilla/ClearSiteData.h"
#include "mozilla/EditorController.h"
#include "mozilla/HTMLEditorController.h"
#include "mozilla/ServoBindings.h"
#include "mozilla/StaticPresData.h"
#include "mozilla/dom/AbstractRange.h"
#include "mozilla/dom/Document.h"
#include "mozilla/dom/WebIDLGlobalNameHash.h"
#include "mozilla/dom/U2FTokenManager.h"
#ifdef OS_WIN
#  include "mozilla/dom/WinWebAuthnManager.h"
#endif
#include "mozilla/dom/PointerEventHandler.h"
#include "mozilla/dom/RemoteWorkerService.h"
#include "mozilla/dom/BlobURLProtocolHandler.h"
#include "mozilla/dom/ReportingHeader.h"
#include "mozilla/dom/BrowserParent.h"
#include "mozilla/dom/quota/ActorsParent.h"
#include "mozilla/dom/localstorage/ActorsParent.h"
#include "mozilla/net/UrlClassifierFeatureFactory.h"
#include "mozilla/RemoteLazyInputStreamStorage.h"
#include "nsLayoutUtils.h"
#include "nsThreadManager.h"
#include "mozilla/css/ImageLoader.h"
#include "gfxUserFontSet.h"
#include "RestoreTabContentObserver.h"

using namespace mozilla;
using namespace mozilla::net;
using namespace mozilla::dom;
using namespace mozilla::dom::ipc;
using namespace mozilla::dom::quota;

nsrefcnt nsLayoutStatics::sLayoutStaticRefcnt = 0;

nsresult nsLayoutStatics::Initialize() {
  NS_ASSERTION(sLayoutStaticRefcnt == 0, "nsLayoutStatics isn't zero!");

  sLayoutStaticRefcnt = 1;
  NS_LOG_ADDREF(&sLayoutStaticRefcnt, sLayoutStaticRefcnt, "nsLayoutStatics",
                1);

  nsresult rv;

  ContentParent::StartUp();

  nsCSSProps::AddRefTable();
  nsColorNames::AddRefTable();

#ifdef DEBUG
  nsCSSPseudoElements::AssertAtoms();
  nsCSSAnonBoxes::AssertAtoms();
  DebugVerifyFrameStateBits();
#endif

  StartupJSEnvironment();

  nsGlobalWindowInner::Init();
  nsGlobalWindowOuter::Init();

  rv = nsContentUtils::Init();
  if (NS_FAILED(rv)) {
    NS_ERROR("Could not initialize nsContentUtils");
    return rv;
  }

  nsAttrValue::Init();

  rv = nsTextFragment::Init();
  if (NS_FAILED(rv)) {
    NS_ERROR("Could not initialize nsTextFragment");
    return rv;
  }

  nsCellMap::Init();

  StaticPresData::Init();
  nsCSSRendering::Init();
  css::ImageLoader::Init();

  rv = HTMLDNSPrefetch::Initialize();
  if (NS_FAILED(rv)) {
    NS_ERROR("Could not initialize HTML DNS prefetch");
    return rv;
  }

  nsMathMLOperators::AddRefTable();

#ifdef DEBUG
  nsIFrame::DisplayReflowStartup();
#endif
  Attr::Initialize();

  PopupBlocker::Initialize();

  rv = txMozillaXSLTProcessor::Startup();
  if (NS_FAILED(rv)) {
    NS_ERROR("Could not initialize txMozillaXSLTProcessor");
    return rv;
  }

  rv = StorageObserver::Init();
  if (NS_FAILED(rv)) {
    NS_ERROR("Could not initialize StorageObserver");
    return rv;
  }

  rv = nsCCUncollectableMarker::Init();
  if (NS_FAILED(rv)) {
    NS_ERROR("Could not initialize nsCCUncollectableMarker");
    return rv;
  }

#ifdef MOZ_XUL
  rv = nsXULPopupManager::Init();
  if (NS_FAILED(rv)) {
    NS_ERROR("Could not initialize nsXULPopupManager");
    return rv;
  }
#endif

  rv = nsFocusManager::Init();
  if (NS_FAILED(rv)) {
    NS_ERROR("Could not initialize nsFocusManager");
    return rv;
  }

  DecoderDoctorLogger::Init();
  MediaManager::StartupInit();
  CubebUtils::InitLibrary();

  nsHtml5Module::InitializeStatics();
  nsLayoutUtils::Initialize();
  PointerEventHandler::InitializeStatics();
  TouchManager::InitializeStatics();

  TimelineConsumers::Init();

  nsWindowMemoryReporter::Init();

  SVGElementFactory::Init();

  ProcessPriorityManager::Init();

  PermissionManager::Startup();

  UIDirectionManager::Initialize();

  CacheObserver::Init();

  IMEStateManager::Init();

  ServiceWorkerRegistrar::Initialize();

  MediaDecoder::InitStatics();

  PromiseDebugging::Init();

  if (XRE_IsParentProcess() || XRE_IsContentProcess()) {
    InitializeServo();
  }

  // This must be initialized on the main-thread.
  mozilla::RemoteLazyInputStreamStorage::Initialize();

  mozilla::dom::U2FTokenManager::Initialize();

#ifdef OS_WIN
  mozilla::dom::WinWebAuthnManager::Initialize();
#endif

  if (XRE_IsParentProcess()) {
    // On content process we initialize these components when PContentChild is
    // fully initialized.
    mozilla::dom::RemoteWorkerService::Initialize();
  }

  ClearSiteData::Initialize();

  // Reporting API.
  ReportingHeader::Initialize();

  if (XRE_IsParentProcess()) {
    InitializeQuotaManager();
    InitializeLocalStorage();
  }

  ThirdPartyUtil::Startup();

  RestoreTabContentObserver::Initialize();

  return NS_OK;
}

void nsLayoutStatics::Shutdown() {
  // Don't need to shutdown nsWindowMemoryReporter, that will be done by the
  // memory reporter manager.

  if (XRE_IsParentProcess() || XRE_IsContentProcess()) {
    ShutdownServo();
  }

  mozilla::dom::AbstractRange::Shutdown();
  Document::Shutdown();
  nsMessageManagerScriptExecutor::Shutdown();
  nsFocusManager::Shutdown();
#ifdef MOZ_XUL
  nsXULPopupManager::Shutdown();
#endif
  UIDirectionManager::Shutdown();
  StorageObserver::Shutdown();
  txMozillaXSLTProcessor::Shutdown();
  Attr::Shutdown();
  PopupBlocker::Shutdown();
  IMEStateManager::Shutdown();
  EditorController::Shutdown();
  HTMLEditorController::Shutdown();
  HTMLDNSPrefetch::Shutdown();
  nsCSSRendering::Shutdown();
  StaticPresData::Shutdown();
#ifdef DEBUG
  nsIFrame::DisplayReflowShutdown();
#endif
  nsCellMap::Shutdown();
  ActiveLayerTracker::Shutdown();

  // Release all of our atoms
  nsColorNames::ReleaseTable();
  nsCSSProps::ReleaseTable();
  nsRepeatService::Shutdown();
  nsStackLayout::Shutdown();

#ifdef MOZ_XUL
  nsXULContentUtils::Finish();
  nsXULPrototypeCache::ReleaseGlobals();
  nsSprocketLayout::Shutdown();
#endif

  SVGElementFactory::Shutdown();
  nsMathMLOperators::ReleaseTable();

  nsFloatManager::Shutdown();
  nsImageFrame::ReleaseGlobals();

  mozilla::css::ErrorReporter::ReleaseGlobals();

  nsTextFragment::Shutdown();

  nsAttrValue::Shutdown();
  nsContentUtils::Shutdown();
  nsMixedContentBlocker::Shutdown();
  GlobalStyleSheetCache::Shutdown();

  ShutdownJSEnvironment();
  nsGlobalWindowInner::ShutDown();
  nsGlobalWindowOuter::ShutDown();
  nsListControlFrame::Shutdown();

  CubebUtils::ShutdownLibrary();
  WebAudioUtils::Shutdown();

  nsCORSListenerProxy::Shutdown();

  PointerEventHandler::ReleaseStatics();

  TouchManager::ReleaseStatics();

  nsTreeSanitizer::ReleaseStatics();

  nsHtml5Module::ReleaseStatics();

  mozilla::EventDispatcher::Shutdown();

  HTMLInputElement::DestroyUploadLastDir();

  nsLayoutUtils::Shutdown();

  nsHyphenationManager::Shutdown();
  nsDOMMutationObserver::Shutdown();

  DateTimeFormat::Shutdown();

  ContentParent::ShutDown();

  DisplayItemClip::Shutdown();
  HitTestInfo::Shutdown();

  CacheObserver::Shutdown();

  PromiseDebugging::Shutdown();

  BlobURLProtocolHandler::RemoveDataEntries();

  css::ImageLoader::Shutdown();

  mozilla::net::UrlClassifierFeatureFactory::Shutdown();

  RestoreTabContentObserver::Shutdown();
}
