/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//
// nsMenuBarFrame
//

#ifndef nsMenuBarFrame_h__
#define nsMenuBarFrame_h__

#include "nsAtom.h"
#include "nsCOMPtr.h"
#include "nsBoxFrame.h"
#include "nsMenuBarListener.h"

class nsIContent;

namespace mozilla {
class PresShell;
namespace dom {
class KeyboardEvent;
class XULMenuParentElement;
}  // namespace dom
}  // namespace mozilla

nsIFrame* NS_NewMenuBarFrame(mozilla::PresShell* aPresShell,
                             mozilla::ComputedStyle* aStyle);

class nsMenuBarFrame final : public nsBoxFrame {
 public:
  NS_DECL_QUERYFRAME
  NS_DECL_FRAMEARENA_HELPERS(nsMenuBarFrame)

  explicit nsMenuBarFrame(ComputedStyle* aStyle, nsPresContext* aPresContext);

  void InstallKeyboardNavigator();
  void RemoveKeyboardNavigator();
  MOZ_CAN_RUN_SCRIPT void MenuClosed();

  void Init(nsIContent* aContent, nsContainerFrame* aParent,
            nsIFrame* aPrevInFlow) override;

  void DestroyFrom(nsIFrame* aDestructRoot,
                   PostDestroyData& aPostDestroyData) override;

  bool IsActiveByKeyboard() { return mActiveByKeyboard; }
  void SetActiveByKeyboard() { mActiveByKeyboard = true; }
  MOZ_CAN_RUN_SCRIPT void SetActive(bool aActive);
  bool IsActive() const { return mIsActive; }

  mozilla::dom::XULMenuParentElement& MenubarElement() const;

  // Called when Enter is pressed while the menubar is focused. If the current
  // menu is open, let the child handle the key.
  MOZ_CAN_RUN_SCRIPT void HandleEnterKeyPress(mozilla::WidgetEvent&);

  bool IsFrameOfType(uint32_t aFlags) const override {
    // Override bogus IsFrameOfType in nsBoxFrame.
    if (aFlags & (nsIFrame::eReplacedContainsBlock | nsIFrame::eReplaced))
      return false;
    return nsBoxFrame::IsFrameOfType(aFlags);
  }

#ifdef DEBUG_FRAME_DUMP
  nsresult GetFrameName(nsAString& aResult) const override {
    return MakeFrameName(u"MenuBar"_ns, aResult);
  }
#endif

 protected:
  RefPtr<nsMenuBarListener> mMenuBarListener;  // The listener that tells us
                                               // about key and mouse events.

  bool mIsActive = false;  // Whether or not the menu bar is active (a menu item
                           // is highlighted or shown).
  // Whether the menubar was made active via the keyboard.
  bool mActiveByKeyboard = false;
};  // class nsMenuBarFrame

#endif
