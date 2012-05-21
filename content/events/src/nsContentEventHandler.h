/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsContentEventHandler_h__
#define nsContentEventHandler_h__

#include "nscore.h"
#include "nsCOMPtr.h"

#include "nsISelection.h"
#include "nsRange.h"
#include "nsIContent.h"
#include "nsIDOMTreeWalker.h"

class nsPresContext;
class nsIPresShell;
class nsQueryContentEvent;
class nsSelectionEvent;
class nsCaret;
struct nsRect;

/*
 * Query Content Event Handler
 *   nsContentEventHandler is a helper class for nsEventStateManager.
 *   The platforms request some content informations, e.g., the selected text,
 *   the offset of the selected text and the text for specified range.
 *   This class answers to NS_QUERY_* events from actual contents.
 */

class NS_STACK_CLASS nsContentEventHandler {
public:
  nsContentEventHandler(nsPresContext *aPresContext);

  // NS_QUERY_SELECTED_TEXT event handler
  nsresult OnQuerySelectedText(nsQueryContentEvent* aEvent);
  // NS_QUERY_TEXT_CONTENT event handler
  nsresult OnQueryTextContent(nsQueryContentEvent* aEvent);
  // NS_QUERY_CARET_RECT event handler
  nsresult OnQueryCaretRect(nsQueryContentEvent* aEvent);
  // NS_QUERY_TEXT_RECT event handler
  nsresult OnQueryTextRect(nsQueryContentEvent* aEvent);
  // NS_QUERY_EDITOR_RECT event handler
  nsresult OnQueryEditorRect(nsQueryContentEvent* aEvent);
  // NS_QUERY_CONTENT_STATE event handler
  nsresult OnQueryContentState(nsQueryContentEvent* aEvent);
  // NS_QUERY_SELECTION_AS_TRANSFERABLE event handler
  nsresult OnQuerySelectionAsTransferable(nsQueryContentEvent* aEvent);
  // NS_QUERY_CHARACTER_AT_POINT event handler
  nsresult OnQueryCharacterAtPoint(nsQueryContentEvent* aEvent);
  // NS_QUERY_DOM_WIDGET_HITTEST event handler
  nsresult OnQueryDOMWidgetHittest(nsQueryContentEvent* aEvent);

  // NS_SELECTION_* event
  nsresult OnSelectionEvent(nsSelectionEvent* aEvent);

protected:
  nsPresContext* mPresContext;
  nsCOMPtr<nsIPresShell> mPresShell;
  nsCOMPtr<nsISelection> mSelection;
  nsRefPtr<nsRange> mFirstSelectedRange;
  nsCOMPtr<nsIContent> mRootContent;

  nsresult Init(nsQueryContentEvent* aEvent);
  nsresult Init(nsSelectionEvent* aEvent);

  // InitCommon() is called from each Init().
  nsresult InitCommon();

public:
  // FlatText means the text that is generated from DOM tree. The BR elements
  // are replaced to native linefeeds. Other elements are ignored.

  // Get the offset in FlatText of the range. (also used by nsIMEStateManager)
  static nsresult GetFlatTextOffsetOfRange(nsIContent* aRootContent,
                                           nsINode* aNode,
                                           PRInt32 aNodeOffset,
                                           PRUint32* aOffset);
  static nsresult GetFlatTextOffsetOfRange(nsIContent* aRootContent,
                                           nsRange* aRange,
                                           PRUint32* aOffset);
protected:
  // Make the DOM range from the offset of FlatText and the text length.
  // If aExpandToClusterBoundaries is true, the start offset and the end one are
  // expanded to nearest cluster boundaries.
  nsresult SetRangeFromFlatTextOffset(nsRange* aRange,
                                      PRUint32 aNativeOffset,
                                      PRUint32 aNativeLength,
                                      bool aExpandToClusterBoundaries);
  // Find the first textframe for the range, and get the start offset in
  // the frame.
  nsresult GetStartFrameAndOffset(nsRange* aRange,
                                  nsIFrame** aFrame,
                                  PRInt32* aOffsetInFrame);
  // Convert the frame relative offset to the root view relative offset.
  nsresult ConvertToRootViewRelativeOffset(nsIFrame* aFrame,
                                           nsRect& aRect);
  // Expand aXPOffset to the nearest offset in cluster boundary. aForward is
  // true, it is expanded to forward.
  nsresult ExpandToClusterBoundary(nsIContent* aContent, bool aForward,
                                   PRUint32* aXPOffset);
};

#endif // nsContentEventHandler_h__
