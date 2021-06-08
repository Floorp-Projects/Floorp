/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "OuterDocAccessible.h"

#include "LocalAccessible-inl.h"
#include "nsAccUtils.h"
#include "DocAccessible-inl.h"
#include "mozilla/a11y/DocAccessibleChild.h"
#include "mozilla/a11y/DocAccessibleParent.h"
#if defined(XP_WIN)
#  include "mozilla/a11y/ProxyWrappers.h"
#endif
#include "mozilla/dom/BrowserBridgeChild.h"
#include "mozilla/dom/BrowserParent.h"
#include "Role.h"
#include "States.h"

#ifdef A11Y_LOG
#  include "Logging.h"
#endif

using namespace mozilla;
using namespace mozilla::a11y;

////////////////////////////////////////////////////////////////////////////////
// OuterDocAccessible
////////////////////////////////////////////////////////////////////////////////

OuterDocAccessible::OuterDocAccessible(nsIContent* aContent,
                                       DocAccessible* aDoc)
    : AccessibleWrap(aContent, aDoc) {
  mType = eOuterDocType;

#ifdef XP_WIN
  if (DocAccessibleParent* remoteDoc = RemoteChildDoc()) {
    remoteDoc->SendParentCOMProxy(this);
  }
#endif

  if (IPCAccessibilityActive()) {
    auto bridge = dom::BrowserBridgeChild::GetFrom(aContent);
    if (bridge) {
      // This is an iframe which will be rendered in another process.
      SendEmbedderAccessible(bridge);
    }
  }

  // Request document accessible for the content document to make sure it's
  // created. It will appended to outerdoc accessible children asynchronously.
  dom::Document* outerDoc = mContent->GetUncomposedDoc();
  if (outerDoc) {
    dom::Document* innerDoc = outerDoc->GetSubDocumentFor(mContent);
    if (innerDoc) GetAccService()->GetDocAccessible(innerDoc);
  }
}

OuterDocAccessible::~OuterDocAccessible() {}

void OuterDocAccessible::SendEmbedderAccessible(
    dom::BrowserBridgeChild* aBridge) {
  MOZ_ASSERT(mDoc);
  DocAccessibleChild* ipcDoc = mDoc->IPCDoc();
  if (ipcDoc) {
    uint64_t id = reinterpret_cast<uintptr_t>(UniqueID());
#if defined(XP_WIN)
    ipcDoc->SetEmbedderOnBridge(aBridge, id);
#else
    aBridge->SendSetEmbedderAccessible(ipcDoc, id);
#endif
  }
}

////////////////////////////////////////////////////////////////////////////////
// LocalAccessible public (DON'T add methods here)

role OuterDocAccessible::NativeRole() const { return roles::INTERNAL_FRAME; }

LocalAccessible* OuterDocAccessible::LocalChildAtPoint(
    int32_t aX, int32_t aY, EWhichChildAtPoint aWhichChild) {
  nsIntRect docRect = Bounds();
  if (!docRect.Contains(aX, aY)) return nullptr;

  // Always return the inner doc as direct child accessible unless bounds
  // outside of it.
  LocalAccessible* child = LocalChildAt(0);
  NS_ENSURE_TRUE(child, nullptr);

  if (aWhichChild == Accessible::EWhichChildAtPoint::DeepestChild) {
#if defined(XP_WIN)
    // On Windows, OuterDocAccessible::GetChildAt can return a proxy wrapper
    // for a remote document. These aren't real Accessibles and
    // shouldn't be returned except to the Windows a11y code (which doesn't use
    // eDeepestChild). Calling ChildAtPoint on these will crash!
    return nullptr;
#else
    return child->LocalChildAtPoint(
        aX, aY, Accessible::EWhichChildAtPoint::DeepestChild);
#endif  // defined(XP_WIN)
  }
  return child;
}

////////////////////////////////////////////////////////////////////////////////
// LocalAccessible public

void OuterDocAccessible::Shutdown() {
#ifdef A11Y_LOG
  if (logging::IsEnabled(logging::eDocDestroy)) logging::OuterDocDestroy(this);
#endif

  LocalAccessible* child = mChildren.SafeElementAt(0, nullptr);
  if (child) {
#ifdef A11Y_LOG
    if (logging::IsEnabled(logging::eDocDestroy)) {
      logging::DocDestroy("outerdoc's child document rebind is scheduled",
                          child->AsDoc()->DocumentNode());
    }
#endif
    RemoveChild(child);

    // XXX: sometimes outerdoc accessible is shutdown because of layout style
    // change however the presshell of underlying document isn't destroyed and
    // the document doesn't get pagehide events. Schedule a document rebind
    // to its parent document. Otherwise a document accessible may be lost if
    // its outerdoc has being recreated (see bug 862863 for details).
    if (!mDoc->IsDefunct()) {
      MOZ_ASSERT(!child->IsDefunct(),
                 "Attempt to reattach shutdown document accessible");
      if (!child->IsDefunct()) {
        mDoc->BindChildDocument(child->AsDoc());
      }
    }
  }

  AccessibleWrap::Shutdown();
}

bool OuterDocAccessible::InsertChildAt(uint32_t aIdx,
                                       LocalAccessible* aAccessible) {
  MOZ_RELEASE_ASSERT(aAccessible->IsDoc(),
                     "OuterDocAccessible can have a document child only!");

  // We keep showing the old document for a bit after creating the new one,
  // and while building the new DOM and frame tree. That's done on purpose
  // to avoid weird flashes of default background color.
  // The old viewer will be destroyed after the new one is created.
  // For a11y, it should be safe to shut down the old document now.
  if (mChildren.Length()) mChildren[0]->Shutdown();

  if (!AccessibleWrap::InsertChildAt(0, aAccessible)) return false;

#ifdef A11Y_LOG
  if (logging::IsEnabled(logging::eDocCreate)) {
    logging::DocCreate("append document to outerdoc",
                       aAccessible->AsDoc()->DocumentNode());
    logging::Address("outerdoc", this);
  }
#endif

  return true;
}

bool OuterDocAccessible::RemoveChild(LocalAccessible* aAccessible) {
  LocalAccessible* child = mChildren.SafeElementAt(0, nullptr);
  MOZ_ASSERT(child == aAccessible, "Wrong child to remove!");
  if (child != aAccessible) {
    return false;
  }

#ifdef A11Y_LOG
  if (logging::IsEnabled(logging::eDocDestroy)) {
    logging::DocDestroy("remove document from outerdoc",
                        child->AsDoc()->DocumentNode(), child->AsDoc());
    logging::Address("outerdoc", this);
  }
#endif

  bool wasRemoved = AccessibleWrap::RemoveChild(child);

  NS_ASSERTION(!mChildren.Length(),
               "This child document of outerdoc accessible wasn't removed!");

  return wasRemoved;
}

bool OuterDocAccessible::IsAcceptableChild(nsIContent* aEl) const {
  // outer document accessible doesn't not participate in ordinal tree
  // mutations.
  return false;
}

#if defined(XP_WIN)

LocalAccessible* OuterDocAccessible::RemoteChildDocAccessible() const {
  RemoteAccessible* docProxy = RemoteChildDoc();
  if (docProxy) {
    // We're in the parent process, but we're embedding a remote document.
    return WrapperFor(docProxy);
  }

  if (IPCAccessibilityActive()) {
    auto bridge = dom::BrowserBridgeChild::GetFrom(mContent);
    if (bridge) {
      // We're an iframe in a content process and we're embedding a remote
      // document (in another content process). The COM proxy for the embedded
      // document accessible was sent to us from the parent via PBrowserBridge.
      return bridge->GetEmbeddedDocAccessible();
    }
  }

  return nullptr;
}

// On Windows e10s, since we don't cache in the chrome process, LocalChildAt
// must be implemented so that we properly cross the chrome-to-content
// boundary when traversing.

LocalAccessible* OuterDocAccessible::LocalChildAt(uint32_t aIndex) const {
  LocalAccessible* result = AccessibleWrap::LocalChildAt(aIndex);
  if (result || aIndex) {
    return result;
  }
  // If we are asking for child 0 and GetChildAt doesn't return anything, try
  // to get the remote child doc and return that instead.
  return RemoteChildDocAccessible();
}

#endif  // defined(XP_WIN)

// Accessible

uint32_t OuterDocAccessible::ChildCount() const {
  uint32_t result = mChildren.Length();
  if (!result &&
#if defined(XP_WIN)
      ((StaticPrefs::accessibility_cache_enabled_AtStartup() &&
        RemoteChildDoc()) ||
       // On Windows with the cache disabled, as well as returning 1 for a
       // remote document in the parent process, we also need to return 1 in a
       // content process for an OOP iframe.
       RemoteChildDocAccessible())
#else
      RemoteChildDoc()
#endif
  ) {
    result = 1;
  }
  return result;
}

Accessible* OuterDocAccessible::ChildAt(uint32_t aIndex) const {
  // We deliberately bypass OuterDocAccessible::LocalChildAt on Windows because
  // it will return a RemoteAccessibleWrap for a remote document.
  LocalAccessible* result = AccessibleWrap::LocalChildAt(aIndex);
  if (result || aIndex) {
    return result;
  }

  return RemoteChildDoc();
}

Accessible* OuterDocAccessible::ChildAtPoint(int32_t aX, int32_t aY,
                                             EWhichChildAtPoint aWhichChild) {
  nsIntRect docRect = Bounds();
  if (!docRect.Contains(aX, aY)) return nullptr;

  // Always return the inner doc as direct child accessible unless bounds
  // outside of it.
  Accessible* child = ChildAt(0);
  NS_ENSURE_TRUE(child, nullptr);

  if (aWhichChild == EWhichChildAtPoint::DeepestChild) {
    return child->ChildAtPoint(aX, aY, EWhichChildAtPoint::DeepestChild);
  }
  return child;
}

DocAccessibleParent* OuterDocAccessible::RemoteChildDoc() const {
  dom::BrowserParent* tab = dom::BrowserParent::GetFrom(GetContent());
  if (!tab) {
    return nullptr;
  }

  return tab->GetTopLevelDocAccessible();
}
