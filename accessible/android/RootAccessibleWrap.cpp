/* -*- Mode: c++; c-basic-offset: 2; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "RootAccessibleWrap.h"

#include "LocalAccessible-inl.h"

#include "DocAccessibleParent.h"
#include "DocAccessible-inl.h"
#include "RemoteAccessibleWrap.h"
#include "SessionAccessibility.h"
#include "mozilla/PresShell.h"
#include "mozilla/dom/EventTarget.h"
#include "mozilla/dom/Event.h"
#include "mozilla/dom/MouseEvent.h"

using namespace mozilla;
using namespace mozilla::a11y;
using namespace mozilla::dom;

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

nsresult RootAccessibleWrap::AddEventListeners() {
  nsPIDOMWindowOuter* window = mDocumentNode->GetWindow();
  nsCOMPtr<EventTarget> nstarget = window ? window->GetParentTarget() : nullptr;

  if (nstarget) {
    nstarget->AddEventListener(u"MozMouseExploreByTouch"_ns, this, false, true);
  }

  return RootAccessible::AddEventListeners();
}

nsresult RootAccessibleWrap::RemoveEventListeners() {
  nsPIDOMWindowOuter* window = mDocumentNode->GetWindow();
  nsCOMPtr<EventTarget> nstarget = window ? window->GetParentTarget() : nullptr;
  if (nstarget) {
    nstarget->RemoveEventListener(u"MozMouseExploreByTouch"_ns, this, true);
  }

  return RootAccessible::RemoveEventListeners();
}

////////////////////////////////////////////////////////////////////////////////
// nsIDOMEventListener

NS_IMETHODIMP
RootAccessibleWrap::HandleEvent(Event* aDOMEvent) {
  WidgetMouseEvent* widgetEvent = aDOMEvent->WidgetEventPtr()->AsMouseEvent();
  if (widgetEvent && widgetEvent->mMessage == eMouseExploreByTouch) {
    if (HasShutdown()) {
      return NS_OK;
    }

    MouseEvent* mouseEvent = aDOMEvent->AsMouseEvent();
    if (mouseEvent) {
      nsPresContext* pc = PresContext();

      int32_t x =
          pc->CSSPixelsToDevPixels(mouseEvent->ScreenX(CallerType::System));
      int32_t y =
          pc->CSSPixelsToDevPixels(mouseEvent->ScreenY(CallerType::System));

      ExploreByTouch(x, y);
    }

    return NS_OK;
  }

  return RootAccessible::HandleEvent(aDOMEvent);
}
