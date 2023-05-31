/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_ContentEventHandler_h_
#define mozilla_ContentEventHandler_h_

#include "mozilla/EventForwards.h"
#include "mozilla/dom/Selection.h"
#include "nsCOMPtr.h"
#include "nsIFrame.h"
#include "nsINode.h"

class nsPresContext;
class nsRange;

struct nsRect;

namespace mozilla {

namespace dom {
class Element;
class Text;
}  // namespace dom

enum LineBreakType { LINE_BREAK_TYPE_NATIVE, LINE_BREAK_TYPE_XP };

/*
 * Query Content Event Handler
 *   ContentEventHandler is a helper class for EventStateManager.
 *   The platforms request some content informations, e.g., the selected text,
 *   the offset of the selected text and the text for specified range.
 *   This class answers to NS_QUERY_* events from actual contents.
 */

class MOZ_STACK_CLASS ContentEventHandler {
 private:
  /**
   * RawRange is a helper class of ContentEventHandler class.  The caller is
   * responsible for making sure the start/end nodes are in document order.
   * This is enforced by assertions in DEBUG builds.
   */
  class MOZ_STACK_CLASS RawRange final {
   public:
    RawRange() = default;

    void Clear() {
      mRoot = nullptr;
      mStart = RangeBoundary();
      mEnd = RangeBoundary();
    }

    bool IsPositioned() const { return mStart.IsSet() && mEnd.IsSet(); }
    bool Collapsed() const { return mStart == mEnd && IsPositioned(); }
    nsINode* GetStartContainer() const { return mStart.Container(); }
    nsINode* GetEndContainer() const { return mEnd.Container(); }
    uint32_t StartOffset() const {
      return *mStart.Offset(
          RangeBoundary::OffsetFilter::kValidOrInvalidOffsets);
    }
    uint32_t EndOffset() const {
      return *mEnd.Offset(RangeBoundary::OffsetFilter::kValidOrInvalidOffsets);
    }
    nsIContent* StartRef() const { return mStart.Ref(); }
    nsIContent* EndRef() const { return mEnd.Ref(); }

    const RangeBoundary& Start() const { return mStart; }
    const RangeBoundary& End() const { return mEnd; }

    // XXX: Make these use RangeBoundaries...
    nsresult CollapseTo(const RawRangeBoundary& aBoundary) {
      return SetStartAndEnd(aBoundary, aBoundary);
    }
    nsresult SetStart(const RawRangeBoundary& aStart);
    nsresult SetEnd(const RawRangeBoundary& aEnd);

    // NOTE: These helpers can hide performance problems, as they perform a
    // search to find aStartOffset in aStartContainer.
    nsresult SetStart(nsINode* aStartContainer, uint32_t aStartOffset) {
      return SetStart(RawRangeBoundary(aStartContainer, aStartOffset));
    }
    nsresult SetEnd(nsINode* aEndContainer, uint32_t aEndOffset) {
      return SetEnd(RawRangeBoundary(aEndContainer, aEndOffset));
    }

    nsresult SetEndAfter(nsINode* aEndContainer);
    void SetStartAndEnd(const nsRange* aRange);
    nsresult SetStartAndEnd(const RawRangeBoundary& aStart,
                            const RawRangeBoundary& aEnd);

    nsresult SelectNodeContents(const nsINode* aNodeToSelectContents);

   private:
    inline void AssertStartIsBeforeOrEqualToEnd();

    nsCOMPtr<nsINode> mRoot;

    RangeBoundary mStart;
    RangeBoundary mEnd;
  };

 public:
  using Element = dom::Element;
  using Selection = dom::Selection;

  explicit ContentEventHandler(nsPresContext* aPresContext);

  // Handle aEvent in the current process.
  MOZ_CAN_RUN_SCRIPT nsresult
  HandleQueryContentEvent(WidgetQueryContentEvent* aEvent);

  // eQuerySelectedText event handler
  MOZ_CAN_RUN_SCRIPT nsresult
  OnQuerySelectedText(WidgetQueryContentEvent* aEvent);
  // eQueryTextContent event handler
  MOZ_CAN_RUN_SCRIPT nsresult
  OnQueryTextContent(WidgetQueryContentEvent* aEvent);
  // eQueryCaretRect event handler
  MOZ_CAN_RUN_SCRIPT nsresult OnQueryCaretRect(WidgetQueryContentEvent* aEvent);
  // eQueryTextRect event handler
  MOZ_CAN_RUN_SCRIPT nsresult OnQueryTextRect(WidgetQueryContentEvent* aEvent);
  // eQueryTextRectArray event handler
  MOZ_CAN_RUN_SCRIPT nsresult
  OnQueryTextRectArray(WidgetQueryContentEvent* aEvent);
  // eQueryEditorRect event handler
  MOZ_CAN_RUN_SCRIPT nsresult
  OnQueryEditorRect(WidgetQueryContentEvent* aEvent);
  // eQueryContentState event handler
  MOZ_CAN_RUN_SCRIPT nsresult
  OnQueryContentState(WidgetQueryContentEvent* aEvent);
  // eQuerySelectionAsTransferable event handler
  MOZ_CAN_RUN_SCRIPT nsresult
  OnQuerySelectionAsTransferable(WidgetQueryContentEvent* aEvent);
  // eQueryCharacterAtPoint event handler
  MOZ_CAN_RUN_SCRIPT nsresult
  OnQueryCharacterAtPoint(WidgetQueryContentEvent* aEvent);
  // eQueryDOMWidgetHittest event handler
  MOZ_CAN_RUN_SCRIPT nsresult
  OnQueryDOMWidgetHittest(WidgetQueryContentEvent* aEvent);

  // NS_SELECTION_* event
  MOZ_CAN_RUN_SCRIPT nsresult OnSelectionEvent(WidgetSelectionEvent* aEvent);

 protected:
  RefPtr<dom::Document> mDocument;
  // mSelection is typically normal selection but if OnQuerySelectedText()
  // is called, i.e., handling eQuerySelectedText, it's the specified selection
  // by WidgetQueryContentEvent::mInput::mSelectionType.
  RefPtr<Selection> mSelection;
  // mFirstSelectedRawRange is initialized from the first range of mSelection,
  // if it exists.  Otherwise, it is reset by Clear().
  RawRange mFirstSelectedRawRange;
  RefPtr<Element> mRootElement;

  MOZ_CAN_RUN_SCRIPT nsresult Init(WidgetQueryContentEvent* aEvent);
  MOZ_CAN_RUN_SCRIPT nsresult Init(WidgetSelectionEvent* aEvent);

  nsresult InitBasic(bool aRequireFlush = true);
  MOZ_CAN_RUN_SCRIPT nsresult
  InitCommon(EventMessage aEventMessage,
             SelectionType aSelectionType = SelectionType::eNormal,
             bool aRequireFlush = true);
  /**
   * InitRootContent() computes the root content of current focused editor.
   *
   * @param aNormalSelection    This must be a Selection instance whose type is
   *                            SelectionType::eNormal.
   */
  MOZ_CAN_RUN_SCRIPT nsresult
  InitRootContent(const Selection& aNormalSelection);

 public:
  // FlatText means the text that is generated from DOM tree. The BR elements
  // are replaced to native linefeeds. Other elements are ignored.

  // NodePosition stores a pair of node and offset in the node.
  // When mNode is an element and mOffset is 0, the start position means after
  // the open tag of mNode.
  // This is useful to receive one or more sets of them instead of nsRange.
  // This type is intended to be used for short-lived operations, and is thus
  // marked MOZ_STACK_CLASS.
  struct MOZ_STACK_CLASS NodePosition : public RangeBoundary {
    // Only when mNode is an element node and mOffset is 0, mAfterOpenTag is
    // referred.
    bool mAfterOpenTag = true;

    NodePosition() = default;

    NodePosition(nsINode* aContainer, uint32_t aOffset)
        : RangeBoundary(aContainer, aOffset) {}

    NodePosition(nsINode* aContainer, nsIContent* aRef)
        : RangeBoundary(aContainer, aRef) {}

    explicit NodePosition(const nsIFrame::ContentOffsets& aContentOffsets)
        : RangeBoundary(aContentOffsets.content, aContentOffsets.offset) {}

   public:
    bool operator==(const NodePosition& aOther) const {
      return RangeBoundary::operator==(aOther) &&
             mAfterOpenTag == aOther.mAfterOpenTag;
    }

    bool IsBeforeOpenTag() const {
      return IsSet() && Container()->IsElement() && !Ref() && !mAfterOpenTag;
    }
    bool IsImmediatelyAfterOpenTag() const {
      return IsSet() && Container()->IsElement() && !Ref() && mAfterOpenTag;
    }
  };

  // NodePositionBefore isn't good name if Container() isn't an element node nor
  // Offset() is not 0, though, when Container() is an element node and mOffset
  // is 0, this is treated as before the open tag of Container().
  struct NodePositionBefore final : public NodePosition {
    NodePositionBefore(nsINode* aContainer, uint32_t aOffset)
        : NodePosition(aContainer, aOffset) {
      mAfterOpenTag = false;
    }

    NodePositionBefore(nsINode* aContainer, nsIContent* aRef)
        : NodePosition(aContainer, aRef) {
      mAfterOpenTag = false;
    }
  };

  // Get the flatten text length in the range.
  // @param aStartPosition      Start node and offset in the node of the range.
  // @param aEndPosition        End node and offset in the node of the range.
  // @param aRootElement        The root element of the editor or document.
  //                            aRootElement won't cause any text including
  //                            line breaks.
  // @param aLength             The result of the flatten text length of the
  //                            range.
  // @param aLineBreakType      Whether this computes flatten text length with
  //                            native line breakers on the platform or
  //                            with XP line breaker (\n).
  // @param aIsRemovingNode     Should be true only when this is called from
  //                            nsIMutationObserver::ContentRemoved().
  //                            When this is true, aStartPosition.mNode should
  //                            be the root node of removing nodes and mOffset
  //                            should be 0 and aEndPosition.mNode should be
  //                            same as aStartPosition.mNode and mOffset should
  //                            be number of the children of mNode.
  static nsresult GetFlatTextLengthInRange(const NodePosition& aStartPosition,
                                           const NodePosition& aEndPosition,
                                           const Element* aRootElement,
                                           uint32_t* aLength,
                                           LineBreakType aLineBreakType,
                                           bool aIsRemovingNode = false);
  // Computes the native text length between aStartOffset and aEndOffset of
  // aTextNode.
  static uint32_t GetNativeTextLength(const dom::Text& aTextNode,
                                      uint32_t aStartOffset,
                                      uint32_t aEndOffset);
  // Get the native text length of aTextNode.
  static uint32_t GetNativeTextLength(const dom::Text& aTextNode,
                                      uint32_t aMaxLength = UINT32_MAX);

  static uint32_t GetNativeTextLength(const nsAString& aText);

 protected:
  // Get the text length of aTextNode.
  static uint32_t GetTextLength(const dom::Text& aTextNode,
                                LineBreakType aLineBreakType,
                                uint32_t aMaxLength = UINT32_MAX);
  // Get the text length of a given range of a content node in
  // the given line break type.
  static uint32_t GetTextLengthInRange(const dom::Text& aTextNode,
                                       uint32_t aXPStartOffset,
                                       uint32_t aXPEndOffset,
                                       LineBreakType aLineBreakType);
  // Get the contents in aElement (meaning all children of aElement) as plain
  // text.  E.g., specifying mRootElement gets whole text in it.
  // Note that the result is not same as .textContent.  The result is
  // optimized for native IMEs.  For example, <br> element and some block
  // elements causes "\n" (or "\r\n"), see also ShouldBreakLineBefore().
  nsresult GenerateFlatTextContent(const Element* aElement, nsString& aString,
                                   LineBreakType aLineBreakType);
  // Get the contents of aRange as plain text.
  nsresult GenerateFlatTextContent(const RawRange& aRawRange, nsString& aString,
                                   LineBreakType aLineBreakType);
  // Get offset of start of aRange.  Note that the result includes the length
  // of line breaker caused by the start of aContent because aRange never
  // includes the line breaker caused by its start node.
  nsresult GetStartOffset(const RawRange& aRawRange, uint32_t* aOffset,
                          LineBreakType aLineBreakType);
  // Check if we should insert a line break before aContent.
  // This should return false only when aContent is an html element which
  // is typically used in a paragraph like <em>.
  static bool ShouldBreakLineBefore(const nsIContent& aContent,
                                    const Element* aRootElement);
  // Get the line breaker length.
  static inline uint32_t GetBRLength(LineBreakType aLineBreakType);
  static LineBreakType GetLineBreakType(WidgetQueryContentEvent* aEvent);
  static LineBreakType GetLineBreakType(WidgetSelectionEvent* aEvent);
  static LineBreakType GetLineBreakType(bool aUseNativeLineBreak);
  // Returns focused content (including its descendant documents).
  nsIContent* GetFocusedContent();
  // QueryContentRect() sets the rect of aContent's frame(s) to aEvent.
  nsresult QueryContentRect(nsIContent* aContent,
                            WidgetQueryContentEvent* aEvent);
  // Initialize aRawRange from the offset of FlatText and the text length.
  // If aExpandToClusterBoundaries is true, the start offset and the end one are
  // expanded to nearest cluster boundaries.
  nsresult SetRawRangeFromFlatTextOffset(RawRange* aRawRange, uint32_t aOffset,
                                         uint32_t aLength,
                                         LineBreakType aLineBreakType,
                                         bool aExpandToClusterBoundaries,
                                         uint32_t* aNewOffset = nullptr,
                                         dom::Text** aLastTextNode = nullptr);
  // If the aCollapsedRawRange isn't in text node but next to a text node,
  // this method modifies it in the text node.  Otherwise, not modified.
  nsresult AdjustCollapsedRangeMaybeIntoTextNode(RawRange& aCollapsedRawRange);
  // Convert the frame relative offset to be relative to the root frame of the
  // root presContext (but still measured in appUnits of aFrame's presContext).
  nsresult ConvertToRootRelativeOffset(nsIFrame* aFrame, nsRect& aRect);
  // Expand aXPOffset to the nearest offset in cluster boundary. aForward is
  // true, it is expanded to forward.
  // FYI: Due to `nsFrameSelection::GetFrameForNodeOffset()`, this cannot
  //      take `const dom::Text&`.
  nsresult ExpandToClusterBoundary(dom::Text& aTextNode, bool aForward,
                                   uint32_t* aXPOffset) const;

  using FontRangeArray = nsTArray<mozilla::FontRange>;
  static void AppendFontRanges(FontRangeArray& aFontRanges,
                               const dom::Text& aTextNode, uint32_t aBaseOffset,
                               uint32_t aXPStartOffset, uint32_t aXPEndOffset,
                               LineBreakType aLineBreakType);
  nsresult GenerateFlatFontRanges(const RawRange& aRawRange,
                                  FontRangeArray& aFontRanges,
                                  uint32_t& aLength,
                                  LineBreakType aLineBreakType);
  nsresult QueryTextRectByRange(const RawRange& aRawRange,
                                LayoutDeviceIntRect& aRect,
                                WritingMode& aWritingMode);

  struct MOZ_STACK_CLASS FrameAndNodeOffset final {
    // mFrame is safe since this can live in only stack class and
    // ContentEventHandler doesn't modify layout after
    // ContentEventHandler::Init() flushes pending layout.  In other words,
    // this struct shouldn't be used before calling
    // ContentEventHandler::Init().
    nsIFrame* mFrame;
    // offset in the node of mFrame
    int32_t mOffsetInNode;

    FrameAndNodeOffset() : mFrame(nullptr), mOffsetInNode(-1) {}

    FrameAndNodeOffset(nsIFrame* aFrame, int32_t aStartOffsetInNode)
        : mFrame(aFrame), mOffsetInNode(aStartOffsetInNode) {}

    nsIFrame* operator->() { return mFrame; }
    const nsIFrame* operator->() const { return mFrame; }
    operator nsIFrame*() { return mFrame; }
    operator const nsIFrame*() const { return mFrame; }
    bool IsValid() const { return mFrame && mOffsetInNode >= 0; }
  };
  // Get first frame after the start of the given range for computing text rect.
  // This returns invalid FrameAndNodeOffset if there is no content which
  // should affect to computing text rect in the range.  mOffsetInNode is start
  // offset in the frame.
  FrameAndNodeOffset GetFirstFrameInRangeForTextRect(const RawRange& aRawRange);

  // Get last frame before the end of the given range for computing text rect.
  // This returns invalid FrameAndNodeOffset if there is no content which
  // should affect to computing text rect in the range.  mOffsetInNode is end
  // offset in the frame.
  FrameAndNodeOffset GetLastFrameInRangeForTextRect(const RawRange& aRawRange);

  struct MOZ_STACK_CLASS FrameRelativeRect final {
    // mRect is relative to the mBaseFrame's position.
    nsRect mRect;
    nsIFrame* mBaseFrame;

    FrameRelativeRect() : mBaseFrame(nullptr) {}

    explicit FrameRelativeRect(nsIFrame* aBaseFrame) : mBaseFrame(aBaseFrame) {}

    FrameRelativeRect(const nsRect& aRect, nsIFrame* aBaseFrame)
        : mRect(aRect), mBaseFrame(aBaseFrame) {}

    bool IsValid() const { return mBaseFrame != nullptr; }

    // Returns an nsRect relative to aBaseFrame instead of mBaseFrame.
    nsRect RectRelativeTo(nsIFrame* aBaseFrame) const;
  };

  // Returns a rect for line breaker before the node of aFrame (If aFrame is
  // a <br> frame or a block level frame, it causes a line break at its
  // element's open tag, see also ShouldBreakLineBefore()).  Note that this
  // doesn't check if aFrame should cause line break in non-debug build.
  FrameRelativeRect GetLineBreakerRectBefore(nsIFrame* aFrame);

  // Returns a line breaker rect after aTextNode as there is a line breaker
  // immediately after aTextNode.  This is useful when following block
  // element causes a line break before it and it needs to compute the line
  // breaker's rect.  For example, if there is |<p>abc</p><p>def</p>|, the
  // rect of 2nd <p>'s line breaker should be at right of "c" in the first
  // <p>, not the start of 2nd <p>.  The result is relative to the last text
  // frame which represents the last character of aTextNode.
  FrameRelativeRect GuessLineBreakerRectAfter(const dom::Text& aTextNode);

  // Returns a guessed first rect.  I.e., it may be different from actual
  // caret when selection is collapsed at start of aFrame.  For example, this
  // guess the caret rect only with the content box of aFrame and its font
  // height like:
  // +-aFrame----------------- (border box)
  // |
  // |  +--------------------- (content box)
  // |  | I
  //      ^ guessed caret rect
  // However, actual caret is computed with more information like line-height,
  // child frames of aFrame etc.  But this does not emulate actual caret
  // behavior exactly for simpler and faster code because it's difficult and
  // we're not sure it's worthwhile to do it with complicated implementation.
  FrameRelativeRect GuessFirstCaretRectIn(nsIFrame* aFrame);

  // Make aRect non-empty.  If width and/or height is 0, these methods set them
  // to 1.  Note that it doesn't set nsRect's width nor height to one device
  // pixel because using nsRect::ToOutsidePixels() makes actual width or height
  // to 2 pixels because x and y may not be aligned to device pixels.
  void EnsureNonEmptyRect(nsRect& aRect) const;
  void EnsureNonEmptyRect(LayoutDeviceIntRect& aRect) const;

  /**
   * Compute caret rect before or after a character rect.
   */
  static LayoutDeviceIntRect GetCaretRectBefore(
      const LayoutDeviceIntRect& aCharRect, const WritingMode& aWritingMode);
  static LayoutDeviceIntRect GetCaretRectAfter(
      const LayoutDeviceIntRect& aCharRect, const WritingMode& aWritingMode);
  static nsRect GetCaretRectBefore(const nsRect& aCharRect,
                                   const WritingMode& aWritingMode);
  static nsRect GetCaretRectAfter(nsPresContext& aPresContext,
                                  const nsRect& aCharRect,
                                  const WritingMode& aWritingMode);
};

}  // namespace mozilla

#endif  // mozilla_ContentEventHandler_h_
