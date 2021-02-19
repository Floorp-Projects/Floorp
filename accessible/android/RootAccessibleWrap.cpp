/* -*- Mode: c++; c-basic-offset: 2; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "RootAccessibleWrap.h"

#include "LocalAccessible-inl.h"
#include "AccessibleOrProxy.h"
#include "DocAccessibleParent.h"
#include "DocAccessible-inl.h"
#include "RemoteAccessibleWrap.h"
#include "SessionAccessibility.h"
#include "mozilla/PresShell.h"

using namespace mozilla;
using namespace mozilla::a11y;

RootAccessibleWrap::RootAccessibleWrap(dom::Document* aDoc,
                                       PresShell* aPresShell)
    : RootAccessible(aDoc, aPresShell) {}

RootAccessibleWrap::~RootAccessibleWrap() {}

AccessibleWrap* RootAccessibleWrap::GetContentAccessible() {
  if (RemoteAccessible* proxy = GetPrimaryRemoteTopLevelContentDoc()) {
    return WrapperFor(proxy);
  }

  // Find first document that is not defunct or hidden.
  // This is exclusively for Fennec which has a deck of browser elements.
  // Otherwise, standard GeckoView will only have one browser element.
  for (size_t i = 0; i < ChildDocumentCount(); i++) {
    DocAccessible* childDoc = GetChildDocumentAt(i);
    if (childDoc && !childDoc->IsDefunct() && !childDoc->IsHidden()) {
      return childDoc;
    }
  }

  return nullptr;
}

AccessibleWrap* RootAccessibleWrap::FindAccessibleById(int32_t aID) {
  AccessibleWrap* contentAcc = GetContentAccessible();

  if (!contentAcc) {
    return nullptr;
  }

  if (aID == AccessibleWrap::kNoID) {
    return contentAcc;
  }

  if (contentAcc->IsProxy()) {
    return FindAccessibleById(static_cast<DocRemoteAccessibleWrap*>(contentAcc),
                              aID);
  }

  return FindAccessibleById(
      static_cast<DocAccessibleWrap*>(contentAcc->AsDoc()), aID);
}

AccessibleWrap* RootAccessibleWrap::FindAccessibleById(
    DocRemoteAccessibleWrap* aDoc, int32_t aID) {
  AccessibleWrap* acc = aDoc->GetAccessibleByID(aID);
  uint32_t index = 0;
  while (!acc) {
    auto child = static_cast<DocRemoteAccessibleWrap*>(
        aDoc->GetChildDocumentAt(index++));
    if (!child) {
      break;
    }
    // A child document's id is not in its parent document's hash table.
    if (child->VirtualViewID() == aID) {
      acc = child;
    } else {
      acc = FindAccessibleById(child, aID);
    }
  }

  return acc;
}

AccessibleWrap* RootAccessibleWrap::FindAccessibleById(DocAccessibleWrap* aDoc,
                                                       int32_t aID) {
  AccessibleWrap* acc = aDoc->GetAccessibleByID(aID);
  uint32_t index = 0;
  while (!acc) {
    auto child =
        static_cast<DocAccessibleWrap*>(aDoc->GetChildDocumentAt(index++));
    if (!child) {
      break;
    }
    acc = FindAccessibleById(child, aID);
  }

  return acc;
}
