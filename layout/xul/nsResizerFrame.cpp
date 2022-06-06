/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsCOMPtr.h"
#include "nsResizerFrame.h"
#include "nsIContent.h"
#include "mozilla/PresShell.h"
#include "mozilla/dom/Document.h"
#include "mozilla/UniquePtr.h"
#include "nsGkAtoms.h"
#include "nsNameSpaceManager.h"

#include "nsPresContext.h"
#include "nsFrameManager.h"
#include "nsDocShell.h"
#include "nsIDocShellTreeOwner.h"
#include "nsIBaseWindow.h"
#include "nsPIDOMWindow.h"
#include "mozilla/MouseEvents.h"
#include "nsContentUtils.h"
#include "nsMenuPopupFrame.h"
#include "nsServiceManagerUtils.h"
#include "nsIScreenManager.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/MouseEventBinding.h"
#include "nsError.h"
#include "nsICSSDeclaration.h"
#include "nsStyledElement.h"
#include <algorithm>

using namespace mozilla;

//
// NS_NewResizerFrame
//
// Creates a new Resizer frame and returns it
//
nsIFrame* NS_NewResizerFrame(PresShell* aPresShell, ComputedStyle* aStyle) {
  return new (aPresShell) nsResizerFrame(aStyle, aPresShell->GetPresContext());
}

NS_IMPL_FRAMEARENA_HELPERS(nsResizerFrame)

nsResizerFrame::nsResizerFrame(ComputedStyle* aStyle,
                               nsPresContext* aPresContext)
    : nsTitleBarFrame(aStyle, aPresContext, kClassID) {}

