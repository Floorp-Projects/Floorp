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
#include "nsRange.h"

class nsPresContext;

struct nsRect;

namespace mozilla {

enum LineBreakType
{
  LINE_BREAK_TYPE_NATIVE,
  LINE_BREAK_TYPE_XP
};

/*
 * Query Content Event Handler
 *   ContentEventHandler is a helper class for EventStateManager.
 *   The platforms request some content informations, e.g., the selected text,
 *   the offset of the selected text and the text for specified range.
 *   This class answers to NS_QUERY_* events from actual contents.
 */

class MOZ_STACK_CLASS ContentEventHandler
{
public:
  typedef dom::Selection Selection;

  explicit ContentEventHandler(nsPresContext* aPresContext);

  // Handle aEvent in the current process.
  nsresult HandleQueryContentEvent(WidgetQueryContentEvent* aEvent);

  // eQuerySelectedText event handler
  nsresult OnQuerySelectedText(WidgetQueryContentEvent* aEvent);
  // eQueryTextContent event handler
  nsresult OnQueryTextContent(WidgetQueryContentEvent* aEvent);
  // eQueryCaretRect event handler
  nsresult OnQueryCaretRect(WidgetQueryContentEvent* aEvent);
  // eQueryTextRect event handler
  nsresult OnQueryTextRect(WidgetQueryContentEvent* aEvent);
  // eQueryEditorRect event handler
  nsresult OnQueryEditorRect(WidgetQueryContentEvent* aEvent);
  // eQueryContentState event handler
  nsresult OnQueryContentState(WidgetQueryContentEvent* aEvent);
  // eQuerySelectionAsTransferable event handler
  nsresult OnQuerySelectionAsTransferable(WidgetQueryContentEvent* aEvent);
  // eQueryCharacterAtPoint event handler
  nsresult OnQueryCharacterAtPoint(WidgetQueryContentEvent* aEvent);
  // eQueryDOMWidgetHittest event handler
  nsresult OnQueryDOMWidgetHittest(WidgetQueryContentEvent* aEvent);

  // NS_SELECTION_* event
  nsresult OnSelectionEvent(WidgetSelectionEvent* aEvent);

protected:
  nsPresContext* mPresContext;
  nsCOMPtr<nsIPresShell> mPresShell;
  RefPtr<Selection> mSelection;
  RefPtr<nsRange> mFirstSelectedRange;
  nsCOMPtr<nsIContent> mRootContent;

  nsresult Init(WidgetQueryContentEvent* aEvent);
  nsresult Init(WidgetSelectionEvent* aEvent);

  nsresult InitBasic();
  nsresult InitCommon();

public:
  // FlatText means the text that is generated from DOM tree. The BR elements
  // are replaced to native linefeeds. Other elements are ignored.

  // NodePosition stores a pair of node and offset in the node.
  // This is useful to receive one or more sets of them instead of nsRange.
  struct NodePosition final
  {
    nsCOMPtr<nsINode> mNode;
    int32_t mOffset;

    NodePosition()
      : mOffset(-1)
    {
    }

    NodePosition(nsINode* aNode, int32_t aOffset)
      : mNode(aNode)
      , mOffset(aOffset)
    {
    }

    explicit NodePosition(const nsIFrame::ContentOffsets& aContentOffsets)
      : mNode(aContentOffsets.content)
      , mOffset(aContentOffsets.offset)
    {
    }

    bool operator==(const NodePosition& aOther) const
    {
      return mNode == aOther.mNode && mOffset == aOther.mOffset;
    }

    bool IsValid() const
    {
      return mNode && mOffset >= 0;
    }
    bool OffsetIsValid() const
    {
      return IsValid() && static_cast<uint32_t>(mOffset) <= mNode->Length();
    }
    nsresult SetToRangeStart(nsRange* aRange) const
    {
      nsCOMPtr<nsIDOMNode> domNode(do_QueryInterface(mNode));
      return aRange->SetStart(domNode, mOffset);
    }
    nsresult SetToRangeEnd(nsRange* aRange) const
    {
      nsCOMPtr<nsIDOMNode> domNode(do_QueryInterface(mNode));
      return aRange->SetEnd(domNode, mOffset);
    }
    nsresult SetToRangeEndAfter(nsRange* aRange) const
    {
      nsCOMPtr<nsIDOMNode> domNode(do_QueryInterface(mNode));
      return aRange->SetEndAfter(domNode);
    }
  };

  // Get the flatten text length in the range.
  // @param aIsRemovingNode     Should be true only when this is called from
  //                            nsIMutationObserver::ContentRemoved().
  static nsresult GetFlatTextLengthInRange(const NodePosition& aStartPosition,
                                           const NodePosition& aEndPosition,
                                           nsIContent* aRootContent,
                                           uint32_t* aLength,
                                           LineBreakType aLineBreakType,
                                           bool aIsRemovingNode = false);
  // Computes the native text length between aStartOffset and aEndOffset of
  // aContent.  aContent must be a text node.
  static uint32_t GetNativeTextLength(nsIContent* aContent,
                                      uint32_t aStartOffset,
                                      uint32_t aEndOffset);
  // Get the native text length of aContent.  aContent must be a text node.
  static uint32_t GetNativeTextLength(nsIContent* aContent,
                                      uint32_t aMaxLength = UINT32_MAX);
  // Get the native text length which is inserted before aContent.
  // aContent should be a <br> element for now.
  static uint32_t GetNativeTextLengthBefore(nsIContent* aContent);

protected:
  // Get the text length of aContent.  aContent must be a text node.
  static uint32_t GetTextLength(nsIContent* aContent,
                                LineBreakType aLineBreakType,
                                uint32_t aMaxLength = UINT32_MAX);
  // Get the text length of a given range of a content node in
  // the given line break type.
  static uint32_t GetTextLengthInRange(nsIContent* aContent,
                                       uint32_t aXPStartOffset,
                                       uint32_t aXPEndOffset,
                                       LineBreakType aLineBreakType);
  // Get the contents of aRange as plain text.
  nsresult GenerateFlatTextContent(nsRange* aRange,
                                   nsAFlatString& aString,
                                   LineBreakType aLineBreakType);
  // Get the text length before the start position of aRange.
  nsresult GetFlatTextLengthBefore(nsRange* aRange,
                                   uint32_t* aOffset,
                                   LineBreakType aLineBreakType);
  // Get the line breaker length.
  static inline uint32_t GetBRLength(LineBreakType aLineBreakType);
  static LineBreakType GetLineBreakType(WidgetQueryContentEvent* aEvent);
  static LineBreakType GetLineBreakType(WidgetSelectionEvent* aEvent);
  static LineBreakType GetLineBreakType(bool aUseNativeLineBreak);
  // Returns focused content (including its descendant documents).
  nsIContent* GetFocusedContent();
  // Returns true if the content is a plugin host.
  bool IsPlugin(nsIContent* aContent);
  // QueryContentRect() sets the rect of aContent's frame(s) to aEvent.
  nsresult QueryContentRect(nsIContent* aContent,
                            WidgetQueryContentEvent* aEvent);
  // Make the DOM range from the offset of FlatText and the text length.
  // If aExpandToClusterBoundaries is true, the start offset and the end one are
  // expanded to nearest cluster boundaries.
  nsresult SetRangeFromFlatTextOffset(nsRange* aRange,
                                      uint32_t aOffset,
                                      uint32_t aLength,
                                      LineBreakType aLineBreakType,
                                      bool aExpandToClusterBoundaries,
                                      uint32_t* aNewOffset = nullptr);
  // If the aRange isn't in text node but next to a text node, this method
  // modifies it in the text node.  Otherwise, not modified.
  nsresult AdjustCollapsedRangeMaybeIntoTextNode(nsRange* aCollapsedRange);
  // Find the first frame for the range and get the start offset in it.
  nsresult GetStartFrameAndOffset(const nsRange* aRange,
                                  nsIFrame*& aFrame,
                                  int32_t& aOffsetInFrame);
  // Convert the frame relative offset to the root frame of the root presContext
  // relative offset.
  nsresult ConvertToRootRelativeOffset(nsIFrame* aFrame,
                                       nsRect& aRect);
  // Expand aXPOffset to the nearest offset in cluster boundary. aForward is
  // true, it is expanded to forward.
  nsresult ExpandToClusterBoundary(nsIContent* aContent, bool aForward,
                                   uint32_t* aXPOffset);

  typedef nsTArray<mozilla::FontRange> FontRangeArray;
  static void AppendFontRanges(FontRangeArray& aFontRanges,
                               nsIContent* aContent,
                               int32_t aBaseOffset,
                               int32_t aXPStartOffset,
                               int32_t aXPEndOffset,
                               LineBreakType aLineBreakType);
  static nsresult GenerateFlatFontRanges(nsRange* aRange,
                                         FontRangeArray& aFontRanges,
                                         uint32_t& aLength,
                                         LineBreakType aLineBreakType);
};

} // namespace mozilla

#endif // mozilla_ContentEventHandler_h_
