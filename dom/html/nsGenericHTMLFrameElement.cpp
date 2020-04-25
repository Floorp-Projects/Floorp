/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsGenericHTMLFrameElement.h"

#include "mozilla/dom/HTMLIFrameElement.h"
#include "mozilla/dom/XULFrameElement.h"
#include "mozilla/dom/BrowserBridgeChild.h"
#include "mozilla/dom/WindowProxyHolder.h"
#include "mozilla/Preferences.h"
#include "mozilla/PresShell.h"
#include "mozilla/StaticPrefs_dom.h"
#include "mozilla/ErrorResult.h"
#include "GeckoProfiler.h"
#include "nsAttrValueInlines.h"
#include "nsContentUtils.h"
#include "nsIDocShell.h"
#include "nsIFrame.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIPermissionManager.h"
#include "nsPresContext.h"
#include "nsServiceManagerUtils.h"
#include "nsSubDocumentFrame.h"
#include "nsAttrValueOrString.h"

using namespace mozilla;
using namespace mozilla::dom;

NS_IMPL_CYCLE_COLLECTION_CLASS(nsGenericHTMLFrameElement)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(nsGenericHTMLFrameElement,
                                                  nsGenericHTMLElement)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mFrameLoader)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mBrowserElementAPI)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(nsGenericHTMLFrameElement,
                                                nsGenericHTMLElement)
  if (tmp->mFrameLoader) {
    tmp->mFrameLoader->Destroy();
  }

  NS_IMPL_CYCLE_COLLECTION_UNLINK(mFrameLoader)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mBrowserElementAPI)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_ISUPPORTS_CYCLE_COLLECTION_INHERITED(
    nsGenericHTMLFrameElement, nsGenericHTMLElement, nsFrameLoaderOwner,
    nsIDOMMozBrowserFrame, nsIMozBrowserFrame, nsGenericHTMLFrameElement)

NS_IMETHODIMP
nsGenericHTMLFrameElement::GetMozbrowser(bool* aValue) {
  *aValue = GetBoolAttr(nsGkAtoms::mozbrowser);
  return NS_OK;
}
NS_IMETHODIMP
nsGenericHTMLFrameElement::SetMozbrowser(bool aValue) {
  return SetBoolAttr(nsGkAtoms::mozbrowser, aValue);
}

int32_t nsGenericHTMLFrameElement::TabIndexDefault() { return 0; }

nsGenericHTMLFrameElement::~nsGenericHTMLFrameElement() {
  if (mFrameLoader) {
    mFrameLoader->Destroy();
  }
}

Document* nsGenericHTMLFrameElement::GetContentDocument(
    nsIPrincipal& aSubjectPrincipal) {
  RefPtr<BrowsingContext> bc = GetContentWindowInternal();
  if (!bc) {
    return nullptr;
  }

  nsPIDOMWindowOuter* window = bc->GetDOMWindow();
  if (!window) {
    // Either our browsing context contents are out-of-process (in which case
    // clearly this is a cross-origin call and we should return null), or our
    // browsing context is torn-down enough to no longer have a window or a
    // document, and we should still return null.
    return nullptr;
  }
  Document* doc = window->GetDoc();
  if (!doc) {
    return nullptr;
  }

  // Return null for cross-origin contentDocument.
  if (!aSubjectPrincipal.SubsumesConsideringDomain(doc->NodePrincipal())) {
    return nullptr;
  }
  return doc;
}

BrowsingContext* nsGenericHTMLFrameElement::GetContentWindowInternal() {
  EnsureFrameLoader();

  if (!mFrameLoader) {
    return nullptr;
  }

  if (mFrameLoader->DepthTooGreat()) {
    // Claim to have no contentWindow
    return nullptr;
  }

  RefPtr<BrowsingContext> bc = mFrameLoader->GetBrowsingContext();
  return bc;
}

Nullable<WindowProxyHolder> nsGenericHTMLFrameElement::GetContentWindow() {
  RefPtr<BrowsingContext> bc = GetContentWindowInternal();
  if (!bc) {
    return nullptr;
  }
  return WindowProxyHolder(bc);
}

void nsGenericHTMLFrameElement::EnsureFrameLoader() {
  if (!IsInComposedDoc() || mFrameLoader || OwnerDoc()->IsStaticDocument()) {
    // If frame loader is there, we just keep it around, cached
    return;
  }

  // Strangely enough, this method doesn't actually ensure that the
  // frameloader exists.  It's more of a best-effort kind of thing.
  mFrameLoader = nsFrameLoader::Create(this, mNetworkCreated);
}

void nsGenericHTMLFrameElement::SwapFrameLoaders(
    HTMLIFrameElement& aOtherLoaderOwner, ErrorResult& rv) {
  if (&aOtherLoaderOwner == this) {
    // nothing to do
    return;
  }

  aOtherLoaderOwner.SwapFrameLoaders(this, rv);
}

void nsGenericHTMLFrameElement::SwapFrameLoaders(
    XULFrameElement& aOtherLoaderOwner, ErrorResult& rv) {
  aOtherLoaderOwner.SwapFrameLoaders(this, rv);
}

void nsGenericHTMLFrameElement::SwapFrameLoaders(
    nsFrameLoaderOwner* aOtherLoaderOwner, mozilla::ErrorResult& rv) {
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

void nsGenericHTMLFrameElement::LoadSrc() {
  EnsureFrameLoader();

  if (!mFrameLoader) {
    return;
  }

  bool origSrc = !mSrcLoadHappened;
  mSrcLoadHappened = true;
  mFrameLoader->LoadFrame(origSrc);
}

nsresult nsGenericHTMLFrameElement::BindToTree(BindContext& aContext,
                                               nsINode& aParent) {
  nsresult rv = nsGenericHTMLElement::BindToTree(aContext, aParent);
  NS_ENSURE_SUCCESS(rv, rv);

  if (IsInComposedDoc()) {
    NS_ASSERTION(!nsContentUtils::IsSafeToRunScript(),
                 "Missing a script blocker!");

    AUTO_PROFILER_LABEL("nsGenericHTMLFrameElement::BindToTree", OTHER);

    // We're in a document now.  Kick off the frame load.
    LoadSrc();
  }

  // We're now in document and scripts may move us, so clear
  // the mNetworkCreated flag.
  mNetworkCreated = false;
  return rv;
}

void nsGenericHTMLFrameElement::UnbindFromTree(bool aNullParent) {
  if (mFrameLoader) {
    // This iframe is being taken out of the document, destroy the
    // iframe's frame loader (doing that will tear down the window in
    // this iframe).
    // XXXbz we really want to only partially destroy the frame
    // loader... we don't want to tear down the docshell.  Food for
    // later bug.
    mFrameLoader->Destroy();
    mFrameLoader = nullptr;
  }

  nsGenericHTMLElement::UnbindFromTree(aNullParent);
}

/* static */
ScrollbarPreference nsGenericHTMLFrameElement::MapScrollingAttribute(
    const nsAttrValue* aValue) {
  if (aValue && aValue->Type() == nsAttrValue::eEnum) {
    switch (aValue->GetEnumValue()) {
      case NS_STYLE_FRAME_OFF:
      case NS_STYLE_FRAME_NOSCROLL:
      case NS_STYLE_FRAME_NO:
        return ScrollbarPreference::Never;
    }
  }
  return ScrollbarPreference::Auto;
}

/* virtual */
nsresult nsGenericHTMLFrameElement::AfterSetAttr(
    int32_t aNameSpaceID, nsAtom* aName, const nsAttrValue* aValue,
    const nsAttrValue* aOldValue, nsIPrincipal* aMaybeScriptedPrincipal,
    bool aNotify) {
  if (aValue) {
    nsAttrValueOrString value(aValue);
    AfterMaybeChangeAttr(aNameSpaceID, aName, &value, aMaybeScriptedPrincipal,
                         aNotify);
  } else {
    AfterMaybeChangeAttr(aNameSpaceID, aName, nullptr, aMaybeScriptedPrincipal,
                         aNotify);
  }

  if (aNameSpaceID == kNameSpaceID_None) {
    if (aName == nsGkAtoms::scrolling) {
      if (mFrameLoader) {
        ScrollbarPreference pref = MapScrollingAttribute(aValue);
        if (nsIDocShell* docshell = mFrameLoader->GetExistingDocShell()) {
          nsDocShell::Cast(docshell)->SetScrollbarPreference(pref);
        } else if (auto* child = mFrameLoader->GetBrowserBridgeChild()) {
          // NOTE(emilio): We intentionally don't deal with the
          // GetBrowserParent() case, and only deal with the fission iframe
          // case. We could make it work, but it's a bit of boilerplate for
          // something that we don't use, and we'd need to think how it
          // interacts with the scrollbar window flags...
          child->SendScrollbarPreferenceChanged(pref);
        }
      }
    } else if (aName == nsGkAtoms::mozbrowser) {
      mReallyIsBrowser =
          !!aValue && StaticPrefs::dom_mozBrowserFramesEnabled() &&
          XRE_IsParentProcess() && NodePrincipal()->IsSystemPrincipal();
    }
  }

  return nsGenericHTMLElement::AfterSetAttr(
      aNameSpaceID, aName, aValue, aOldValue, aMaybeScriptedPrincipal, aNotify);
}

nsresult nsGenericHTMLFrameElement::OnAttrSetButNotChanged(
    int32_t aNamespaceID, nsAtom* aName, const nsAttrValueOrString& aValue,
    bool aNotify) {
  AfterMaybeChangeAttr(aNamespaceID, aName, &aValue, nullptr, aNotify);

  return nsGenericHTMLElement::OnAttrSetButNotChanged(aNamespaceID, aName,
                                                      aValue, aNotify);
}

void nsGenericHTMLFrameElement::AfterMaybeChangeAttr(
    int32_t aNamespaceID, nsAtom* aName, const nsAttrValueOrString* aValue,
    nsIPrincipal* aMaybeScriptedPrincipal, bool aNotify) {
  if (aNamespaceID == kNameSpaceID_None) {
    if (aName == nsGkAtoms::src) {
      mSrcTriggeringPrincipal = nsContentUtils::GetAttrTriggeringPrincipal(
          this, aValue ? aValue->String() : EmptyString(),
          aMaybeScriptedPrincipal);
      if (!IsHTMLElement(nsGkAtoms::iframe) ||
          !HasAttr(kNameSpaceID_None, nsGkAtoms::srcdoc)) {
        // Don't propagate error here. The attribute was successfully
        // set or removed; that's what we should reflect.
        LoadSrc();
      }
    } else if (aName == nsGkAtoms::name) {
      // Propagate "name" to the browsing context per HTML5.
      RefPtr<BrowsingContext> bc =
          mFrameLoader ? mFrameLoader->GetExtantBrowsingContext() : nullptr;
      if (bc) {
        if (aValue) {
          bc->SetName(aValue->String());
        } else {
          bc->SetName(EmptyString());
        }
      }
    }
  }
}

void nsGenericHTMLFrameElement::DestroyContent() {
  if (mFrameLoader) {
    mFrameLoader->Destroy();
    mFrameLoader = nullptr;
  }

  nsGenericHTMLElement::DestroyContent();
}

nsresult nsGenericHTMLFrameElement::CopyInnerTo(Element* aDest) {
  nsresult rv = nsGenericHTMLElement::CopyInnerTo(aDest);
  NS_ENSURE_SUCCESS(rv, rv);

  Document* doc = aDest->OwnerDoc();
  if (doc->IsStaticDocument() && mFrameLoader) {
    nsGenericHTMLFrameElement* dest =
        static_cast<nsGenericHTMLFrameElement*>(aDest);
    doc->AddPendingFrameStaticClone(dest, mFrameLoader);
  }

  return rv;
}

bool nsGenericHTMLFrameElement::IsHTMLFocusable(bool aWithMouse,
                                                bool* aIsFocusable,
                                                int32_t* aTabIndex) {
  if (nsGenericHTMLElement::IsHTMLFocusable(aWithMouse, aIsFocusable,
                                            aTabIndex)) {
    return true;
  }

  *aIsFocusable = nsContentUtils::IsSubDocumentTabbable(this);

  if (!*aIsFocusable && aTabIndex) {
    *aTabIndex = -1;
  }

  return false;
}

/**
 * Return true if this frame element really is a mozbrowser.  (It
 * needs to have the right attributes, and its creator must have the right
 * permissions.)
 */
/* [infallible] */
nsresult nsGenericHTMLFrameElement::GetReallyIsBrowser(bool* aOut) {
  *aOut = mReallyIsBrowser;
  return NS_OK;
}

NS_IMETHODIMP
nsGenericHTMLFrameElement::InitializeBrowserAPI() {
  MOZ_ASSERT(mFrameLoader);
  InitBrowserElementAPI();
  return NS_OK;
}

NS_IMETHODIMP
nsGenericHTMLFrameElement::DestroyBrowserFrameScripts() {
  MOZ_ASSERT(mFrameLoader);
  DestroyBrowserElementFrameScripts();
  return NS_OK;
}
