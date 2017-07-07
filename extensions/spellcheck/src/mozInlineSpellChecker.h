/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __mozinlinespellchecker_h__
#define __mozinlinespellchecker_h__

#include "mozilla/EditorBase.h"
#include "nsRange.h"
#include "nsIEditorSpellCheck.h"
#include "nsIEditActionListener.h"
#include "nsIInlineSpellChecker.h"
#include "nsIDOMTreeWalker.h"
#include "nsWeakReference.h"
#include "nsIDOMEventListener.h"
#include "nsWeakReference.h"
#include "mozISpellI18NUtil.h"
#include "nsCycleCollectionParticipant.h"

// X.h defines KeyPress
#ifdef KeyPress
#undef KeyPress
#endif

class mozInlineSpellWordUtil;
class mozInlineSpellChecker;
class mozInlineSpellResume;
class InitEditorSpellCheckCallback;
class UpdateCurrentDictionaryCallback;
class mozInlineSpellResume;

class mozInlineSpellStatus
{
public:
  explicit mozInlineSpellStatus(mozInlineSpellChecker* aSpellChecker);

  nsresult InitForEditorChange(EditAction aAction,
                               nsIDOMNode* aAnchorNode, int32_t aAnchorOffset,
                               nsIDOMNode* aPreviousNode, int32_t aPreviousOffset,
                               nsIDOMNode* aStartNode, int32_t aStartOffset,
                               nsIDOMNode* aEndNode, int32_t aEndOffset);
  nsresult InitForNavigation(bool aForceCheck, int32_t aNewPositionOffset,
                             nsIDOMNode* aOldAnchorNode, int32_t aOldAnchorOffset,
                             nsIDOMNode* aNewAnchorNode, int32_t aNewAnchorOffset,
                             bool* aContinue);
  nsresult InitForSelection();
  nsresult InitForRange(nsRange* aRange);

  nsresult FinishInitOnEvent(mozInlineSpellWordUtil& aWordUtil);

  // Return true if we plan to spell-check everything
  bool IsFullSpellCheck() const {
    return mOp == eOpChange && !mRange;
  }

  RefPtr<mozInlineSpellChecker> mSpellChecker;

  // The total number of words checked in this sequence, using this tally tells
  // us when to stop. This count is preserved as we continue checking in new
  // messages.
  int32_t mWordCount;

  // what happened?
  enum Operation { eOpChange,       // for SpellCheckAfterChange except deleteSelection
                   eOpChangeDelete, // for SpellCheckAfterChange deleteSelection
                   eOpNavigation,   // for HandleNavigationEvent
                   eOpSelection,    // re-check all misspelled words
                   eOpResume };     // for resuming a previously started check
  Operation mOp;

  // Used for events where we have already computed the range to use. It can
  // also be nullptr in these cases where we need to check the entire range.
  RefPtr<nsRange> mRange;

  // If we happen to know something was inserted, this is that range.
  // Can be nullptr (this only allows an optimization, so not setting doesn't hurt)
  RefPtr<nsRange> mCreatedRange;

  // Contains the range computed for the current word. Can be nullptr.
  RefPtr<nsRange> mNoCheckRange;

  // Indicates the position of the cursor for the event (so we can compute
  // mNoCheckRange). It can be nullptr if we don't care about the cursor position
  // (such as for the intial check of everything).
  //
  // For mOp == eOpNavigation, this is the NEW position of the cursor
  RefPtr<nsRange> mAnchorRange;

  // -----
  // The following members are only for navigation events and are only
  // stored for FinishNavigationEvent to initialize the other members.
  // -----

  // this is the OLD position of the cursor
  RefPtr<nsRange> mOldNavigationAnchorRange;

  // Set when we should force checking the current word. See
  // mozInlineSpellChecker::HandleNavigationEvent for a description of why we
  // have this.
  bool mForceNavigationWordCheck;

  // Contains the offset passed in to HandleNavigationEvent
  int32_t mNewNavigationPositionOffset;

protected:
  nsresult FinishNavigationEvent(mozInlineSpellWordUtil& aWordUtil);

  nsresult FillNoCheckRangeFromAnchor(mozInlineSpellWordUtil& aWordUtil);

  nsresult GetDocument(nsIDOMDocument** aDocument);
  nsresult PositionToCollapsedRange(nsIDOMDocument* aDocument,
                                    nsIDOMNode* aNode, int32_t aOffset,
                                    nsRange** aRange);
};

class mozInlineSpellChecker final : public nsIInlineSpellChecker,
                                    public nsIEditActionListener,
                                    public nsIDOMEventListener,
                                    public nsSupportsWeakReference
{
private:
  friend class mozInlineSpellStatus;
  friend class InitEditorSpellCheckCallback;
  friend class UpdateCurrentDictionaryCallback;
  friend class AutoChangeNumPendingSpellChecks;
  friend class mozInlineSpellResume;

  // Access with CanEnableInlineSpellChecking
  enum SpellCheckingState { SpellCheck_Uninitialized = -1,
                            SpellCheck_NotAvailable = 0,
                            SpellCheck_Available = 1};
  static SpellCheckingState gCanEnableSpellChecking;

  nsWeakPtr mEditor;
  nsCOMPtr<nsIEditorSpellCheck> mSpellCheck;
  nsCOMPtr<nsIEditorSpellCheck> mPendingSpellCheck;
  nsCOMPtr<nsIDOMTreeWalker> mTreeWalker;
  nsCOMPtr<mozISpellI18NUtil> mConverter;

  int32_t mNumWordsInSpellSelection;
  int32_t mMaxNumWordsInSpellSelection;

  // How many misspellings we can add at once. This is often less than the max
  // total number of misspellings. When you have a large textarea prepopulated
  // with text with many misspellings, we can hit this limit. By making it
  // lower than the total number of misspelled words, new text typed by the
  // user can also have spellchecking in it.
  int32_t mMaxMisspellingsPerCheck;

  // we need to keep track of the current text position in the document
  // so we can spell check the old word when the user clicks around the document.
  nsCOMPtr<nsIDOMNode> mCurrentSelectionAnchorNode;
  int32_t              mCurrentSelectionOffset;

  // Tracks the number of pending spell checks *and* async operations that may
  // lead to spell checks, like updating the current dictionary.  This is
  // necessary so that observers can know when to wait for spell check to
  // complete.
  int32_t mNumPendingSpellChecks;

  // The number of calls to UpdateCurrentDictionary that haven't finished yet.
  int32_t mNumPendingUpdateCurrentDictionary;

  // This number is incremented each time the spell checker is disabled so that
  // pending scheduled spell checks and UpdateCurrentDictionary calls can be
  // ignored when they finish.
  uint32_t mDisabledAsyncToken;

  // When mPendingSpellCheck is non-null, this is the callback passed when
  // it was initialized.
  RefPtr<InitEditorSpellCheckCallback> mPendingInitEditorSpellCheckCallback;

  // Set when we have spellchecked after the last edit operation. See the
  // commment at the top of the .cpp file for more info.
  bool mNeedsCheckAfterNavigation;

  // Set when we have a pending mozInlineSpellResume which will check
  // the whole document.
  bool mFullSpellCheckScheduled;

  // Maintains state during the asynchronous UpdateCurrentDictionary call.
  nsString mPreviousDictionary;

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

  // spell checks all of the words between two nodes
  nsresult SpellCheckBetweenNodes(nsIDOMNode *aStartNode,
                                  int32_t aStartOffset,
                                  nsIDOMNode *aEndNode,
                                  int32_t aEndOffset);

  // examines the dom node in question and returns true if the inline spell
  // checker should skip the node (i.e. the text is inside of a block quote
  // or an e-mail signature...)
  bool ShouldSpellCheckNode(nsIEditor* aEditor, nsINode *aNode);

  nsresult SpellCheckAfterChange(nsIDOMNode* aCursorNode, int32_t aCursorOffset,
                                 nsIDOMNode* aPreviousNode, int32_t aPreviousOffset,
                                 nsISelection* aSpellCheckSelection);

  // spell check the text contained within aRange, potentially scheduling
  // another check in the future if the time threshold is reached
  nsresult ScheduleSpellCheck(mozilla::UniquePtr<mozInlineSpellStatus>&& aStatus);

  nsresult DoSpellCheckSelection(mozInlineSpellWordUtil& aWordUtil,
                                 mozilla::dom::Selection* aSpellCheckSelection);
  nsresult DoSpellCheck(mozInlineSpellWordUtil& aWordUtil,
                        mozilla::dom::Selection *aSpellCheckSelection,
                        const mozilla::UniquePtr<mozInlineSpellStatus>& aStatus,
                        bool* aDoneChecking);

  // helper routine to determine if a point is inside of the passed in selection.
  nsresult IsPointInSelection(nsISelection *aSelection,
                              nsIDOMNode *aNode,
                              int32_t aOffset,
                              nsIDOMRange **aRange);

  nsresult CleanupRangesInSelection(mozilla::dom::Selection *aSelection);

  nsresult RemoveRange(mozilla::dom::Selection *aSpellCheckSelection,
                       nsRange *aRange);
  nsresult AddRange(nsISelection *aSpellCheckSelection, nsIDOMRange * aRange);
  bool     SpellCheckSelectionIsFull() { return mNumWordsInSpellSelection >= mMaxNumWordsInSpellSelection; }

  nsresult MakeSpellCheckRange(nsIDOMNode* aStartNode, int32_t aStartOffset,
                               nsIDOMNode* aEndNode, int32_t aEndOffset,
                               nsRange** aRange);

  // DOM and editor event registration helper routines
  nsresult RegisterEventListeners();
  nsresult UnregisterEventListeners();
  nsresult HandleNavigationEvent(bool aForceWordSpellCheck, int32_t aNewPositionOffset = 0);

  nsresult GetSpellCheckSelection(nsISelection ** aSpellCheckSelection);
  nsresult SaveCurrentSelectionPosition();

  nsresult ResumeCheck(mozilla::UniquePtr<mozInlineSpellStatus>&& aStatus);

protected:
  virtual ~mozInlineSpellChecker();

  // called when async nsIEditorSpellCheck methods complete
  nsresult EditorSpellCheckInited();
  nsresult CurrentDictionaryUpdated();

  // track the number of pending spell checks and async operations that may lead
  // to spell checks, notifying observers accordingly
  void ChangeNumPendingSpellChecks(int32_t aDelta,
                                   nsIEditor* aEditor = nullptr);
  void NotifyObservers(const char* aTopic, nsIEditor* aEditor);
};

#endif /* __mozinlinespellchecker_h__ */
