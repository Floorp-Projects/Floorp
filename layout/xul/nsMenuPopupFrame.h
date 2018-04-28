/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//
// nsMenuPopupFrame
//

#ifndef nsMenuPopupFrame_h__
#define nsMenuPopupFrame_h__

#include "mozilla/Attributes.h"
#include "mozilla/gfx/Types.h"
#include "nsAtom.h"
#include "nsGkAtoms.h"
#include "nsCOMPtr.h"
#include "nsMenuFrame.h"

#include "nsBoxFrame.h"
#include "nsMenuParent.h"

#include "nsITimer.h"

#include "Units.h"

class nsIWidget;

namespace mozilla {
namespace dom {
class KeyboardEvent;
} // namespace dom
} // namespace mozilla

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
  // state while a popup is waiting to be laid out and positioned
  ePopupPositioning,
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

enum ConsumeOutsideClicksResult {
  ConsumeOutsideClicks_ParentOnly = 0, // Only consume clicks on the parent anchor
  ConsumeOutsideClicks_True = 1, // Always consume clicks
  ConsumeOutsideClicks_Never = 2 // Never consume clicks
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

enum MenuPopupAnchorType {
  MenuPopupAnchorType_Node = 0, // anchored to a node
  MenuPopupAnchorType_Point = 1, // unanchored and positioned at a screen point
  MenuPopupAnchorType_Rect = 2, // anchored at a screen rectangle
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
#define POPUPPOSITION_SELECTION 10

#define POPUPPOSITION_HFLIP(v) (v ^ 1)
#define POPUPPOSITION_VFLIP(v) (v ^ 2)

nsIFrame* NS_NewMenuPopupFrame(nsIPresShell* aPresShell, mozilla::ComputedStyle* aStyle);

class nsView;
class nsMenuPopupFrame;

// this class is used for dispatching popupshown events asynchronously.
class nsXULPopupShownEvent : public mozilla::Runnable,
                             public nsIDOMEventListener
{
public:
  nsXULPopupShownEvent(nsIContent* aPopup, nsPresContext* aPresContext)
    : mozilla::Runnable("nsXULPopupShownEvent")
    , mPopup(aPopup)
    , mPresContext(aPresContext)
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
  RefPtr<nsPresContext> mPresContext;
};

class nsMenuPopupFrame final : public nsBoxFrame, public nsMenuParent,
                               public nsIReflowCallback
{
public:
  NS_DECL_QUERYFRAME
  NS_DECL_FRAMEARENA_HELPERS(nsMenuPopupFrame)

  explicit nsMenuPopupFrame(ComputedStyle* aStyle);

  // nsMenuParent interface
  virtual nsMenuFrame* GetCurrentMenuItem() override;
  NS_IMETHOD SetCurrentMenuItem(nsMenuFrame* aMenuItem) override;
  virtual void CurrentMenuIsBeingDestroyed() override;
  NS_IMETHOD ChangeMenuItem(nsMenuFrame* aMenuItem,
                            bool aSelectFirstItem,
                            bool aFromKey) override;

  // as popups are opened asynchronously, the popup pending state is used to
  // prevent multiple requests from attempting to open the same popup twice
  nsPopupState PopupState() { return mPopupState; }
  void SetPopupState(nsPopupState aPopupState) { mPopupState = aPopupState; }

  NS_IMETHOD SetActive(bool aActiveFlag) override { return NS_OK; } // We don't care.
  virtual bool IsActive() override { return false; }
  virtual bool IsMenuBar() override { return false; }

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
  ConsumeOutsideClicksResult ConsumeOutsideClicks();

  virtual bool IsContextMenu() override { return mIsContextMenu; }

  virtual bool MenuClosed() override { return true; }

  virtual void LockMenuUntilClosed(bool aLock) override;
  virtual bool IsMenuLocked() override { return mIsMenuLocked; }

  nsIWidget* GetWidget();

  // Overridden methods
  virtual void Init(nsIContent*       aContent,
                    nsContainerFrame* aParent,
                    nsIFrame*         aPrevInFlow) override;

  virtual nsresult AttributeChanged(int32_t aNameSpaceID,
                                    nsAtom* aAttribute,
                                    int32_t aModType) override;

  virtual void DestroyFrom(nsIFrame* aDestructRoot, PostDestroyData& aPostDestroyData) override;

  bool HasRemoteContent() const;

  // returns true if the popup is a panel with the noautohide attribute set to
  // true. These panels do not roll up automatically.
  bool IsNoAutoHide() const;

  nsPopupLevel PopupLevel() const
  {
    return PopupLevel(IsNoAutoHide());
  }

  // Ensure that a widget has already been created for this view, and create
  // one if it hasn't. If aRecreate is true, destroys any existing widget and
  // creates a new one, regardless of whether one has already been created.
  void EnsureWidget(bool aRecreate = false);

  nsresult CreateWidgetForView(nsView* aView);
  uint8_t GetShadowStyle();

  virtual void SetInitialChildList(ChildListID  aListID,
                                   nsFrameList& aChildList) override;

  virtual bool IsLeafDynamic() const override;

  virtual void UpdateWidgetProperties() override;

  // layout, position and display the popup as needed
  void LayoutPopup(nsBoxLayoutState& aState, nsIFrame* aParentMenu,
                   nsIFrame* aAnchor, bool aSizedToPopup);

  nsView* GetRootViewForPopup(nsIFrame* aStartFrame);

  // Set the position of the popup either relative to the anchor aAnchorFrame
  // (or the frame for mAnchorContent if aAnchorFrame is null), anchored at a
  // rectangle, or at a specific point if a screen position is set. The popup
  // will be adjusted so that it is on screen. If aIsMove is true, then the
  // popup is being moved, and should not be flipped. If aNotify is true, then
  // a popuppositioned event is sent.
  nsresult SetPopupPosition(nsIFrame* aAnchorFrame, bool aIsMove,
                            bool aSizedToPopup, bool aNotify);

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
  bool IsMenu() override { return mPopupType == ePopupTypeMenu; }
  bool IsOpen() override { return mPopupState == ePopupOpening ||
                                      mPopupState == ePopupVisible ||
                                      mPopupState == ePopupShown; }
  bool IsVisible() { return mPopupState == ePopupVisible ||
                            mPopupState == ePopupShown; }

  // Return true if the popup is for a menulist.
  bool IsMenuList();

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
                       MenuPopupAnchorType aAnchorType,
                       bool aAttributesOverride);

  void InitializePopupAtRect(nsIContent* aTriggerContent,
                             const nsAString& aPosition,
                             const nsIntRect& aRect,
                             bool aAttributesOverride);

  /**
   * @param aIsContextMenu if true, then the popup is
   * positioned at a slight offset from aXPos/aYPos to ensure the
   * (presumed) mouse position is not over the menu.
   */
  void InitializePopupAtScreen(nsIContent* aTriggerContent,
                               int32_t aXPos, int32_t aYPos,
                               bool aIsContextMenu);

  // indicate that the popup should be opened
  void ShowPopup(bool aIsContextMenu);
  // indicate that the popup should be hidden. The new state should either be
  // ePopupClosed or ePopupInvisible.
  void HidePopup(bool aDeselectMenu, nsPopupState aNewState);

  // locate and return the menu frame that should be activated for the
  // supplied key event. If doAction is set to true by this method,
  // then the menu's action should be carried out, as if the user had pressed
  // the Enter key. If doAction is false, the menu should just be highlighted.
  // This method also handles incremental searching in menus so the user can
  // type the first few letters of an item/s name to select it.
  nsMenuFrame* FindMenuWithShortcut(mozilla::dom::KeyboardEvent* aKeyEvent,
                                    bool& doAction);

  void ClearIncrementalString() { mIncrementalString.Truncate(); }
  static bool IsWithinIncrementalTime(DOMTimeStamp time) {
    return !sTimeoutOfIncrementalSearch || time - sLastKeyTime <= sTimeoutOfIncrementalSearch;
  }

#ifdef DEBUG_FRAME_DUMP
  virtual nsresult GetFrameName(nsAString& aResult) const override
  {
      return MakeFrameName(NS_LITERAL_STRING("MenuPopup"), aResult);
  }
#endif

  void EnsureMenuItemIsVisible(nsMenuFrame* aMenuFrame);

  void ChangeByPage(bool aIsUp);

  // Move the popup to the screen coordinate |aPos| in CSS pixels.
  // If aUpdateAttrs is true, and the popup already has left or top attributes,
  // then those attributes are updated to the new location.
  // The frame may be destroyed by this method.
  void MoveTo(const mozilla::CSSIntPoint& aPos, bool aUpdateAttrs);

  void MoveToAnchor(nsIContent* aAnchorContent,
                    const nsAString& aPosition,
                    int32_t aXPos, int32_t aYPos,
                    bool aAttributesOverride);

  bool GetAutoPosition();
  void SetAutoPosition(bool aShouldAutoPosition);

  nsIScrollableFrame* GetScrollFrame(nsIFrame* aStart);

  void SetOverrideConstraintRect(mozilla::LayoutDeviceIntRect aRect) {
    mOverrideConstraintRect = ToAppUnits(aRect, PresContext()->AppUnitsPerCSSPixel());
  }

  // For a popup that should appear anchored at the given rect, determine
  // the screen area that it is constrained by. This will be the available
  // area of the screen the popup should be displayed on. Content popups,
  // however, will also be constrained by the content area, given by
  // aRootScreenRect. All coordinates are in app units.
  // For non-toplevel popups (which will always be panels), we will also
  // constrain them to the available screen rect, ie they will not fall
  // underneath the taskbar, dock or other fixed OS elements.
  // This operates in device pixels.
  mozilla::LayoutDeviceIntRect
  GetConstraintRect(const mozilla::LayoutDeviceIntRect& aAnchorRect,
                    const mozilla::LayoutDeviceIntRect& aRootScreenRect,
                    nsPopupLevel aPopupLevel);

  // Determines whether the given edges of the popup may be moved, where
  // aHorizontalSide and aVerticalSide are one of the enum Side constants.
  // aChange is the distance to move on those sides. If will be reset to 0
  // if the side cannot be adjusted at all in that direction. For example, a
  // popup cannot be moved if it is anchored on a particular side.
  //
  // Later, when bug 357725 is implemented, we can make this adjust aChange by
  // the amount that the side can be resized, so that minimums and maximums
  // can be taken into account.
  void CanAdjustEdges(mozilla::Side aHorizontalSide,
                      mozilla::Side aVerticalSide,
                      mozilla::LayoutDeviceIntPoint& aChange);

  // Return true if the popup is positioned relative to an anchor.
  bool IsAnchored() const { return mAnchorType != MenuPopupAnchorType_Point; }

  // Return the anchor if there is one.
  nsIContent* GetAnchor() const { return mAnchorContent; }

  // Return the screen coordinates in CSS pixels of the popup,
  // or (-1, -1, 0, 0) if anchored.
  nsIntRect GetScreenAnchorRect() const { return mScreenRect; }

  mozilla::LayoutDeviceIntPoint GetLastClientOffset() const
  {
    return mLastClientOffset;
  }

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

  void ShowWithPositionedEvent() {
    mPopupState = ePopupPositioning;
    mShouldAutoPosition = true;
  }

  // Checks for the anchor to change and either moves or hides the popup
  // accordingly. The original position of the anchor should be supplied as
  // the argument. If the popup needs to be hidden, HidePopup will be called by
  // CheckForAnchorChange. If the popup needs to be moved, aRect will be updated
  // with the new rectangle.
  void CheckForAnchorChange(nsRect& aRect);

  // nsIReflowCallback
  virtual bool ReflowFinished() override;
  virtual void ReflowCallbackCanceled() override;

protected:

  // returns the popup's level.
  nsPopupLevel PopupLevel(bool aIsNoAutoHide) const;

  // redefine to tell the box system not to move the views.
  virtual uint32_t GetXULLayoutFlags() override;

  void InitPositionFromAnchorAlign(const nsAString& aAnchor,
                                   const nsAString& aAlign);

  // return the position where the popup should be, when it should be
  // anchored at anchorRect. aHFlip and aVFlip will be set if the popup may be
  // flipped in that direction if there is not enough space available.
  nsPoint AdjustPositionForAnchorAlign(nsRect& anchorRect,
                                       FlipStyle& aHFlip, FlipStyle& aVFlip);

  // For popups that are going to align to their selected item, get the frame of
  // the selected item.
  nsIFrame* GetSelectedItemForAlignment();

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
                       bool aIsOnEnd, bool* aFlipSide);

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

  // Given an anchor frame, compute the anchor rectangle relative to the screen,
  // using the popup frame's app units, and taking into account transforms.
  nsRect ComputeAnchorRect(nsPresContext* aRootPresContext, nsIFrame* aAnchorFrame);

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

  nsView* GetViewInternal() const override { return mView; }
  void SetViewInternal(nsView* aView) override { mView = aView; }

  // Returns true if the popup should try to remain at the same relative
  // location as the anchor while it is open. If the anchor becomes hidden
  // either directly or indirectly because a parent popup or other element
  // is no longer visible, or a parent deck page is changed, the popup hides
  // as well. The second variation also sets the anchor rectangle, relative to
  // the popup frame.
  bool ShouldFollowAnchor();
public:
  bool ShouldFollowAnchor(nsRect& aRect);

protected:
  nsString     mIncrementalString;  // for incremental typing navigation

  // the content that the popup is anchored to, if any, which may be in a
  // different document than the popup.
  nsCOMPtr<nsIContent> mAnchorContent;

  // the content that triggered the popup, typically the node where the mouse
  // was clicked. It will be cleared when the popup is hidden.
  nsCOMPtr<nsIContent> mTriggerContent;

  nsMenuFrame* mCurrentMenu; // The current menu that is active.
  nsView* mView;

  RefPtr<nsXULPopupShownEvent> mPopupShownDispatcher;

  // The popup's screen rectangle in app units.
  nsIntRect mUsedScreenRect;

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
  nsIntRect mScreenRect;

  // If the panel prefers to "slide" rather than resize, then the arrow gets
  // positioned at this offset (along either the x or y axis, depending on
  // mPosition)
  nscoord mAlignmentOffset;

  // The value of the client offset of our widget the last time we positioned
  // ourselves. We store this so that we can detect when it changes but the
  // position of our widget didn't change.
  mozilla::LayoutDeviceIntPoint mLastClientOffset;

  nsPopupType mPopupType; // type of popup
  nsPopupState mPopupState; // open state of the popup

  // popup alignment relative to the anchor node
  int8_t mPopupAlignment;
  int8_t mPopupAnchor;
  int8_t mPosition;

  FlipType mFlip; // Whether to flip

  struct ReflowCallbackData {
    ReflowCallbackData() :
      mPosted(false),
      mAnchor(nullptr),
      mSizedToPopup(false)
    {}
    void MarkPosted(nsIFrame* aAnchor, bool aSizedToPopup, bool aIsOpenChanged) {
      mPosted = true;
      mAnchor = aAnchor;
      mSizedToPopup = aSizedToPopup;
      mIsOpenChanged = aIsOpenChanged;
    }
    void Clear() {
      mPosted = false;
      mAnchor = nullptr;
      mSizedToPopup = false;
      mIsOpenChanged = false;
    }
    bool mPosted;
    nsIFrame* mAnchor;
    bool mSizedToPopup;
    bool mIsOpenChanged;
  };
  ReflowCallbackData mReflowCallbackData;

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

  // True if this popup has been offset due to moving off / near the edge of the screen.
  // (This is useful for ensuring that a move, which can't offset the popup, doesn't undo
  // a previously set offset.)
  bool mIsOffset;

  // the flip modes that were used when the popup was opened
  bool mHFlip;
  bool mVFlip;

  // When POPUPPOSITION_SELECTION is used, this indicates the vertical offset that the
  // original selected item was. This needs to be used in case the popup gets changed
  // so that we can keep the popup at the same vertical offset.
  nscoord mPositionedOffset;

  // How the popup is anchored.
  MenuPopupAnchorType mAnchorType;

  nsRect mOverrideConstraintRect;

  static int8_t sDefaultLevelIsTop;

  static DOMTimeStamp sLastKeyTime;

  // If 0, never timed out.  Otherwise, the value is in milliseconds.
  static uint32_t sTimeoutOfIncrementalSearch;
}; // class nsMenuPopupFrame

#endif
