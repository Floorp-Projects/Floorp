/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * This Original Code has been modified by IBM Corporation.
 * Modifications made by IBM described herein are
 * Copyright (c) International Business Machines
 * Corporation, 2000
 *
 * Modifications to Mozilla code or documentation
 * identified per MPL Section 3.3
 *
 * Date         Modified by     Description of modification
 * 05/03/2000   IBM Corp.       Observer related defines for reflow
 */

/* a presentation of a document, part 2 */

#ifndef nsIPresShell_h___
#define nsIPresShell_h___

#include "nsTHashtable.h"
#include "nsHashKeys.h"
#include "nsISupports.h"
#include "nsQueryFrame.h"
#include "nsCoord.h"
#include "nsColor.h"
#include "nsEvent.h"
#include "nsCompatibility.h"
#include "nsFrameManagerBase.h"
#include "nsRect.h"
#include "mozFlushType.h"
#include "nsWeakReference.h"
#include <stdio.h> // for FILE definition
#include "nsChangeHint.h"
#include "nsGUIEvent.h"
#include "nsInterfaceHashtable.h"
#include "nsEventStates.h"
#include "nsPresArena.h"

class nsIContent;
class nsIDocument;
class nsIFrame;
class nsPresContext;
class nsStyleSet;
class nsIViewManager;
class nsIView;
class nsRenderingContext;
class nsIPageSequenceFrame;
class nsAString;
class nsCaret;
class nsFrameSelection;
class nsFrameManager;
class nsILayoutHistoryState;
class nsIReflowCallback;
class nsIDOMNode;
class nsIntRegion;
class nsIStyleSheet;
class nsCSSFrameConstructor;
class nsISelection;
template<class E> class nsCOMArray;
class nsWeakFrame;
class nsIScrollableFrame;
class gfxASurface;
class gfxContext;
class nsIDOMEvent;
class nsDisplayList;
class nsDisplayListBuilder;
class nsPIDOMWindow;
struct nsPoint;
struct nsIntPoint;
struct nsIntRect;
class nsRegion;
class nsRefreshDriver;
class nsARefreshObserver;
#ifdef ACCESSIBILITY
class nsAccessibilityService;
#endif
class nsIWidget;

typedef short SelectionType;
typedef PRUint64 nsFrameState;

namespace mozilla {
namespace dom {
class Element;
} // namespace dom

namespace layers{
class LayerManager;
} // namespace layers
} // namespace mozilla

// Flags to pass to SetCapturingContent
//
// when assigning capture, ignore whether capture is allowed or not
#define CAPTURE_IGNOREALLOWED 1
// true if events should be targeted at the capturing content or its children
#define CAPTURE_RETARGETTOELEMENT 2
// true if the current capture wants drags to be prevented
#define CAPTURE_PREVENTDRAG 4
// true when the mouse is pointer locked, and events are sent to locked elemnt
#define CAPTURE_POINTERLOCK 8

typedef struct CapturingContentInfo {
  // capture should only be allowed during a mousedown event
  bool mAllowed;
  bool mPointerLock;
  bool mRetargetToElement;
  bool mPreventDrag;
  nsIContent* mContent;
} CapturingContentInfo;

// fcada634-fdea-45f5-b841-0a361d5f6a68
#define NS_IPRESSHELL_IID \
  { 0xfcada634, 0xfdea, 0x45f5, \
    { 0xb8, 0x41, 0x0a, 0x36, 0x1d, 0x5f, 0x6a, 0x68 } }

// debug VerifyReflow flags
#define VERIFY_REFLOW_ON                    0x01
#define VERIFY_REFLOW_NOISY                 0x02
#define VERIFY_REFLOW_ALL                   0x04
#define VERIFY_REFLOW_DUMP_COMMANDS         0x08
#define VERIFY_REFLOW_NOISY_RC              0x10
#define VERIFY_REFLOW_REALLY_NOISY_RC       0x20
#define VERIFY_REFLOW_DURING_RESIZE_REFLOW  0x40

#undef NOISY_INTERRUPTIBLE_REFLOW

enum nsRectVisibility { 
  nsRectVisibility_kVisible, 
  nsRectVisibility_kAboveViewport, 
  nsRectVisibility_kBelowViewport, 
  nsRectVisibility_kLeftOfViewport, 
  nsRectVisibility_kRightOfViewport
};

/**
 * Presentation shell interface. Presentation shells are the
 * controlling point for managing the presentation of a document. The
 * presentation shell holds a live reference to the document, the
 * presentation context, the style manager, the style set and the root
 * frame. <p>
 *
 * When this object is Release'd, it will release the document, the
 * presentation context, the style manager, the style set and the root
 * frame.
 */

// hack to make egcs / gcc 2.95.2 happy
class nsIPresShell_base : public nsISupports
{
public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_IPRESSHELL_IID)
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsIPresShell_base, NS_IPRESSHELL_IID)

class nsIPresShell : public nsIPresShell_base
{
protected:
  typedef mozilla::layers::LayerManager LayerManager;

  enum eRenderFlag {
    STATE_IGNORING_VIEWPORT_SCROLLING = 0x1
  };
  typedef PRUint8 RenderFlags; // for storing the above flags

public:
  virtual NS_HIDDEN_(nsresult) Init(nsIDocument* aDocument,
                                   nsPresContext* aPresContext,
                                   nsIViewManager* aViewManager,
                                   nsStyleSet* aStyleSet,
                                   nsCompatibility aCompatMode) = 0;

  /**
   * All callers are responsible for calling |Destroy| after calling
   * |EndObservingDocument|.  It needs to be separate only because form
   * controls incorrectly store their data in the frames rather than the
   * content model and printing calls |EndObservingDocument| multiple
   * times to make form controls behave nicely when printed.
   */
  virtual NS_HIDDEN_(void) Destroy() = 0;

  bool IsDestroying() { return mIsDestroying; }

  /**
   * All frames owned by the shell are allocated from an arena.  They
   * are also recycled using free lists.  Separate free lists are
   * maintained for each frame type (aID), which must always correspond
   * to the same aSize value.  AllocateFrame returns zero-filled memory.
   * AllocateFrame is fallible, it returns nsnull on out-of-memory.
   */
  void* AllocateFrame(nsQueryFrame::FrameIID aID, size_t aSize)
  {
#ifdef DEBUG
    mPresArenaAllocCount++;
#endif
    void* result = mFrameArena.AllocateByFrameID(aID, aSize);
  
    if (result) {
      memset(result, 0, aSize);
    }
    return result;
  }

  void FreeFrame(nsQueryFrame::FrameIID aID, void* aPtr)
  {
#ifdef DEBUG
    mPresArenaAllocCount--;
#endif
    if (PRESARENA_MUST_FREE_DURING_DESTROY || !mIsDestroying)
      mFrameArena.FreeByFrameID(aID, aPtr);
  }

  /**
   * This is for allocating other types of objects (not frames).  Separate free
   * lists are maintained for each type (aID), which must always correspond to
   * the same aSize value.  AllocateByObjectID returns zero-filled memory.
   * AllocateByObjectID is fallible, it returns nsnull on out-of-memory.
   */
  void* AllocateByObjectID(nsPresArena::ObjectID aID, size_t aSize)
  {
#ifdef DEBUG
    mPresArenaAllocCount++;
#endif
    void* result = mFrameArena.AllocateByObjectID(aID, aSize);
  
    if (result) {
      memset(result, 0, aSize);
    }
    return result;
  }

  void FreeByObjectID(nsPresArena::ObjectID aID, void* aPtr)
  {
#ifdef DEBUG
    mPresArenaAllocCount--;
#endif
    if (PRESARENA_MUST_FREE_DURING_DESTROY || !mIsDestroying)
      mFrameArena.FreeByObjectID(aID, aPtr);
  }

  /**
   * Other objects closely related to the frame tree that are allocated
   * from a separate set of per-size free lists.  Note that different types
   * of objects that has the same size are allocated from the same list.
   * AllocateMisc does *not* clear the memory that it returns.
   * AllocateMisc is fallible, it returns nsnull on out-of-memory.
   *
   * @deprecated use AllocateByObjectID/FreeByObjectID instead
   */
  void* AllocateMisc(size_t aSize)
  {
#ifdef DEBUG
    mPresArenaAllocCount++;
#endif
    return mFrameArena.AllocateBySize(aSize);
  }

  void FreeMisc(size_t aSize, void* aPtr)
  {
#ifdef DEBUG
    mPresArenaAllocCount--;
#endif
    if (PRESARENA_MUST_FREE_DURING_DESTROY || !mIsDestroying)
      mFrameArena.FreeBySize(aSize, aPtr);
  }

  nsIDocument* GetDocument() const { return mDocument; }

  nsPresContext* GetPresContext() const { return mPresContext; }

  nsIViewManager* GetViewManager() const { return mViewManager; }

#ifdef _IMPL_NS_LAYOUT
  nsStyleSet* StyleSet() const { return mStyleSet; }

  nsCSSFrameConstructor* FrameConstructor() const { return mFrameConstructor; }

  nsFrameManager* FrameManager() const {
    // reinterpret_cast is valid since nsFrameManager does not add
    // any members over nsFrameManagerBase.
    return reinterpret_cast<nsFrameManager*>
                           (const_cast<nsIPresShell*>(this)->mFrameManager);
  }

#endif

  /* Enable/disable author style level. Disabling author style disables the entire
   * author level of the cascade, including the HTML preshint level.
   */
  // XXX these could easily be inlined, but there is a circular #include
  // problem with nsStyleSet.
  NS_HIDDEN_(void) SetAuthorStyleDisabled(bool aDisabled);
  NS_HIDDEN_(bool) GetAuthorStyleDisabled() const;

  /*
   * Called when stylesheets are added/removed/enabled/disabled to rebuild
   * all style data for a given pres shell without necessarily reconstructing
   * all of the frames.  This will not reconstruct style synchronously; if
   * you need to do that, call FlushPendingNotifications to flush out style
   * reresolves.
   * // XXXbz why do we have this on the interface anyway?  The only consumer
   * is calling AddOverrideStyleSheet/RemoveOverrideStyleSheet, and I think
   * those should just handle reconstructing style data...
   */
  virtual NS_HIDDEN_(void) ReconstructStyleDataExternal();
  NS_HIDDEN_(void) ReconstructStyleDataInternal();
#ifdef _IMPL_NS_LAYOUT
  void ReconstructStyleData() { ReconstructStyleDataInternal(); }
#else
  void ReconstructStyleData() { ReconstructStyleDataExternal(); }
#endif

  /** Setup all style rules required to implement preferences
   * - used for background/text/link colors and link underlining
   *    may be extended for any prefs that are implemented via style rules
   * - aForceReflow argument is used to force a full reframe to make the rules show
   *   (only used when the current page needs to reflect changed pref rules)
   *
   * - initially created for bugs 31816, 20760, 22963
   */
  virtual NS_HIDDEN_(nsresult) SetPreferenceStyleRules(bool aForceReflow) = 0;

  /**
   * FrameSelection will return the Frame based selection API.
   * You cannot go back and forth anymore with QI between nsIDOM sel and
   * nsIFrame sel.
   */
  already_AddRefed<nsFrameSelection> FrameSelection();

  /**
   * ConstFrameSelection returns an object which methods are safe to use for
   * example in nsIFrame code.
   */
  const nsFrameSelection* ConstFrameSelection() const { return mSelection; }

  // Make shell be a document observer.  If called after Destroy() has
  // been called on the shell, this will be ignored.
  virtual NS_HIDDEN_(void) BeginObservingDocument() = 0;

  // Make shell stop being a document observer
  virtual NS_HIDDEN_(void) EndObservingDocument() = 0;

  /**
   * Return whether InitialReflow() was previously called.
   */
  bool DidInitialReflow() const { return mDidInitialReflow; }

  /**
   * Perform the initial reflow. Constructs the frame for the root content
   * object and then reflows the frame model into the specified width and
   * height.
   *
   * The coordinates for aWidth and aHeight must be in standard nscoords.
   *
   * Callers of this method must hold a reference to this shell that
   * is guaranteed to survive through arbitrary script execution.
   * Calling InitialReflow can execute arbitrary script.
   */
  virtual NS_HIDDEN_(nsresult) InitialReflow(nscoord aWidth, nscoord aHeight) = 0;

  /**
   * Reflow the frame model into a new width and height.  The
   * coordinates for aWidth and aHeight must be in standard nscoord's.
   */
  virtual NS_HIDDEN_(nsresult) ResizeReflow(nscoord aWidth, nscoord aHeight) = 0;
  /**
   * Reflow, and also change presshell state so as to only permit
   * reflowing off calls to ResizeReflowOverride() in the future.
   * ResizeReflow() calls are ignored after ResizeReflowOverride().
   */
  virtual NS_HIDDEN_(nsresult) ResizeReflowOverride(nscoord aWidth, nscoord aHeight) = 0;

  /**
   * Returns true if ResizeReflowOverride has been called.
   */
  virtual bool GetIsViewportOverridden() = 0;

  /**
   * Return true if the presshell expects layout flush.
   */
  virtual bool IsLayoutFlushObserver() = 0;

  /**
   * Reflow the frame model with a reflow reason of eReflowReason_StyleChange
   */
  virtual NS_HIDDEN_(void) StyleChangeReflow() = 0;

  /**
   * This calls through to the frame manager to get the root frame.
   */
  virtual NS_HIDDEN_(nsIFrame*) GetRootFrameExternal() const;
  nsIFrame* GetRootFrame() const {
#ifdef _IMPL_NS_LAYOUT
    return mFrameManager->GetRootFrame();
#else
    return GetRootFrameExternal();
#endif
  }

  /*
   * Get root scroll frame from FrameManager()->GetRootFrame().
   */
  nsIFrame* GetRootScrollFrame() const;

  /*
   * The same as GetRootScrollFrame, but returns an nsIScrollableFrame
   */
  nsIScrollableFrame* GetRootScrollFrameAsScrollable() const;

  /*
   * The same as GetRootScrollFrame, but returns an nsIScrollableFrame.
   * Can be called by code not linked into gklayout.
   */
  virtual nsIScrollableFrame* GetRootScrollFrameAsScrollableExternal() const;

  /*
   * Gets nearest scrollable frame from current focused content or DOM
   * selection if there is no focused content. The frame is scrollable with
   * overflow:scroll or overflow:auto in some direction when aDirection is
   * eEither.  Otherwise, this returns a nearest frame that is scrollable in
   * the specified direction.
   */
  enum ScrollDirection { eHorizontal, eVertical, eEither };
  nsIScrollableFrame* GetFrameToScrollAsScrollable(ScrollDirection aDirection);

  /**
   * Returns the page sequence frame associated with the frame hierarchy.
   * Returns NULL if not a paginated view.
   */
  virtual NS_HIDDEN_(nsIPageSequenceFrame*) GetPageSequenceFrame() const = 0;

  /**
   * Gets the real primary frame associated with the content object.
   *
   * In the case of absolutely positioned elements and floated elements,
   * the real primary frame is the frame that is out of the flow and not the
   * placeholder frame.
   */
  virtual NS_HIDDEN_(nsIFrame*) GetRealPrimaryFrameFor(nsIContent* aContent) const = 0;

  /**
   * Gets the placeholder frame associated with the specified frame. This is
   * a helper frame that forwards the request to the frame manager.
   */
  virtual NS_HIDDEN_(nsIFrame*) GetPlaceholderFrameFor(nsIFrame* aFrame) const = 0;

  /**
   * Tell the pres shell that a frame needs to be marked dirty and needs
   * Reflow.  It's OK if this is an ancestor of the frame needing reflow as
   * long as the ancestor chain between them doesn't cross a reflow root.  The
   * bit to add should be either NS_FRAME_IS_DIRTY or
   * NS_FRAME_HAS_DIRTY_CHILDREN (but not both!).
   */
  enum IntrinsicDirty {
    // XXXldb eResize should be renamed
    eResize,     // don't mark any intrinsic widths dirty
    eTreeChange, // mark intrinsic widths dirty on aFrame and its ancestors
    eStyleChange // Do eTreeChange, plus all of aFrame's descendants
  };
  virtual NS_HIDDEN_(void) FrameNeedsReflow(nsIFrame *aFrame,
                                            IntrinsicDirty aIntrinsicDirty,
                                            nsFrameState aBitToAdd) = 0;

  /**
   * Tell the presshell that the given frame's reflow was interrupted.  This
   * will mark as having dirty children a path from the given frame (inclusive)
   * to the nearest ancestor with a dirty subtree, or to the reflow root
   * currently being reflowed if no such ancestor exists (inclusive).  This is
   * to be done immediately after reflow of the current reflow root completes.
   * This method must only be called during reflow, and the frame it's being
   * called on must be in the process of being reflowed when it's called.  This
   * method doesn't mark any intrinsic widths dirty and doesn't add any bits
   * other than NS_FRAME_HAS_DIRTY_CHILDREN.
   */
  virtual NS_HIDDEN_(void) FrameNeedsToContinueReflow(nsIFrame *aFrame) = 0;

  virtual NS_HIDDEN_(void) CancelAllPendingReflows() = 0;

  /**
   * Recreates the frames for a node
   */
  virtual NS_HIDDEN_(nsresult) RecreateFramesFor(nsIContent* aContent) = 0;

  void PostRecreateFramesFor(mozilla::dom::Element* aElement);
  void RestyleForAnimation(mozilla::dom::Element* aElement,
                           nsRestyleHint aHint);

  /**
   * Determine if it is safe to flush all pending notifications
   * @param aIsSafeToFlush true if it is safe, false otherwise.
   * 
   */
  virtual NS_HIDDEN_(bool) IsSafeToFlush() const = 0;

  /**
   * Flush pending notifications of the type specified.  This method
   * will not affect the content model; it'll just affect style and
   * frames. Callers that actually want up-to-date presentation (other
   * than the document itself) should probably be calling
   * nsIDocument::FlushPendingNotifications.
   *
   * @param aType the type of notifications to flush
   */
  virtual NS_HIDDEN_(void) FlushPendingNotifications(mozFlushType aType) = 0;

  /**
   * Callbacks will be called even if reflow itself fails for
   * some reason.
   */
  virtual NS_HIDDEN_(nsresult) PostReflowCallback(nsIReflowCallback* aCallback) = 0;
  virtual NS_HIDDEN_(void) CancelReflowCallback(nsIReflowCallback* aCallback) = 0;

  virtual NS_HIDDEN_(void) ClearFrameRefs(nsIFrame* aFrame) = 0;

  /**
   * Get a reference rendering context. This is a context that should not
   * be rendered to, but is suitable for measuring text and performing
   * other non-rendering operations.
   */
  virtual already_AddRefed<nsRenderingContext> GetReferenceRenderingContext() = 0;

  /**
   * Informs the pres shell that the document is now at the anchor with
   * the given name.  If |aScroll| is true, scrolls the view of the
   * document so that the anchor with the specified name is displayed at
   * the top of the window.  If |aAnchorName| is empty, then this informs
   * the pres shell that there is no current target, and |aScroll| must
   * be false.
   */
  virtual NS_HIDDEN_(nsresult) GoToAnchor(const nsAString& aAnchorName, bool aScroll) = 0;

  /**
   * Tells the presshell to scroll again to the last anchor scrolled to by
   * GoToAnchor, if any. This scroll only happens if the scroll
   * position has not changed since the last GoToAnchor. This is called
   * by nsDocumentViewer::LoadComplete. This clears the last anchor
   * scrolled to by GoToAnchor (we don't want to keep it alive if it's
   * removed from the DOM), so don't call this more than once.
   */
  virtual NS_HIDDEN_(nsresult) ScrollToAnchor() = 0;

  enum {
    SCROLL_TOP     = 0,
    SCROLL_BOTTOM  = 100,
    SCROLL_LEFT    = 0,
    SCROLL_RIGHT   = 100,
    SCROLL_CENTER  = 50,
    SCROLL_MINIMUM = -1
  };

  enum WhenToScroll {
    SCROLL_ALWAYS,
    SCROLL_IF_NOT_VISIBLE,
    SCROLL_IF_NOT_FULLY_VISIBLE
  };
  typedef struct ScrollAxis {
    PRInt16 mWhereToScroll;
    WhenToScroll mWhenToScroll : 16;
  /**
   * @param aWhere: Either a percentage or a special value.
   *                nsIPresShell defines:
   *                * (Default) SCROLL_MINIMUM = -1: The visible area is
   *                scrolled to show the entire frame. If the frame is too
   *                large, the top and left edges are given precedence.
   *                * SCROLL_TOP = 0: The frame's upper edge is aligned with the
   *                top edge of the visible area.
   *                * SCROLL_BOTTOM = 100: The frame's bottom edge is aligned
   *                with the bottom edge of the visible area.
   *                * SCROLL_LEFT = 0: The frame's left edge is aligned with the
   *                left edge of the visible area.
   *                * SCROLL_RIGHT = 100: The frame's right edge is aligned with
   *                the right edge of the visible area.
   *                * SCROLL_CENTER = 50: The frame is centered along the axis
   *                the ScrollAxis is used for.
   *
   *                Other values are treated as a percentage, and the point
   *                "percent" down the frame is placed at the point "percent"
   *                down the visible area.
   * @param aWhen:
   *                * (Default) SCROLL_IF_NOT_FULLY_VISIBLE: Move the frame only
   *                if it is not fully visible (including if it's not visible
   *                at all). Note that in this case if the frame is too large to
   *                fit in view, it will only be scrolled if more of it can fit
   *                than is already in view.
   *                * SCROLL_IF_NOT_VISIBLE: Move the frame only if none of it
   *                is visible.
   *                * SCROLL_ALWAYS: Move the frame regardless of its current
   *                visibility.
   */
    ScrollAxis(PRInt16 aWhere = SCROLL_MINIMUM,
               WhenToScroll aWhen = SCROLL_IF_NOT_FULLY_VISIBLE) :
                 mWhereToScroll(aWhere), mWhenToScroll(aWhen) {}
  } ScrollAxis;
  /**
   * Scrolls the view of the document so that the primary frame of the content
   * is displayed in the window. Layout is flushed before scrolling.
   *
   * @param aContent  The content object of which primary frame should be
   *                  scrolled into view.
   * @param aVertical How to align the frame vertically and when to do so.
   *                  This is a ScrollAxis of Where and When.
   * @param aHorizontal How to align the frame horizontally and when to do so.
   *                  This is a ScrollAxis of Where and When.
   * @param aFlags    If SCROLL_FIRST_ANCESTOR_ONLY is set, only the nearest
   *                  scrollable ancestor is scrolled, otherwise all
   *                  scrollable ancestors may be scrolled if necessary.
   *                  If SCROLL_OVERFLOW_HIDDEN is set then we may scroll in a
   *                  direction even if overflow:hidden is specified in that
   *                  direction; otherwise we will not scroll in that direction
   *                  when overflow:hidden is set for that direction.
   *                  If SCROLL_NO_PARENT_FRAMES is set then we only scroll
   *                  nodes in this document, not in any parent documents which
   *                  contain this document in a iframe or the like.
   */
  virtual NS_HIDDEN_(nsresult) ScrollContentIntoView(nsIContent* aContent,
                                                     ScrollAxis  aVertical,
                                                     ScrollAxis  aHorizontal,
                                                     PRUint32    aFlags) = 0;

  enum {
    SCROLL_FIRST_ANCESTOR_ONLY = 0x01,
    SCROLL_OVERFLOW_HIDDEN = 0x02,
    SCROLL_NO_PARENT_FRAMES = 0x04
  };
  /**
   * Scrolls the view of the document so that the given area of a frame
   * is visible, if possible. Layout is not flushed before scrolling.
   * 
   * @param aRect relative to aFrame
   * @param aVertical see ScrollContentIntoView and ScrollAxis
   * @param aHorizontal see ScrollContentIntoView and ScrollAxis
   * @param aFlags if SCROLL_FIRST_ANCESTOR_ONLY is set, only the
   * nearest scrollable ancestor is scrolled, otherwise all
   * scrollable ancestors may be scrolled if necessary
   * if SCROLL_OVERFLOW_HIDDEN is set then we may scroll in a direction
   * even if overflow:hidden is specified in that direction; otherwise
   * we will not scroll in that direction when overflow:hidden is
   * set for that direction
   * If SCROLL_NO_PARENT_FRAMES is set then we only scroll
   * nodes in this document, not in any parent documents which
   * contain this document in a iframe or the like.
   * @return true if any scrolling happened, false if no scrolling happened
   */
  virtual bool ScrollFrameRectIntoView(nsIFrame*     aFrame,
                                       const nsRect& aRect,
                                       ScrollAxis    aVertical,
                                       ScrollAxis    aHorizontal,
                                       PRUint32      aFlags) = 0;

  /**
   * Determine if a rectangle specified in the frame's coordinate system 
   * intersects the viewport "enough" to be considered visible.
   * @param aFrame frame that aRect coordinates are specified relative to
   * @param aRect rectangle in twips to test for visibility 
   * @param aMinTwips is the minimum distance in from the edge of the viewport
   *                  that an object must be to be counted visible
   * @return nsRectVisibility_kVisible if the rect is visible
   *         nsRectVisibility_kAboveViewport
   *         nsRectVisibility_kBelowViewport 
   *         nsRectVisibility_kLeftOfViewport 
   *         nsRectVisibility_kRightOfViewport rectangle is outside the viewport
   *         in the specified direction 
   */
  virtual nsRectVisibility GetRectVisibility(nsIFrame *aFrame,
                                             const nsRect &aRect,
                                             nscoord aMinTwips) const = 0;

  /**
   * Suppress notification of the frame manager that frames are
   * being destroyed.
   */
  virtual NS_HIDDEN_(void) SetIgnoreFrameDestruction(bool aIgnore) = 0;

  /**
   * Notification sent by a frame informing the pres shell that it is about to
   * be destroyed.
   * This allows any outstanding references to the frame to be cleaned up
   */
  virtual NS_HIDDEN_(void) NotifyDestroyingFrame(nsIFrame* aFrame) = 0;

  /**
   * Get the caret, if it exists. AddRefs it.
   */
  virtual NS_HIDDEN_(already_AddRefed<nsCaret>) GetCaret() const = 0;

  /**
   * Invalidate the caret's current position if it's outside of its frame's
   * boundaries. This function is useful if you're batching selection
   * notifications and might remove the caret's frame out from under it.
   */
  virtual NS_HIDDEN_(void) MaybeInvalidateCaretPosition() = 0;

  /**
   * Set the current caret to a new caret. To undo this, call RestoreCaret.
   */
  virtual void SetCaret(nsCaret *aNewCaret) = 0;

  /**
   * Restore the caret to the original caret that this pres shell was created
   * with.
   */
  virtual void RestoreCaret() = 0;

  /**
   * Should the images have borders etc.  Actual visual effects are determined
   * by the frames.  Visual effects may not effect layout, only display.
   * Takes effect on next repaint, does not force a repaint itself.
   *
   * @param aInEnable  if true, visual selection effects are enabled
   *                   if false visual selection effects are disabled
   */
  NS_IMETHOD SetSelectionFlags(PRInt16 aInEnable) = 0;

  /** 
    * Gets the current state of non text selection effects
    * @return   current state of non text selection,
    *           as set by SetDisplayNonTextSelection
    */
  PRInt16 GetSelectionFlags() const { return mSelectionFlags; }

  virtual nsISelection* GetCurrentSelection(SelectionType aType) = 0;

  /**
    * Interface to dispatch events via the presshell
    * @note The caller must have a strong reference to the PresShell.
    */
  virtual NS_HIDDEN_(nsresult) HandleEventWithTarget(nsEvent* aEvent,
                                                     nsIFrame* aFrame,
                                                     nsIContent* aContent,
                                                     nsEventStatus* aStatus) = 0;

  /**
   * Dispatch event to content only (NOT full processing)
   * @note The caller must have a strong reference to the PresShell.
   */
  virtual NS_HIDDEN_(nsresult) HandleDOMEventWithTarget(nsIContent* aTargetContent,
                                                        nsEvent* aEvent,
                                                        nsEventStatus* aStatus) = 0;

  /**
   * Dispatch event to content only (NOT full processing)
   * @note The caller must have a strong reference to the PresShell.
   */
  virtual NS_HIDDEN_(nsresult) HandleDOMEventWithTarget(nsIContent* aTargetContent,
                                                        nsIDOMEvent* aEvent,
                                                        nsEventStatus* aStatus) = 0;

  /**
    * Gets the current target event frame from the PresShell
    */
  virtual NS_HIDDEN_(nsIFrame*) GetEventTargetFrame() = 0;

  /**
    * Gets the current target event frame from the PresShell
    */
  virtual NS_HIDDEN_(already_AddRefed<nsIContent>) GetEventTargetContent(nsEvent* aEvent) = 0;

  /**
   * Get and set the history state for the current document 
   */

  virtual NS_HIDDEN_(nsresult) CaptureHistoryState(nsILayoutHistoryState** aLayoutHistoryState, bool aLeavingPage = false) = 0;

  /**
   * Determine if reflow is currently locked
   * returns true if reflow is locked, false otherwise
   */
  bool IsReflowLocked() const { return mIsReflowing; }

  /**
   * Called to find out if painting is suppressed for this presshell.  If it is suppressd,
   * we don't allow the painting of any layer but the background, and we don't
   * recur into our children.
   */
  bool IsPaintingSuppressed() const { return mPaintingSuppressed; }

  /**
   * Unsuppress painting.
   */
  virtual NS_HIDDEN_(void) UnsuppressPainting() = 0;

  /**
   * Called to disable nsITheme support in a specific presshell.
   */
  void DisableThemeSupport()
  {
    // Doesn't have to be dynamic.  Just set the bool.
    mIsThemeSupportDisabled = true;
  }

  /**
   * Indicates whether theme support is enabled.
   */
  bool IsThemeSupportEnabled() const { return !mIsThemeSupportDisabled; }

  /**
   * Get the set of agent style sheets for this presentation
   */
  virtual nsresult GetAgentStyleSheets(nsCOMArray<nsIStyleSheet>& aSheets) = 0;

  /**
   * Replace the set of agent style sheets
   */
  virtual nsresult SetAgentStyleSheets(const nsCOMArray<nsIStyleSheet>& aSheets) = 0;

  /**
   * Add an override style sheet for this presentation
   */
  virtual nsresult AddOverrideStyleSheet(nsIStyleSheet *aSheet) = 0;

  /**
   * Remove an override style sheet
   */
  virtual nsresult RemoveOverrideStyleSheet(nsIStyleSheet *aSheet) = 0;

  /**
   * Reconstruct frames for all elements in the document
   */
  virtual nsresult ReconstructFrames() = 0;

  /**
   * Notify that a content node's state has changed
   */
  virtual void ContentStateChanged(nsIDocument* aDocument,
                                   nsIContent* aContent,
                                   nsEventStates aStateMask) = 0;

  /**
   * Given aFrame, the root frame of a stacking context, find its descendant
   * frame under the point aPt that receives a mouse event at that location,
   * or nsnull if there is no such frame.
   * @param aPt the point, relative to the frame origin
   */
  virtual nsIFrame* GetFrameForPoint(nsIFrame* aFrame, nsPoint aPt) = 0;

  /**
   * See if reflow verification is enabled. To enable reflow verification add
   * "verifyreflow:1" to your NSPR_LOG_MODULES environment variable
   * (any non-zero debug level will work). Or, call SetVerifyReflowEnable
   * with true.
   */
  static bool GetVerifyReflowEnable();

  /**
   * Set the verify-reflow enable flag.
   */
  static void SetVerifyReflowEnable(bool aEnabled);

  virtual nsIFrame* GetAbsoluteContainingBlock(nsIFrame* aFrame);

#ifdef MOZ_REFLOW_PERF
  virtual NS_HIDDEN_(void) DumpReflows() = 0;
  virtual NS_HIDDEN_(void) CountReflows(const char * aName, nsIFrame * aFrame) = 0;
  virtual NS_HIDDEN_(void) PaintCount(const char * aName,
                                      nsRenderingContext* aRenderingContext,
                                      nsPresContext * aPresContext,
                                      nsIFrame * aFrame,
                                      const nsPoint& aOffset,
                                      PRUint32 aColor) = 0;
  virtual NS_HIDDEN_(void) SetPaintFrameCount(bool aOn) = 0;
  virtual bool IsPaintingFrameCounts() = 0;
#endif

#ifdef DEBUG
  // Debugging hooks
  virtual void ListStyleContexts(nsIFrame *aRootFrame, FILE *out,
                                 PRInt32 aIndent = 0) = 0;

  virtual void ListStyleSheets(FILE *out, PRInt32 aIndent = 0) = 0;
  virtual void VerifyStyleTree() = 0;
#endif

#ifdef ACCESSIBILITY
  /**
   * Return true if accessibility is active.
   */
  static bool IsAccessibilityActive();

  /**
   * Return accessibility service if accessibility is active.
   */
  static nsAccessibilityService* AccService();
#endif

  /**
   * Stop all active elements (plugins and the caret) in this presentation and
   * in the presentations of subdocuments.  Resets painting to a suppressed state.
   * XXX this should include image animations
   */
  virtual void Freeze() = 0;
  bool IsFrozen() { return mFrozen; }

  /**
   * Restarts active elements (plugins) in this presentation and in the
   * presentations of subdocuments, then do a full invalidate of the content area.
   */
  virtual void Thaw() = 0;

  virtual void FireOrClearDelayedEvents(bool aFireEvents) = 0;

  /**
   * When this shell is disconnected from its containing docshell, we
   * lose our container pointer.  However, we'd still like to be able to target
   * user events at the docshell's parent.  This pointer allows us to do that.
   * It should not be used for any other purpose.
   */
  void SetForwardingContainer(nsWeakPtr aContainer)
  {
    mForwardingContainer = aContainer;
  }
  
  /**
   * Render the document into an arbitrary gfxContext
   * Designed for getting a picture of a document or a piece of a document
   * Note that callers will generally want to call FlushPendingNotifications
   * to get an up-to-date view of the document
   * @param aRect is the region to capture into the offscreen buffer, in the
   * root frame's coordinate system (if aIgnoreViewportScrolling is false)
   * or in the root scrolled frame's coordinate system
   * (if aIgnoreViewportScrolling is true). The coordinates are in appunits.
   * @param aFlags see below;
   *   set RENDER_IS_UNTRUSTED if the contents may be passed to malicious
   * agents. E.g. we might choose not to paint the contents of sensitive widgets
   * such as the file name in a file upload widget, and we might choose not
   * to paint themes.
   *   set RENDER_IGNORE_VIEWPORT_SCROLLING to ignore
   * clipping/scrolling/scrollbar painting due to scrolling in the viewport
   *   set RENDER_CARET to draw the caret if one would be visible
   * (by default the caret is never drawn)
   *   set RENDER_USE_LAYER_MANAGER to force rendering to go through
   * the layer manager for the window. This may be unexpectedly slow
   * (if the layer manager must read back data from the GPU) or low-quality
   * (if the layer manager reads back pixel data and scales it
   * instead of rendering using the appropriate scaling). It may also
   * slow everything down if the area rendered does not correspond to the
   * normal visible area of the window.
   *   set RENDER_ASYNC_DECODE_IMAGES to avoid having images synchronously
   * decoded during rendering.
   * (by default images decode synchronously with RenderDocument)
   *   set RENDER_DOCUMENT_RELATIVE to interpret |aRect| relative to the
   * document instead of the CSS viewport
   * @param aBackgroundColor a background color to render onto
   * @param aRenderedContext the gfxContext to render to. We render so that
   * one CSS pixel in the source document is rendered to one unit in the current
   * transform.
   */
  enum {
    RENDER_IS_UNTRUSTED = 0x01,
    RENDER_IGNORE_VIEWPORT_SCROLLING = 0x02,
    RENDER_CARET = 0x04,
    RENDER_USE_WIDGET_LAYERS = 0x08,
    RENDER_ASYNC_DECODE_IMAGES = 0x10,
    RENDER_DOCUMENT_RELATIVE = 0x20
  };
  virtual NS_HIDDEN_(nsresult) RenderDocument(const nsRect& aRect, PRUint32 aFlags,
                                              nscolor aBackgroundColor,
                                              gfxContext* aRenderedContext) = 0;

  /**
   * Renders a node aNode to a surface and returns it. The aRegion may be used
   * to clip the rendering. This region is measured in CSS pixels from the
   * edge of the presshell area. The aPoint, aScreenRect and aSurface
   * arguments function in a similar manner as RenderSelection.
   */
  virtual already_AddRefed<gfxASurface> RenderNode(nsIDOMNode* aNode,
                                                   nsIntRegion* aRegion,
                                                   nsIntPoint& aPoint,
                                                   nsIntRect* aScreenRect) = 0;

  /**
   * Renders a selection to a surface and returns it. This method is primarily
   * intended to create the drag feedback when dragging a selection.
   *
   * aScreenRect will be filled in with the bounding rectangle of the
   * selection area on screen.
   *
   * If the area of the selection is large, the image will be scaled down.
   * The argument aPoint is used in this case as a reference point when
   * determining the new screen rectangle after scaling. Typically, this
   * will be the mouse position, so that the screen rectangle is positioned
   * such that the mouse is over the same point in the scaled image as in
   * the original. When scaling does not occur, the mouse point isn't used
   * as the position can be determined from the displayed frames.
   */
  virtual already_AddRefed<gfxASurface> RenderSelection(nsISelection* aSelection,
                                                        nsIntPoint& aPoint,
                                                        nsIntRect* aScreenRect) = 0;

  void AddWeakFrameInternal(nsWeakFrame* aWeakFrame);
  virtual void AddWeakFrameExternal(nsWeakFrame* aWeakFrame);

  void AddWeakFrame(nsWeakFrame* aWeakFrame)
  {
#ifdef _IMPL_NS_LAYOUT
    AddWeakFrameInternal(aWeakFrame);
#else
    AddWeakFrameExternal(aWeakFrame);
#endif
  }

  void RemoveWeakFrameInternal(nsWeakFrame* aWeakFrame);
  virtual void RemoveWeakFrameExternal(nsWeakFrame* aWeakFrame);

  void RemoveWeakFrame(nsWeakFrame* aWeakFrame)
  {
#ifdef _IMPL_NS_LAYOUT
    RemoveWeakFrameInternal(aWeakFrame);
#else
    RemoveWeakFrameExternal(aWeakFrame);
#endif
  }

#ifdef NS_DEBUG
  nsIFrame* GetDrawEventTargetFrame() { return mDrawEventTargetFrame; }
#endif

  /**
   * Stop or restart non synthetic test mouse event handling on *all*
   * presShells.
   *
   * @param aDisable If true, disable all non synthetic test mouse
   * events on all presShells.  Otherwise, enable them.
   */
  virtual NS_HIDDEN_(void) DisableNonTestMouseEvents(bool aDisable) = 0;

  /**
   * Record the background color of the most recently drawn canvas. This color
   * is composited on top of the user's default background color and then used
   * to draw the background color of the canvas. See PresShell::Paint,
   * PresShell::PaintDefaultBackground, and nsDocShell::SetupNewViewer;
   * bug 488242, bug 476557 and other bugs mentioned there.
   */
  void SetCanvasBackground(nscolor aColor) { mCanvasBackgroundColor = aColor; }
  nscolor GetCanvasBackground() { return mCanvasBackgroundColor; }

  /**
   * Use the current frame tree (if it exists) to update the background
   * color of the most recently drawn canvas.
   */
  virtual void UpdateCanvasBackground() = 0;

  /**
   * Add a solid color item to the bottom of aList with frame aFrame and bounds
   * aBounds. Checks first if this needs to be done by checking if aFrame is a
   * canvas frame (if the FORCE_DRAW flag is passed then this check is skipped).
   * aBackstopColor is composed behind the background color of the canvas, it is
   * transparent by default.
   */
  enum {
    FORCE_DRAW = 0x01
  };
  virtual nsresult AddCanvasBackgroundColorItem(nsDisplayListBuilder& aBuilder,
                                                nsDisplayList& aList,
                                                nsIFrame* aFrame,
                                                const nsRect& aBounds,
                                                nscolor aBackstopColor = NS_RGBA(0,0,0,0),
                                                PRUint32 aFlags = 0) = 0;


  /**
   * Add a solid color item to the bottom of aList with frame aFrame and
   * bounds aBounds representing the dark grey background behind the page of a
   * print preview presentation.
   */
  virtual nsresult AddPrintPreviewBackgroundItem(nsDisplayListBuilder& aBuilder,
                                                 nsDisplayList& aList,
                                                 nsIFrame* aFrame,
                                                 const nsRect& aBounds) = 0;

  /**
   * Computes the backstop color for the view: transparent if in a transparent
   * widget, otherwise the PresContext default background color. This color is
   * only visible if the contents of the view as a whole are translucent.
   */
  virtual nscolor ComputeBackstopColor(nsIView* aDisplayRoot) = 0;

  void ObserveNativeAnonMutationsForPrint(bool aObserve)
  {
    mObservesMutationsForPrint = aObserve;
  }
  bool ObservesNativeAnonMutationsForPrint()
  {
    return mObservesMutationsForPrint;
  }

  virtual nsresult SetIsActive(bool aIsActive) = 0;

  bool IsActive()
  {
    return mIsActive;
  }

  // mouse capturing

  static CapturingContentInfo gCaptureInfo;

  static nsInterfaceHashtable<nsUint32HashKey, nsIDOMTouch> gCaptureTouchList;
  static bool gPreventMouseEvents;

  /**
   * When capturing content is set, it traps all mouse events and retargets
   * them at this content node. If capturing is not allowed
   * (gCaptureInfo.mAllowed is false), then capturing is not set. However, if
   * the CAPTURE_IGNOREALLOWED flag is set, the allowed state is ignored and
   * capturing is set regardless. To disable capture, pass null for the value
   * of aContent.
   *
   * If CAPTURE_RETARGETTOELEMENT is set, all mouse events are targeted at
   * aContent only. Otherwise, mouse events are targeted at aContent or its
   * descendants. That is, descendants of aContent receive mouse events as
   * they normally would, but mouse events outside of aContent are retargeted
   * to aContent.
   *
   * If CAPTURE_PREVENTDRAG is set then drags are prevented from starting while
   * this capture is active.
   *
   * If CAPTURE_POINTERLOCK is set, similar to CAPTURE_RETARGETTOELEMENT, then
   * events are targeted at aContent, but capturing is held more strongly (i.e.,
   * calls to SetCapturingContent won't unlock unless CAPTURE_POINTERLOCK is
   * set again).
   */
  static void SetCapturingContent(nsIContent* aContent, PRUint8 aFlags);

  /**
   * Return the active content currently capturing the mouse if any.
   */
  static nsIContent* GetCapturingContent()
  {
    return gCaptureInfo.mContent;
  }

  /**
   * Allow or disallow mouse capturing.
   */
  static void AllowMouseCapture(bool aAllowed)
  {
    gCaptureInfo.mAllowed = aAllowed;
  }

  /**
   * Returns true if there is an active mouse capture that wants to prevent
   * drags.
   */
  static bool IsMouseCapturePreventingDrag()
  {
    return gCaptureInfo.mPreventDrag && gCaptureInfo.mContent;
  }

  /**
   * Keep track of how many times this presshell has been rendered to
   * a window.
   */
  PRUint64 GetPaintCount() { return mPaintCount; }
  void IncrementPaintCount() { ++mPaintCount; }

  /**
   * Get the root DOM window of this presShell.
   */
  virtual already_AddRefed<nsPIDOMWindow> GetRootWindow() = 0;

  /**
   * Get the layer manager for the widget of the root view, if it has
   * one.
   */
  virtual LayerManager* GetLayerManager() = 0;

  /**
   * Track whether we're ignoring viewport scrolling for the purposes
   * of painting.  If we are ignoring, then layers aren't clipped to
   * the CSS viewport and scrollbars aren't drawn.
   */
  virtual void SetIgnoreViewportScrolling(bool aIgnore) = 0;
  bool IgnoringViewportScrolling() const
  { return mRenderFlags & STATE_IGNORING_VIEWPORT_SCROLLING; }

   /**
   * Set a "resolution" for the document, which if not 1.0 will
   * allocate more or fewer pixels for rescalable content by a factor
   * of |resolution| in both dimensions.  Return NS_OK iff the
   * resolution bounds are sane, and the resolution of this was
   * actually updated.
   *
   * The resolution defaults to 1.0.
   */
  virtual nsresult SetResolution(float aXResolution, float aYResolution) = 0;
  float GetXResolution() { return mXResolution; }
  float GetYResolution() { return mYResolution; }

  /**
   * Set the isFirstPaint flag.
   */
  void SetIsFirstPaint(bool aIsFirstPaint) { mIsFirstPaint = aIsFirstPaint; }

  /**
   * Get the isFirstPaint flag.
   */
  bool GetIsFirstPaint() const { return mIsFirstPaint; }

  /**
   * Dispatch a mouse move event based on the most recent mouse position if
   * this PresShell is visible. This is used when the contents of the page
   * moved (aFromScroll is false) or scrolled (aFromScroll is true).
   */
  virtual void SynthesizeMouseMove(bool aFromScroll) = 0;

  virtual void Paint(nsIView* aViewToPaint, nsIWidget* aWidget,
                     const nsRegion& aDirtyRegion, const nsIntRegion& aIntDirtyRegion,
                     bool aWillSendDidPaint) = 0;
  virtual nsresult HandleEvent(nsIFrame*       aFrame,
                               nsGUIEvent*     aEvent,
                               bool            aDontRetargetEvents,
                               nsEventStatus*  aEventStatus) = 0;
  virtual bool ShouldIgnoreInvalidation() = 0;
  /**
   * Notify that the NS_WILL_PAINT event was received. Fires on every
   * visible presshell in the document tree.
   */
  virtual void WillPaint(bool aWillSendDidPaint) = 0;
  /**
   * Notify that the NS_DID_PAINT event was received. Only fires on the
   * root pres shell.
   */
  virtual void DidPaint() = 0;
  virtual void ScheduleViewManagerFlush() = 0;
  virtual void ClearMouseCaptureOnView(nsIView* aView) = 0;
  virtual bool IsVisible() = 0;
  virtual void DispatchSynthMouseMove(nsGUIEvent *aEvent, bool aFlushOnHoverChange) = 0;

  virtual void SizeOfIncludingThis(nsMallocSizeOfFun aMallocSizeOf,
                                   size_t *aArenasSize,
                                   size_t *aStyleSetsSize,
                                   size_t *aTextRunsSize,
                                   size_t *aPresContextSize) const = 0;

  /**
   * Refresh observer management.
   */
protected:
  virtual bool AddRefreshObserverExternal(nsARefreshObserver* aObserver,
                                            mozFlushType aFlushType);
  bool AddRefreshObserverInternal(nsARefreshObserver* aObserver,
                                    mozFlushType aFlushType);
  virtual bool RemoveRefreshObserverExternal(nsARefreshObserver* aObserver,
                                               mozFlushType aFlushType);
  bool RemoveRefreshObserverInternal(nsARefreshObserver* aObserver,
                                       mozFlushType aFlushType);
public:
  bool AddRefreshObserver(nsARefreshObserver* aObserver,
                            mozFlushType aFlushType) {
#ifdef _IMPL_NS_LAYOUT
    return AddRefreshObserverInternal(aObserver, aFlushType);
#else
    return AddRefreshObserverExternal(aObserver, aFlushType);
#endif
  }

  bool RemoveRefreshObserver(nsARefreshObserver* aObserver,
                               mozFlushType aFlushType) {
#ifdef _IMPL_NS_LAYOUT
    return RemoveRefreshObserverInternal(aObserver, aFlushType);
#else
    return RemoveRefreshObserverExternal(aObserver, aFlushType);
#endif
  }

  /**
   * Initialize and shut down static variables.
   */
  static void InitializeStatics();
  static void ReleaseStatics();

  // If a frame in the subtree rooted at aFrame is capturing the mouse then
  // clears that capture.
  static void ClearMouseCapture(nsIFrame* aFrame);

  void SetScrollPositionClampingScrollPortSize(nscoord aWidth, nscoord aHeight);
  bool IsScrollPositionClampingScrollPortSizeSet() {
    return mScrollPositionClampingScrollPortSizeSet;
  }
  nsSize GetScrollPositionClampingScrollPortSize() {
    NS_ASSERTION(mScrollPositionClampingScrollPortSizeSet, "asking for scroll port when its not set?");
    return mScrollPositionClampingScrollPortSize;
  }

protected:
  friend class nsRefreshDriver;

  // IMPORTANT: The ownership implicit in the following member variables
  // has been explicitly checked.  If you add any members to this class,
  // please make the ownership explicit (pinkerton, scc).

  // These are the same Document and PresContext owned by the DocViewer.
  // we must share ownership.
  nsIDocument*              mDocument;      // [STRONG]
  nsPresContext*            mPresContext;   // [STRONG]
  nsStyleSet*               mStyleSet;      // [OWNS]
  nsCSSFrameConstructor*    mFrameConstructor; // [OWNS]
  nsIViewManager*           mViewManager;   // [WEAK] docViewer owns it so I don't have to
  nsPresArena               mFrameArena;
  nsFrameSelection*         mSelection;
  // Pointer into mFrameConstructor - this is purely so that FrameManager() and
  // GetRootFrame() can be inlined:
  nsFrameManagerBase*       mFrameManager;
  nsWeakPtr                 mForwardingContainer;

#ifdef NS_DEBUG
  nsIFrame*                 mDrawEventTargetFrame;
  // Ensure that every allocation from the PresArena is eventually freed.
  PRUint32                  mPresArenaAllocCount;
#endif

  // Count of the number of times this presshell has been painted to a window.
  PRUint64                  mPaintCount;

  nsSize                    mScrollPositionClampingScrollPortSize;

  // A list of weak frames. This is a pointer to the last item in the list.
  nsWeakFrame*              mWeakFrames;

  // Most recent canvas background color.
  nscolor                   mCanvasBackgroundColor;

  // Used to force allocation and rendering of proportionally more or
  // less pixels in the given dimension.
  float                     mXResolution;
  float                     mYResolution;

  PRInt16                   mSelectionFlags;

  // Flags controlling how our document is rendered.  These persist
  // between paints and so are tied with retained layer pixels.
  // PresShell flushes retained layers when the rendering state
  // changes in a way that prevents us from being able to (usefully)
  // re-use old pixels.
  RenderFlags               mRenderFlags;

  bool                      mStylesHaveChanged : 1;
  bool                      mDidInitialReflow : 1;
  bool                      mIsDestroying : 1;
  bool                      mIsReflowing : 1;

  // For all documents we initially lock down painting.
  bool                      mPaintingSuppressed : 1;

  // Whether or not form controls should use nsITheme in this shell.
  bool                      mIsThemeSupportDisabled : 1;

  bool                      mIsActive : 1;
  bool                      mFrozen : 1;
  bool                      mIsFirstPaint : 1;
  bool                      mObservesMutationsForPrint : 1;

  // If true, we have a reflow scheduled. Guaranteed to be false if
  // mReflowContinueTimer is non-null.
  bool                      mReflowScheduled : 1;

  bool                      mSuppressInterruptibleReflows : 1;
  bool                      mScrollPositionClampingScrollPortSizeSet : 1;

  static nsIContent*        gKeyDownTarget;
};

/**
 * Create a new empty presentation shell. Upon success, call Init
 * before attempting to use the shell.
 */
nsresult
NS_NewPresShell(nsIPresShell** aInstancePtrResult);

#endif /* nsIPresShell_h___ */
