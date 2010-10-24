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
#include "nsEvent.h"
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
#include "nsCycleCollectionParticipant.h"
#include "nsIMarkupDocumentViewer.h"
#include "nsIScrollableFrame.h"

class nsIPresShell;
class nsIDocShell;
class nsIDocShellTreeNode;
class nsIDocShellTreeItem;
class imgIContainer;
class nsDOMDataTransfer;

namespace mozilla {
namespace dom {
class TabParent;
}
}

/*
 * Event listener manager
 */

class nsEventStateManager : public nsSupportsWeakReference,
                            public nsIEventStateManager,
                            public nsIObserver
{
  friend class nsMouseWheelTransaction;
public:
  nsEventStateManager();
  virtual ~nsEventStateManager();

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
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

  virtual nsEventStates GetContentState(nsIContent *aContent,
                                        PRBool aFollowLabels = PR_FALSE);
  virtual PRBool SetContentState(nsIContent *aContent, nsEventStates aState);
  NS_IMETHOD ContentRemoved(nsIDocument* aDocument, nsIContent* aContent);
  NS_IMETHOD EventStatusOK(nsGUIEvent* aEvent, PRBool *aOK);

  // Access Key Registration
  NS_IMETHOD RegisterAccessKey(nsIContent* aContent, PRUint32 aKey);
  NS_IMETHOD UnregisterAccessKey(nsIContent* aContent, PRUint32 aKey);
  NS_IMETHOD GetRegisteredAccessKey(nsIContent* aContent, PRUint32* aKey);

  NS_IMETHOD SetCursor(PRInt32 aCursor, imgIContainer* aContainer,
                       PRBool aHaveHotspot, float aHotspotX, float aHotspotY,
                       nsIWidget* aWidget, PRBool aLockCursor);

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

  NS_IMETHOD_(PRBool) IsHandlingUserInputExternal() { return IsHandlingUserInput(); }
  
  nsPresContext* GetPresContext() { return mPresContext; }

  NS_DECL_CYCLE_COLLECTION_CLASS_AMBIGUOUS(nsEventStateManager,
                                           nsIEventStateManager)

  static nsIDocument* sMouseOverDocument;

  static nsIEventStateManager* GetActiveEventStateManager() { return sActiveESM; }

  static void SetGlobalActiveContent(nsEventStateManager* aNewESM,
                                     nsIContent* aContent);
protected:
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
  /**
   * Update the initial drag session data transfer with any changes that occur
   * on cloned data transfer objects used for events.
   */
  void UpdateDragDataTransfer(nsDragEvent* dragEvent);

  nsresult SetClickCount(nsPresContext* aPresContext, nsMouseEvent *aEvent, nsEventStatus* aStatus);
  nsresult CheckForAndDispatchClick(nsPresContext* aPresContext, nsMouseEvent *aEvent, nsEventStatus* aStatus);
  void EnsureDocument(nsPresContext* aPresContext);
  void FlushPendingEvents(nsPresContext* aPresContext);

  /**
   * The phases of HandleAccessKey processing. See below.
   */
  typedef enum {
    eAccessKeyProcessingNormal = 0,
    eAccessKeyProcessingUp,
    eAccessKeyProcessingDown
  } ProcessingAccessKeyState;

  /**
   * Access key handling.  If there is registered content for the accesskey
   * given by the key event and modifier mask then call
   * content.PerformAccesskey(), otherwise call HandleAccessKey() recursively,
   * on descendant docshells first, then on the ancestor (with |aBubbledFrom|
   * set to the docshell associated with |this|), until something matches.
   *
   * @param aPresContext the presentation context
   * @param aEvent the key event
   * @param aStatus the event status
   * @param aBubbledFrom is used by an ancestor to avoid calling HandleAccessKey()
   *        on the child the call originally came from, i.e. this is the child
   *        that recursively called us in its Up phase. The initial caller
   *        passes |nsnull| here. This is to avoid an infinite loop.
   * @param aAccessKeyState Normal, Down or Up processing phase (see enums
   *        above). The initial event receiver uses 'normal', then 'down' when
   *        processing children and Up when recursively calling its ancestor.
   * @param aModifierMask modifier mask for the key event
   */
  void HandleAccessKey(nsPresContext* aPresContext,
                       nsKeyEvent* aEvent,
                       nsEventStatus* aStatus,
                       nsIDocShellTreeItem* aBubbledFrom,
                       ProcessingAccessKeyState aAccessKeyState,
                       PRInt32 aModifierMask);

  PRBool ExecuteAccessKey(nsTArray<PRUint32>& aAccessCharCodes,
                          PRBool aIsTrustedEvent);

  //---------------------------------------------
  // DocShell Focus Traversal Methods
  //---------------------------------------------

  nsIContent* GetFocusedContent();
  PRBool IsShellVisible(nsIDocShell* aShell);

  // These functions are for mousewheel and pixel scrolling
  void SendLineScrollEvent(nsIFrame* aTargetFrame,
                           nsMouseScrollEvent* aEvent,
                           nsPresContext* aPresContext,
                           nsEventStatus* aStatus,
                           PRInt32 aNumLines);
  void SendPixelScrollEvent(nsIFrame* aTargetFrame,
                            nsMouseScrollEvent* aEvent,
                            nsPresContext* aPresContext,
                            nsEventStatus* aStatus);
  nsresult DoScrollText(nsIFrame* aTargetFrame,
                        nsMouseScrollEvent* aMouseEvent,
                        nsIScrollableFrame::ScrollUnit aScrollQuantity,
                        PRBool aAllowScrollSpeedOverride);
  void DoScrollHistory(PRInt32 direction);
  void DoScrollZoom(nsIFrame *aTargetFrame, PRInt32 adjustment);
  nsresult GetMarkupDocumentViewer(nsIMarkupDocumentViewer** aMv);
  nsresult ChangeTextSize(PRInt32 change);
  nsresult ChangeFullZoom(PRInt32 change);
  // end mousewheel functions

  /*
   * When a touch gesture is about to start, this function determines what
   * kind of gesture interaction we will want to use, based on what is
   * underneath the initial touch point.
   * Currently it decides between panning (finger scrolling) or dragging
   * the target element, as well as the orientation to trigger panning and
   * display visual boundary feedback. The decision is stored back in aEvent.
   */
  void DecideGestureEvent(nsGestureNotifyEvent* aEvent, nsIFrame* targetFrame);

  // routines for the d&d gesture tracking state machine
  void BeginTrackingDragGesture ( nsPresContext* aPresContext, nsMouseEvent* inDownEvent,
                                  nsIFrame* inDownFrame ) ;
  void StopTrackingDragGesture ( ) ;
  void GenerateDragGesture ( nsPresContext* aPresContext, nsMouseEvent *aEvent ) ;

  /**
   * Determine which node the drag should be targeted at.
   * This is either the node clicked when there is a selection, or, for HTML,
   * the element with a draggable property set to true.
   *
   * aSelectionTarget - target to check for selection
   * aDataTransfer - data transfer object that will contain the data to drag
   * aIsSelection - [out] set to true if a selection is being dragged
   * aIsInEditor - [out] set to true if the content is in an editor field
   * aTargetNode - [out] the draggable node, or null if there isn't one
   */
  void DetermineDragTarget(nsPresContext* aPresContext,
                           nsIContent* aSelectionTarget,
                           nsDOMDataTransfer* aDataTransfer,
                           PRBool* aIsSelection,
                           PRBool* aIsInEditor,
                           nsIContent** aTargetNode);

  /*
   * Perform the default handling for the dragstart/draggesture event and set up a
   * drag for aDataTransfer if it contains any data. Returns true if a drag has
   * started.
   *
   * aDragEvent - the dragstart/draggesture event
   * aDataTransfer - the data transfer that holds the data to be dragged
   * aDragTarget - the target of the drag
   * aIsSelection - true if a selection is being dragged
   */
  PRBool DoDefaultDragStart(nsPresContext* aPresContext,
                            nsDragEvent* aDragEvent,
                            nsDOMDataTransfer* aDataTransfer,
                            nsIContent* aDragTarget,
                            PRBool aIsSelection);

  PRBool IsTrackingDragGesture ( ) const { return mGestureDownContent != nsnull; }
  /**
   * Set the fields of aEvent to reflect the mouse position and modifier keys
   * that were set when the user first pressed the mouse button (stored by
   * BeginTrackingDragGesture). aEvent->widget must be
   * mCurrentTarget->GetNearestWidget().
   */
  void FillInEventFromGestureDown(nsMouseEvent* aEvent);

  nsresult DoContentCommandEvent(nsContentCommandEvent* aEvent);
  nsresult DoContentCommandScrollEvent(nsContentCommandEvent* aEvent);

#ifdef MOZ_IPC
  PRBool RemoteQueryContentEvent(nsEvent *aEvent);
  mozilla::dom::TabParent *GetCrossProcessTarget();
  PRBool IsTargetCrossProcess(nsGUIEvent *aEvent);
#endif

  PRInt32     mLockCursor;

  nsWeakFrame mCurrentTarget;
  nsCOMPtr<nsIContent> mCurrentTargetContent;
  nsWeakFrame mLastMouseOverFrame;
  nsCOMPtr<nsIContent> mLastMouseOverElement;
  nsWeakFrame mLastDragOverFrame;

  // member variables for the d&d gesture state machine
  nsIntPoint mGestureDownPoint; // screen coordinates
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
  nsCOMPtr<nsIContent> mLastLeftMouseDownContentParent;
  nsCOMPtr<nsIContent> mLastMiddleMouseDownContent;
  nsCOMPtr<nsIContent> mLastMiddleMouseDownContentParent;
  nsCOMPtr<nsIContent> mLastRightMouseDownContent;
  nsCOMPtr<nsIContent> mLastRightMouseDownContentParent;

  nsCOMPtr<nsIContent> mActiveContent;
  nsCOMPtr<nsIContent> mHoverContent;
  nsCOMPtr<nsIContent> mDragOverContent;
  nsCOMPtr<nsIContent> mURLTargetContent;

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

  PRPackedBool m_haveShutdown;


public:
  static nsresult UpdateUserActivityTimer(void);
  // Array for accesskey support
  nsCOMArray<nsIContent> mAccessKeys;

  // Unlocks pixel scrolling
  PRPackedBool mLastLineScrollConsumedX;
  PRPackedBool mLastLineScrollConsumedY;

  static PRInt32 sUserInputEventDepth;
  
  static PRBool sNormalLMouseEventInProcess;

  static nsEventStateManager* sActiveESM;
  
  static void ClearGlobalActiveContent();

  // Functions used for click hold context menus
  PRBool mClickHoldContextMenu;
  nsCOMPtr<nsITimer> mClickHoldTimer;
  void CreateClickHoldTimer ( nsPresContext* aPresContext, nsIFrame* inDownFrame,
                              nsGUIEvent* inMouseDownEvent ) ;
  void KillClickHoldTimer ( ) ;
  void FireContextClick ( ) ;
  static void sClickHoldCallback ( nsITimer* aTimer, void* aESM ) ;
};

/**
 * This class is used while processing real user input. During this time, popups
 * are allowed. For mousedown events, mouse capturing is also permitted.
 */
class nsAutoHandlingUserInputStatePusher
{
public:
  nsAutoHandlingUserInputStatePusher(PRBool aIsHandlingUserInput, PRBool aIsMouseDown)
    : mIsHandlingUserInput(aIsHandlingUserInput), mIsMouseDown(aIsMouseDown)
  {
    if (aIsHandlingUserInput) {
      nsEventStateManager::StartHandlingUserInput();
      if (aIsMouseDown) {
        nsIPresShell::SetCapturingContent(nsnull, 0);
        nsIPresShell::AllowMouseCapture(PR_TRUE);
      }
    }
  }

  ~nsAutoHandlingUserInputStatePusher()
  {
    if (mIsHandlingUserInput) {
      nsEventStateManager::StopHandlingUserInput();
      if (mIsMouseDown) {
        nsIPresShell::AllowMouseCapture(PR_FALSE);
      }
    }
  }

protected:
  PRBool mIsHandlingUserInput;
  PRBool mIsMouseDown;

private:
  // Hide so that this class can only be stack-allocated
  static void* operator new(size_t /*size*/) CPP_THROW_NEW { return nsnull; }
  static void operator delete(void* /*memory*/) {}
};

#endif // nsEventStateManager_h__
