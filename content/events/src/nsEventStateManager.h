/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Mats Palmgren <mats.palmgren@bredband.net>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef nsEventStateManager_h__
#define nsEventStateManager_h__

#include "nsIEventStateManager.h"
#include "nsGUIEvent.h"
#include "nsIContent.h"
#include "nsIObserver.h"
#include "nsWeakReference.h"
#include "nsHashtable.h"
#include "nsITimer.h"
#include "nsCOMPtr.h"
#include "nsIDocument.h"
#include "nsCOMArray.h"
#include "nsIFrame.h"

class nsIScrollableView;
class nsIPresShell;
class nsIDocShell;
class nsIDocShellTreeNode;
class nsIDocShellTreeItem;
class nsIFocusController;
class imgIContainer;

// mac uses click-hold context menus, a holdover from 4.x
#if defined(XP_MAC) || defined(XP_MACOSX)
#define CLICK_HOLD_CONTEXT_MENUS 1
#endif


/*
 * Event listener manager
 */

class nsEventStateManager : public nsSupportsWeakReference,
                            public nsIEventStateManager,
                            public nsIObserver
{
public:
  nsEventStateManager();
  virtual ~nsEventStateManager();

  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER

  NS_IMETHOD Init();
  nsresult Shutdown();

  /* The PreHandleEvent method is called before event dispatch to either
   * the DOM or frames.  Any processing which must not be prevented or
   * cancelled should occur here.  Any processing which is intended to
   * be conditional based on either DOM or frame processing should occur in
   * PostHandleEvent.  Any centralized event processing which must occur before
   * DOM or frame event handling should occur here as well.
   */
  NS_IMETHOD PreHandleEvent(nsPresContext* aPresContext,
                         nsEvent *aEvent,
                         nsIFrame* aTargetFrame,
                         nsEventStatus* aStatus,
                         nsIView* aView);

  /* The PostHandleEvent method should contain all system processing which
   * should occur conditionally based on DOM or frame processing.  It should
   * also contain any centralized event processing which must occur after
   * DOM and frame processing.
   */
  NS_IMETHOD PostHandleEvent(nsPresContext* aPresContext,
                             nsEvent *aEvent,
                             nsIFrame* aTargetFrame,
                             nsEventStatus* aStatus,
                             nsIView* aView);

  NS_IMETHOD NotifyDestroyPresContext(nsPresContext* aPresContext);
  NS_IMETHOD SetPresContext(nsPresContext* aPresContext);
  NS_IMETHOD ClearFrameRefs(nsIFrame* aFrame);

  NS_IMETHOD GetEventTarget(nsIFrame **aFrame);
  NS_IMETHOD GetEventTargetContent(nsEvent* aEvent, nsIContent** aContent);

  NS_IMETHOD GetContentState(nsIContent *aContent, PRInt32& aState);
  virtual PRBool SetContentState(nsIContent *aContent, PRInt32 aState);
  NS_IMETHOD GetFocusedContent(nsIContent **aContent);
  NS_IMETHOD SetFocusedContent(nsIContent* aContent);
  NS_IMETHOD GetLastFocusedContent(nsIContent **aContent);
  NS_IMETHOD GetFocusedFrame(nsIFrame **aFrame);
  NS_IMETHOD ContentRemoved(nsIContent* aContent);
  NS_IMETHOD EventStatusOK(nsGUIEvent* aEvent, PRBool *aOK);

  // Access Key Registration
  NS_IMETHOD RegisterAccessKey(nsIContent* aContent, PRUint32 aKey);
  NS_IMETHOD UnregisterAccessKey(nsIContent* aContent, PRUint32 aKey);

  NS_IMETHOD SetCursor(PRInt32 aCursor, imgIContainer* aContainer,
                       PRBool aHaveHotspot, float aHotspotX, float aHotspotY,
                       nsIWidget* aWidget, PRBool aLockCursor);

  NS_IMETHOD ShiftFocus(PRBool aForward, nsIContent* aStart=nsnull);

  virtual PRBool GetBrowseWithCaret();
  void ResetBrowseWithCaret();

  NS_IMETHOD MoveFocusToCaret(PRBool aCanFocusDoc, PRBool *aIsSelectionWithFocus);
  NS_IMETHOD MoveCaretToFocus();
  NS_IMETHOD ChangeFocusWith(nsIContent* aFocus, EFocusedWithType aFocusedWith);

  static void StartHandlingUserInput()
  {
    ++sUserInputEventDepth;
  }

  static void StopHandlingUserInput()
  {
    --sUserInputEventDepth;
  }

  static PRBool IsHandlingUserInput()
  {
    return sUserInputEventDepth > 0;
  }

protected:
  /**
   * In certain situations the focus controller's concept of focus gets out of
   * whack with mCurrentFocus. This is used in known cases to reset the focus
   * controller's focus. At some point we should probably move to a single
   * focus storage mechanism because tracking it in several places is error-prone.
   */
  void EnsureFocusSynchronization();

  void UpdateCursor(nsPresContext* aPresContext, nsEvent* aEvent, nsIFrame* aTargetFrame, nsEventStatus* aStatus);
  /**
   * Turn a GUI mouse event into a mouse event targeted at the specified
   * content.  This returns the primary frame for the content (or null
   * if it goes away during the event).
   */
  nsIFrame* DispatchMouseEvent(nsGUIEvent* aEvent, PRUint32 aMessage,
                               nsIContent* aTargetContent,
                               nsIContent* aRelatedContent);
  /**
   * Synthesize DOM and frame mouseover and mouseout events from this
   * MOUSE_MOVE or MOUSE_EXIT event.
   */
  void GenerateMouseEnterExit(nsGUIEvent* aEvent);
  /**
   * Tell this ESM and ESMs in parent documents that the mouse is
   * over some content in this document.
   */
  void NotifyMouseOver(nsGUIEvent* aEvent, nsIContent* aContent);
  /**
   * Tell this ESM and ESMs in affected child documents that the mouse
   * has exited this document's currently hovered content.
   * @param aEvent the event that triggered the mouseout
   * @param aMovingInto the content node we've moved into.  This is used to set
   *        the relatedTarget for mouseout events.  Also, if it's non-null
   *        NotifyMouseOut will NOT change the current hover content to null;
   *        in that case the caller is responsible for updating hover state.
   */
  void NotifyMouseOut(nsGUIEvent* aEvent, nsIContent* aMovingInto);
  void GenerateDragDropEnterExit(nsPresContext* aPresContext, nsGUIEvent* aEvent);
  /**
   * Fire the dragenter and dragexit/dragleave events when the mouse moves to a
   * new target.
   *
   * @param aRelatedTarget relatedTarget to set for the event
   * @param aTargetContent target to set for the event
   * @param aTargetFrame target frame for the event
   */
  void FireDragEnterOrExit(nsPresContext* aPresContext,
                           nsGUIEvent* aEvent,
                           PRUint32 aMsg,
                           nsIContent* aRelatedTarget,
                           nsIContent* aTargetContent,
                           nsWeakFrame& aTargetFrame);
  nsresult SetClickCount(nsPresContext* aPresContext, nsMouseEvent *aEvent, nsEventStatus* aStatus);
  nsresult CheckForAndDispatchClick(nsPresContext* aPresContext, nsMouseEvent *aEvent, nsEventStatus* aStatus);
  nsresult GetNextTabbableContent(nsIContent* aRootContent,
                                  nsIContent* aStartContent,
                                  nsIFrame* aStartFrame,
                                  PRBool forward, PRBool ignoreTabIndex,
                                  nsIContent** aResultNode,
                                  nsIFrame** aResultFrame);
  nsIContent *GetNextTabbableMapArea(PRBool aForward, nsIContent *imageContent);

  PRInt32 GetNextTabIndex(nsIContent* aParent, PRBool foward);
  nsresult SendFocusBlur(nsPresContext* aPresContext, nsIContent *aContent, PRBool aEnsureWindowHasFocus);
  void EnsureDocument(nsIPresShell* aPresShell);
  void EnsureDocument(nsPresContext* aPresContext);
  void FlushPendingEvents(nsPresContext* aPresContext);
  nsIFocusController* GetFocusControllerForDocument(nsIDocument* aDocument);

  typedef enum {
    eAccessKeyProcessingNormal = 0,
    eAccessKeyProcessingUp,
    eAccessKeyProcessingDown
  } ProcessingAccessKeyState;
  void HandleAccessKey(nsPresContext* aPresContext,
                       nsKeyEvent* aEvent,
                       nsEventStatus* aStatus,
                       PRInt32 aChildOffset,
                       ProcessingAccessKeyState aAccessKeyState,
                       PRInt32 aModifierMask);

  //---------------------------------------------
  // DocShell Focus Traversal Methods
  //---------------------------------------------

  nsresult ShiftFocusInternal(PRBool aForward, nsIContent* aStart = nsnull);
  void TabIntoDocument(nsIDocShell* aDocShell, PRBool aForward);
  void ShiftFocusByDoc(PRBool forward);
  PRBool IsFrameSetDoc(nsIDocShell* aDocShell);
  PRBool IsIFrameDoc(nsIDocShell* aDocShell);
  PRBool IsShellVisible(nsIDocShell* aShell);
  void GetLastChildDocShell(nsIDocShellTreeItem* aItem,
                            nsIDocShellTreeItem** aResult);
  void GetNextDocShell(nsIDocShellTreeNode* aNode,
                       nsIDocShellTreeItem** aResult);
  void GetPrevDocShell(nsIDocShellTreeNode* aNode,
                       nsIDocShellTreeItem** aResult);

  // These functions are for mousewheel scrolling
  nsresult GetParentScrollingView(nsInputEvent* aEvent,
                                  nsPresContext* aPresContext,
                                  nsIFrame* &targetOuterFrame,
                                  nsPresContext* &presCtxOuter);

  typedef enum {
    eScrollByPixel,
    eScrollByLine,
    eScrollByPage
  } ScrollQuantity;
  nsresult DoScrollText(nsPresContext* aPresContext,
                        nsIFrame* aTargetFrame,
                        nsInputEvent* aEvent,
                        PRInt32 aNumLines,
                        PRBool aScrollHorizontal,
                        ScrollQuantity aScrollQuantity);
  void ForceViewUpdate(nsIView* aView);
  void DoScrollHistory(PRInt32 direction);
  void DoScrollTextsize(nsIFrame *aTargetFrame, PRInt32 adjustment);
  nsresult ChangeTextSize(PRInt32 change);
  // end mousewheel functions

  // routines for the d&d gesture tracking state machine
  void BeginTrackingDragGesture ( nsPresContext* aPresContext, nsMouseEvent* inDownEvent,
                                  nsIFrame* inDownFrame ) ;
  void StopTrackingDragGesture ( ) ;
  void GenerateDragGesture ( nsPresContext* aPresContext, nsMouseEvent *aEvent ) ;
  PRBool IsTrackingDragGesture ( ) const { return mGestureDownContent != nsnull; }
  /**
   * Set the fields of aEvent to reflect the mouse position and modifier keys
   * that were set when the user first pressed the mouse button (stored by
   * BeginTrackingDragGesture). aEvent->widget must be
   * mCurrentTarget->GetWindow().
   */
  void FillInEventFromGestureDown(nsMouseEvent* aEvent);

  PRBool mSuppressFocusChange; // Used only for Ender text fields to suppress a focus firing on mouse down

  nsresult SetCaretEnabled(nsIPresShell *aPresShell, PRBool aVisibility);
  nsresult SetContentCaretVisible(nsIPresShell* aPresShell, nsIContent *aContent, PRBool aVisible);
  void FocusElementButNotDocument(nsIContent *aElement);

  // Return the location of the caret
  nsresult GetDocSelectionLocation(nsIContent **start, nsIContent **end, 
                                   nsIFrame **startFrame, PRUint32 *startOffset);

  PRInt32     mLockCursor;

  nsWeakFrame mCurrentTarget;
  nsCOMPtr<nsIContent> mCurrentTargetContent;
  nsWeakFrame mLastMouseOverFrame;
  nsCOMPtr<nsIContent> mLastMouseOverElement;
  nsWeakFrame mLastDragOverFrame;

  // member variables for the d&d gesture state machine
  nsPoint mGestureDownPoint; // screen coordinates
  // The content to use as target if we start a d&d (what we drag).
  nsCOMPtr<nsIContent> mGestureDownContent;
  // The content of the frame where the mouse-down event occurred. It's the same
  // as the target in most cases but not always - for example when dragging
  // an <area> of an image map this is the image. (bug 289667)
  nsCOMPtr<nsIContent> mGestureDownFrameOwner;
  // State of keys when the original gesture-down happened
  PRPackedBool mGestureDownShift;
  PRPackedBool mGestureDownControl;
  PRPackedBool mGestureDownAlt;
  PRPackedBool mGestureDownMeta;

  nsCOMPtr<nsIContent> mLastLeftMouseDownContent;
  nsCOMPtr<nsIContent> mLastMiddleMouseDownContent;
  nsCOMPtr<nsIContent> mLastRightMouseDownContent;

  nsCOMPtr<nsIContent> mActiveContent;
  nsCOMPtr<nsIContent> mHoverContent;
  nsCOMPtr<nsIContent> mDragOverContent;
  nsCOMPtr<nsIContent> mURLTargetContent;
  nsCOMPtr<nsIContent> mCurrentFocus;
  nsCOMPtr<nsIContent> mLastFocus;
  nsWeakFrame mCurrentFocusFrame;
  PRInt32 mCurrentTabIndex;
  EFocusedWithType mLastFocusedWith;

  // DocShell Traversal Data Memebers
  nsCOMPtr<nsIContent> mLastContentFocus;

  //Anti-recursive stack controls

  nsCOMPtr<nsIContent> mFirstBlurEvent;
  nsCOMPtr<nsIContent> mFirstFocusEvent;

  // The last element on which we fired a mouseover event, or null if
  // the last mouseover event we fired has finished processing.
  nsCOMPtr<nsIContent> mFirstMouseOverEventElement;

  // The last element on which we fired a mouseout event, or null if
  // the last mouseout event we fired has finished processing.
  nsCOMPtr<nsIContent> mFirstMouseOutEventElement;

  nsPresContext* mPresContext;      // Not refcnted
  nsCOMPtr<nsIDocument> mDocument;   // Doesn't necessarily need to be owner

  PRUint32 mLClickCount;
  PRUint32 mMClickCount;
  PRUint32 mRClickCount;

  PRPackedBool mNormalLMouseEventInProcess;

  PRPackedBool m_haveShutdown;

  // So we don't have to keep checking accessibility.browsewithcaret pref
  PRPackedBool mBrowseWithCaret;

  // Recursion guard for tabbing
  PRPackedBool mTabbedThroughDocument;

  //Hashtable for accesskey support
  nsSupportsHashtable *mAccessKeys;

  nsCOMArray<nsIDocShell> mTabbingFromDocShells;

#ifdef CLICK_HOLD_CONTEXT_MENUS
  enum { kClickHoldDelay = 500 } ;        // 500ms == 1/2 second

  void CreateClickHoldTimer ( nsPresContext* aPresContext, nsIFrame* inDownFrame,
                              nsGUIEvent* inMouseDownEvent ) ;
  void KillClickHoldTimer ( ) ;
  void FireContextClick ( ) ;
  static void sClickHoldCallback ( nsITimer* aTimer, void* aESM ) ;
  
  nsCOMPtr<nsITimer> mClickHoldTimer;
#endif

  static PRInt32 sUserInputEventDepth;
};


class nsAutoHandlingUserInputStatePusher
{
public:
  nsAutoHandlingUserInputStatePusher(PRBool aIsHandlingUserInput)
    : mIsHandlingUserInput(aIsHandlingUserInput)
  {
    if (aIsHandlingUserInput) {
      nsEventStateManager::StartHandlingUserInput();
    }
  }

  ~nsAutoHandlingUserInputStatePusher()
  {
    if (mIsHandlingUserInput) {
      nsEventStateManager::StopHandlingUserInput();
    }
  }

protected:
  PRBool mIsHandlingUserInput;

private:
  // Hide so that this class can only be stack-allocated
  static void* operator new(size_t /*size*/) CPP_THROW_NEW { return nsnull; }
  static void operator delete(void* /*memory*/) {}
};

#endif // nsEventStateManager_h__
