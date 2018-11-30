/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "VisualViewport.h"
#include "nsIScrollableFrame.h"
#include "nsIDocShell.h"

using namespace mozilla;
using namespace mozilla::dom;

VisualViewport::VisualViewport(nsPIDOMWindowInner* aWindow)
    : DOMEventTargetHelper(aWindow) {}

VisualViewport::~VisualViewport() {}

/* virtual */
JSObject* VisualViewport::WrapObject(JSContext* aCx,
                                     JS::Handle<JSObject*> aGivenProto) {
  return VisualViewport_Binding::Wrap(aCx, this, aGivenProto);
}

CSSSize VisualViewport::VisualViewportSize() const {
  CSSSize size = CSSSize(0, 0);

  nsIPresShell* presShell = GetPresShell();
  if (presShell) {
    if (presShell->IsVisualViewportSizeSet()) {
      size = CSSRect::FromAppUnits(presShell->GetVisualViewportSize());
    } else {
      nsIScrollableFrame* sf = presShell->GetRootScrollFrameAsScrollable();
      if (sf) {
        size = CSSRect::FromAppUnits(sf->GetScrollPortRect().Size());
      }
    }
  }
  return size;
}

double VisualViewport::Width() const {
  CSSSize size = VisualViewportSize();
  return size.width;
}

double VisualViewport::Height() const {
  CSSSize size = VisualViewportSize();
  return size.height;
}

double VisualViewport::Scale() const {
  double scale = 1;
  nsIPresShell* presShell = GetPresShell();
  if (presShell) {
    scale = presShell->GetResolution();
  }
  return scale;
}

CSSPoint VisualViewport::VisualViewportOffset() const {
  CSSPoint offset = CSSPoint(0, 0);

  nsIPresShell* presShell = GetPresShell();
  if (presShell) {
    offset = CSSPoint::FromAppUnits(presShell->GetVisualViewportOffset());
  }
  return offset;
}

CSSPoint VisualViewport::LayoutViewportOffset() const {
  CSSPoint offset = CSSPoint(0, 0);

  nsIPresShell* presShell = GetPresShell();
  if (presShell) {
    nsIScrollableFrame* sf = presShell->GetRootScrollFrameAsScrollable();
    if (sf) {
      offset = CSSPoint::FromAppUnits(sf->GetScrollPosition());
    }
  }
  return offset;
}

double VisualViewport::PageLeft() const { return VisualViewportOffset().X(); }

double VisualViewport::PageTop() const { return VisualViewportOffset().Y(); }

double VisualViewport::OffsetLeft() const {
  return PageLeft() - LayoutViewportOffset().X();
}

double VisualViewport::OffsetTop() const {
  return PageTop() - LayoutViewportOffset().Y();
}

nsIPresShell* VisualViewport::GetPresShell() const {
  nsCOMPtr<nsPIDOMWindowInner> window = GetOwner();
  if (!window) {
    return nullptr;
  }

  nsIDocShell* docShell = window->GetDocShell();
  if (!docShell) {
    return nullptr;
  }

  return docShell->GetPresShell();
}
