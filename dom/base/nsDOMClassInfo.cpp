/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ArrayUtils.h"

// JavaScript includes
#include "jsapi.h"
#include "jsfriendapi.h"
#include "WrapperFactory.h"
#include "AccessCheck.h"
#include "XrayWrapper.h"

#include "xpcpublic.h"
#include "xpcprivate.h"
#include "xpc_make_class.h"
#include "XPCWrapper.h"

#include "mozilla/DOMEventTargetHelper.h"
#include "mozilla/dom/RegisterBindings.h"

#include "nscore.h"
#include "nsDOMClassInfo.h"
#include "nsIDOMClassInfo.h"
#include "nsCRT.h"
#include "nsCRTGlue.h"
#include "nsICategoryManager.h"
#include "nsIComponentRegistrar.h"
#include "nsXPCOM.h"
#include "nsISimpleEnumerator.h"
#include "nsISupportsPrimitives.h"
#include "nsIXPConnect.h"
#include "xptcall.h"
#include "nsTArray.h"

// General helper includes
#include "nsGlobalWindow.h"
#include "nsIContent.h"
#include "nsIDocument.h"
#include "nsIDOMDocument.h"
#include "nsIDOMEvent.h"
#include "nsIDOMEventListener.h"
#include "nsContentUtils.h"
#include "nsIDOMGlobalPropertyInitializer.h"
#include "mozilla/Attributes.h"
#include "mozilla/Telemetry.h"

// DOM base includes
#include "nsIDOMWindow.h"
#include "nsPIDOMWindow.h"

// DOM core includes
#include "nsError.h"

// Event related includes
#include "nsIDOMEventTarget.h"

// CSS related includes
#include "nsMemory.h"

// includes needed for the prototype chain interfaces

#include "nsIEventListenerService.h"

#include "mozilla/dom/TouchEvent.h"

#include "nsWrapperCacheInlines.h"
#include "mozilla/dom/HTMLCollectionBinding.h"

#include "nsDebug.h"

#include "mozilla/dom/BindingUtils.h"
#include "mozilla/Likely.h"
#include "nsIInterfaceInfoManager.h"

#ifdef MOZ_TIME_MANAGER
#include "TimeManager.h"
#endif

using namespace mozilla;
using namespace mozilla::dom;

bool nsDOMClassInfo::sIsInitialized = false;

// static
nsresult
nsDOMClassInfo::Init()
{
  /* Errors that can trigger early returns are done first,
     otherwise nsDOMClassInfo is left in a half inited state. */
  static_assert(sizeof(uintptr_t) == sizeof(void*),
                "BAD! You'll need to adjust the size of uintptr_t to the "
                "size of a pointer on your platform.");

  NS_ENSURE_TRUE(!sIsInitialized, NS_ERROR_ALREADY_INITIALIZED);

  nsCOMPtr<nsIXPCFunctionThisTranslator> elt = new nsEventListenerThisTranslator();
  nsContentUtils::XPConnect()->SetFunctionThisTranslator(NS_GET_IID(nsIDOMEventListener), elt);

  sIsInitialized = true;

  return NS_OK;
}

// static
void
nsDOMClassInfo::ShutDown()
{
  sIsInitialized = false;
}

// nsIDOMEventListener::HandleEvent() 'this' converter helper

NS_INTERFACE_MAP_BEGIN(nsEventListenerThisTranslator)
  NS_INTERFACE_MAP_ENTRY(nsIXPCFunctionThisTranslator)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END


NS_IMPL_ADDREF(nsEventListenerThisTranslator)
NS_IMPL_RELEASE(nsEventListenerThisTranslator)


NS_IMETHODIMP
nsEventListenerThisTranslator::TranslateThis(nsISupports *aInitialThis,
                                             nsISupports **_retval)
{
  nsCOMPtr<nsIDOMEvent> event(do_QueryInterface(aInitialThis));
  NS_ENSURE_TRUE(event, NS_ERROR_UNEXPECTED);

  nsCOMPtr<EventTarget> target = event->InternalDOMEvent()->GetCurrentTarget();
  target.forget(_retval);
  return NS_OK;
}
