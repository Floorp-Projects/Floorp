/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsMenuBarFrame.h"
#include "mozilla/BasicEvents.h"
#include "nsIContent.h"
#include "nsAtom.h"
#include "nsPresContext.h"
#include "nsCSSRendering.h"
#include "nsNameSpaceManager.h"
#include "nsGkAtoms.h"
#include "nsMenuPopupFrame.h"
#include "nsUnicharUtils.h"
#include "nsPIDOMWindow.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsCSSFrameConstructor.h"
#ifdef XP_WIN
#  include "nsISound.h"
#  include "nsWidgetsCID.h"
#endif
#include "nsUTF8Utils.h"
#include "mozilla/ComputedStyle.h"
#include "mozilla/PresShell.h"
#include "mozilla/dom/Document.h"
#include "mozilla/dom/XULMenuParentElement.h"
#include "mozilla/dom/XULButtonElement.h"

using namespace mozilla;

//
// NS_NewMenuBarFrame
//
// Wrapper for creating a new menu Bar container
//
nsIFrame* NS_NewMenuBarFrame(PresShell* aPresShell, ComputedStyle* aStyle) {
  return new (aPresShell) nsMenuBarFrame(aStyle, aPresShell->GetPresContext());
}

NS_IMPL_FRAMEARENA_HELPERS(nsMenuBarFrame)

NS_QUERYFRAME_HEAD(nsMenuBarFrame)
  NS_QUERYFRAME_ENTRY(nsMenuBarFrame)
NS_QUERYFRAME_TAIL_INHERITING(nsBoxFrame)

//
// nsMenuBarFrame cntr
//
nsMenuBarFrame::nsMenuBarFrame(ComputedStyle* aStyle,
                               nsPresContext* aPresContext)
    : nsBoxFrame(aStyle, aPresContext, kClassID) {}

void nsMenuBarFrame::Init(nsIContent* aContent, nsContainerFrame* aParent,
                          nsIFrame* aPrevInFlow) {
  nsBoxFrame::Init(aContent, aParent, aPrevInFlow);

  // Create the menu bar listener.
  mMenuBarListener = new nsMenuBarListener(this, aContent);
}

dom::XULMenuParentElement& nsMenuBarFrame::MenubarElement() const {
  auto* content = dom::XULMenuParentElement::FromNode(GetContent());
  MOZ_DIAGNOSTIC_ASSERT(content);
  return *content;
}

MOZ_CAN_RUN_SCRIPT void nsMenuBarFrame::SetActive(bool aActiveFlag) {
  // If the activity is not changed, there is nothing to do.
  if (mIsActive == aActiveFlag) {
    return;
  }

  if (!aActiveFlag) {
    // If there is a request to deactivate the menu bar, check to see whether
    // there is a menu popup open for the menu bar. In this case, don't
    // deactivate the menu bar.
    if (auto* activeChild = MenubarElement().GetActiveMenuChild()) {
      if (activeChild->IsMenuPopupOpen()) {
        return;
      }
    }
  }

  mIsActive = aActiveFlag;
  if (mIsActive) {
    InstallKeyboardNavigator();
  } else {
    mActiveByKeyboard = false;
    RemoveKeyboardNavigator();
  }

  RefPtr menubar = &MenubarElement();
  if (!aActiveFlag) {
    menubar->SetActiveMenuChild(nullptr);
  }

  constexpr auto active = u"DOMMenuBarActive"_ns;
  constexpr auto inactive = u"DOMMenuBarInactive"_ns;
  FireDOMEvent(aActiveFlag ? active : inactive, menubar);
}

void nsMenuBarFrame::InstallKeyboardNavigator() {
  if (nsXULPopupManager* pm = nsXULPopupManager::GetInstance()) {
    pm->SetActiveMenuBar(this, true);
  }
}

void nsMenuBarFrame::MenuClosed() { SetActive(false); }

void nsMenuBarFrame::HandleEnterKeyPress(WidgetEvent& aEvent) {
  if (RefPtr<dom::XULButtonElement> activeChild =
          MenubarElement().GetActiveMenuChild()) {
    activeChild->HandleEnterKeyPress(aEvent);
  }
}

void nsMenuBarFrame::RemoveKeyboardNavigator() {
  if (!mIsActive) {
    if (nsXULPopupManager* pm = nsXULPopupManager::GetInstance()) {
      pm->SetActiveMenuBar(this, false);
    }
  }
}

void nsMenuBarFrame::DestroyFrom(nsIFrame* aDestructRoot,
                                 PostDestroyData& aPostDestroyData) {
  nsXULPopupManager* pm = nsXULPopupManager::GetInstance();
  if (pm) pm->SetActiveMenuBar(this, false);

  mMenuBarListener->OnDestroyMenuBarFrame();
  mMenuBarListener = nullptr;

  nsBoxFrame::DestroyFrom(aDestructRoot, aPostDestroyData);
}
