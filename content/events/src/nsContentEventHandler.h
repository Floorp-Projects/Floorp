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
 * Mozilla Japan.
 * Portions created by the Initial Developer are Copyright (C) 2008
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Masayuki Nakano <masayuki@d-toybox.com>
 *   Ningjie Chen <chenn@email.uc.edu>
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

#ifndef nsContentEventHandler_h__
#define nsContentEventHandler_h__

#include "nscore.h"
#include "nsCOMPtr.h"

#include "nsISelection.h"
#include "nsIRange.h"
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
  nsCOMPtr<nsIRange> mFirstSelectedRange;
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
                                           nsIRange* aRange,
                                           PRUint32* aOffset);
protected:
  // Make the DOM range from the offset of FlatText and the text length.
  // If aExpandToClusterBoundaries is true, the start offset and the end one are
  // expanded to nearest cluster boundaries.
  nsresult SetRangeFromFlatTextOffset(nsIRange* aRange,
                                      PRUint32 aNativeOffset,
                                      PRUint32 aNativeLength,
                                      bool aExpandToClusterBoundaries);
  // Find the first textframe for the range, and get the start offset in
  // the frame.
  nsresult GetStartFrameAndOffset(nsIRange* aRange,
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
