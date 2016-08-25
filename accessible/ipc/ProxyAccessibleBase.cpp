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
#include "mozilla/dom/TabParent.h"
#include "mozilla/Unused.h"
#include "RelationType.h"
#include "xpcAccessibleDocument.h"

namespace mozilla {
namespace a11y {

template <class Derived>
void
ProxyAccessibleBase<Derived>::Shutdown()
{
  MOZ_DIAGNOSTIC_ASSERT(!IsDoc());
  NS_ASSERTION(!mOuterDoc, "Why do we still have a child doc?");
  xpcAccessibleDocument* xpcDoc =
    GetAccService()->GetCachedXPCDocument(Document());
  if (xpcDoc) {
    xpcDoc->NotifyOfShutdown(static_cast<Derived*>(this));
  }

  // XXX Ideally  this wouldn't be necessary, but it seems OuterDoc accessibles
  // can be destroyed before the doc they own.
  if (!mOuterDoc) {
    uint32_t childCount = mChildren.Length();
    for (uint32_t idx = 0; idx < childCount; idx++)
      mChildren[idx]->Shutdown();
  } else {
    if (mChildren.Length() != 1)
      MOZ_CRASH("outer doc doesn't own adoc!");

    mChildren[0]->AsDoc()->Unbind();
  }

  mChildren.Clear();
  ProxyDestroyed(static_cast<Derived*>(this));
  mDoc->RemoveAccessible(static_cast<Derived*>(this));
}

template <class Derived>
void
ProxyAccessibleBase<Derived>::SetChildDoc(DocAccessibleParent* aParent)
{
  if (aParent) {
    MOZ_ASSERT(mChildren.IsEmpty());
    mChildren.AppendElement(aParent);
    mOuterDoc = true;
  } else {
    MOZ_ASSERT(mChildren.Length() == 1);
    mChildren.Clear();
    mOuterDoc = false;
  }
}

template <class Derived>
bool
ProxyAccessibleBase<Derived>::MustPruneChildren() const
{
  // this is the equivalent to nsAccUtils::MustPrune for proxies and should be
  // kept in sync with that.
  if (mRole != roles::MENUITEM && mRole != roles::COMBOBOX_OPTION
      && mRole != roles::OPTION && mRole != roles::ENTRY
      && mRole != roles::FLAT_EQUATION && mRole != roles::PASSWORD_TEXT
      && mRole != roles::PUSHBUTTON && mRole != roles::TOGGLE_BUTTON
      && mRole != roles::GRAPHIC && mRole != roles::SLIDER
      && mRole != roles::PROGRESSBAR && mRole != roles::SEPARATOR)
    return false;

  if (mChildren.Length() != 1)
    return false;

  return mChildren[0]->Role() == roles::TEXT_LEAF
    || mChildren[0]->Role() == roles::STATICTEXT;
}

template <class Derived>
uint32_t
ProxyAccessibleBase<Derived>::EmbeddedChildCount() const
{
  size_t count = 0, kids = mChildren.Length();
  for (size_t i = 0; i < kids; i++) {
    if (mChildren[i]->IsEmbeddedObject()) {
      count++;
    }
  }

  return count;
}

template <class Derived>
int32_t
ProxyAccessibleBase<Derived>::IndexOfEmbeddedChild(const Derived* aChild)
{
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
Derived*
ProxyAccessibleBase<Derived>::EmbeddedChildAt(size_t aChildIdx)
{
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
Accessible*
ProxyAccessibleBase<Derived>::OuterDocOfRemoteBrowser() const
{
  auto tab = static_cast<dom::TabParent*>(mDoc->Manager());
  dom::Element* frame = tab->GetOwnerElement();
  NS_ASSERTION(frame, "why isn't the tab in a frame!");
  if (!frame)
    return nullptr;

  DocAccessible* chromeDoc = GetExistingDocAccessible(frame->OwnerDoc());

  return chromeDoc ? chromeDoc->GetAccessible(frame) : nullptr;
}

template class ProxyAccessibleBase<ProxyAccessible>;

} // namespace a11y
} // namespace mozilla
