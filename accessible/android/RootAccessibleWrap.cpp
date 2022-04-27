/* -*- Mode: c++; c-basic-offset: 2; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "RootAccessibleWrap.h"

#include "LocalAccessible-inl.h"

#include "DocAccessibleParent.h"
#include "DocAccessible-inl.h"
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

    if (MouseEvent* mouseEvent = aDOMEvent->AsMouseEvent()) {
      LayoutDeviceIntPoint point = mouseEvent->ScreenPointLayoutDevicePix();
      ExploreByTouch(point.x, point.y);
    }

    return NS_OK;
  }

  return RootAccessible::HandleEvent(aDOMEvent);
}
