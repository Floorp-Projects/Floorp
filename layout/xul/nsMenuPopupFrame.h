/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//
// nsMenuPopupFrame
//

#ifndef nsMenuPopupFrame_h__
#define nsMenuPopupFrame_h__

#include "mozilla/Attributes.h"
#include "nsIAtom.h"
#include "nsGkAtoms.h"
#include "nsCOMPtr.h"
#include "nsMenuFrame.h"

#include "nsBoxFrame.h"
#include "nsMenuParent.h"

#include "nsITimer.h"

class nsIWidget;

// XUL popups can be in several different states. When opening a popup, the
// state changes as follows:
//   ePopupClosed - initial state
//   ePopupShowing - during the period when the popupshowing event fires
//   ePopupOpening - between the popupshowing event and being visible. Creation
//                   of the child frames, layout and reflow occurs in this
//                   state. The popup is stored in the popup manager's list of
//                   open popups during this state.
//   ePopupVisible - layout is done and the popup's view and widget are made
//                   visible. The popup is visible on screen but may be
//                   transitioning. The popupshown event has not yet fired.
//   ePopupShown - the popup has been shown and is fully ready. This state is
//                 assigned just before the popupshown event fires.
// When closing a popup:
//   ePopupHidden - during the period when the popuphiding event fires and
//                  the popup is removed.
//   ePopupClosed - the popup's widget is made invisible.
enum nsPopupState {
  // state when a popup is not open
  ePopupClosed,
  // state from when a popup is requested to be shown to after the
  // popupshowing event has been fired.
  ePopupShowing,
  // state while a popup is open but the widget is not yet visible
  ePopupOpening,
  // state while a popup is visible and waiting for the popupshown event
  ePopupVisible,
  // state while a popup is open and visible on screen
  ePopupShown,
  // state from when a popup is requested to be hidden to when it is closed.
  ePopupHiding,
  // state which indicates that the popup was hidden without firing the
  // popuphiding or popuphidden events. It is used when executing a menu
  // command because the menu needs to be hidden before the command event
  // fires, yet the popuphiding and popuphidden events are fired after. This
  // state can also occur when the popup is removed because the document is
  // unloaded.
  ePopupInvisible
};

// How a popup may be flipped. Flipping to the outside edge is like how
// a submenu would work. The entire popup is flipped to the opposite side
// of the anchor.
enum FlipStyle {
  FlipStyle_None = 0,
  FlipStyle_Outside = 1,
  FlipStyle_Inside = 2
};

// Values for the flip attribute
enum FlipType {
  FlipType_Default = 0,
  FlipType_None = 1,    // don't try to flip or translate to stay onscreen
  FlipType_Both = 2,    // flip in both directions
  FlipType_Slide = 3    // allow the arrow to "slide" instead of resizing
};

// values are selected so that the direction can be flipped just by
// changing the sign
#define POPUPALIGNMENT_NONE 0
#define POPUPALIGNMENT_TOPLEFT 1
#define POPUPALIGNMENT_TOPRIGHT -1
#define POPUPALIGNMENT_BOTTOMLEFT 2
#define POPUPALIGNMENT_BOTTOMRIGHT -2

#define POPUPALIGNMENT_LEFTCENTER 16
#define POPUPALIGNMENT_RIGHTCENTER -16
#define POPUPALIGNMENT_TOPCENTER 17
#define POPUPALIGNMENT_BOTTOMCENTER 18

// The constants here are selected so that horizontally and vertically flipping
// can be easily handled using the two flip macros below.
#define POPUPPOSITION_UNKNOWN -1
#define POPUPPOSITION_BEFORESTART 0
#define POPUPPOSITION_BEFOREEND 1
#define POPUPPOSITION_AFTERSTART 2
#define POPUPPOSITION_AFTEREND 3
#define POPUPPOSITION_STARTBEFORE 4
#define POPUPPOSITION_ENDBEFORE 5
#define POPUPPOSITION_STARTAFTER 6
#define POPUPPOSITION_ENDAFTER 7
#define POPUPPOSITION_OVERLAP 8
#define POPUPPOSITION_AFTERPOINTER 9

#define POPUPPOSITION_HFLIP(v) (v ^ 1)
#define POPUPPOSITION_VFLIP(v) (v ^ 2)

#define INC_TYP_INTERVAL  1000  // 1s. If the interval between two keypresses is shorter than this, 
                                //   treat as a continue typing
// XXX, kyle.yuan@sun.com, there are 4 definitions for the same purpose:
//  nsMenuPopupFrame.h, nsListControlFrame.cpp, listbox.xml, tree.xml
//  need to find a good place to put them together.
//  if someone changes one, please also change the other.

#define CONTEXT_MENU_OFFSET_PIXELS 2

nsIFrame* NS_NewMenuPopupFrame(nsIPresShell* aPresShell, nsStyleContext* aContext);

class nsViewManager;
class nsView;
class nsMenuPopupFrame;

// this class is used for dispatching popupshown events asynchronously.
class nsXULPopupShownEvent : public nsRunnable, public nsIDOMEventListener
{
public:
  nsXULPopupShownEvent(nsIContent *aPopup, nsPresContext* aPresContext)
    : mPopup(aPopup), mPresContext(aPresContext)
  {
  }

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIRUNNABLE
  NS_DECL_NSIDOMEVENTLISTENER

  void CancelListener();

protected:
  virtual ~nsXULPopupShownEvent() { }

private:
  nsCOMPtr<nsIContent> mPopup;
  nsRefPtr<nsPresContext> mPresContext;
};

class nsMenuPopupFrame MOZ_FINAL : public nsBoxFrame, public nsMenuParent
{
public:
  NS_DECL_QUERYFRAME_TARGET(nsMenuPopupFrame)
  NS_DECL_QUERYFRAME
  NS_DECL_FRAMEARENA_HELPERS

  nsMenuPopupFrame(nsIPresShell* aShell, nsStyleContext* aContext);

  // nsMenuParent interface
  virtual nsMenuFrame* GetCurrentMenuItem() MOZ_OVERRIDE;
  NS_IMETHOD SetCurrentMenuItem(nsMenuFrame* aMenuItem) MOZ_OVERRIDE;
  virtual void CurrentMenuIsBeingDestroyed() MOZ_OVERRIDE;
  NS_IMETHOD ChangeMenuItem(nsMenuFrame* aMenuItem, bool aSelectFirstItem) MOZ_OVERRIDE;

  // as popups are opened asynchronously, the popup pending state is used to
  // prevent multiple requests from attempting to open the same popup twice
  nsPopupState PopupState() { return mPopupState; }
  void SetPopupState(nsPopupState aPopupState) { mPopupState = aPopupState; }

  NS_IMETHOD SetActive(bool aActiveFlag) MOZ_OVERRIDE { return NS_OK; } // We don't care.
  virtual bool IsActive() MOZ_OVERRIDE { return false; }
  virtual bool IsMenuBar() MOZ_OVERRIDE { return false; }

  /*
   * When this popup is open, should clicks outside of it be consumed?
   * Return true if the popup should rollup on an outside click, 
   * but consume that click so it can't be used for anything else.
   * Return false to allow clicks outside the popup to activate content 
   * even when the popup is open.
   * ---------------------------------------------------------------------
   * 
   * Should clicks outside of a popup be eaten?
   *
   *       Menus     Autocomplete     Comboboxes
   * Mac     Eat           No              Eat
   * Win     No            No              Eat     
   * Unix    Eat           No              Eat
   *
   */
  bool ConsumeOutsideClicks();

  virtual bool IsContextMenu() MOZ_OVERRIDE { return mIsContextMenu; }

  virtual bool MenuClosed() MOZ_OVERRIDE { return true; }

  virtual void LockMenuUntilClosed(bool aLock) MOZ_OVERRIDE;
  virtual bool IsMenuLocked() MOZ_OVERRIDE { return mIsMenuLocked; }

  nsIWidget* GetWidget();

  // The dismissal listener gets created and attached to the window.
  void AttachedDismissalListener();

  // Overridden methods
  virtual void Init(nsIContent*       aContent,
                    nsContainerFrame* aParent,
                    nsIFrame*         aPrevInFlow) MOZ_OVERRIDE;

  virtual nsresult AttributeChanged(int32_t aNameSpaceID,
                                    nsIAtom* aAttribute,
                                    int32_t aModType) MOZ_OVERRIDE;

  virtual void DestroyFrom(nsIFrame* aDestructRoot) MOZ_OVERRIDE;

  // returns true if the popup is a panel with the noautohide attribute set to
  // true. These panels do not roll up automatically.
  bool IsNoAutoHide() const;

  nsPopupLevel PopupLevel() const
  {
    return PopupLevel(IsNoAutoHide()); 
  }

  void EnsureWidget();

  nsresult CreateWidgetForView(nsView* aView);
  uint8_t GetShadowStyle();

  virtual void SetInitialChildList(ChildListID  aListID,
                                   nsFrameList& aChildList) MOZ_OVERRIDE;

  virtual bool IsLeaf() const MOZ_OVERRIDE;

  // layout, position and display the popup as needed
  void LayoutPopup(nsBoxLayoutState& aState, nsIFrame* aParentMenu,
                   nsIFrame* aAnchor, bool aSizedToPopup);

  nsView* GetRootViewForPopup(nsIFrame* aStartFrame);

  // set the position of the popup either relative to the anchor aAnchorFrame
  // (or the frame for mAnchorContent if aAnchorFrame is null) or at a specific
  // point if a screen position (mScreenXPos and mScreenYPos) are set. The popup
  // will be adjusted so that it is on screen. If aIsMove is true, then the popup
  // is being moved, and should not be flipped.
  nsresult SetPopupPosition(nsIFrame* aAnchorFrame, bool aIsMove, bool aSizedToPopup);

  bool HasGeneratedChildren() { return mGeneratedChildren; }
  void SetGeneratedChildren() { mGeneratedChildren = true; }

  // called when the Enter key is pressed while the popup is open. This will
  // just pass the call down to the current menu, if any. If a current menu
  // should be opened as a result, this method should return the frame for
  // that menu, or null if no menu should be opened. Also, calling Enter will
  // reset the current incremental search string, calculated in
  // FindMenuWithShortcut.
  nsMenuFrame* Enter(mozilla::WidgetGUIEvent* aEvent);

  nsPopupType PopupType() const { return mPopupType; }
  bool IsMenu() MOZ_OVERRIDE { return mPopupType == ePopupTypeMenu; }
  bool IsOpen() MOZ_OVERRIDE { return mPopupState == ePopupOpening ||
                                      mPopupState == ePopupVisible ||
                                      mPopupState == ePopupShown; }
  bool IsVisible() { return mPopupState == ePopupVisible ||
                            mPopupState == ePopupShown; }

  bool IsMouseTransparent() { return mMouseTransparent; }

  static nsIContent* GetTriggerContent(nsMenuPopupFrame* aMenuPopupFrame);
  void ClearTriggerContent() { mTriggerContent = nullptr; }

  // returns true if the popup is in a content shell, or false for a popup in
  // a chrome shell
  bool IsInContentShell() { return mInContentShell; }

  // the Initialize methods are used to set the anchor position for
  // each way of opening a popup.
  void InitializePopup(nsIContent* aAnchorContent,
                       nsIContent* aTriggerContent,
                       const nsAString& aPosition,
                       int32_t aXPos, int32_t aYPos,
                       bool aAttributesOverride);

  /**
   * @param aIsContextMenu if true, then the popup is
   * positioned at a slight offset from aXPos/aYPos to ensure the
   * (presumed) mouse position is not over the menu.
   */
  void InitializePopupAtScreen(nsIContent* aTriggerContent,
                               int32_t aXPos, int32_t aYPos,
                               bool aIsContextMenu);

  void InitializePopupWithAnchorAlign(nsIContent* aAnchorContent,
                                      nsAString& aAnchor,
                                      nsAString& aAlign,
                                      int32_t aXPos, int32_t aYPos);

  // indicate that the popup should be opened
  void ShowPopup(bool aIsContextMenu, bool aSelectFirstItem);
  // indicate that the popup should be hidden. The new state should either be
  // ePopupClosed or ePopupInvisible.
  void HidePopup(bool aDeselectMenu, nsPopupState aNewState);

  // locate and return the menu frame that should be activated for the
  // supplied key event. If doAction is set to true by this method,
  // then the menu's action should be carried out, as if the user had pressed
  // the Enter key. If doAction is false, the menu should just be highlighted.
  // This method also handles incremental searching in menus so the user can
  // type the first few letters of an item/s name to select it.
  nsMenuFrame* FindMenuWithShortcut(nsIDOMKeyEvent* aKeyEvent, bool& doAction);

  void ClearIncrementalString() { mIncrementalString.Truncate(); }

  virtual nsIAtom* GetType() const MOZ_OVERRIDE { return nsGkAtoms::menuPopupFrame; }

#ifdef DEBUG_FRAME_DUMP
  virtual nsresult GetFrameName(nsAString& aResult) const MOZ_OVERRIDE
  {
      return MakeFrameName(NS_LITERAL_STRING("MenuPopup"), aResult);
  }
#endif

  void EnsureMenuItemIsVisible(nsMenuFrame* aMenuFrame);

  // Move the popup to the screen coordinate (aLeft, aTop) in CSS pixels.
  // If aUpdateAttrs is true, and the popup already has left or top attributes,
  // then those attributes are updated to the new location.
  // The frame may be destroyed by this method.
  void MoveTo(int32_t aLeft, int32_t aTop, bool aUpdateAttrs);

  void MoveToAnchor(nsIContent* aAnchorContent,
                    const nsAString& aPosition,
                    int32_t aXPos, int32_t aYPos,
                    bool aAttributesOverride);

  bool GetAutoPosition();
  void SetAutoPosition(bool aShouldAutoPosition);
  void SetConsumeRollupEvent(uint32_t aConsumeMode);

  nsIScrollableFrame* GetScrollFrame(nsIFrame* aStart);

  // For a popup that should appear anchored at the given rect, determine
  // the screen area that it is constrained by. This will be the available
  // area of the screen the popup should be displayed on. Content popups,
  // however, will also be constrained by the content area, given by
  // aRootScreenRect. All coordinates are in app units.
  // For non-toplevel popups (which will always be panels), we will also
  // constrain them to the available screen rect, ie they will not fall
  // underneath the taskbar, dock or other fixed OS elements.
  nsRect GetConstraintRect(const nsRect& aAnchorRect, const nsRect& aRootScreenRect,
                           nsPopupLevel aPopupLevel);

  // Determines whether the given edges of the popup may be moved, where
  // aHorizontalSide and aVerticalSide are one of the NS_SIDE_* constants, or
  // 0 for no movement in that direction. aChange is the distance to move on
  // those sides. If will be reset to 0 if the side cannot be adjusted at all
  // in that direction. For example, a popup cannot be moved if it is anchored
  // on a particular side.
  //
  // Later, when bug 357725 is implemented, we can make this adjust aChange by
  // the amount that the side can be resized, so that minimums and maximums
  // can be taken into account.
  void CanAdjustEdges(int8_t aHorizontalSide, int8_t aVerticalSide, nsIntPoint& aChange);

  // Return true if the popup is positioned relative to an anchor.
  bool IsAnchored() const { return mScreenXPos == -1 && mScreenYPos == -1; }

  // Return the anchor if there is one.
  nsIContent* GetAnchor() const { return mAnchorContent; }

  // Return the screen coordinates of the popup, or (-1, -1) if anchored.
  // This position is in CSS pixels.
  nsIntPoint ScreenPosition() const { return nsIntPoint(mScreenXPos, mScreenYPos); }

  nsIntPoint GetLastClientOffset() const { return mLastClientOffset; }

  // Return the alignment of the popup
  int8_t GetAlignmentPosition() const;

  // Return the offset applied to the alignment of the popup
  nscoord GetAlignmentOffset() const { return mAlignmentOffset; }

  // Clear the mPopupShownDispatcher, remove the listener and return true if
  // mPopupShownDispatcher was non-null.
  bool ClearPopupShownDispatcher()
  {
    if (mPopupShownDispatcher) {
      mPopupShownDispatcher->CancelListener();
      mPopupShownDispatcher = nullptr;
      return true;
    }

    return false;
  }

protected:

  // returns the popup's level.
  nsPopupLevel PopupLevel(bool aIsNoAutoHide) const;

  // redefine to tell the box system not to move the views.
  virtual void GetLayoutFlags(uint32_t& aFlags) MOZ_OVERRIDE;

  void InitPositionFromAnchorAlign(const nsAString& aAnchor,
                                   const nsAString& aAlign);

  // return the position where the popup should be, when it should be
  // anchored at anchorRect. aHFlip and aVFlip will be set if the popup may be
  // flipped in that direction if there is not enough space available.
  nsPoint AdjustPositionForAnchorAlign(nsRect& anchorRect,
                                       FlipStyle& aHFlip, FlipStyle& aVFlip);

  // check if the popup will fit into the available space and resize it. This
  // method handles only one axis at a time so is called twice, once for
  // horizontal and once for vertical. All arguments are specified for this
  // one axis. All coordinates are in app units relative to the screen.
  //   aScreenPoint - the point where the popup should appear
  //   aSize - the size of the popup
  //   aScreenBegin - the left or top edge of the screen
  //   aScreenEnd - the right or bottom edge of the screen
  //   aAnchorBegin - the left or top edge of the anchor rectangle
  //   aAnchorEnd - the right or bottom edge of the anchor rectangle
  //   aMarginBegin - the left or top margin of the popup
  //   aMarginEnd - the right or bottom margin of the popup
  //   aOffsetForContextMenu - the additional offset to add for context menus
  //   aFlip - how to flip or resize the popup when there isn't space
  //   aFlipSide - pointer to where current flip mode is stored
  nscoord FlipOrResize(nscoord& aScreenPoint, nscoord aSize, 
                       nscoord aScreenBegin, nscoord aScreenEnd,
                       nscoord aAnchorBegin, nscoord aAnchorEnd,
                       nscoord aMarginBegin, nscoord aMarginEnd,
                       nscoord aOffsetForContextMenu, FlipStyle aFlip,
                       bool* aFlipSide);

  // check if the popup can fit into the available space by "sliding" (i.e.,
  // by having the anchor arrow slide along one axis and only resizing if that
  // can't provide the requested size). Only one axis can be slid - the other
  // axis is "flipped" as normal. This method can handle either axis, but is
  // only called for the sliding axis. All coordinates are in app units
  // relative to the screen.
  //   aScreenPoint - the point where the popup should appear
  //   aSize - the size of the popup
  //   aScreenBegin - the left or top edge of the screen
  //   aScreenEnd - the right or bottom edge of the screen
  //   aOffset - the amount by which the arrow must be slid such that it is
  //             still aligned with the anchor.
  // Result is the new size of the popup, which will typically be the same
  // as aSize, unless aSize is greater than the screen width/height.
  nscoord SlideOrResize(nscoord& aScreenPoint, nscoord aSize,
                        nscoord aScreenBegin, nscoord aScreenEnd,
                        nscoord *aOffset);

  // Move the popup to the position specified in its |left| and |top| attributes.
  void MoveToAttributePosition();

  /**
   * Return whether the popup direction should be RTL.
   * If the popup has an anchor, its direction is the anchor direction.
   * Otherwise, its the general direction of the UI.
   *
   * Return whether the popup direction should be RTL.
   */
  bool IsDirectionRTL() const {
    return mAnchorContent && mAnchorContent->GetPrimaryFrame()
      ? mAnchorContent->GetPrimaryFrame()->StyleVisibility()->mDirection == NS_STYLE_DIRECTION_RTL
      : StyleVisibility()->mDirection == NS_STYLE_DIRECTION_RTL;
  }

  // Create a popup view for this frame. The view is added a child of the root
  // view, and is initially hidden.
  void CreatePopupView();

  nsString     mIncrementalString;  // for incremental typing navigation

  // the content that the popup is anchored to, if any, which may be in a
  // different document than the popup.
  nsCOMPtr<nsIContent> mAnchorContent;

  // the content that triggered the popup, typically the node where the mouse
  // was clicked. It will be cleared when the popup is hidden.
  nsCOMPtr<nsIContent> mTriggerContent;

  nsMenuFrame* mCurrentMenu; // The current menu that is active.

  nsRefPtr<nsXULPopupShownEvent> mPopupShownDispatcher;

  // A popup's preferred size may be different than its actual size stored in
  // mRect in the case where the popup was resized because it was too large
  // for the screen. The preferred size mPrefSize holds the full size the popup
  // would be before resizing. Computations are performed using this size.
  nsSize mPrefSize;

  // The position of the popup, in CSS pixels.
  // The screen coordinates, if set to values other than -1,
  // override mXPos and mYPos.
  int32_t mXPos;
  int32_t mYPos;
  int32_t mScreenXPos;
  int32_t mScreenYPos;

  // If the panel prefers to "slide" rather than resize, then the arrow gets
  // positioned at this offset (along either the x or y axis, depending on
  // mPosition)
  nscoord mAlignmentOffset;

  // The value of the client offset of our widget the last time we positioned
  // ourselves. We store this so that we can detect when it changes but the
  // position of our widget didn't change.
  nsIntPoint mLastClientOffset;

  nsPopupType mPopupType; // type of popup
  nsPopupState mPopupState; // open state of the popup

  // popup alignment relative to the anchor node
  int8_t mPopupAlignment;
  int8_t mPopupAnchor;
  int8_t mPosition;

  // One of nsIPopupBoxObject::ROLLUP_DEFAULT/ROLLUP_CONSUME/ROLLUP_NO_CONSUME
  int8_t mConsumeRollupEvent;
  FlipType mFlip; // Whether to flip

  bool mIsOpenChanged; // true if the open state changed since the last layout
  bool mIsContextMenu; // true for context menus
  // true if we need to offset the popup to ensure it's not under the mouse
  bool mAdjustOffsetForContextMenu;
  bool mGeneratedChildren; // true if the contents have been created

  bool mMenuCanOverlapOSBar;    // can we appear over the taskbar/menubar?
  bool mShouldAutoPosition; // Should SetPopupPosition be allowed to auto position popup?
  bool mInContentShell; // True if the popup is in a content shell
  bool mIsMenuLocked; // Should events inside this menu be ignored?
  bool mMouseTransparent; // True if this is a popup is transparent to mouse events

  // the flip modes that were used when the popup was opened
  bool mHFlip;
  bool mVFlip;

  static int8_t sDefaultLevelIsTop;
}; // class nsMenuPopupFrame

#endif
