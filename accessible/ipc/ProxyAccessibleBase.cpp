/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DocAccessible.h"
#include "mozilla/a11y/DocAccessibleParent.h"
#include "mozilla/a11y/DocManager.h"
#include "mozilla/a11y/Platform.h"
#include "mozilla/a11y/ProxyAccessibleBase.h"
#include "mozilla/a11y/ProxyAccessible.h"
#include "mozilla/a11y/Role.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/BrowserParent.h"
#include "mozilla/Unused.h"
#include "RelationType.h"
#include "xpcAccessibleDocument.h"

namespace mozilla {
namespace a11y {

template <class Derived>
void ProxyAccessibleBase<Derived>::Shutdown() {
  MOZ_DIAGNOSTIC_ASSERT(!IsDoc());
  xpcAccessibleDocument* xpcDoc =
      GetAccService()->GetCachedXPCDocument(Document());
  if (xpcDoc) {
    xpcDoc->NotifyOfShutdown(static_cast<Derived*>(this));
  }

  // XXX Ideally  this wouldn't be necessary, but it seems OuterDoc accessibles
  // can be destroyed before the doc they own.
  uint32_t childCount = mChildren.Length();
  if (!mOuterDoc) {
    for (uint32_t idx = 0; idx < childCount; idx++) mChildren[idx]->Shutdown();
  } else {
    if (childCount > 1) {
      MOZ_CRASH("outer doc has too many documents!");
    } else if (childCount == 1) {
      mChildren[0]->AsDoc()->Unbind();
    }
  }

  mChildren.Clear();
  ProxyDestroyed(static_cast<Derived*>(this));
  mDoc->RemoveAccessible(static_cast<Derived*>(this));
}

template <class Derived>
void ProxyAccessibleBase<Derived>::SetChildDoc(DocAccessibleParent* aChildDoc) {
  MOZ_ASSERT(aChildDoc);
  MOZ_ASSERT(mChildren.Length() == 0);
  mChildren.AppendElement(aChildDoc);
  mOuterDoc = true;
}

template <class Derived>
void ProxyAccessibleBase<Derived>::ClearChildDoc(
    DocAccessibleParent* aChildDoc) {
  MOZ_ASSERT(aChildDoc);
  // This is possible if we're replacing one document with another: Doc 1
  // has not had a chance to remove itself, but was already replaced by Doc 2
  // in SetChildDoc(). This could result in two subsequent calls to
  // ClearChildDoc() even though mChildren.Length() == 1.
  MOZ_ASSERT(mChildren.Length() <= 1);
  mChildren.RemoveElement(aChildDoc);
}

template <class Derived>
uint32_t ProxyAccessibleBase<Derived>::EmbeddedChildCount() const {
  size_t count = 0, kids = mChildren.Length();
  for (size_t i = 0; i < kids; i++) {
    if (mChildren[i]->IsEmbeddedObject()) {
      count++;
    }
  }

  return count;
}

template <class Derived>
int32_t ProxyAccessibleBase<Derived>::IndexOfEmbeddedChild(
    const Derived* aChild) {
  size_t index = 0, kids = mChildren.Length();
  for (size_t i = 0; i < kids; i++) {
    if (mChildren[i]->IsEmbeddedObject()) {
      if (mChildren[i] == aChild) {
        return index;
      }

      index++;
    }
  }

  return -1;
}

template <class Derived>
Derived* ProxyAccessibleBase<Derived>::EmbeddedChildAt(size_t aChildIdx) {
  size_t index = 0, kids = mChildren.Length();
  for (size_t i = 0; i < kids; i++) {
    if (!mChildren[i]->IsEmbeddedObject()) {
      continue;
    }

    if (index == aChildIdx) {
      return mChildren[i];
    }

    index++;
  }

  return nullptr;
}

template <class Derived>
Accessible* ProxyAccessibleBase<Derived>::OuterDocOfRemoteBrowser() const {
  auto tab = static_cast<dom::BrowserParent*>(mDoc->Manager());
  dom::Element* frame = tab->GetOwnerElement();
  NS_ASSERTION(frame, "why isn't the tab in a frame!");
  if (!frame) return nullptr;

  DocAccessible* chromeDoc = GetExistingDocAccessible(frame->OwnerDoc());

  return chromeDoc ? chromeDoc->GetAccessible(frame) : nullptr;
}

template <class Derived>
void ProxyAccessibleBase<Derived>::SetParent(Derived* aParent) {
  MOZ_ASSERT(IsDoc(), "we should only reparent documents");
  if (!aParent) {
    mParent = kNoParent;
  } else {
    MOZ_ASSERT(!aParent->IsDoc());
    mParent = aParent->ID();
  }
}

template <class Derived>
Derived* ProxyAccessibleBase<Derived>::Parent() const {
  if (mParent == kNoParent) {
    return nullptr;
  }

  // if we are not a document then are parent is another proxy in the same
  // document.  That means we can just ask our document for the proxy with our
  // parent id.
  if (!IsDoc()) {
    return Document()->GetAccessible(mParent);
  }

  // If we are a top level document then our parent is not a proxy.
  if (AsDoc()->IsTopLevel()) {
    return nullptr;
  }

  // Finally if we are a non top level document then our parent id is for a
  // proxy in our parent document so get the proxy from there.
  DocAccessibleParent* parentDoc = AsDoc()->ParentDoc();
  MOZ_ASSERT(parentDoc);
  MOZ_ASSERT(mParent);
  return parentDoc->GetAccessible(mParent);
}

template class ProxyAccessibleBase<ProxyAccessible>;

}  // namespace a11y
}  // namespace mozilla
