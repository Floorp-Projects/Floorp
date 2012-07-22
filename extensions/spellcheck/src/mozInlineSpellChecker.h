/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __mozinlinespellchecker_h__
#define __mozinlinespellchecker_h__

#include "nsAutoPtr.h"
#include "nsRange.h"
#include "nsIEditorSpellCheck.h"
#include "nsIEditActionListener.h"
#include "nsIInlineSpellChecker.h"
#include "nsITextServicesDocument.h"
#include "nsIDOMTreeWalker.h"
#include "nsWeakReference.h"
#include "nsIEditor.h"
#include "nsIDOMEventListener.h"
#include "nsWeakReference.h"
#include "mozISpellI18NUtil.h"
#include "nsCycleCollectionParticipant.h"

// X.h defines KeyPress
#ifdef KeyPress
#undef KeyPress
#endif

class nsIDOMMouseEventListener;
class mozInlineSpellWordUtil;
class mozInlineSpellChecker;
class mozInlineSpellResume;

class mozInlineSpellStatus
{
public:
  mozInlineSpellStatus(mozInlineSpellChecker* aSpellChecker);

  nsresult InitForEditorChange(PRInt32 aAction,
                               nsIDOMNode* aAnchorNode, PRInt32 aAnchorOffset,
                               nsIDOMNode* aPreviousNode, PRInt32 aPreviousOffset,
                               nsIDOMNode* aStartNode, PRInt32 aStartOffset,
                               nsIDOMNode* aEndNode, PRInt32 aEndOffset);
  nsresult InitForNavigation(bool aForceCheck, PRInt32 aNewPositionOffset,
                             nsIDOMNode* aOldAnchorNode, PRInt32 aOldAnchorOffset,
                             nsIDOMNode* aNewAnchorNode, PRInt32 aNewAnchorOffset,
                             bool* aContinue);
  nsresult InitForSelection();
  nsresult InitForRange(nsRange* aRange);

  nsresult FinishInitOnEvent(mozInlineSpellWordUtil& aWordUtil);

  // Return true if we plan to spell-check everything
  bool IsFullSpellCheck() const {
    return mOp == eOpChange && !mRange;
  }

  nsRefPtr<mozInlineSpellChecker> mSpellChecker;

  // The total number of words checked in this sequence, using this tally tells
  // us when to stop. This count is preserved as we continue checking in new
  // messages.
  PRInt32 mWordCount;

  // what happened?
  enum Operation { eOpChange,       // for SpellCheckAfterChange except kOpDeleteSelection
                   eOpChangeDelete, // for SpellCheckAfterChange kOpDeleteSelection
                   eOpNavigation,   // for HandleNavigationEvent
                   eOpSelection,    // re-check all misspelled words
                   eOpResume };     // for resuming a previously started check
  Operation mOp;

  // Used for events where we have already computed the range to use. It can
  // also be NULL in these cases where we need to check the entire range.
  nsRefPtr<nsRange> mRange;

  // If we happen to know something was inserted, this is that range.
  // Can be NULL (this only allows an optimization, so not setting doesn't hurt)
  nsCOMPtr<nsIDOMRange> mCreatedRange;

  // Contains the range computed for the current word. Can be NULL.
  nsRefPtr<nsRange> mNoCheckRange;

  // Indicates the position of the cursor for the event (so we can compute
  // mNoCheckRange). It can be NULL if we don't care about the cursor position
  // (such as for the intial check of everything).
  //
  // For mOp == eOpNavigation, this is the NEW position of the cursor
  nsCOMPtr<nsIDOMRange> mAnchorRange;

  // -----
  // The following members are only for navigation events and are only
  // stored for FinishNavigationEvent to initialize the other members.
  // -----

  // this is the OLD position of the cursor
  nsCOMPtr<nsIDOMRange> mOldNavigationAnchorRange;

  // Set when we should force checking the current word. See
  // mozInlineSpellChecker::HandleNavigationEvent for a description of why we
  // have this.
  bool mForceNavigationWordCheck;

  // Contains the offset passed in to HandleNavigationEvent
  PRInt32 mNewNavigationPositionOffset;

protected:
  nsresult FinishNavigationEvent(mozInlineSpellWordUtil& aWordUtil);

  nsresult FillNoCheckRangeFromAnchor(mozInlineSpellWordUtil& aWordUtil);

  nsresult GetDocument(nsIDOMDocument** aDocument);
  nsresult PositionToCollapsedRange(nsIDOMDocument* aDocument,
                                    nsIDOMNode* aNode, PRInt32 aOffset,
                                    nsIDOMRange** aRange);
};

class mozInlineSpellChecker : public nsIInlineSpellChecker,
                              public nsIEditActionListener,
                              public nsIDOMEventListener,
                              public nsSupportsWeakReference
{
private:
  friend class mozInlineSpellStatus;

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

  // How many misspellings we can add at once. This is often less than the max
  // total number of misspellings. When you have a large textarea prepopulated
  // with text with many misspellings, we can hit this limit. By making it
  // lower than the total number of misspelled words, new text typed by the
  // user can also have spellchecking in it.
  PRInt32 mMaxMisspellingsPerCheck;

  // we need to keep track of the current text position in the document
  // so we can spell check the old word when the user clicks around the document.
  nsCOMPtr<nsIDOMNode> mCurrentSelectionAnchorNode;
  PRInt32              mCurrentSelectionOffset;

  // Set when we have spellchecked after the last edit operation. See the
  // commment at the top of the .cpp file for more info.
  bool mNeedsCheckAfterNavigation;

  // Set when we have a pending mozInlineSpellResume which will check
  // the whole document.
  bool mFullSpellCheckScheduled;

public:

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_NSIEDITACTIONLISTENER
  NS_DECL_NSIINLINESPELLCHECKER
  NS_DECL_NSIDOMEVENTLISTENER
  NS_DECL_CYCLE_COLLECTION_CLASS_AMBIGUOUS(mozInlineSpellChecker, nsIDOMEventListener)

  // returns true if there are any spell checking dictionaries available
  static bool CanEnableInlineSpellChecking();
  // update the cached value whenever the list of available dictionaries changes
  static void UpdateCanEnableInlineSpellChecking();

  nsresult Blur(nsIDOMEvent* aEvent);
  nsresult MouseClick(nsIDOMEvent* aMouseEvent);
  nsresult KeyPress(nsIDOMEvent* aKeyEvent);

  mozInlineSpellChecker();
  virtual ~mozInlineSpellChecker();

  // spell checks all of the words between two nodes
  nsresult SpellCheckBetweenNodes(nsIDOMNode *aStartNode,
                                  PRInt32 aStartOffset,
                                  nsIDOMNode *aEndNode,
                                  PRInt32 aEndOffset);

  // examines the dom node in question and returns true if the inline spell
  // checker should skip the node (i.e. the text is inside of a block quote
  // or an e-mail signature...)
  nsresult SkipSpellCheckForNode(nsIEditor* aEditor,
                                 nsIDOMNode *aNode, bool * aCheckSpelling);

  nsresult SpellCheckAfterChange(nsIDOMNode* aCursorNode, PRInt32 aCursorOffset,
                                 nsIDOMNode* aPreviousNode, PRInt32 aPreviousOffset,
                                 nsISelection* aSpellCheckSelection);

  // spell check the text contained within aRange, potentially scheduling
  // another check in the future if the time threshold is reached
  nsresult ScheduleSpellCheck(const mozInlineSpellStatus& aStatus);

  nsresult DoSpellCheckSelection(mozInlineSpellWordUtil& aWordUtil,
                                 nsISelection* aSpellCheckSelection,
                                 mozInlineSpellStatus* aStatus);
  nsresult DoSpellCheck(mozInlineSpellWordUtil& aWordUtil,
                        nsISelection *aSpellCheckSelection,
                        mozInlineSpellStatus* aStatus,
                        bool* aDoneChecking);

  // helper routine to determine if a point is inside of the passed in selection.
  nsresult IsPointInSelection(nsISelection *aSelection,
                              nsIDOMNode *aNode,
                              PRInt32 aOffset,
                              nsIDOMRange **aRange);

  nsresult CleanupRangesInSelection(nsISelection *aSelection);

  nsresult RemoveRange(nsISelection *aSpellCheckSelection, nsIDOMRange * aRange);
  nsresult AddRange(nsISelection *aSpellCheckSelection, nsIDOMRange * aRange);
  bool     SpellCheckSelectionIsFull() { return mNumWordsInSpellSelection >= mMaxNumWordsInSpellSelection; }

  nsresult MakeSpellCheckRange(nsIDOMNode* aStartNode, PRInt32 aStartOffset,
                               nsIDOMNode* aEndNode, PRInt32 aEndOffset,
                               nsRange** aRange);

  // DOM and editor event registration helper routines
  nsresult RegisterEventListeners();
  nsresult UnregisterEventListeners();
  nsresult HandleNavigationEvent(bool aForceWordSpellCheck, PRInt32 aNewPositionOffset = 0);

  nsresult GetSpellCheckSelection(nsISelection ** aSpellCheckSelection);
  nsresult SaveCurrentSelectionPosition();

  nsresult ResumeCheck(mozInlineSpellStatus* aStatus);
};

#endif /* __mozinlinespellchecker_h__ */
