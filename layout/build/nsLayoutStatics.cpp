/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsLayoutStatics.h"
#include "nscore.h"

#include "nsAttrValue.h"
#include "nsAutoCopyListener.h"
#include "nsColorNames.h"
#include "nsComputedDOMStyle.h"
#include "nsContentDLF.h"
#include "nsContentUtils.h"
#include "nsCSSAnonBoxes.h"
#include "nsCSSKeywords.h"
#include "nsCSSParser.h"
#include "nsCSSProps.h"
#include "nsCSSPseudoClasses.h"
#include "nsCSSPseudoElements.h"
#include "nsCSSRendering.h"
#include "nsCSSScanner.h"
#include "nsDOMAttribute.h"
#include "nsDOMClassInfo.h"
#include "nsEventListenerManager.h"
#include "nsFrame.h"
#include "nsGlobalWindow.h"
#include "nsGkAtoms.h"
#include "nsImageFrame.h"
#include "nsLayoutStylesheetCache.h"
#include "nsNodeInfo.h"
#include "nsRange.h"
#include "nsRegion.h"
#include "nsRepeatService.h"
#include "nsFloatManager.h"
#include "nsSprocketLayout.h"
#include "nsStackLayout.h"
#include "nsStyleSet.h"
#include "nsTextControlFrame.h"
#include "nsXBLWindowKeyHandler.h"
#include "nsXBLService.h"
#include "txMozillaXSLTProcessor.h"
#include "nsDOMStorage.h"
#include "nsTreeSanitizer.h"
#include "nsCellMap.h"
#include "nsTextFrameTextRunCache.h"
#include "nsCCUncollectableMarker.h"
#include "nsTextFragment.h"
#include "nsCSSRuleProcessor.h"
#include "nsCrossSiteListenerProxy.h"
#include "nsHTMLDNSPrefetch.h"
#include "nsHtml5Module.h"
#include "nsFocusManager.h"
#include "nsFrameList.h"
#include "nsListControlFrame.h"
#include "nsHTMLInputElement.h"
#include "nsSVGUtils.h"
#include "nsMathMLAtoms.h"
#include "nsMathMLOperators.h"
#include "Navigator.h"

#ifdef MOZ_XUL
#include "nsXULPopupManager.h"
#include "nsXULContentUtils.h"
#include "nsXULPrototypeCache.h"
#include "nsXULTooltipListener.h"

#include "inDOMView.h"
#endif

#include "nsHTMLEditor.h"
#include "nsTextServicesDocument.h"

#ifdef MOZ_MEDIA
#include "nsMediaDecoder.h"
#include "nsHTMLMediaElement.h"
#endif

#ifdef MOZ_MEDIA_PLUGINS
#include "nsMediaPluginHost.h"
#endif

#ifdef MOZ_SYDNEYAUDIO
#include "nsAudioStream.h"
#endif

#include "nsError.h"

#include "nsCycleCollector.h"
#include "nsJSEnvironment.h"
#include "nsContentSink.h"
#include "nsFrameMessageManager.h"
#include "nsRefreshDriver.h"
#include "nsDOMMutationObserver.h"
#include "nsHyphenationManager.h"
#include "nsEditorSpellCheck.h"
#include "nsWindowMemoryReporter.h"

extern void NS_ShutdownChainItemPool();

using namespace mozilla;
using namespace mozilla::dom;

nsrefcnt nsLayoutStatics::sLayoutStaticRefcnt = 0;

nsresult
nsLayoutStatics::Initialize()
{
  NS_ASSERTION(sLayoutStaticRefcnt == 0,
               "nsLayoutStatics isn't zero!");

  sLayoutStaticRefcnt = 1;
  NS_LOG_ADDREF(&sLayoutStaticRefcnt, sLayoutStaticRefcnt,
                "nsLayoutStatics", 1);

  nsresult rv;

  // Register all of our atoms once
  nsCSSAnonBoxes::AddRefAtoms();
  nsCSSPseudoClasses::AddRefAtoms();
  nsCSSPseudoElements::AddRefAtoms();
  nsCSSKeywords::AddRefTable();
  nsCSSProps::AddRefTable();
  nsColorNames::AddRefTable();
  nsGkAtoms::AddRefAtoms();

  nsJSRuntime::Startup();
  rv = nsRegion::InitStatic();
  if (NS_FAILED(rv)) {
    NS_ERROR("Could not initialize nsRegion");
    return rv;
  }

  nsGlobalWindow::Init();
  Navigator::Init();
  nsXBLService::Init();

  rv = nsContentUtils::Init();
  if (NS_FAILED(rv)) {
    NS_ERROR("Could not initialize nsContentUtils");
    return rv;
  }

  rv = nsAttrValue::Init();
  if (NS_FAILED(rv)) {
    NS_ERROR("Could not initialize nsAttrValue");
    return rv;
  }

  rv = nsTextFragment::Init();
  if (NS_FAILED(rv)) {
    NS_ERROR("Could not initialize nsTextFragment");
    return rv;
  }

  nsCellMap::Init();

  nsCSSRendering::Init();

  nsTextFrameTextRunCache::Init();

  rv = nsHTMLDNSPrefetch::Initialize();
  if (NS_FAILED(rv)) {
    NS_ERROR("Could not initialize HTML DNS prefetch");
    return rv;
  }

#ifdef MOZ_XUL
  rv = nsXULContentUtils::Init();
  if (NS_FAILED(rv)) {
    NS_ERROR("Could not initialize nsXULContentUtils");
    return rv;
  }

  inDOMView::InitAtoms();

#endif

  nsMathMLOperators::AddRefTable();

  nsEditProperty::RegisterAtoms();
  nsTextServicesDocument::RegisterAtoms();

#ifdef DEBUG
  nsFrame::DisplayReflowStartup();
#endif
  nsDOMAttribute::Initialize();

  rv = txMozillaXSLTProcessor::Startup();
  if (NS_FAILED(rv)) {
    NS_ERROR("Could not initialize txMozillaXSLTProcessor");
    return rv;
  }

  rv = nsDOMStorageManager::Initialize();
  if (NS_FAILED(rv)) {
    NS_ERROR("Could not initialize nsDOMStorageManager");
    return rv;
  }

  rv = nsCCUncollectableMarker::Init();
  if (NS_FAILED(rv)) {
    NS_ERROR("Could not initialize nsCCUncollectableMarker");
    return rv;
  }

  rv = nsCSSRuleProcessor::Startup();
  if (NS_FAILED(rv)) {
    NS_ERROR("Could not initialize nsCSSRuleProcessor");
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

#ifdef MOZ_SYDNEYAUDIO
  nsAudioStream::InitLibrary();
#endif

  nsContentSink::InitializeStatics();
  nsHtml5Module::InitializeStatics();
  nsLayoutUtils::Initialize();
  nsIPresShell::InitializeStatics();
  nsRefreshDriver::InitializeStatics();

  nsCORSListenerProxy::Startup();

  nsFrameList::Init();

  NS_SealStaticAtomTable();

  nsWindowMemoryReporter::Init();

  nsSVGUtils::Init();

  return NS_OK;
}

void
nsLayoutStatics::Shutdown()
{
  // Don't need to shutdown nsWindowMemoryReporter, that will be done by the
  // memory reporter manager.

  nsFrameScriptExecutor::Shutdown();
  nsFocusManager::Shutdown();
#ifdef MOZ_XUL
  nsXULPopupManager::Shutdown();
#endif
  nsDOMStorageManager::Shutdown();
  txMozillaXSLTProcessor::Shutdown();
  nsDOMAttribute::Shutdown();
  nsEventListenerManager::Shutdown();
  nsComputedDOMStyle::Shutdown();
  nsCSSParser::Shutdown();
  nsCSSRuleProcessor::Shutdown();
  nsTextFrameTextRunCache::Shutdown();
  nsHTMLDNSPrefetch::Shutdown();
  nsCSSRendering::Shutdown();
#ifdef DEBUG
  nsFrame::DisplayReflowShutdown();
#endif
  nsCellMap::Shutdown();
  nsFrame::ShutdownLayerActivityTimer();

  // Release all of our atoms
  nsColorNames::ReleaseTable();
  nsCSSProps::ReleaseTable();
  nsCSSKeywords::ReleaseTable();
  nsRepeatService::Shutdown();
  nsStackLayout::Shutdown();
  nsBox::Shutdown();

#ifdef MOZ_XUL
  nsXULContentUtils::Finish();
  nsXULPrototypeCache::ReleaseGlobals();
  nsSprocketLayout::Shutdown();
#endif

  nsMathMLOperators::ReleaseTable();

  nsFloatManager::Shutdown();
  nsImageFrame::ReleaseGlobals();

  nsCSSScanner::ReleaseGlobals();

  nsTextFragment::Shutdown();

  nsAttrValue::Shutdown();
  nsContentUtils::Shutdown();
  nsNodeInfo::ClearCache();
  nsLayoutStylesheetCache::Shutdown();
  NS_NameSpaceManagerShutdown();

  nsJSRuntime::Shutdown();
  nsGlobalWindow::ShutDown();
  nsDOMClassInfo::ShutDown();
  nsListControlFrame::Shutdown();
  nsXBLWindowKeyHandler::ShutDown();
  nsXBLService::Shutdown();
  nsAutoCopyListener::Shutdown();
  FrameLayerBuilder::Shutdown();

#ifdef MOZ_MEDIA_PLUGINS
  nsMediaPluginHost::Shutdown();  
#endif

#ifdef MOZ_SYDNEYAUDIO
  nsAudioStream::ShutdownLibrary();
#endif

  nsCORSListenerProxy::Shutdown();
  
  nsIPresShell::ReleaseStatics();

  nsTreeSanitizer::ReleaseStatics();

  nsHtml5Module::ReleaseStatics();

  nsRegion::ShutdownStatic();

  NS_ShutdownChainItemPool();

  nsFrameList::Shutdown();

  nsHTMLInputElement::DestroyUploadLastDir();

  nsLayoutUtils::Shutdown();

  nsHyphenationManager::Shutdown();
  nsEditorSpellCheck::ShutDown();
  nsDOMMutationObserver::Shutdown();
}
