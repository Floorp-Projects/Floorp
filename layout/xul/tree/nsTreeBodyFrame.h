/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsTreeBodyFrame_h
#define nsTreeBodyFrame_h

#include "mozilla/Attributes.h"

#include "nsLeafBoxFrame.h"
#include "nsITreeView.h"
#include "nsICSSPseudoComparator.h"
#include "nsIScrollbarMediator.h"
#include "nsITimer.h"
#include "nsIReflowCallback.h"
#include "nsTArray.h"
#include "nsTreeStyleCache.h"
#include "nsTreeColumns.h"
#include "nsDataHashtable.h"
#include "imgIRequest.h"
#include "imgINotificationObserver.h"
#include "nsScrollbarFrame.h"
#include "nsThreadUtils.h"
#include "mozilla/LookAndFeel.h"

class nsFontMetrics;
class nsOverflowChecker;
class nsTreeImageListener;

namespace mozilla {
namespace layout {
class ScrollbarActivity;
} // namespace layout
} // namespace mozilla

// An entry in the tree's image cache
struct nsTreeImageCacheEntry
{
  nsTreeImageCacheEntry() {}
  nsTreeImageCacheEntry(imgIRequest *aRequest, imgINotificationObserver *aListener)
    : request(aRequest), listener(aListener) {}

  nsCOMPtr<imgIRequest> request;
  nsCOMPtr<imgINotificationObserver> listener;
};

// The actual frame that paints the cells and rows.
class nsTreeBodyFrame final
  : public nsLeafBoxFrame
  , public nsICSSPseudoComparator
  , public nsIScrollbarMediator
  , public nsIReflowCallback
{
  typedef mozilla::layout::ScrollbarActivity ScrollbarActivity;
  typedef mozilla::image::DrawResult DrawResult;

public:
  explicit nsTreeBodyFrame(nsStyleContext* aContext);
  ~nsTreeBodyFrame();

  NS_DECL_QUERYFRAME
  NS_DECL_FRAMEARENA_HELPERS(nsTreeBodyFrame)

  // Callback handler methods for refresh driver based animations.
  // Calls to these functions are forwarded from nsTreeImageListener. These
  // mirror how nsImageFrame works.
  nsresult OnImageIsAnimated(imgIRequest* aRequest);

  // non-virtual signatures like nsITreeBodyFrame
  already_AddRefed<nsTreeColumns> Columns() const
  {
    RefPtr<nsTreeColumns> cols = mColumns;
    return cols.forget();
  }
  already_AddRefed<nsITreeView> GetExistingView() const
  {
    nsCOMPtr<nsITreeView> view = mView;
    return view.forget();
  }
  nsresult GetView(nsITreeView **aView);
  nsresult SetView(nsITreeView *aView);
  bool GetFocused() const { return mFocused; }
  nsresult SetFocused(bool aFocused);
  nsresult GetTreeBody(nsIDOMElement **aElement);
  int32_t RowHeight() const;
  int32_t RowWidth();
  int32_t GetHorizontalPosition() const;
  nsresult GetSelectionRegion(nsIScriptableRegion **aRegion);
  int32_t FirstVisibleRow() const { return mTopRowIndex; }
  int32_t LastVisibleRow() const { return mTopRowIndex + mPageLength; }
  int32_t PageLength() const { return mPageLength; }
  nsresult EnsureRowIsVisible(int32_t aRow);
  nsresult EnsureCellIsVisible(int32_t aRow, nsITreeColumn *aCol);
  nsresult ScrollToRow(int32_t aRow);
  nsresult ScrollByLines(int32_t aNumLines);
  nsresult ScrollByPages(int32_t aNumPages);
  nsresult ScrollToCell(int32_t aRow, nsITreeColumn *aCol);
  nsresult ScrollToColumn(nsITreeColumn *aCol);
  nsresult ScrollToHorizontalPosition(int32_t aValue);
  nsresult Invalidate();
  nsresult InvalidateColumn(nsITreeColumn *aCol);
  nsresult InvalidateRow(int32_t aRow);
  nsresult InvalidateCell(int32_t aRow, nsITreeColumn *aCol);
  nsresult InvalidateRange(int32_t aStart, int32_t aEnd);
  nsresult InvalidateColumnRange(int32_t aStart, int32_t aEnd,
                                 nsITreeColumn *aCol);
  nsresult GetRowAt(int32_t aX, int32_t aY, int32_t *aValue);
  nsresult GetCellAt(int32_t aX, int32_t aY, int32_t *aRow,
                     nsITreeColumn **aCol, nsACString &aChildElt);
  nsresult GetCoordsForCellItem(int32_t aRow, nsITreeColumn *aCol,
                                const nsACString &aElt,
                                int32_t *aX, int32_t *aY,
                                int32_t *aWidth, int32_t *aHeight);
  nsresult IsCellCropped(int32_t aRow, nsITreeColumn *aCol, bool *aResult);
  nsresult RowCountChanged(int32_t aIndex, int32_t aCount);
  nsresult BeginUpdateBatch();
  nsresult EndUpdateBatch();
  nsresult ClearStyleAndImageCaches();
  nsresult RemoveImageCacheEntry(int32_t aRowIndex, nsITreeColumn* aCol);

  void CancelImageRequests();

  void ManageReflowCallback(const nsRect& aRect, nscoord aHorzWidth);

  virtual nsSize GetXULMinSize(nsBoxLayoutState& aBoxLayoutState) override;
  virtual void SetXULBounds(nsBoxLayoutState& aBoxLayoutState, const nsRect& aRect,
                            bool aRemoveOverflowArea = false) override;

  // nsIReflowCallback
  virtual bool ReflowFinished() override;
  virtual void ReflowCallbackCanceled() override;

  // nsICSSPseudoComparator
  virtual bool PseudoMatches(nsCSSSelector* aSelector) override;

  // nsIScrollbarMediator
  virtual void ScrollByPage(nsScrollbarFrame* aScrollbar, int32_t aDirection,
                            nsIScrollbarMediator::ScrollSnapMode aSnap
                              = nsIScrollbarMediator::DISABLE_SNAP) override;
  virtual void ScrollByWhole(nsScrollbarFrame* aScrollbar, int32_t aDirection,
                             nsIScrollbarMediator::ScrollSnapMode aSnap
                               = nsIScrollbarMediator::DISABLE_SNAP) override;
  virtual void ScrollByLine(nsScrollbarFrame* aScrollbar, int32_t aDirection,
                            nsIScrollbarMediator::ScrollSnapMode aSnap
                              = nsIScrollbarMediator::DISABLE_SNAP) override;
  virtual void RepeatButtonScroll(nsScrollbarFrame* aScrollbar) override;
  virtual void ThumbMoved(nsScrollbarFrame* aScrollbar,
                          nscoord aOldPos,
                          nscoord aNewPos) override;
  virtual void ScrollbarReleased(nsScrollbarFrame* aScrollbar) override {}
  virtual void VisibilityChanged(bool aVisible) override { Invalidate(); }
  virtual nsIFrame* GetScrollbarBox(bool aVertical) override {
    ScrollParts parts = GetScrollParts();
    return aVertical ? parts.mVScrollbar : parts.mHScrollbar;
  }
  virtual void ScrollbarActivityStarted() const override;
  virtual void ScrollbarActivityStopped() const override;
  virtual bool IsScrollbarOnRight() const override {
    return (StyleVisibility()->mDirection == NS_STYLE_DIRECTION_LTR);
  }
  virtual bool ShouldSuppressScrollbarRepaints() const override {
    return false;
  }

  // Overridden from nsIFrame to cache our pres context.
  virtual void Init(nsIContent*       aContent,
                    nsContainerFrame* aParent,
                    nsIFrame*         aPrevInFlow) override;
  virtual void DestroyFrom(nsIFrame* aDestructRoot) override;

  virtual nsresult GetCursor(const nsPoint& aPoint,
                             nsIFrame::Cursor& aCursor) override;

  virtual nsresult HandleEvent(nsPresContext* aPresContext,
                               mozilla::WidgetGUIEvent* aEvent,
                               nsEventStatus* aEventStatus) override;

  virtual void BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                                const nsRect&           aDirtyRect,
                                const nsDisplayListSet& aLists) override;

  virtual void DidSetStyleContext(nsStyleContext* aOldStyleContext) override;

  friend nsIFrame* NS_NewTreeBodyFrame(nsIPresShell* aPresShell);
  friend class nsTreeColumn;

  struct ScrollParts {
    nsScrollbarFrame*    mVScrollbar;
    nsCOMPtr<nsIContent> mVScrollbarContent;
    nsScrollbarFrame*    mHScrollbar;
    nsCOMPtr<nsIContent> mHScrollbarContent;
    nsIFrame*            mColumnsFrame;
    nsIScrollableFrame*  mColumnsScrollFrame;
  };

  DrawResult PaintTreeBody(gfxContext& aRenderingContext,
                           const nsRect& aDirtyRect, nsPoint aPt,
                           nsDisplayListBuilder* aBuilder);

  nsITreeBoxObject* GetTreeBoxObject() const { return mTreeBoxObject; }

  // Get the base element, <tree> or <select>
  nsIContent* GetBaseElement();

  bool GetVerticalOverflow() const { return mVerticalOverflow; }
  bool GetHorizontalOverflow() const {return mHorizontalOverflow; }

protected:
  friend class nsOverflowChecker;

  // This method paints a specific column background of the tree.
  DrawResult PaintColumn(nsTreeColumn*        aColumn,
                         const nsRect&        aColumnRect,
                         nsPresContext*       aPresContext,
                         gfxContext&          aRenderingContext,
                         const nsRect&        aDirtyRect);

  // This method paints a single row in the tree.
  DrawResult PaintRow(int32_t               aRowIndex,
                      const nsRect&         aRowRect,
                      nsPresContext*        aPresContext,
                      gfxContext&           aRenderingContext,
                      const nsRect&         aDirtyRect,
                      nsPoint               aPt,
                      nsDisplayListBuilder* aBuilder);

  // This method paints a single separator in the tree.
  DrawResult PaintSeparator(int32_t              aRowIndex,
                            const nsRect&        aSeparatorRect,
                            nsPresContext*       aPresContext,
                            gfxContext&          aRenderingContext,
                            const nsRect&        aDirtyRect);

  // This method paints a specific cell in a given row of the tree.
  DrawResult PaintCell(int32_t               aRowIndex,
                       nsTreeColumn*         aColumn,
                       const nsRect&         aCellRect,
                       nsPresContext*        aPresContext,
                       gfxContext&           aRenderingContext,
                       const nsRect&         aDirtyRect,
                       nscoord&              aCurrX,
                       nsPoint               aPt,
                       nsDisplayListBuilder* aBuilder);

  // This method paints the twisty inside a cell in the primary column of an tree.
  DrawResult PaintTwisty(int32_t              aRowIndex,
                         nsTreeColumn*        aColumn,
                         const nsRect&        aTwistyRect,
                         nsPresContext*       aPresContext,
                         gfxContext&          aRenderingContext,
                         const nsRect&        aDirtyRect,
                         nscoord&             aRemainingWidth,
                         nscoord&             aCurrX);

  // This method paints the image inside the cell of an tree.
  DrawResult PaintImage(int32_t               aRowIndex,
                        nsTreeColumn*         aColumn,
                        const nsRect&         aImageRect,
                        nsPresContext*        aPresContext,
                        gfxContext&           aRenderingContext,
                        const nsRect&         aDirtyRect,
                        nscoord&              aRemainingWidth,
                        nscoord&              aCurrX,
                        nsDisplayListBuilder* aBuilder);

  // This method paints the text string inside a particular cell of the tree.
  DrawResult PaintText(int32_t             aRowIndex,
                       nsTreeColumn*       aColumn,
                       const nsRect&       aTextRect,
                       nsPresContext*      aPresContext,
                       gfxContext&         aRenderingContext,
                       const nsRect&       aDirtyRect,
                       nscoord&            aCurrX);

  // This method paints the checkbox inside a particular cell of the tree.
  DrawResult PaintCheckbox(int32_t              aRowIndex, 
                           nsTreeColumn*        aColumn,
                           const nsRect&        aCheckboxRect,
                           nsPresContext*       aPresContext,
                           gfxContext&          aRenderingContext,
                           const nsRect&        aDirtyRect);

  // This method paints the progress meter inside a particular cell of the tree.
  DrawResult PaintProgressMeter(int32_t               aRowIndex,
                                nsTreeColumn*         aColumn,
                                const nsRect&         aProgressMeterRect,
                                nsPresContext*        aPresContext,
                                gfxContext&           aRenderingContext,
                                const nsRect&         aDirtyRect,
                                nsDisplayListBuilder* aBuilder);

  // This method paints a drop feedback of the tree.
  DrawResult PaintDropFeedback(const nsRect&        aDropFeedbackRect, 
                               nsPresContext*       aPresContext,
                               gfxContext&          aRenderingContext,
                               const nsRect&        aDirtyRect,
                               nsPoint              aPt);

  // This method is called with a specific style context and rect to
  // paint the background rect as if it were a full-blown frame.
  DrawResult PaintBackgroundLayer(nsStyleContext*      aStyleContext,
                                  nsPresContext*       aPresContext, 
                                  gfxContext&          aRenderingContext,
                                  const nsRect&        aRect,
                                  const nsRect&        aDirtyRect);


  // An internal hit test.  aX and aY are expected to be in twips in the
  // coordinate system of this frame.
  int32_t GetRowAt(nscoord aX, nscoord aY);

  // Check for bidi characters in the text, and if there are any, ensure
  // that the prescontext is in bidi mode.
  void CheckTextForBidi(nsAutoString& aText);

  void AdjustForCellText(nsAutoString& aText,
                         int32_t aRowIndex,
                         nsTreeColumn* aColumn,
                         gfxContext& aRenderingContext,
                         nsFontMetrics& aFontMetrics,
                         nsRect& aTextRect);

  // A helper used when hit testing.
  nsICSSAnonBoxPseudo* GetItemWithinCellAt(nscoord aX,
                                           const nsRect& aCellRect,
                                           int32_t aRowIndex,
                                           nsTreeColumn* aColumn);

  // An internal hit test.  aX and aY are expected to be in twips in the
  // coordinate system of this frame.
  void GetCellAt(nscoord aX, nscoord aY, int32_t* aRow, nsTreeColumn** aCol,
                 nsICSSAnonBoxPseudo** aChildElt);

  // Retrieve the area for the twisty for a cell.
  nsITheme* GetTwistyRect(int32_t aRowIndex,
                          nsTreeColumn* aColumn,
                          nsRect& aImageRect,
                          nsRect& aTwistyRect,
                          nsPresContext* aPresContext,
                          nsStyleContext* aTwistyContext);

  // Fetch an image from the image cache.
  nsresult GetImage(int32_t aRowIndex, nsTreeColumn* aCol, bool aUseContext,
                    nsStyleContext* aStyleContext, bool& aAllowImageRegions, imgIContainer** aResult);

  // Returns the size of a given image.   This size *includes* border and
  // padding.  It does not include margins.
  nsRect GetImageSize(int32_t aRowIndex, nsTreeColumn* aCol, bool aUseContext, nsStyleContext* aStyleContext);

  // Returns the destination size of the image, not including borders and padding.
  nsSize GetImageDestSize(nsStyleContext* aStyleContext, bool useImageRegion, imgIContainer* image);

  // Returns the source rectangle of the image to be displayed.
  nsRect GetImageSourceRect(nsStyleContext* aStyleContext, bool useImageRegion, imgIContainer* image);

  // Returns the height of rows in the tree.
  int32_t GetRowHeight();

  // Returns our indentation width.
  int32_t GetIndentation();

  // Calculates our width/height once border and padding have been removed.
  void CalcInnerBox();

  // Calculate the total width of our scrollable portion
  nscoord CalcHorzWidth(const ScrollParts& aParts);

  // Looks up a style context in the style cache.  On a cache miss we resolve
  // the pseudo-styles passed in and place them into the cache.
  nsStyleContext* GetPseudoStyleContext(nsICSSAnonBoxPseudo* aPseudoElement);

  // Retrieves the scrollbars and scrollview relevant to this treebody. We
  // traverse the frame tree under our base element, in frame order, looking
  // for the first relevant vertical scrollbar, horizontal scrollbar, and
  // scrollable frame (with associated content and scrollable view). These
  // are all volatile and should not be retained.
  ScrollParts GetScrollParts();

  // Update the curpos of the scrollbar.
  void UpdateScrollbars(const ScrollParts& aParts);

  // Update the maxpos of the scrollbar.
  void InvalidateScrollbars(const ScrollParts& aParts, AutoWeakFrame& aWeakColumnsFrame);

  // Check overflow and generate events.
  void CheckOverflow(const ScrollParts& aParts);

  // Calls UpdateScrollbars, Invalidate aNeedsFullInvalidation if true,
  // InvalidateScrollbars and finally CheckOverflow.
  // returns true if the frame is still alive after the method call.
  bool FullScrollbarsUpdate(bool aNeedsFullInvalidation);

  // Use to auto-fill some of the common properties without the view having to do it.
  // Examples include container, open, selected, and focus.
  void PrefillPropertyArray(int32_t aRowIndex, nsTreeColumn* aCol);

  // Our internal scroll method, used by all the public scroll methods.
  nsresult ScrollInternal(const ScrollParts& aParts, int32_t aRow);
  nsresult ScrollToRowInternal(const ScrollParts& aParts, int32_t aRow);
  nsresult ScrollToColumnInternal(const ScrollParts& aParts, nsITreeColumn* aCol);
  nsresult ScrollHorzInternal(const ScrollParts& aParts, int32_t aPosition);
  nsresult EnsureRowIsVisibleInternal(const ScrollParts& aParts, int32_t aRow);

  // Convert client pixels into appunits in our coordinate space.
  nsPoint AdjustClientCoordsToBoxCoordSpace(int32_t aX, int32_t aY);

  // Cache the box object
  void EnsureBoxObject();

  void EnsureView();

  nsresult GetCellWidth(int32_t aRow, nsTreeColumn* aCol,
                        gfxContext* aRenderingContext,
                        nscoord& aDesiredSize, nscoord& aCurrentSize);
  nscoord CalcMaxRowWidth();

  // Translate the given rect horizontally from tree coordinates into the
  // coordinate system of our nsTreeBodyFrame.  If clip is true, then clip the
  // rect to its intersection with mInnerBox in the horizontal direction.
  // Return whether the result has a nonempty intersection with mInnerBox
  // after projecting both onto the horizontal coordinate axis.
  bool OffsetForHorzScroll(nsRect& rect, bool clip);

  bool CanAutoScroll(int32_t aRowIndex);

  // Calc the row and above/below/on status given where the mouse currently is hovering.
  // Also calc if we're in the region in which we want to auto-scroll the tree.
  // A positive value of |aScrollLines| means scroll down, a negative value
  // means scroll up, a zero value means that we aren't in drag scroll region.
  void ComputeDropPosition(mozilla::WidgetGUIEvent* aEvent,
                           int32_t* aRow,
                           int16_t* aOrient,
                           int16_t* aScrollLines);

  // Mark ourselves dirty if we're a select widget
  void MarkDirtyIfSelect();

  void InvalidateDropFeedback(int32_t aRow, int16_t aOrientation) {
    InvalidateRow(aRow);
    if (aOrientation != nsITreeView::DROP_ON)
      InvalidateRow(aRow + aOrientation);
  }

public:
  static
  already_AddRefed<nsTreeColumn> GetColumnImpl(nsITreeColumn* aUnknownCol) {
    if (!aUnknownCol)
      return nullptr;

    nsCOMPtr<nsTreeColumn> col = do_QueryInterface(aUnknownCol);
    return col.forget();
  }

  /**
   * Remove an nsITreeImageListener from being tracked by this frame. Only tree
   * image listeners that are created by this frame are tracked.
   *
   * @param aListener A pointer to an nsTreeImageListener to no longer
   *        track.
   */
  void RemoveTreeImageListener(nsTreeImageListener* aListener);

protected:

  // Create a new timer. This method is used to delay various actions like
  // opening/closing folders or tree scrolling.
  // aID is type of the action, aFunc is the function to be called when
  // the timer fires and aType is type of timer - one shot or repeating.
  nsresult CreateTimer(const mozilla::LookAndFeel::IntID aID,
                       nsTimerCallbackFunc aFunc, int32_t aType,
                       nsITimer** aTimer, const char* aName);

  static void OpenCallback(nsITimer *aTimer, void *aClosure);

  static void CloseCallback(nsITimer *aTimer, void *aClosure);

  static void LazyScrollCallback(nsITimer *aTimer, void *aClosure);

  static void ScrollCallback(nsITimer *aTimer, void *aClosure);

  class ScrollEvent : public mozilla::Runnable {
  public:
    NS_DECL_NSIRUNNABLE
    explicit ScrollEvent(nsTreeBodyFrame *aInner) : mInner(aInner) {}
    void Revoke() { mInner = nullptr; }
  private:
    nsTreeBodyFrame* mInner;
  };

  void PostScrollEvent();
  void FireScrollEvent();

  /**
   * Clear the pointer to this frame for all nsTreeImageListeners that were
   * created by this frame.
   */
  void DetachImageListeners();

#ifdef ACCESSIBILITY
  /**
   * Fires 'treeRowCountChanged' event asynchronously. The event supports
   * nsIDOMCustomEvent interface that is used to expose the following
   * information structures.
   *
   * @param aIndex  the row index rows are added/removed from
   * @param aCount  the number of added/removed rows (the sign points to
   *                an operation, plus - addition, minus - removing)
   */
  void FireRowCountChangedEvent(int32_t aIndex, int32_t aCount);

  /**
   * Fires 'treeInvalidated' event asynchronously. The event supports
   * nsIDOMCustomEvent interface that is used to expose the information
   * structures described by method arguments.
   *
   * @param aStartRow  the start index of invalidated rows, -1 means that
   *                   columns have been invalidated only
   * @param aEndRow    the end index of invalidated rows, -1 means that columns
   *                   have been invalidated only
   * @param aStartCol  the start invalidated column, nullptr means that only rows
   *                   have been invalidated
   * @param aEndCol    the end invalidated column, nullptr means that rows have
   *                   been invalidated only
   */
  void FireInvalidateEvent(int32_t aStartRow, int32_t aEndRow,
                           nsITreeColumn *aStartCol, nsITreeColumn *aEndCol);
#endif

protected: // Data Members

  class Slots {
    public:
      Slots() {
      }

      ~Slots() {
        if (mTimer)
          mTimer->Cancel();
      }

      friend class nsTreeBodyFrame;

    protected:
      // If the drop is actually allowed here or not.
      bool                     mDropAllowed;

      // True while dragging over the tree.
      bool mIsDragging;

      // The row the mouse is hovering over during a drop.
      int32_t                  mDropRow;

      // Where we want to draw feedback (above/on this row/below) if allowed.
      int16_t                  mDropOrient;

      // Number of lines to be scrolled.
      int16_t                  mScrollLines;

      // The drag action that was received for this slot
      uint32_t                 mDragAction;

      // Timer for opening/closing spring loaded folders or scrolling the tree.
      nsCOMPtr<nsITimer>       mTimer;

      // An array used to keep track of all spring loaded folders.
      nsTArray<int32_t>        mArray;
  };

  Slots* mSlots;

  nsRevocableEventPtr<ScrollEvent> mScrollEvent;

  RefPtr<ScrollbarActivity> mScrollbarActivity;

  // The cached box object parent.
  nsCOMPtr<nsITreeBoxObject> mTreeBoxObject;

  // Cached column information.
  RefPtr<nsTreeColumns> mColumns;

  // The current view for this tree widget.  We get all of our row and cell data
  // from the view.
  nsCOMPtr<nsITreeView> mView;

  // A cache of all the style contexts we have seen for rows and cells of the tree.  This is a mapping from
  // a list of atoms to a corresponding style context.  This cache stores every combination that
  // occurs in the tree, so for n distinct properties, this cache could have 2 to the n entries
  // (the power set of all row properties).
  nsTreeStyleCache mStyleCache;

  // A hashtable that maps from URLs to image request/listener pairs.  The URL
  // is provided by the view or by the style context. The style context
  // represents a resolved :-moz-tree-cell-image (or twisty) pseudo-element.
  // It maps directly to an imgIRequest.
  nsDataHashtable<nsStringHashKey, nsTreeImageCacheEntry> mImageCache;

  // A scratch array used when looking up cached style contexts.
  AtomArray mScratchArray;

  // The index of the first visible row and the # of rows visible onscreen.  
  // The tree only examines onscreen rows, starting from
  // this index and going up to index+pageLength.
  int32_t mTopRowIndex;
  int32_t mPageLength;

  // The horizontal scroll position
  nscoord mHorzPosition;

  // The original desired horizontal width before changing it and posting a
  // reflow callback. In some cases, the desired horizontal width can first be
  // different from the current desired horizontal width, only to return to
  // the same value later during the same reflow. In this case, we can cancel
  // the posted reflow callback and prevent an unnecessary reflow.
  nscoord mOriginalHorzWidth;
  // Our desired horizontal width (the width for which we actually have tree
  // columns).
  nscoord mHorzWidth;
  // The amount by which to adjust the width of the last cell.
  // This depends on whether or not the columnpicker and scrollbars are present.
  nscoord mAdjustWidth;

  // Cached heights and indent info.
  nsRect mInnerBox; // 4-byte aligned
  int32_t mRowHeight;
  int32_t mIndentation;
  nscoord mStringWidth;

  int32_t mUpdateBatchNest;

  // Cached row count.
  int32_t mRowCount;

  // The row the mouse is hovering over.
  int32_t mMouseOverRow;

  // Whether or not we're currently focused.
  bool mFocused;

  // Do we have a fixed number of onscreen rows?
  bool mHasFixedRowCount;

  bool mVerticalOverflow;
  bool mHorizontalOverflow;

  bool mReflowCallbackPosted;

  // Set while we flush layout to take account of effects of
  // overflow/underflow event handlers
  bool mCheckingOverflow;

  // Hash table to keep track of which listeners we created and thus
  // have pointers to us.
  nsTHashtable<nsPtrHashKey<nsTreeImageListener> > mCreatedListeners;

}; // class nsTreeBodyFrame

#endif
