/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsLayoutModule.h"

#include "base/basictypes.h"

#include "XPCModule.h"
#include "mozilla/Components.h"
#include "mozilla/ModuleUtils.h"
#include "nsImageModule.h"
#include "nsLayoutStatics.h"
#include "nsContentCID.h"
#include "nsContentDLF.h"
#include "nsContentPolicyUtils.h"
#include "nsDataDocumentContentPolicy.h"
#include "nsNoDataProtocolContentPolicy.h"
#include "nsDOMCID.h"
#include "nsFrameMessageManager.h"
#include "nsHTMLContentSerializer.h"
#include "nsHTMLParts.h"
#include "nsIContentSerializer.h"
#include "nsIDocumentViewer.h"
#include "nsPlainTextSerializer.h"
#include "nsXMLContentSerializer.h"
#include "nsXHTMLContentSerializer.h"
#include "nsLayoutCID.h"
#include "nsFocusManager.h"
#include "ThirdPartyUtil.h"
#include "gfxPlatform.h"
#include "mozilla/gfx/gfxVars.h"
#include "mozilla/dom/quota/QuotaManagerService.h"

#include "nsIEventListenerService.h"

// view stuff
#include "nsContentCreatorFunctions.h"

#include "mozilla/dom/LocalStorageCommon.h"
#include "mozilla/dom/LocalStorageManager.h"
#include "mozilla/dom/LocalStorageManager2.h"
#include "mozilla/dom/SessionStorageManager.h"

#ifdef MOZ_WEBSPEECH
#  include "mozilla/dom/nsSynthVoiceRegistry.h"
#  include "mozilla/dom/OnlineSpeechRecognitionService.h"
#endif

#include "mozilla/dom/PushNotifier.h"
using mozilla::dom::PushNotifier;
#define PUSHNOTIFIER_CID                             \
  {                                                  \
    0x2fc2d3e3, 0x020f, 0x404e, {                    \
      0xb0, 0x6a, 0x6e, 0xcf, 0x3e, 0xa2, 0x33, 0x4a \
    }                                                \
  }

#include "nsScriptSecurityManager.h"
#include "nsNetCID.h"
#if defined(MOZ_WIDGET_ANDROID)
#  include "nsHapticFeedback.h"
#endif
#include "nsParserUtils.h"

class nsIDocumentLoaderFactory;

#define PRODUCT_NAME "Gecko"

#include "inDeepTreeWalker.h"

static void Shutdown();

#include "mozilla/dom/nsContentSecurityManager.h"

using namespace mozilla;
using namespace mozilla::dom;
using namespace mozilla::net;

//-----------------------------------------------------------------------------

static bool gInitialized = false;

// Perform our one-time intialization for this module

void nsLayoutModuleInitialize() {
  if (gInitialized) {
    MOZ_CRASH("Recursive layout module initialization");
  }

  static_assert(sizeof(uintptr_t) == sizeof(void*),
                "Eeek! You'll need to adjust the size of uintptr_t to the "
                "size of a pointer on your platform.");

  gInitialized = true;

  if (NS_FAILED(xpcModuleCtor())) {
    MOZ_CRASH("xpcModuleCtor failed");
  }

  if (NS_FAILED(nsLayoutStatics::Initialize())) {
    Shutdown();
    MOZ_CRASH("nsLayoutStatics::Initialize failed");
  }
}

// Shutdown this module, releasing all of the module resources

// static
void Shutdown() {
  MOZ_ASSERT(gInitialized, "module not initialized");
  if (!gInitialized) return;

  gInitialized = false;

  nsLayoutStatics::Release();
}

already_AddRefed<nsIDocumentViewer> NS_NewDocumentViewer();
nsresult NS_NewContentDocumentLoaderFactory(nsIDocumentLoaderFactory** aResult);
nsresult NS_NewContentPolicy(nsIContentPolicy** aResult);

nsresult NS_NewEventListenerService(nsIEventListenerService** aResult);
nsresult NS_NewGlobalMessageManager(nsISupports** aResult);
nsresult NS_NewParentProcessMessageManager(nsISupports** aResult);
nsresult NS_NewChildProcessMessageManager(nsISupports** aResult);

#define MAKE_CTOR(ctor_, iface_, func_)           \
  nsresult ctor_(REFNSIID aIID, void** aResult) { \
    *aResult = nullptr;                           \
    iface_* inst;                                 \
    nsresult rv = func_(&inst);                   \
    if (NS_SUCCEEDED(rv)) {                       \
      rv = inst->QueryInterface(aIID, aResult);   \
      NS_RELEASE(inst);                           \
    }                                             \
    return rv;                                    \
  }

#define MAKE_GENERIC_CTOR(iface_, func_)             \
  NS_IMPL_COMPONENT_FACTORY(iface_) {                \
    nsCOMPtr<iface_> inst;                           \
    if (NS_SUCCEEDED(func_(getter_AddRefs(inst)))) { \
      return inst.forget();                          \
    }                                                \
    return nullptr;                                  \
  }

#define MAKE_GENERIC_CTOR2(iface_, func_) \
  NS_IMPL_COMPONENT_FACTORY(iface_) { return func_(); }

MAKE_GENERIC_CTOR2(nsIDocumentViewer, NS_NewDocumentViewer)

MAKE_CTOR(CreateXMLContentSerializer, nsIContentSerializer,
          NS_NewXMLContentSerializer)
MAKE_CTOR(CreateHTMLContentSerializer, nsIContentSerializer,
          NS_NewHTMLContentSerializer)
MAKE_CTOR(CreateXHTMLContentSerializer, nsIContentSerializer,
          NS_NewXHTMLContentSerializer)
MAKE_CTOR(CreatePlainTextSerializer, nsIContentSerializer,
          NS_NewPlainTextSerializer)
MAKE_CTOR(CreateContentPolicy, nsIContentPolicy, NS_NewContentPolicy)

MAKE_GENERIC_CTOR(nsIDocumentLoaderFactory, NS_NewContentDocumentLoaderFactory)
MAKE_GENERIC_CTOR(nsIEventListenerService, NS_NewEventListenerService)
MAKE_CTOR(CreateGlobalMessageManager, nsISupports, NS_NewGlobalMessageManager)
MAKE_CTOR(CreateParentMessageManager, nsISupports,
          NS_NewParentProcessMessageManager)
MAKE_CTOR(CreateChildMessageManager, nsISupports,
          NS_NewChildProcessMessageManager)

MAKE_GENERIC_CTOR(nsIFocusManager, NS_NewFocusManager)

// views are not refcounted, so this is the same as
// NS_GENERIC_FACTORY_CONSTRUCTOR without the NS_ADDREF/NS_RELEASE
#define NS_GENERIC_FACTORY_CONSTRUCTOR_NOREFS(_InstanceClass)                  \
  static nsresult _InstanceClass##Constructor(REFNSIID aIID, void** aResult) { \
    nsresult rv;                                                               \
                                                                               \
    *aResult = nullptr;                                                        \
                                                                               \
    _InstanceClass* inst = new _InstanceClass();                               \
    if (nullptr == inst) {                                                     \
      rv = NS_ERROR_OUT_OF_MEMORY;                                             \
      return rv;                                                               \
    }                                                                          \
    rv = inst->QueryInterface(aIID, aResult);                                  \
                                                                               \
    return rv;                                                                 \
  }

#ifdef ACCESSIBILITY
#  include "xpcAccessibilityService.h"

MAKE_GENERIC_CTOR(nsIAccessibilityService, NS_GetAccessibilityService)
#endif

nsresult Construct_nsIScriptSecurityManager(REFNSIID aIID, void** aResult) {
  if (!aResult) return NS_ERROR_NULL_POINTER;
  *aResult = nullptr;
  nsScriptSecurityManager* obj =
      nsScriptSecurityManager::GetScriptSecurityManager();
  if (!obj) return NS_ERROR_OUT_OF_MEMORY;
  if (NS_FAILED(obj->QueryInterface(aIID, aResult))) return NS_ERROR_FAILURE;
  return NS_OK;
}

nsresult LocalStorageManagerConstructor(REFNSIID aIID, void** aResult) {
  if (NextGenLocalStorageEnabled()) {
    RefPtr<LocalStorageManager2> manager = new LocalStorageManager2();
    return manager->QueryInterface(aIID, aResult);
  }

  RefPtr<LocalStorageManager> manager = new LocalStorageManager();
  return manager->QueryInterface(aIID, aResult);
}

nsresult SessionStorageManagerConstructor(REFNSIID aIID, void** aResult) {
  RefPtr<SessionStorageManager> manager = new SessionStorageManager(nullptr);
  return manager->QueryInterface(aIID, aResult);
}

void nsLayoutModuleDtor() {
  if (XRE_GetProcessType() == GeckoProcessType_GPU ||
      XRE_GetProcessType() == GeckoProcessType_VR ||
      XRE_GetProcessType() == GeckoProcessType_RDD) {
    return;
  }

  Shutdown();
  nsContentUtils::XPCOMShutdown();

  // Layout depends heavily on gfx and imagelib, so we want to make sure that
  // these modules are shut down after all the layout cleanup runs.
  mozilla::image::ShutdownModule();
  gfxPlatform::Shutdown();
  gfx::gfxVars::Shutdown();

  nsScriptSecurityManager::Shutdown();
  xpcModuleDtor();
}
