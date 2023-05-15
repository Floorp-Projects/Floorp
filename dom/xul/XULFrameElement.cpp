/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsCOMPtr.h"
#include "nsIBrowser.h"
#include "nsIContent.h"
#include "nsIOpenWindowInfo.h"
#include "nsFrameLoader.h"
#include "mozilla/AsyncEventDispatcher.h"
#include "mozilla/dom/HTMLIFrameElement.h"
#include "mozilla/dom/WindowProxyHolder.h"
#include "mozilla/dom/XULFrameElement.h"
#include "mozilla/dom/XULFrameElementBinding.h"

namespace mozilla::dom {

NS_IMPL_CYCLE_COLLECTION_CLASS(XULFrameElement)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(XULFrameElement, nsXULElement)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mFrameLoader)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mOpenWindowInfo)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(XULFrameElement, nsXULElement)
  if (tmp->mFrameLoader) {
    tmp->mFrameLoader->Destroy();
  }
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mOpenWindowInfo)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_ISUPPORTS_CYCLE_COLLECTION_INHERITED(XULFrameElement, nsXULElement,
                                             nsFrameLoaderOwner)

JSObject* XULFrameElement::WrapNode(JSContext* aCx,
                                    JS::Handle<JSObject*> aGivenProto) {
  return XULFrameElement_Binding::Wrap(aCx, this, aGivenProto);
}

nsDocShell* XULFrameElement::GetDocShell() {
  RefPtr<nsFrameLoader> frameLoader = GetFrameLoader();
  return frameLoader ? frameLoader->GetDocShell(IgnoreErrors()) : nullptr;
}

already_AddRefed<nsIWebNavigation> XULFrameElement::GetWebNavigation() {
  nsCOMPtr<nsIDocShell> docShell = GetDocShell();
  nsCOMPtr<nsIWebNavigation> webnav = do_QueryInterface(docShell);
  return webnav.forget();
}

Nullable<WindowProxyHolder> XULFrameElement::GetContentWindow() {
  RefPtr<nsDocShell> docShell = GetDocShell();
  if (docShell) {
    return docShell->GetWindowProxy();
  }

  return nullptr;
}

Document* XULFrameElement::GetContentDocument() {
  nsCOMPtr<nsIDocShell> docShell = GetDocShell();
  if (docShell) {
    nsCOMPtr<nsPIDOMWindowOuter> win = docShell->GetWindow();
    if (win) {
      return win->GetDoc();
    }
  }
  return nullptr;
}

uint64_t XULFrameElement::BrowserId() {
  if (mFrameLoader) {
    if (auto* bc = mFrameLoader->GetExtantBrowsingContext()) {
      return bc->GetBrowserId();
    }
  }
  return 0;
}

nsIOpenWindowInfo* XULFrameElement::GetOpenWindowInfo() const {
  return mOpenWindowInfo;
}

void XULFrameElement::SetOpenWindowInfo(nsIOpenWindowInfo* aInfo) {
  mOpenWindowInfo = aInfo;
}

void XULFrameElement::LoadSrc() {
  if (!IsInUncomposedDoc() || !OwnerDoc()->GetRootElement()) {
    return;
  }
  RefPtr<nsFrameLoader> frameLoader = GetFrameLoader();
  if (!frameLoader) {
    // We may have had a nsIOpenWindowInfo set on us by browser chrome, due to
    // being used as the target for a `window.open` call. Fetch that information
    // if it's available, and clear it out so we don't read it again.
    nsCOMPtr<nsIOpenWindowInfo> openWindowInfo = mOpenWindowInfo.forget();

    // false as the networkCreated parameter so that xul:iframe/browser/editor
    // session history handling works like dynamic html:iframes. Usually xul
    // elements are used in chrome, which doesn't have session history at all.
    mFrameLoader = nsFrameLoader::Create(this, false, openWindowInfo);
    if (NS_WARN_IF(!mFrameLoader)) {
      return;
    }

    AsyncEventDispatcher::RunDOMEventWhenSafe(
        *this, u"XULFrameLoaderCreated"_ns, CanBubble::eYes);
  }

  mFrameLoader->LoadFrame(false);
}

void XULFrameElement::SwapFrameLoaders(HTMLIFrameElement& aOtherLoaderOwner,
                                       ErrorResult& rv) {
  aOtherLoaderOwner.SwapFrameLoaders(this, rv);
}

void XULFrameElement::SwapFrameLoaders(XULFrameElement& aOtherLoaderOwner,
                                       ErrorResult& rv) {
  if (&aOtherLoaderOwner == this) {
    // nothing to do
    return;
  }

  aOtherLoaderOwner.SwapFrameLoaders(this, rv);
}

void XULFrameElement::SwapFrameLoaders(nsFrameLoaderOwner* aOtherLoaderOwner,
                                       mozilla::ErrorResult& rv) {
  if (RefPtr<Document> doc = GetComposedDoc()) {
    // SwapWithOtherLoader relies on frames being up-to-date.
    doc->FlushPendingNotifications(FlushType::Frames);
  }

  RefPtr<nsFrameLoader> loader = GetFrameLoader();
  RefPtr<nsFrameLoader> otherLoader = aOtherLoaderOwner->GetFrameLoader();
  if (!loader || !otherLoader) {
    rv.Throw(NS_ERROR_NOT_IMPLEMENTED);
    return;
  }

  rv = loader->SwapWithOtherLoader(otherLoader, this, aOtherLoaderOwner);
}

nsresult XULFrameElement::BindToTree(BindContext& aContext, nsINode& aParent) {
  nsresult rv = nsXULElement::BindToTree(aContext, aParent);
  NS_ENSURE_SUCCESS(rv, rv);

  if (IsInUncomposedDoc()) {
    NS_ASSERTION(!nsContentUtils::IsSafeToRunScript(),
                 "Missing a script blocker!");
    // We're in a document now.  Kick off the frame load.
    LoadSrc();
  }

  return NS_OK;
}

void XULFrameElement::UnbindFromTree(bool aNullParent) {
  if (RefPtr<nsFrameLoader> frameLoader = GetFrameLoader()) {
    frameLoader->Destroy();
  }
  mFrameLoader = nullptr;

  nsXULElement::UnbindFromTree(aNullParent);
}

void XULFrameElement::DestroyContent() {
  RefPtr<nsFrameLoader> frameLoader = GetFrameLoader();
  if (frameLoader) {
    frameLoader->Destroy();
  }
  mFrameLoader = nullptr;

  nsXULElement::DestroyContent();
}

void XULFrameElement::AfterSetAttr(int32_t aNamespaceID, nsAtom* aName,
                                   const nsAttrValue* aValue,
                                   const nsAttrValue* aOldValue,
                                   nsIPrincipal* aSubjectPrincipal,
                                   bool aNotify) {
  if (aNamespaceID == kNameSpaceID_None) {
    if (aName == nsGkAtoms::src && aValue) {
      LoadSrc();
    } else if (aName == nsGkAtoms::disablefullscreen && mFrameLoader) {
      if (auto* bc = mFrameLoader->GetExtantBrowsingContext()) {
        MOZ_ALWAYS_SUCCEEDS(bc->SetFullscreenAllowedByOwner(!aValue));
      }
    }
  }

  return nsXULElement::AfterSetAttr(aNamespaceID, aName, aValue, aOldValue,
                                    aSubjectPrincipal, aNotify);
}

}  // namespace mozilla::dom
