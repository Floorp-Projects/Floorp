/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Presentation.h"

#include <ctype.h>

#include "mozilla/dom/PresentationBinding.h"
#include "mozilla/dom/Promise.h"
#include "nsContentUtils.h"
#include "nsCycleCollectionParticipant.h"
#include "nsIDocShell.h"
#include "nsIPresentationService.h"
#include "nsIScriptSecurityManager.h"
#include "nsJSUtils.h"
#include "nsNetUtil.h"
#include "nsPIDOMWindow.h"
#include "nsSandboxFlags.h"
#include "nsServiceManagerUtils.h"
#include "PresentationReceiver.h"

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(Presentation,
                                      mWindow,
                                      mDefaultRequest, mReceiver)

NS_IMPL_CYCLE_COLLECTING_ADDREF(Presentation)
NS_IMPL_CYCLE_COLLECTING_RELEASE(Presentation)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(Presentation)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

/* static */ already_AddRefed<Presentation>
Presentation::Create(nsPIDOMWindowInner* aWindow)
{
  RefPtr<Presentation> presentation = new Presentation(aWindow);
  return presentation.forget();
}

/* static */ bool
Presentation::HasReceiverSupport(JSContext* aCx, JSObject* aGlobal)
{
  JS::Rooted<JSObject*> global(aCx, aGlobal);

  nsCOMPtr<nsPIDOMWindowInner> inner =
    do_QueryInterface(nsJSUtils::GetStaticScriptGlobal(global));
  if (NS_WARN_IF(!inner)) {
    return false;
  }

  // Grant access to browser receiving pages and their same-origin iframes. (App
  // pages should be controlled by "presentation" permission in app manifests.)
  nsCOMPtr<nsIDocShell> docshell = inner->GetDocShell();
  if (!docshell) {
    return false;
  }

  if (!Preferences::GetBool("dom.presentation.testing.simulate-receiver") &&
      !docshell->GetIsInMozBrowserOrApp()) {
    return false;
  }

  nsAutoString presentationURL;
  nsContentUtils::GetPresentationURL(docshell, presentationURL);

  if (presentationURL.IsEmpty()) {
    return false;
  }

  nsCOMPtr<nsIScriptSecurityManager> securityManager =
    nsContentUtils::GetSecurityManager();
  if (!securityManager) {
    return false;
  }

  nsCOMPtr<nsIURI> presentationURI;
  nsresult rv = NS_NewURI(getter_AddRefs(presentationURI), presentationURL);
  if (NS_FAILED(rv)) {
    return false;
  }

  nsCOMPtr<nsIURI> docURI = inner->GetDocumentURI();
  return NS_SUCCEEDED(securityManager->CheckSameOriginURI(presentationURI,
                                                          docURI,
                                                          false));
}

Presentation::Presentation(nsPIDOMWindowInner* aWindow)
  : mWindow(aWindow)
{
}

Presentation::~Presentation()
{
}

/* virtual */ JSObject*
Presentation::WrapObject(JSContext* aCx,
                         JS::Handle<JSObject*> aGivenProto)
{
  return PresentationBinding::Wrap(aCx, this, aGivenProto);
}

void
Presentation::SetDefaultRequest(PresentationRequest* aRequest)
{
  if (IsInPresentedContent()) {
    return;
  }

  nsCOMPtr<nsIDocument> doc = mWindow ? mWindow->GetExtantDoc() : nullptr;
  if (NS_WARN_IF(!doc)) {
    return;
  }

  if (doc->GetSandboxFlags() & SANDBOXED_PRESENTATION) {
    return;
  }

  mDefaultRequest = aRequest;
}

already_AddRefed<PresentationRequest>
Presentation::GetDefaultRequest() const
{
  if (IsInPresentedContent()) {
    return nullptr;
  }

  RefPtr<PresentationRequest> request = mDefaultRequest;
  return request.forget();
}

already_AddRefed<PresentationReceiver>
Presentation::GetReceiver()
{
  // return the same receiver if already created
  if (mReceiver) {
    RefPtr<PresentationReceiver> receiver = mReceiver;
    return receiver.forget();
  }

  if (!IsInPresentedContent()) {
    return nullptr;
  }

  mReceiver = PresentationReceiver::Create(mWindow);
  if (NS_WARN_IF(!mReceiver)) {
    MOZ_ASSERT(mReceiver);
    return nullptr;
  }

  RefPtr<PresentationReceiver> receiver = mReceiver;
  return receiver.forget();
}

bool
Presentation::IsInPresentedContent() const
{
  if (!mWindow) {
    return false;
  }

  nsCOMPtr<nsIDocShell> docShell = mWindow->GetDocShell();
  MOZ_ASSERT(docShell);

  nsAutoString presentationURL;
  nsContentUtils::GetPresentationURL(docShell, presentationURL);

  return !presentationURL.IsEmpty();
}

} // namespace dom
} // namespace mozilla
