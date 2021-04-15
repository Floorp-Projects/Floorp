/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//
// nsMenuFrame
//

#ifndef nsMenuFrame_h__
#define nsMenuFrame_h__

#include "nsAtom.h"
#include "nsCOMPtr.h"

#include "nsBoxFrame.h"
#include "nsFrameList.h"
#include "nsGkAtoms.h"
#include "nsMenuParent.h"
#include "nsXULPopupManager.h"
#include "nsINamed.h"
#include "nsIReflowCallback.h"
#include "nsITimer.h"
#include "mozilla/Attributes.h"

namespace mozilla {
class PresShell;
}  // namespace mozilla

nsIFrame* NS_NewMenuFrame(mozilla::PresShell* aPresShell,
                          mozilla::ComputedStyle*);
nsIFrame* NS_NewMenuItemFrame(mozilla::PresShell* aPresShell,
                              mozilla::ComputedStyle*);

class nsIContent;

namespace mozilla {
namespace dom {
class Element;
}  // namespace dom
}  // namespace mozilla

// the type of menuitem
enum nsMenuType {
  // a normal menuitem where a command is carried out when activated
  eMenuType_Normal = 0,
  // a menuitem with a checkmark that toggles when activated
  eMenuType_Checkbox = 1,
  // a radio menuitem where only one of it and its siblings with the same
  // name attribute can be checked at a time
  eMenuType_Radio = 2
};

class nsMenuFrame;

/**
 * nsMenuTimerMediator is a wrapper around an nsMenuFrame which can be safely
 * passed to timers. The class is reference counted unlike the underlying
 * nsMenuFrame, so that it will exist as long as the timer holds a reference
 * to it. The callback is delegated to the contained nsMenuFrame as long as
 * the contained nsMenuFrame has not been destroyed.
 */
class nsMenuTimerMediator final : public nsITimerCallback, public nsINamed {
 public:
  explicit nsMenuTimerMediator(nsMenuFrame* aFrame);

  NS_DECL_ISUPPORTS
  NS_DECL_NSITIMERCALLBACK
  NS_DECL_NSINAMED

  void ClearFrame();

 private:
  ~nsMenuTimerMediator();

  // Pointer to the wrapped frame.
  nsMenuFrame* mFrame;
};

class nsMenuFrame final : public nsBoxFrame, public nsIReflowCallback {
 public:
  explicit nsMenuFrame(ComputedStyle* aStyle, nsPresContext* aPresContext);

  NS_DECL_QUERYFRAME
  NS_DECL_FRAMEARENA_HELPERS(nsMenuFrame)

  NS_IMETHOD DoXULLayout(nsBoxLayoutState& aBoxLayoutState) override;
  virtual nsSize GetXULMinSize(nsBoxLayoutState& aBoxLayoutState) override;
  virtual nsSize GetXULPrefSize(nsBoxLayoutState& aBoxLayoutState) override;

  virtual void Init(nsIContent* aContent, nsContainerFrame* aParent,
                    nsIFrame* aPrevInFlow) override;

  // The following methods are all overridden so that the menupopup
  // can be stored in a separate list, so that it doesn't impact reflow of the
  // actual menu item at all.
  virtual const nsFrameList& GetChildList(ChildListID aList) const override;
  virtual void GetChildLists(nsTArray<ChildList>* aLists) const override;
  virtual void DestroyFrom(nsIFrame* aDestructRoot,
                           PostDestroyData& aPostDestroyData) override;

  // Overridden to prevent events from going to children of the menu.
  virtual void BuildDisplayListForChildren(
      nsDisplayListBuilder* aBuilder, const nsDisplayListSet& aLists) override;

  // this method can destroy the frame
  virtual nsresult HandleEvent(nsPresContext* aPresContext,
                               mozilla::WidgetGUIEvent* aEvent,
                               nsEventStatus* aEventStatus) override;

  virtual void SetInitialChildList(ChildListID aListID,
                                   nsFrameList& aChildList) override;
  virtual void AppendFrames(ChildListID aListID,
                            nsFrameList& aFrameList) override;
  virtual void InsertFrames(ChildListID aListID, nsIFrame* aPrevFrame,
                            const nsLineList::iterator* aPrevFrameLine,
                            nsFrameList& aFrameList) override;
  virtual void RemoveFrame(ChildListID aListID, nsIFrame* aOldFrame) override;

  NS_IMETHOD SelectMenu(bool aActivateFlag);

  virtual nsIScrollableFrame* GetScrollTargetFrame() const override;

  /**
   * NOTE: OpenMenu will open the menu asynchronously.
   */
  void OpenMenu(bool aSelectFirstItem);
  // CloseMenu closes the menu asynchronously
  void CloseMenu(bool aDeselectMenu);

  bool IsChecked() { return mChecked; }

  NS_IMETHOD GetActiveChild(mozilla::dom::Element** aResult);
  NS_IMETHOD SetActiveChild(mozilla::dom::Element* aChild);

  // called when the Enter key is pressed while the menuitem is the current
  // one in its parent popup. This will carry out the command attached to
  // the menuitem. If the menu should be opened, this frame will be returned,
  // otherwise null will be returned.
  nsMenuFrame* Enter(mozilla::WidgetGUIEvent* aEvent);

  // Return the nearest menu bar or menupopup ancestor frame.
  nsMenuParent* GetMenuParent() const;

  const nsAString& GetRadioGroupName() { return mGroupName; }
  nsMenuType GetMenuType() { return mType; }
  nsMenuPopupFrame* GetPopup() const;

  /**
   * @return true if this frame has a popup child frame.
   */
  bool HasPopup() const {
    return HasAnyStateBits(NS_STATE_MENU_HAS_POPUP_LIST);
  }

  // nsMenuFrame methods

  bool IsOnMenuBar() const {
    nsMenuParent* menuParent = GetMenuParent();
    return menuParent && menuParent->IsMenuBar();
  }
  bool IsOnActiveMenuBar() const {
    nsMenuParent* menuParent = GetMenuParent();
    return menuParent && menuParent->IsMenuBar() && menuParent->IsActive();
  }
  virtual bool IsOpen();
  virtual bool IsMenu();
  bool IsParentMenuList();
  bool IsDisabled();
  void ToggleMenuState();

  // Activate this menu item.
  void ActivateItem(mozilla::Modifiers aModifiers);

  // indiciate that the menu's popup has just been opened, so that the menu
  // can update its open state. This method modifies the open attribute on
  // the menu, so the frames could be gone after this call.
  void PopupOpened();
  // indiciate that the menu's popup has just been closed, so that the menu
  // can update its open state. The menu should be unhighlighted if
  // aDeselectedMenu is true. This method modifies the open attribute on
  // the menu, so the frames could be gone after this call.
  void PopupClosed(bool aDeselectMenu);

  // returns true if this is a menu on another menu popup. A menu is a submenu
  // if it has a parent popup or menupopup.
  bool IsOnMenu() const {
    nsMenuParent* menuParent = GetMenuParent();
    return menuParent && menuParent->IsMenu();
  }
  void SetIsMenu(bool aIsMenu) { mIsMenu = aIsMenu; }

#ifdef DEBUG_FRAME_DUMP
  virtual nsresult GetFrameName(nsAString& aResult) const override {
    return MakeFrameName(u"Menu"_ns, aResult);
  }
#endif

  static bool IsSizedToPopup(nsIContent* aContent, bool aRequireAlways);

  // nsIReflowCallback
  virtual bool ReflowFinished() override;
  virtual void ReflowCallbackCanceled() override;

 protected:
  friend class nsMenuTimerMediator;
  friend class nsASyncMenuInitialization;
  friend class nsMenuAttributeChangedEvent;

  /**
   * Initialize the popup list to the first popup frame within
   * aChildList. Removes the popup, if any, from aChildList.
   */
  void SetPopupFrame(nsFrameList& aChildList);

  /**
   * Get the popup frame list from the frame property.
   * @return the property value if it exists, nullptr otherwise.
   */
  nsFrameList* GetPopupList() const;

  /**
   * Destroy the popup list property.  The list must exist and be empty.
   */
  void DestroyPopupList();

  // Update the menu's type (normal, checkbox, radio).
  // This method can destroy the frame.
  void UpdateMenuType();
  // Update the checked state of the menu, and for radios, clear any other
  // checked items. This method can destroy the frame.
  void UpdateMenuSpecialState();

  // Called to execute our command handler. This method can destroy the frame.
  void Execute(mozilla::WidgetGUIEvent* aEvent);

  // This method can destroy the frame
  virtual nsresult AttributeChanged(int32_t aNameSpaceID, nsAtom* aAttribute,
                                    int32_t aModType) override;
  virtual ~nsMenuFrame() = default;

  bool SizeToPopup(nsBoxLayoutState& aState, nsSize& aSize);

  bool ShouldBlink();
  void StartBlinking();
  void StopBlinking();
  void CreateMenuCommandEvent(bool aIsTrusted, mozilla::Modifiers aModifiers);
  void PassMenuCommandEventToPopupManager();

 protected:
  nsresult Notify(nsITimer* aTimer);

  bool mIsMenu;   // Whether or not we can even have children or not.
  bool mChecked;  // are we checked?
  bool mReflowCallbackPosted;
  nsMenuType mType;

  // Reference to the mediator which wraps this frame.
  RefPtr<nsMenuTimerMediator> mTimerMediator;

  nsCOMPtr<nsITimer> mOpenTimer;
  nsCOMPtr<nsITimer> mBlinkTimer;

  uint8_t mBlinkState;  // 0: not blinking, 1: off, 2: on
  RefPtr<nsXULMenuCommandEvent> mDelayedMenuCommandEvent;

  nsString mGroupName;

};  // class nsMenuFrame

#endif
