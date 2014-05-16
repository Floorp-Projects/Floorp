/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_ContentEventHandler_h_
#define mozilla_ContentEventHandler_h_

#include "mozilla/EventForwards.h"
#include "nsCOMPtr.h"
#include "nsISelection.h"
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
  ContentEventHandler(nsPresContext* aPresContext);

  // NS_QUERY_SELECTED_TEXT event handler
  nsresult OnQuerySelectedText(WidgetQueryContentEvent* aEvent);
  // NS_QUERY_TEXT_CONTENT event handler
  nsresult OnQueryTextContent(WidgetQueryContentEvent* aEvent);
  // NS_QUERY_CARET_RECT event handler
  nsresult OnQueryCaretRect(WidgetQueryContentEvent* aEvent);
  // NS_QUERY_TEXT_RECT event handler
  nsresult OnQueryTextRect(WidgetQueryContentEvent* aEvent);
  // NS_QUERY_EDITOR_RECT event handler
  nsresult OnQueryEditorRect(WidgetQueryContentEvent* aEvent);
  // NS_QUERY_CONTENT_STATE event handler
  nsresult OnQueryContentState(WidgetQueryContentEvent* aEvent);
  // NS_QUERY_SELECTION_AS_TRANSFERABLE event handler
  nsresult OnQuerySelectionAsTransferable(WidgetQueryContentEvent* aEvent);
  // NS_QUERY_CHARACTER_AT_POINT event handler
  nsresult OnQueryCharacterAtPoint(WidgetQueryContentEvent* aEvent);
  // NS_QUERY_DOM_WIDGET_HITTEST event handler
  nsresult OnQueryDOMWidgetHittest(WidgetQueryContentEvent* aEvent);

  // NS_SELECTION_* event
  nsresult OnSelectionEvent(WidgetSelectionEvent* aEvent);

protected:
  nsPresContext* mPresContext;
  nsCOMPtr<nsIPresShell> mPresShell;
  nsCOMPtr<nsISelection> mSelection;
  nsRefPtr<nsRange> mFirstSelectedRange;
  nsCOMPtr<nsIContent> mRootContent;

  nsresult Init(WidgetQueryContentEvent* aEvent);
  nsresult Init(WidgetSelectionEvent* aEvent);

  nsresult InitBasic();
  nsresult InitCommon();

public:
  // FlatText means the text that is generated from DOM tree. The BR elements
  // are replaced to native linefeeds. Other elements are ignored.

  // Get the offset in FlatText of the range. (also used by IMEContentObserver)
  static nsresult GetFlatTextOffsetOfRange(nsIContent* aRootContent,
                                           nsINode* aNode,
                                           int32_t aNodeOffset,
                                           uint32_t* aOffset,
                                           LineBreakType aLineBreakType);
  static nsresult GetFlatTextOffsetOfRange(nsIContent* aRootContent,
                                           nsRange* aRange,
                                           uint32_t* aOffset,
                                           LineBreakType aLineBreakType);
  // Get the native text length of a content node excluding any children
  static uint32_t GetNativeTextLength(nsIContent* aContent,
                                      uint32_t aMaxLength = UINT32_MAX);
protected:
  static uint32_t GetTextLength(nsIContent* aContent,
                                LineBreakType aLineBreakType,
                                uint32_t aMaxLength = UINT32_MAX);
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
  // Find the first textframe for the range, and get the start offset in
  // the frame.
  nsresult GetStartFrameAndOffset(nsRange* aRange,
                                  nsIFrame** aFrame,
                                  int32_t* aOffsetInFrame);
  // Convert the frame relative offset to the root view relative offset.
  nsresult ConvertToRootViewRelativeOffset(nsIFrame* aFrame,
                                           nsRect& aRect);
  // Expand aXPOffset to the nearest offset in cluster boundary. aForward is
  // true, it is expanded to forward.
  nsresult ExpandToClusterBoundary(nsIContent* aContent, bool aForward,
                                   uint32_t* aXPOffset);
};

} // namespace mozilla

#endif // mozilla_ContentEventHandler_h_
