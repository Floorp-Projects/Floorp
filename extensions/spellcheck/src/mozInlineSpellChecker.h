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
 * The Original Code is Inline Spellchecker
 *
 * The Initial Developer of the Original Code is Mozdev Group, Inc.
 * Portions created by the Initial Developer are Copyright (C) 2004
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s): Neil Deakin (neil@mozdevgroup.com)
 *                 Scott MacGregor (mscott@mozilla.org)
 *                 Brett Wilson <brettw@gmail.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#ifndef __mozinlinespellchecker_h__
#define __mozinlinespellchecker_h__

#include "nsIEditorSpellCheck.h"
#include "nsIEditActionListener.h"
#include "nsIInlineSpellChecker.h"
#include "nsITextServicesDocument.h"
#include "nsIDOMTreeWalker.h"
#include "nsWeakReference.h"
#include "nsIEditor.h"
#include "nsIDOMMouseListener.h"
#include "nsIDOMKeyListener.h"
#include "nsWeakReference.h"
#include "mozISpellI18NUtil.h"

class nsIDOMMouseEventListener;
class mozInlineSpellWordUtil;

class mozInlineSpellChecker : public nsIInlineSpellChecker, nsIEditActionListener, nsIDOMMouseListener, nsIDOMKeyListener,
                                     nsSupportsWeakReference
{
private:

  // Access with CanEnableInlineSpellChecking
  enum SpellCheckingState { SpellCheck_Uninitialized = -1,
                            SpellCheck_NotAvailable = 0,
                            SpellCheck_Available = 1};
  static SpellCheckingState gCanEnableSpellChecking;

  nsWeakPtr mEditor; 
  nsCOMPtr<nsIEditorSpellCheck> mSpellCheck;
  nsCOMPtr<nsITextServicesDocument> mTextServicesDocument;
  nsCOMPtr<nsIDOMTreeWalker> mTreeWalker;
  nsCOMPtr<mozISpellI18NUtil> mConverter;

  PRInt32 mNumWordsInSpellSelection;
  PRInt32 mMaxNumWordsInSpellSelection;

  // we need to keep track of the current text position in the document
  // so we can spell check the old word when the user clicks around the document.
  nsCOMPtr<nsIDOMNode> mCurrentSelectionAnchorNode;
  PRInt32              mCurrentSelectionOffset;

  // TODO: these should be defined somewhere so that they don't have to be copied
  // from editor!
  enum OperationID
  {
    kOpIgnore = -1,
    kOpNone = 0,
    kOpUndo,
    kOpRedo,
    kOpInsertNode,
    kOpCreateNode,
    kOpDeleteNode,
    kOpSplitNode,
    kOpJoinNode,
    kOpDeleteSelection,

    kOpInsertBreak    = 1000,
    kOpInsertText     = 1001,
    kOpInsertIMEText  = 1002,
    kOpDeleteText     = 1003,

    kOpMakeList            = 3001,
    kOpIndent              = 3002,
    kOpOutdent             = 3003,
    kOpAlign               = 3004,
    kOpMakeBasicBlock      = 3005,
    kOpRemoveList          = 3006,
    kOpMakeDefListItem     = 3007,
    kOpInsertElement       = 3008,
    kOpInsertQuotation     = 3009,
    kOpSetTextProperty     = 3010,
    kOpRemoveTextProperty  = 3011,
    kOpHTMLPaste           = 3012,
    kOpLoadHTML            = 3013,
    kOpResetTextProperties = 3014,
    kOpSetAbsolutePosition = 3015,
    kOpRemoveAbsolutePosition = 3016,
    kOpDecreaseZIndex      = 3017,
    kOpIncreaseZIndex      = 3018
  };

public:

  NS_DECL_ISUPPORTS
  NS_DECL_NSIEDITACTIONLISTENER
  NS_DECL_NSIINLINESPELLCHECKER

  // returns true if it looks likely that we can enable real-time spell checking
  static PRBool CanEnableInlineSpellChecking();

  /*BEGIN implementations of mouseevent handler interface*/
  NS_IMETHOD HandleEvent(nsIDOMEvent* aEvent);
  NS_IMETHOD MouseDown(nsIDOMEvent* aMouseEvent);
  NS_IMETHOD MouseUp(nsIDOMEvent* aMouseEvent);
  NS_IMETHOD MouseClick(nsIDOMEvent* aMouseEvent);
  NS_IMETHOD MouseDblClick(nsIDOMEvent* aMouseEvent);
  NS_IMETHOD MouseOver(nsIDOMEvent* aMouseEvent);
  NS_IMETHOD MouseOut(nsIDOMEvent* aMouseEvent);
  /*END implementations of mouseevent handler interface*/

  /* BEGIN interfaces in to the keylistener  interface. */
  NS_IMETHOD KeyDown(nsIDOMEvent* aKeyEvent);
  NS_IMETHOD KeyUp(nsIDOMEvent* aKeyEvent);
  NS_IMETHOD KeyPress(nsIDOMEvent* aKeyEvent);
  /* END interfaces from nsIDOMKeyListener */ 

  mozInlineSpellChecker();
  virtual ~mozInlineSpellChecker();

  // spell check the selected text specified by aSelection
  nsresult SpellCheckSelection(nsISelection *aSelection);

  // spell checks all of the words between two nodes
  nsresult SpellCheckBetweenNodes(nsIDOMNode *aStartNode,
                                  PRInt32 aStartOffset,
                                  nsIDOMNode *aEndNode,
                                  PRInt32 aEndOffset,
                                  nsISelection *aSpellCheckSelection);
  
  // examines the dom node in question and returns true if the inline spell
  // checker should skip the node (i.e. the text is inside of a block quote
  // or an e-mail signature...)
  nsresult SkipSpellCheckForNode(nsIDOMNode *aNode, PRBool * aCheckSpelling);

  nsresult SpellCheckAfterChange(nsIDOMNode* aCursorNode, PRInt32 aCursorOffset,
                                 nsIDOMNode* aPreviousNode, PRInt32 aPreviousOffset,
                                 nsISelection* aSpellCheckSelection);

  // spell check  the text contained within aRange
  nsresult DoSpellCheck(mozInlineSpellWordUtil& aWordUtil,
                        nsIDOMRange *aRange, nsIDOMRange* aNoCheckRange,
                        nsISelection *aSpellCheckSelection);

  // helper routine to determine if a point is inside of a the passed in selection.
  nsresult IsPointInSelection(nsISelection *aSelection,
                              nsIDOMNode *aNode,
                              PRInt32 aOffset,
                              nsIDOMRange **aRange);
  
  nsresult CleanupRangesInSelection(nsISelection *aSelection);

  // helper routines used to control how many ranges we allow into the spell check selection
  nsresult RemoveRange(nsISelection *aSpellCheckSelection, nsIDOMRange * aRange);
  nsresult AddRange(nsISelection *aSpellCheckSelection, nsIDOMRange * aRange);
  PRBool   SpellCheckSelectionIsFull() { return mNumWordsInSpellSelection >= mMaxNumWordsInSpellSelection; }

  // DOM and editor event registration helper routines
  nsresult RegisterEventListeners();
  nsresult UnregisterEventListeners();
  nsresult HandleNavigationEvent(nsIDOMEvent * aEvent, PRBool aForceWordSpellCheck, PRInt32 aNewPositionOffset = 0);

  nsresult GetSpellCheckSelection(nsISelection ** aSpellCheckSelection);
  nsresult SaveCurrentSelectionPosition();
};

#endif /* __mozinlinespellchecker_h__ */
