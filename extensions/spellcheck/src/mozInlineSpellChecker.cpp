/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=2 sts=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * This class is called by the editor to handle spellchecking after various
 * events. The main entrypoint is SpellCheckAfterEditorChange, which is called
 * when the text is changed.
 *
 * It is VERY IMPORTANT that we do NOT do any operations that might cause DOM
 * notifications to be flushed when we are called from the editor. This is
 * because the call might originate from a frame, and flushing the
 * notifications might cause that frame to be deleted.
 *
 * Using the WordUtil class to find words causes DOM notifications to be
 * flushed because it asks for style information. As a result, we post an event
 * and do all of the spellchecking in that event handler, which occurs later.
 * We store all DOM pointers in ranges because they are kept up-to-date with
 * DOM changes that may have happened while the event was on the queue.
 *
 * We also allow the spellcheck to be suspended and resumed later. This makes
 * large pastes or initializations with a lot of text not hang the browser UI.
 *
 * An optimization is the mNeedsCheckAfterNavigation flag. This is set to
 * true when we get any change, and false once there is no possibility
 * something changed that we need to check on navigation. Navigation events
 * tend to be a little tricky because we want to check the current word on
 * exit if something has changed. If we navigate inside the word, we don't want
 * to do anything. As a result, this flag is cleared in FinishNavigationEvent
 * when we know that we are checking as a result of navigation.
 */

#include "mozInlineSpellChecker.h"

#include "mozilla/EditAction.h"
#include "mozilla/EditorSpellCheck.h"
#include "mozilla/EditorUtils.h"
#include "mozilla/Services.h"
#include "mozilla/TextEditor.h"
#include "mozilla/dom/Event.h"
#include "mozilla/dom/KeyboardEvent.h"
#include "mozilla/dom/KeyboardEventBinding.h"
#include "mozilla/dom/MouseEvent.h"
#include "mozilla/dom/Selection.h"
#include "mozInlineSpellWordUtil.h"
#include "mozISpellI18NManager.h"
#include "mozISpellI18NUtil.h"
#include "nsCOMPtr.h"
#include "nsCRT.h"
#include "nsGenericHTMLElement.h"
#include "nsRange.h"
#include "nsIPlaintextEditor.h"
#include "nsIPrefBranch.h"
#include "nsIPrefService.h"
#include "nsIRunnable.h"
#include "nsISelectionController.h"
#include "nsIServiceManager.h"
#include "nsITextServicesFilter.h"
#include "nsString.h"
#include "nsThreadUtils.h"
#include "nsUnicharUtils.h"
#include "nsIContent.h"
#include "nsIContentInlines.h"
#include "nsRange.h"
#include "nsContentUtils.h"
#include "nsIObserverService.h"
#include "nsITextControlElement.h"
#include "prtime.h"

using namespace mozilla;
using namespace mozilla::dom;

// Set to spew messages to the console about what is happening.
//#define DEBUG_INLINESPELL

// the number of milliseconds that we will take at once to do spellchecking
#define INLINESPELL_CHECK_TIMEOUT 1

// The number of words to check before we look at the time to see if
// INLINESPELL_CHECK_TIMEOUT ms have elapsed. This prevents us from getting
// stuck and not moving forward because the INLINESPELL_CHECK_TIMEOUT might
// be too short to a low-end machine.
#define INLINESPELL_MINIMUM_WORDS_BEFORE_TIMEOUT 5

// These notifications are broadcast when spell check starts and ends.  STARTED
// must always be followed by ENDED.
#define INLINESPELL_STARTED_TOPIC "inlineSpellChecker-spellCheck-started"
#define INLINESPELL_ENDED_TOPIC "inlineSpellChecker-spellCheck-ended"

static bool ContentIsDescendantOf(nsINode* aPossibleDescendant,
                                    nsINode* aPossibleAncestor);

static const char kMaxSpellCheckSelectionSize[] =
  "extensions.spellcheck.inline.max-misspellings";
static const PRTime kMaxSpellCheckTimeInUsec =
  INLINESPELL_CHECK_TIMEOUT * PR_USEC_PER_MSEC;

mozInlineSpellStatus::mozInlineSpellStatus(mozInlineSpellChecker* aSpellChecker)
    : mSpellChecker(aSpellChecker), mWordCount(0)
{
}

// mozInlineSpellStatus::InitForEditorChange
//
//    This is the most complicated case. For changes, we need to compute the
//    range of stuff that changed based on the old and new caret positions,
//    as well as use a range possibly provided by the editor (start and end,
//    which are usually nullptr) to get a range with the union of these.

nsresult
mozInlineSpellStatus::InitForEditorChange(
    EditSubAction aEditSubAction,
    nsINode* aAnchorNode, uint32_t aAnchorOffset,
    nsINode* aPreviousNode, uint32_t aPreviousOffset,
    nsINode* aStartNode, uint32_t aStartOffset,
    nsINode* aEndNode, uint32_t aEndOffset)
{
  if (NS_WARN_IF(!aAnchorNode) || NS_WARN_IF(!aPreviousNode)) {
    return NS_ERROR_FAILURE;
  }

  // save the anchor point as a range so we can find the current word later
  mAnchorRange = PositionToCollapsedRange(aAnchorNode, aAnchorOffset);
  if (NS_WARN_IF(!mAnchorRange)) {
    return NS_ERROR_FAILURE;
  }

  bool deleted = aEditSubAction == EditSubAction::eDeleteSelectedContent;
  if (aEditSubAction == EditSubAction::eInsertTextComingFromIME) {
    // IME may remove the previous node if it cancels composition when
    // there is no text around the composition.
    deleted = !aPreviousNode->IsInComposedDoc();
  }

  if (deleted) {
    // Deletes are easy, the range is just the current anchor. We set the range
    // to check to be empty, FinishInitOnEvent will fill in the range to be
    // the current word.
    mOp = eOpChangeDelete;
    mRange = nullptr;
    return NS_OK;
  }

  mOp = eOpChange;

  // range to check
  mRange = new nsRange(aPreviousNode);

  // ...we need to put the start and end in the correct order
  ErrorResult errorResult;
  int16_t cmpResult =
    mAnchorRange->ComparePoint(*aPreviousNode, aPreviousOffset, errorResult);
  if (NS_WARN_IF(errorResult.Failed())) {
    return errorResult.StealNSResult();
  }
  nsresult rv;
  if (cmpResult < 0) {
    // previous anchor node is before the current anchor
    rv = mRange->SetStartAndEnd(aPreviousNode, aPreviousOffset,
                                aAnchorNode, aAnchorOffset);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  } else {
    // previous anchor node is after (or the same as) the current anchor
    rv = mRange->SetStartAndEnd(aAnchorNode, aAnchorOffset,
                                aPreviousNode, aPreviousOffset);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  // On insert save this range: DoSpellCheck optimizes things in this range.
  // Otherwise, just leave this nullptr.
  if (aEditSubAction == EditSubAction::eInsertText) {
    mCreatedRange = mRange;
  }

  // if we were given a range, we need to expand our range to encompass it
  if (aStartNode && aEndNode) {
    cmpResult = mRange->ComparePoint(*aStartNode, aStartOffset, errorResult);
    if (NS_WARN_IF(errorResult.Failed())) {
      return errorResult.StealNSResult();
    }
    if (cmpResult < 0) { // given range starts before
      rv = mRange->SetStart(aStartNode, aStartOffset);
      NS_ENSURE_SUCCESS(rv, rv);
    }

    cmpResult = mRange->ComparePoint(*aEndNode, aEndOffset, errorResult);
    if (NS_WARN_IF(errorResult.Failed())) {
      return errorResult.StealNSResult();
    }
    if (cmpResult > 0) { // given range ends after
      rv = mRange->SetEnd(aEndNode, aEndOffset);
      NS_ENSURE_SUCCESS(rv, rv);
    }
  }

  return NS_OK;
}

// mozInlineSpellStatis::InitForNavigation
//
//    For navigation events, we just need to store the new and old positions.
//
//    In some cases, we detect that we shouldn't check. If this event should
//    not be processed, *aContinue will be false.

nsresult
mozInlineSpellStatus::InitForNavigation(
    bool aForceCheck, int32_t aNewPositionOffset,
    nsINode* aOldAnchorNode, uint32_t aOldAnchorOffset,
    nsINode* aNewAnchorNode, uint32_t aNewAnchorOffset,
    bool* aContinue)
{
  mOp = eOpNavigation;

  mForceNavigationWordCheck = aForceCheck;
  mNewNavigationPositionOffset = aNewPositionOffset;

  // get the root node for checking
  TextEditor* textEditor = mSpellChecker->mTextEditor;
  if (NS_WARN_IF(!textEditor)) {
    return NS_ERROR_FAILURE;
  }
  Element* root = textEditor->GetRoot();
  if (NS_WARN_IF(!root)) {
    return NS_ERROR_FAILURE;
  }
  // the anchor node might not be in the DOM anymore, check
  if (root && aOldAnchorNode && ! ContentIsDescendantOf(aOldAnchorNode, root)) {
    *aContinue = false;
    return NS_OK;
  }

  mOldNavigationAnchorRange =
    PositionToCollapsedRange(aOldAnchorNode, aOldAnchorOffset);
  if (NS_WARN_IF(!mOldNavigationAnchorRange)) {
    return NS_ERROR_FAILURE;
  }
  mAnchorRange =
    PositionToCollapsedRange(aNewAnchorNode, aNewAnchorOffset);
  if (NS_WARN_IF(!mAnchorRange)) {
    return NS_ERROR_FAILURE;
  }

  *aContinue = true;
  return NS_OK;
}

// mozInlineSpellStatus::InitForSelection
//
//    It is easy for selections since we always re-check the spellcheck
//    selection.

nsresult
mozInlineSpellStatus::InitForSelection()
{
  mOp = eOpSelection;
  return NS_OK;
}

// mozInlineSpellStatus::InitForRange
//
//    Called to cause the spellcheck of the given range. This will look like
//    a change operation over the given range.

nsresult
mozInlineSpellStatus::InitForRange(nsRange* aRange)
{
  mOp = eOpChange;
  mRange = aRange;
  return NS_OK;
}

// mozInlineSpellStatus::FinishInitOnEvent
//
//    Called when the event is triggered to complete initialization that
//    might require the WordUtil. This calls to the operation-specific
//    initializer, and also sets the range to be the entire element if it
//    is nullptr.
//
//    Watch out: the range might still be nullptr if there is nothing to do,
//    the caller will have to check for this.

nsresult
mozInlineSpellStatus::FinishInitOnEvent(mozInlineSpellWordUtil& aWordUtil)
{
  nsresult rv;
  if (! mRange) {
    rv = mSpellChecker->MakeSpellCheckRange(nullptr, 0, nullptr, 0,
                                            getter_AddRefs(mRange));
    NS_ENSURE_SUCCESS(rv, rv);
  }

  switch (mOp) {
    case eOpChange:
      if (mAnchorRange)
        return FillNoCheckRangeFromAnchor(aWordUtil);
      break;
    case eOpChangeDelete:
      if (mAnchorRange) {
        rv = FillNoCheckRangeFromAnchor(aWordUtil);
        NS_ENSURE_SUCCESS(rv, rv);
      }
      // Delete events will have no range for the changed text (because it was
      // deleted), and InitForEditorChange will set it to nullptr. Here, we select
      // the entire word to cause any underlining to be removed.
      mRange = mNoCheckRange;
      break;
    case eOpNavigation:
      return FinishNavigationEvent(aWordUtil);
    case eOpSelection:
      // this gets special handling in ResumeCheck
      break;
    case eOpResume:
      // everything should be initialized already in this case
      break;
    default:
      NS_NOTREACHED("Bad operation");
      return NS_ERROR_NOT_INITIALIZED;
  }
  return NS_OK;
}

// mozInlineSpellStatus::FinishNavigationEvent
//
//    This verifies that we need to check the word at the previous caret
//    position. Now that we have the word util, we can find the word belonging
//    to the previous caret position. If the new position is inside that word,
//    we don't want to do anything. In this case, we'll nullptr out mRange so
//    that the caller will know not to continue.
//
//    Notice that we don't set mNoCheckRange. We check here whether the cursor
//    is in the word that needs checking, so it isn't necessary. Plus, the
//    spellchecker isn't guaranteed to only check the given word, and it could
//    remove the underline from the new word under the cursor.

nsresult
mozInlineSpellStatus::FinishNavigationEvent(mozInlineSpellWordUtil& aWordUtil)
{
  RefPtr<TextEditor> textEditor = mSpellChecker->mTextEditor;
  if (!textEditor) {
    return NS_ERROR_FAILURE; // editor is gone
  }

  NS_ASSERTION(mAnchorRange, "No anchor for navigation!");

  // get the DOM position of the old caret, the range should be collapsed
  ErrorResult err;
  nsCOMPtr<nsINode> oldAnchorNode =
    mOldNavigationAnchorRange->GetStartContainer(err);
  if (NS_WARN_IF(err.Failed())) {
    return err.StealNSResult();
  }
  uint32_t oldAnchorOffset = mOldNavigationAnchorRange->StartOffset();

  // find the word on the old caret position, this is the one that we MAY need
  // to check
  RefPtr<nsRange> oldWord;
  nsresult rv = aWordUtil.GetRangeForWord(oldAnchorNode,
                                          static_cast<int32_t>(oldAnchorOffset),
                                          getter_AddRefs(oldWord));
  NS_ENSURE_SUCCESS(rv, rv);

  // aWordUtil.GetRangeForWord flushes pending notifications, check editor again.
  if (!mSpellChecker->mTextEditor) {
    return NS_ERROR_FAILURE; // editor is gone
  }

  // get the DOM position of the new caret, the range should be collapsed
  nsCOMPtr<nsINode> newAnchorNode = mAnchorRange->GetStartContainer(err);
  if (NS_WARN_IF(err.Failed())) {
    return err.StealNSResult();
  }
  uint32_t newAnchorOffset = mAnchorRange->StartOffset();

  // see if the new cursor position is in the word of the old cursor position
  bool isInRange = false;
  if (! mForceNavigationWordCheck) {
    isInRange =
      oldWord->IsPointInRange(*newAnchorNode,
                              newAnchorOffset + mNewNavigationPositionOffset,
                              err);
    if (NS_WARN_IF(err.Failed())) {
      return err.StealNSResult();
    }
  }

  if (isInRange) {
    // caller should give up
    mRange = nullptr;
  } else {
    // check the old word
    mRange = oldWord;

    // Once we've spellchecked the current word, we don't need to spellcheck
    // for any more navigation events.
    mSpellChecker->mNeedsCheckAfterNavigation = false;
  }
  return NS_OK;
}

// mozInlineSpellStatus::FillNoCheckRangeFromAnchor
//
//    Given the mAnchorRange object, computes the range of the word it is on
//    (if any) and fills that range into mNoCheckRange. This is used for
//    change and navigation events to know which word we should skip spell
//    checking on

nsresult
mozInlineSpellStatus::FillNoCheckRangeFromAnchor(
    mozInlineSpellWordUtil& aWordUtil)
{
  ErrorResult err;
  nsCOMPtr<nsINode> anchorNode = mAnchorRange->GetStartContainer(err);
  if (NS_WARN_IF(err.Failed())) {
    return err.StealNSResult();
  }

  uint32_t anchorOffset = mAnchorRange->StartOffset();
  return aWordUtil.GetRangeForWord(anchorNode,
                                   static_cast<int32_t>(anchorOffset),
                                   getter_AddRefs(mNoCheckRange));
}

// mozInlineSpellStatus::GetDocument
//
//    Returns the nsIDocument object for the document for the
//    current spellchecker.

nsIDocument*
mozInlineSpellStatus::GetDocument() const
{
  if (!mSpellChecker->mTextEditor) {
    return nullptr;
  }

  return mSpellChecker->mTextEditor->GetDocument();
}

// mozInlineSpellStatus::PositionToCollapsedRange
//
//    Converts a given DOM position to a collapsed range covering that
//    position. We use ranges to store DOM positions becuase they stay
//    updated as the DOM is changed.

already_AddRefed<nsRange>
mozInlineSpellStatus::PositionToCollapsedRange(nsINode* aNode,
                                               uint32_t aOffset)
{
  nsCOMPtr<nsIDocument> document = GetDocument();
  if (NS_WARN_IF(!document)) {
    return nullptr;
  }

  RefPtr<nsRange> range = new nsRange(document);

  nsresult rv = range->CollapseTo(aNode, aOffset);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return nullptr;
  }

  return range.forget();
}

// mozInlineSpellResume

class mozInlineSpellResume : public Runnable
{
public:
  mozInlineSpellResume(UniquePtr<mozInlineSpellStatus>&& aStatus,
                       uint32_t aDisabledAsyncToken)
    : Runnable("mozInlineSpellResume")
    , mDisabledAsyncToken(aDisabledAsyncToken)
    , mStatus(std::move(aStatus))
  {
  }

  nsresult Post()
  {
    nsCOMPtr<nsIRunnable> runnable(this);
    return NS_IdleDispatchToCurrentThread(runnable.forget(), 1000);
  }

  NS_IMETHOD Run() override
  {
    // Discard the resumption if the spell checker was disabled after the
    // resumption was scheduled.
    if (mDisabledAsyncToken == mStatus->mSpellChecker->mDisabledAsyncToken) {
      mStatus->mSpellChecker->ResumeCheck(std::move(mStatus));
    }
    return NS_OK;
  }

private:
  uint32_t mDisabledAsyncToken;
  UniquePtr<mozInlineSpellStatus> mStatus;
};

// Used as the nsIEditorSpellCheck::InitSpellChecker callback.
class InitEditorSpellCheckCallback final : public nsIEditorSpellCheckCallback
{
  ~InitEditorSpellCheckCallback() {}
public:
  NS_DECL_ISUPPORTS

  explicit InitEditorSpellCheckCallback(mozInlineSpellChecker* aSpellChecker)
    : mSpellChecker(aSpellChecker) {}

  NS_IMETHOD EditorSpellCheckDone() override
  {
    return mSpellChecker ? mSpellChecker->EditorSpellCheckInited() : NS_OK;
  }

  void Cancel()
  {
    mSpellChecker = nullptr;
  }

private:
  RefPtr<mozInlineSpellChecker> mSpellChecker;
};
NS_IMPL_ISUPPORTS(InitEditorSpellCheckCallback, nsIEditorSpellCheckCallback)


NS_INTERFACE_MAP_BEGIN(mozInlineSpellChecker)
  NS_INTERFACE_MAP_ENTRY(nsIInlineSpellChecker)
  NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
  NS_INTERFACE_MAP_ENTRY(nsIDOMEventListener)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIDOMEventListener)
  NS_INTERFACE_MAP_ENTRIES_CYCLE_COLLECTION(mozInlineSpellChecker)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(mozInlineSpellChecker)
NS_IMPL_CYCLE_COLLECTING_RELEASE(mozInlineSpellChecker)

NS_IMPL_CYCLE_COLLECTION(mozInlineSpellChecker,
                         mTextEditor,
                         mSpellCheck,
                         mCurrentSelectionAnchorNode)

mozInlineSpellChecker::SpellCheckingState
  mozInlineSpellChecker::gCanEnableSpellChecking =
  mozInlineSpellChecker::SpellCheck_Uninitialized;

mozInlineSpellChecker::mozInlineSpellChecker() :
    mNumWordsInSpellSelection(0),
    mMaxNumWordsInSpellSelection(250),
    mNumPendingSpellChecks(0),
    mNumPendingUpdateCurrentDictionary(0),
    mDisabledAsyncToken(0),
    mNeedsCheckAfterNavigation(false),
    mFullSpellCheckScheduled(false),
    mIsListeningToEditSubActions(false)
{
  nsCOMPtr<nsIPrefBranch> prefs = do_GetService(NS_PREFSERVICE_CONTRACTID);
  if (prefs)
    prefs->GetIntPref(kMaxSpellCheckSelectionSize, &mMaxNumWordsInSpellSelection);
  mMaxMisspellingsPerCheck = mMaxNumWordsInSpellSelection * 3 / 4;
}

mozInlineSpellChecker::~mozInlineSpellChecker()
{
}

EditorSpellCheck*
mozInlineSpellChecker::GetEditorSpellCheck()
{
  return mSpellCheck ? mSpellCheck : mPendingSpellCheck;
}

NS_IMETHODIMP
mozInlineSpellChecker::GetSpellChecker(nsIEditorSpellCheck **aSpellCheck)
{
  *aSpellCheck = mSpellCheck;
  NS_IF_ADDREF(*aSpellCheck);
  return NS_OK;
}

NS_IMETHODIMP
mozInlineSpellChecker::Init(nsIEditor* aEditor)
{
  mTextEditor = aEditor ? aEditor->AsTextEditor() : nullptr;
  return NS_OK;
}

// mozInlineSpellChecker::Cleanup
//
//    Called by the editor when the editor is going away. This is important
//    because we remove listeners. We do NOT clean up anything else in this
//    function, because it can get called while DoSpellCheck is running!
//
//    Getting the style information there can cause DOM notifications to be
//    flushed, which can cause editors to go away which will bring us here.
//    We can not do anything that will cause DoSpellCheck to freak out.

nsresult mozInlineSpellChecker::Cleanup(bool aDestroyingFrames)
{
  mNumWordsInSpellSelection = 0;
  RefPtr<Selection> spellCheckSelection = GetSpellCheckSelection();
  nsresult rv = NS_OK;
  if (!spellCheckSelection) {
    // Ensure we still unregister event listeners (but return a failure code)
    UnregisterEventListeners();
    rv = NS_ERROR_FAILURE;
  } else {
    if (!aDestroyingFrames) {
      spellCheckSelection->RemoveAllRanges(IgnoreErrors());
    }

    rv = UnregisterEventListeners();
  }

  // Notify ENDED observers now.  If we wait to notify as we normally do when
  // these async operations finish, then in the meantime the editor may create
  // another inline spell checker and cause more STARTED and ENDED
  // notifications to be broadcast.  Interleaved notifications for the same
  // editor but different inline spell checkers could easily confuse
  // observers.  They may receive two consecutive STARTED notifications for
  // example, which we guarantee will not happen.

  RefPtr<TextEditor> textEditor = mTextEditor.forget();
  if (mPendingSpellCheck) {
    // Cancel the pending editor spell checker initialization.
    mPendingSpellCheck = nullptr;
    mPendingInitEditorSpellCheckCallback->Cancel();
    mPendingInitEditorSpellCheckCallback = nullptr;
    ChangeNumPendingSpellChecks(-1, textEditor);
  }

  // Increment this token so that pending UpdateCurrentDictionary calls and
  // scheduled spell checks are discarded when they finish.
  mDisabledAsyncToken++;

  if (mNumPendingUpdateCurrentDictionary > 0) {
    // Account for pending UpdateCurrentDictionary calls.
    ChangeNumPendingSpellChecks(-mNumPendingUpdateCurrentDictionary,
                                textEditor);
    mNumPendingUpdateCurrentDictionary = 0;
  }
  if (mNumPendingSpellChecks > 0) {
    // If mNumPendingSpellChecks is still > 0 at this point, the remainder is
    // pending scheduled spell checks.
    ChangeNumPendingSpellChecks(-mNumPendingSpellChecks, textEditor);
  }

  mFullSpellCheckScheduled = false;

  return rv;
}

// mozInlineSpellChecker::CanEnableInlineSpellChecking
//
//    This function can be called to see if it seems likely that we can enable
//    spellchecking before actually creating the InlineSpellChecking objects.
//
//    The problem is that we can't get the dictionary list without actually
//    creating a whole bunch of spellchecking objects. This function tries to
//    do that and caches the result so we don't have to keep allocating those
//    objects if there are no dictionaries or spellchecking.
//
//    Whenever dictionaries are added or removed at runtime, this value must be
//    updated before an observer notification is sent out about the change, to
//    avoid editors getting a wrong cached result.

bool // static
mozInlineSpellChecker::CanEnableInlineSpellChecking()
{
  nsresult rv;
  if (gCanEnableSpellChecking == SpellCheck_Uninitialized) {
    gCanEnableSpellChecking = SpellCheck_NotAvailable;

    nsCOMPtr<nsIEditorSpellCheck> spellchecker =
      do_CreateInstance("@mozilla.org/editor/editorspellchecker;1", &rv);
    NS_ENSURE_SUCCESS(rv, false);

    bool canSpellCheck = false;
    rv = spellchecker->CanSpellCheck(&canSpellCheck);
    NS_ENSURE_SUCCESS(rv, false);

    if (canSpellCheck)
      gCanEnableSpellChecking = SpellCheck_Available;
  }
  return (gCanEnableSpellChecking == SpellCheck_Available);
}

void // static
mozInlineSpellChecker::UpdateCanEnableInlineSpellChecking()
{
  gCanEnableSpellChecking = SpellCheck_Uninitialized;
}

// mozInlineSpellChecker::RegisterEventListeners
//
//    The inline spell checker listens to mouse events and keyboard navigation
//    events.

nsresult
mozInlineSpellChecker::RegisterEventListeners()
{
  if (NS_WARN_IF(!mTextEditor)) {
    return NS_ERROR_FAILURE;
  }

  StartToListenToEditSubActions();

  nsCOMPtr<nsIDocument> doc = mTextEditor->GetDocument();
  if (NS_WARN_IF(!doc)) {
    return NS_ERROR_FAILURE;
  }
  doc->AddEventListener(NS_LITERAL_STRING("blur"), this, true, false);
  doc->AddEventListener(NS_LITERAL_STRING("click"), this, false, false);
  doc->AddEventListener(NS_LITERAL_STRING("keypress"), this, false, false);
  return NS_OK;
}

// mozInlineSpellChecker::UnregisterEventListeners

nsresult
mozInlineSpellChecker::UnregisterEventListeners()
{
  if (NS_WARN_IF(!mTextEditor)) {
    return NS_ERROR_FAILURE;
  }

  EndListeningToEditSubActions();

  nsCOMPtr<nsIDocument> doc = mTextEditor->GetDocument();
  if (NS_WARN_IF(!doc)) {
    return NS_ERROR_FAILURE;
  }
  doc->RemoveEventListener(NS_LITERAL_STRING("blur"), this, true);
  doc->RemoveEventListener(NS_LITERAL_STRING("click"), this, false);
  doc->RemoveEventListener(NS_LITERAL_STRING("keypress"), this, false);
  return NS_OK;
}

// mozInlineSpellChecker::GetEnableRealTimeSpell

NS_IMETHODIMP
mozInlineSpellChecker::GetEnableRealTimeSpell(bool* aEnabled)
{
  NS_ENSURE_ARG_POINTER(aEnabled);
  *aEnabled = mSpellCheck != nullptr || mPendingSpellCheck != nullptr;
  return NS_OK;
}

// mozInlineSpellChecker::SetEnableRealTimeSpell

NS_IMETHODIMP
mozInlineSpellChecker::SetEnableRealTimeSpell(bool aEnabled)
{
  if (!aEnabled) {
    mSpellCheck = nullptr;
    return Cleanup(false);
  }

  if (mSpellCheck) {
    // spellcheck the current contents. SpellCheckRange doesn't supply a created
    // range to DoSpellCheck, which in our case is the entire range. But this
    // optimization doesn't matter because there is nothing in the spellcheck
    // selection when starting, which triggers a better optimization.
    return SpellCheckRange(nullptr);
  }

  if (mPendingSpellCheck) {
    // The editor spell checker is already being initialized.
    return NS_OK;
  }

  nsCOMPtr<nsITextServicesFilter> filter =
    do_CreateInstance("@mozilla.org/editor/txtsrvfiltermail;1");
  if (NS_WARN_IF(!filter)) {
    return NS_ERROR_FAILURE;
  }

  mPendingSpellCheck = new EditorSpellCheck();
  mPendingSpellCheck->SetFilter(filter);

  mPendingInitEditorSpellCheckCallback = new InitEditorSpellCheckCallback(this);
  nsresult rv = mPendingSpellCheck->InitSpellChecker(
                  mTextEditor, false, mPendingInitEditorSpellCheckCallback);
  if (NS_FAILED(rv)) {
    mPendingSpellCheck = nullptr;
    mPendingInitEditorSpellCheckCallback = nullptr;
    NS_ENSURE_SUCCESS(rv, rv);
  }

  ChangeNumPendingSpellChecks(1);

  return NS_OK;
}

// Called when nsIEditorSpellCheck::InitSpellChecker completes.
nsresult
mozInlineSpellChecker::EditorSpellCheckInited()
{
  NS_ASSERTION(mPendingSpellCheck, "Spell check should be pending!");

  // spell checking is enabled, register our event listeners to track navigation
  RegisterEventListeners();

  mSpellCheck = mPendingSpellCheck;
  mPendingSpellCheck = nullptr;
  mPendingInitEditorSpellCheckCallback = nullptr;
  ChangeNumPendingSpellChecks(-1);

  // spellcheck the current contents. SpellCheckRange doesn't supply a created
  // range to DoSpellCheck, which in our case is the entire range. But this
  // optimization doesn't matter because there is nothing in the spellcheck
  // selection when starting, which triggers a better optimization.
  return SpellCheckRange(nullptr);
}

// Changes the number of pending spell checks by the given delta.  If the number
// becomes zero or nonzero, observers are notified.  See NotifyObservers for
// info on the aEditor parameter.
void
mozInlineSpellChecker::ChangeNumPendingSpellChecks(int32_t aDelta,
                                                   TextEditor* aTextEditor)
{
  int8_t oldNumPending = mNumPendingSpellChecks;
  mNumPendingSpellChecks += aDelta;
  NS_ASSERTION(mNumPendingSpellChecks >= 0,
               "Unbalanced ChangeNumPendingSpellChecks calls!");
  if (oldNumPending == 0 && mNumPendingSpellChecks > 0) {
    NotifyObservers(INLINESPELL_STARTED_TOPIC, aTextEditor);
  } else if (oldNumPending > 0 && mNumPendingSpellChecks == 0) {
    NotifyObservers(INLINESPELL_ENDED_TOPIC, aTextEditor);
  }
}

// Broadcasts the given topic to observers.  aEditor is passed to observers if
// nonnull; otherwise mTextEditor is passed.
void
mozInlineSpellChecker::NotifyObservers(const char* aTopic,
                                       TextEditor* aTextEditor)
{
  nsCOMPtr<nsIObserverService> os = mozilla::services::GetObserverService();
  if (!os)
    return;
  // XXX Do we need to grab the editor here?  If it's necessary, each observer
  //     should do it instead.
  RefPtr<TextEditor> textEditor =
    aTextEditor ? aTextEditor : mTextEditor.get();
  os->NotifyObservers(static_cast<nsIEditor*>(textEditor.get()),
                      aTopic, nullptr);
}

// mozInlineSpellChecker::SpellCheckAfterEditorChange
//
//    Called by the editor when nearly anything happens to change the content.
//
//    The start and end positions specify a range for the thing that happened,
//    but these are usually nullptr, even when you'd think they would be useful
//    because you want the range (for example, pasting). We ignore them in
//    this case.

nsresult
mozInlineSpellChecker::SpellCheckAfterEditorChange(
    EditSubAction aEditSubAction, Selection& aSelection,
    nsINode *aPreviousSelectedNode, uint32_t aPreviousSelectedOffset,
    nsINode *aStartNode, uint32_t aStartOffset,
    nsINode *aEndNode, uint32_t aEndOffset)
{
  nsresult rv;
  if (!mSpellCheck)
    return NS_OK; // disabling spell checking is not an error

  // this means something has changed, and we never check the current word,
  // therefore, we should spellcheck for subsequent caret navigations
  mNeedsCheckAfterNavigation = true;

  // the anchor node is the position of the caret
  auto status = MakeUnique<mozInlineSpellStatus>(this);
  rv = status->InitForEditorChange(aEditSubAction,
                                   aSelection.GetAnchorNode(),
                                   aSelection.AnchorOffset(),
                                   aPreviousSelectedNode,
                                   aPreviousSelectedOffset,
                                   aStartNode, aStartOffset,
                                   aEndNode, aEndOffset);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = ScheduleSpellCheck(std::move(status));
  NS_ENSURE_SUCCESS(rv, rv);

  // remember the current caret position after every change
  SaveCurrentSelectionPosition();
  return NS_OK;
}

// mozInlineSpellChecker::SpellCheckRange
//
//    Spellchecks all the words in the given range.
//    Supply a nullptr range and this will check the entire editor.

nsresult
mozInlineSpellChecker::SpellCheckRange(nsRange* aRange)
{
  if (!mSpellCheck) {
    NS_WARNING_ASSERTION(
      mPendingSpellCheck,
      "Trying to spellcheck, but checking seems to be disabled");
    return NS_ERROR_NOT_INITIALIZED;
  }

  auto status = MakeUnique<mozInlineSpellStatus>(this);
  nsresult rv = status->InitForRange(aRange);
  NS_ENSURE_SUCCESS(rv, rv);
  return ScheduleSpellCheck(std::move(status));
}

// mozInlineSpellChecker::GetMisspelledWord

NS_IMETHODIMP
mozInlineSpellChecker::GetMisspelledWord(nsINode *aNode, int32_t aOffset,
                                         nsRange** newword)
{
  if (NS_WARN_IF(!aNode)) {
    return NS_ERROR_INVALID_ARG;
  }
  RefPtr<Selection> spellCheckSelection = GetSpellCheckSelection();
  if (NS_WARN_IF(!spellCheckSelection)) {
    return NS_ERROR_FAILURE;
  }
  return IsPointInSelection(*spellCheckSelection, aNode, aOffset, newword);
}

// mozInlineSpellChecker::ReplaceWord

NS_IMETHODIMP
mozInlineSpellChecker::ReplaceWord(nsINode *aNode, int32_t aOffset,
                                   const nsAString &newword)
{
  if (NS_WARN_IF(!mTextEditor) || NS_WARN_IF(newword.IsEmpty())) {
    return NS_ERROR_FAILURE;
  }

  RefPtr<nsRange> range;
  nsresult res = GetMisspelledWord(aNode, aOffset, getter_AddRefs(range));
  NS_ENSURE_SUCCESS(res, res);

  if (range)
  {
    // This range was retrieved from the spellchecker selection. As
    // ranges cannot be shared between selections, we must clone it
    // before adding it to the editor's selection.
    RefPtr<nsRange> editorRange = range->CloneRange();

    AutoPlaceholderBatch phb(mTextEditor, nullptr);

    RefPtr<Selection> selection = mTextEditor->GetSelection();
    NS_ENSURE_TRUE(selection, NS_ERROR_UNEXPECTED);
    selection->RemoveAllRanges(IgnoreErrors());
    selection->AddRange(*editorRange, IgnoreErrors());

    MOZ_ASSERT(mTextEditor);
    DebugOnly<nsresult> rv = mTextEditor->InsertTextAsAction(newword);
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "Failed to insert the new word");
  }

  return NS_OK;
}

// mozInlineSpellChecker::AddWordToDictionary

NS_IMETHODIMP
mozInlineSpellChecker::AddWordToDictionary(const nsAString &word)
{
  NS_ENSURE_TRUE(mSpellCheck, NS_ERROR_NOT_INITIALIZED);

  nsresult rv = mSpellCheck->AddWordToDictionary(word);
  NS_ENSURE_SUCCESS(rv, rv);

  auto status = MakeUnique<mozInlineSpellStatus>(this);
  rv = status->InitForSelection();
  NS_ENSURE_SUCCESS(rv, rv);
  return ScheduleSpellCheck(std::move(status));
}

//  mozInlineSpellChecker::RemoveWordFromDictionary

NS_IMETHODIMP
mozInlineSpellChecker::RemoveWordFromDictionary(const nsAString &word)
{
  NS_ENSURE_TRUE(mSpellCheck, NS_ERROR_NOT_INITIALIZED);

  nsresult rv = mSpellCheck->RemoveWordFromDictionary(word);
  NS_ENSURE_SUCCESS(rv, rv);

  auto status = MakeUnique<mozInlineSpellStatus>(this);
  rv = status->InitForRange(nullptr);
  NS_ENSURE_SUCCESS(rv, rv);
  return ScheduleSpellCheck(std::move(status));
}

// mozInlineSpellChecker::IgnoreWord

NS_IMETHODIMP
mozInlineSpellChecker::IgnoreWord(const nsAString &word)
{
  NS_ENSURE_TRUE(mSpellCheck, NS_ERROR_NOT_INITIALIZED);

  nsresult rv = mSpellCheck->IgnoreWordAllOccurrences(word);
  NS_ENSURE_SUCCESS(rv, rv);

  auto status = MakeUnique<mozInlineSpellStatus>(this);
  rv = status->InitForSelection();
  NS_ENSURE_SUCCESS(rv, rv);
  return ScheduleSpellCheck(std::move(status));
}

// mozInlineSpellChecker::IgnoreWords

NS_IMETHODIMP
mozInlineSpellChecker::IgnoreWords(const char16_t **aWordsToIgnore,
                                   uint32_t aCount)
{
  NS_ENSURE_TRUE(mSpellCheck, NS_ERROR_NOT_INITIALIZED);

  // add each word to the ignore list and then recheck the document
  for (uint32_t index = 0; index < aCount; index++)
    mSpellCheck->IgnoreWordAllOccurrences(
                   nsDependentString(aWordsToIgnore[index]));

  auto status = MakeUnique<mozInlineSpellStatus>(this);
  nsresult rv = status->InitForSelection();
  NS_ENSURE_SUCCESS(rv, rv);
  return ScheduleSpellCheck(std::move(status));
}

void
mozInlineSpellChecker::DidSplitNode(nsINode* aExistingRightNode,
                                    nsINode* aNewLeftNode)
{
  if (!mIsListeningToEditSubActions) {
    return;
  }
  SpellCheckBetweenNodes(aNewLeftNode, 0, aNewLeftNode, 0);
}

void
mozInlineSpellChecker::DidJoinNodes(nsINode& aLeftNode,
                                    nsINode& aRightNode)
{
  if (!mIsListeningToEditSubActions) {
    return;
  }
  SpellCheckBetweenNodes(&aRightNode, 0, &aRightNode, 0);
}

// mozInlineSpellChecker::MakeSpellCheckRange
//
//    Given begin and end positions, this function constructs a range as
//    required for ScheduleSpellCheck. If the start and end nodes are nullptr,
//    then the entire range will be selected, and you can supply -1 as the
//    offset to the end range to select all of that node.
//
//    If the resulting range would be empty, nullptr is put into *aRange and the
//    function succeeds.

nsresult
mozInlineSpellChecker::MakeSpellCheckRange(
    nsINode* aStartNode, int32_t aStartOffset,
    nsINode* aEndNode, int32_t aEndOffset,
    nsRange** aRange)
{
  nsresult rv;
  *aRange = nullptr;

  if (NS_WARN_IF(!mTextEditor)) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIDocument> doc = mTextEditor->GetDocument();
  if (NS_WARN_IF(!doc)) {
    return NS_ERROR_FAILURE;
  }

  RefPtr<nsRange> range = new nsRange(doc);

  // possibly use full range of the editor
  if (! aStartNode || ! aEndNode) {
    Element* domRootElement = mTextEditor->GetRoot();
    if (NS_WARN_IF(!domRootElement)) {
      return NS_ERROR_FAILURE;
    }
    aStartNode = aEndNode = domRootElement;
    aStartOffset = 0;
    aEndOffset = -1;
  }

  if (aEndOffset == -1) {
    // It's hard to say whether it's better to just do nsINode::GetChildCount or
    // get the ChildNodes() and then its length.  The latter is faster if we
    // keep going through this code for the same nodes (because it caches the
    // length).  The former is faster if we keep getting different nodes here...
    //
    // Let's do the thing which can't end up with bad O(N^2) behavior.
    aEndOffset = aEndNode->ChildNodes()->Length();
  }

  // sometimes we are are requested to check an empty range (possibly an empty
  // document). This will result in assertions later.
  if (aStartNode == aEndNode && aStartOffset == aEndOffset)
    return NS_OK;

  if (aEndOffset) {
    rv = range->SetStartAndEnd(aStartNode, aStartOffset, aEndNode, aEndOffset);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  } else {
    uint32_t endOffset;
    aEndNode = nsRange::GetContainerAndOffsetAfter(aEndNode, &endOffset);
    rv = range->SetStartAndEnd(aStartNode, aStartOffset, aEndNode, endOffset);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  range.swap(*aRange);
  return NS_OK;
}

nsresult
mozInlineSpellChecker::SpellCheckBetweenNodes(nsINode* aStartNode,
                                              int32_t aStartOffset,
                                              nsINode* aEndNode,
                                              int32_t aEndOffset)
{
  RefPtr<nsRange> range;
  nsresult rv = MakeSpellCheckRange(aStartNode, aStartOffset,
                                    aEndNode, aEndOffset,
                                    getter_AddRefs(range));
  NS_ENSURE_SUCCESS(rv, rv);

  if (! range)
    return NS_OK; // range is empty: nothing to do

  auto status = MakeUnique<mozInlineSpellStatus>(this);
  rv = status->InitForRange(range);
  NS_ENSURE_SUCCESS(rv, rv);
  return ScheduleSpellCheck(std::move(status));
}

// mozInlineSpellChecker::ShouldSpellCheckNode
//
//    There are certain conditions when we don't want to spell check a node. In
//    particular quotations, moz signatures, etc. This routine returns false
//    for these cases.

bool
mozInlineSpellChecker::ShouldSpellCheckNode(TextEditor* aTextEditor,
                                            nsINode *aNode)
{
  MOZ_ASSERT(aNode);
  if (!aNode->IsContent())
    return false;

  nsIContent *content = aNode->AsContent();

  if (aTextEditor->IsMailEditor()) {
    nsIContent *parent = content->GetParent();
    while (parent) {
      if (parent->IsHTMLElement(nsGkAtoms::blockquote) &&
          parent->AsElement()->AttrValueIs(kNameSpaceID_None,
                                           nsGkAtoms::type,
                                           nsGkAtoms::cite,
                                           eIgnoreCase)) {
        return false;
      }
      if (parent->IsHTMLElement(nsGkAtoms::pre) &&
          parent->AsElement()->AttrValueIs(kNameSpaceID_None,
                                           nsGkAtoms::_class,
                                           nsGkAtoms::mozsignature,
                                           eIgnoreCase)) {
        return false;
      }

      parent = parent->GetParent();
    }
  } else {
    // Check spelling only if the node is editable, and GetSpellcheck() is true
    // on the nearest HTMLElement ancestor.
    if (!content->IsEditable()) {
      return false;
    }

    // Make sure that we can always turn on spell checking for inputs/textareas.
    // Note that because of the previous check, at this point we know that the
    // node is editable.
    if (content->IsInAnonymousSubtree()) {
      nsIContent *node = content->GetParent();
      while (node && node->IsInNativeAnonymousSubtree()) {
        node = node->GetParent();
      }
      nsCOMPtr<nsITextControlElement> textControl = do_QueryInterface(node);
      if (textControl) {
        return true;
      }
    }

    // Get HTML element ancestor (might be aNode itself, although probably that
    // has to be a text node in real life here)
    nsIContent *parent = content;
    while (!parent->IsHTMLElement()) {
      parent = parent->GetParent();
      if (!parent) {
        return true;
      }
    }

    // See if it's spellcheckable
    return static_cast<nsGenericHTMLElement *>(parent)->Spellcheck();
  }

  return true;
}

// mozInlineSpellChecker::ScheduleSpellCheck
//
//    This is called by code to do the actual spellchecking. We will set up
//    the proper structures for calls to DoSpellCheck.

nsresult
mozInlineSpellChecker::ScheduleSpellCheck(UniquePtr<mozInlineSpellStatus>&& aStatus)
{
  if (mFullSpellCheckScheduled) {
    // Just ignore this; we're going to spell-check everything anyway
    return NS_OK;
  }
  // Cache the value because we are going to move aStatus's ownership to
  // the new created mozInlineSpellResume instance.
  bool isFullSpellCheck = aStatus->IsFullSpellCheck();

  RefPtr<mozInlineSpellResume> resume =
    new mozInlineSpellResume(std::move(aStatus), mDisabledAsyncToken);
  NS_ENSURE_TRUE(resume, NS_ERROR_OUT_OF_MEMORY);

  nsresult rv = resume->Post();
  if (NS_SUCCEEDED(rv)) {
    if (isFullSpellCheck) {
      // We're going to check everything.  Suppress further spell-check attempts
      // until that happens.
      mFullSpellCheckScheduled = true;
    }
    ChangeNumPendingSpellChecks(1);
  }
  return rv;
}

// mozInlineSpellChecker::DoSpellCheckSelection
//
//    Called to re-check all misspelled words. We iterate over all ranges in
//    the selection and call DoSpellCheck on them. This is used when a word
//    is ignored or added to the dictionary: all instances of that word should
//    be removed from the selection.
//
//    FIXME-PERFORMANCE: This takes as long as it takes and is not resumable.
//    Typically, checking this small amount of text is relatively fast, but
//    for large numbers of words, a lag may be noticeable.

nsresult
mozInlineSpellChecker::DoSpellCheckSelection(mozInlineSpellWordUtil& aWordUtil,
                                             Selection* aSpellCheckSelection)
{
  nsresult rv;

  // clear out mNumWordsInSpellSelection since we'll be rebuilding the ranges.
  mNumWordsInSpellSelection = 0;

  // Since we could be modifying the ranges for the spellCheckSelection while
  // looping on the spell check selection, keep a separate array of range
  // elements inside the selection
  nsTArray<RefPtr<nsRange>> ranges;

  int32_t count = aSpellCheckSelection->RangeCount();

  for (int32_t idx = 0; idx < count; idx++) {
    nsRange *range = aSpellCheckSelection->GetRangeAt(idx);
    if (range) {
      ranges.AppendElement(range);
    }
  }

  // We have saved the ranges above. Clearing the spellcheck selection here
  // isn't necessary (rechecking each word will modify it as necessary) but
  // provides better performance. By ensuring that no ranges need to be
  // removed in DoSpellCheck, we can save checking range inclusion which is
  // slow.
  aSpellCheckSelection->RemoveAllRanges(IgnoreErrors());

  // We use this state object for all calls, and just update its range. Note
  // that we don't need to call FinishInit since we will be filling in the
  // necessary information.
  auto status = MakeUnique<mozInlineSpellStatus>(this);
  rv = status->InitForRange(nullptr);
  NS_ENSURE_SUCCESS(rv, rv);

  bool doneChecking;
  for (int32_t idx = 0; idx < count; idx++) {
    // We can consider this word as "added" since we know it has no spell
    // check range over it that needs to be deleted. All the old ranges
    // were cleared above. We also need to clear the word count so that we
    // check all words instead of stopping early.
    status->mRange = ranges[idx];
    rv = DoSpellCheck(aWordUtil, aSpellCheckSelection, status,
                      &doneChecking);
    NS_ENSURE_SUCCESS(rv, rv);
    MOZ_ASSERT(doneChecking,
               "We gave the spellchecker one word, but it didn't finish checking?!?!");

    status->mWordCount = 0;
  }

  return NS_OK;
}

// mozInlineSpellChecker::DoSpellCheck
//
//    This function checks words intersecting the given range, excluding those
//    inside mStatus->mNoCheckRange (can be nullptr). Words inside aNoCheckRange
//    will have any spell selection removed (this is used to hide the
//    underlining for the word that the caret is in). aNoCheckRange should be
//    on word boundaries.
//
//    mResume->mCreatedRange is a possibly nullptr range of new text that was
//    inserted.  Inside this range, we don't bother to check whether things are
//    inside the spellcheck selection, which speeds up large paste operations
//    considerably.
//
//    Normal case when editing text by typing
//       h e l l o   w o r k d   h o w   a r e   y o u
//                            ^ caret
//                   [-------] mRange
//                   [-------] mNoCheckRange
//      -> does nothing (range is the same as the no check range)
//
//    Case when pasting:
//             [---------- pasted text ----------]
//       h e l l o   w o r k d   h o w   a r e   y o u
//                                                ^ caret
//                                               [---] aNoCheckRange
//      -> recheck all words in range except those in aNoCheckRange
//
//    If checking is complete, *aDoneChecking will be set. If there is more
//    but we ran out of time, this will be false and the range will be
//    updated with the stuff that still needs checking.

nsresult mozInlineSpellChecker::DoSpellCheck(mozInlineSpellWordUtil& aWordUtil,
                                             Selection *aSpellCheckSelection,
                                             const UniquePtr<mozInlineSpellStatus>& aStatus,
                                             bool* aDoneChecking)
{
  *aDoneChecking = true;

  NS_ENSURE_TRUE(mSpellCheck, NS_ERROR_NOT_INITIALIZED);

  // get the editor for ShouldSpellCheckNode, this may fail in reasonable
  // circumstances since the editor could have gone away
  RefPtr<TextEditor> textEditor = mTextEditor;
  if (!textEditor) {
    return NS_ERROR_FAILURE;
  }

  if (aStatus->mRange->Collapsed())
    return NS_OK;

  // see if the selection has any ranges, if not, then we can optimize checking
  // range inclusion later (we have no ranges when we are initially checking or
  // when there are no misspelled words yet).
  int32_t originalRangeCount = aSpellCheckSelection->RangeCount();

  // set the starting DOM position to be the beginning of our range
  {
    // Scope for the node/offset pairs here so they don't get
    // accidentally used later
    nsINode* beginNode = aStatus->mRange->GetStartContainer();
    int32_t beginOffset = aStatus->mRange->StartOffset();
    nsINode* endNode = aStatus->mRange->GetEndContainer();
    int32_t endOffset = aStatus->mRange->EndOffset();

    // Now check that we're still looking at a range that's under
    // aWordUtil.GetRootNode()
    nsINode* rootNode = aWordUtil.GetRootNode();
    if (!beginNode->IsInComposedDoc() || !endNode->IsInComposedDoc() ||
        !nsContentUtils::ContentIsShadowIncludingDescendantOf(beginNode, rootNode) ||
        !nsContentUtils::ContentIsShadowIncludingDescendantOf(endNode, rootNode)) {
      // Just bail out and don't try to spell-check this
      return NS_OK;
    }

    aWordUtil.SetEnd(endNode, endOffset);
    aWordUtil.SetPosition(beginNode, beginOffset);
  }

  // aWordUtil.SetPosition flushes pending notifications, check editor again.
  if (!mTextEditor) {
    return NS_ERROR_FAILURE;
  }

  int32_t wordsChecked = 0;
  PRTime beginTime = PR_Now();

  nsAutoString wordText;
  NodeOffsetRange wordNodeOffsetRange;
  bool dontCheckWord;
  while (NS_SUCCEEDED(aWordUtil.GetNextWord(wordText, &wordNodeOffsetRange,
                                            &dontCheckWord)) &&
         !wordNodeOffsetRange.Empty()) {

    // get the range for the current word.
    nsINode* beginNode = wordNodeOffsetRange.Begin().mNode;
    nsINode* endNode = wordNodeOffsetRange.End().mNode;
    int32_t beginOffset = wordNodeOffsetRange.Begin().mOffset;
    int32_t endOffset = wordNodeOffsetRange.End().mOffset;

    // see if we've done enough words in this round and run out of time.
    if (wordsChecked >= INLINESPELL_MINIMUM_WORDS_BEFORE_TIMEOUT &&
        PR_Now() > PRTime(beginTime + kMaxSpellCheckTimeInUsec)) {
      // stop checking, our time limit has been exceeded.
      #ifdef DEBUG_INLINESPELL
        printf("We have run out of the time, schedule next round.");
      #endif
      // move the range to encompass the stuff that needs checking.
      nsresult rv = aStatus->mRange->SetStart(beginNode, beginOffset);
      if (NS_FAILED(rv)) {
        // The range might be unhappy because the beginning is after the
        // end. This is possible when the requested end was in the middle
        // of a word, just ignore this situation and assume we're done.
        return NS_OK;
      }
      *aDoneChecking = false;
      return NS_OK;
    }

#ifdef DEBUG_INLINESPELL
    printf("->Got word \"%s\"", NS_ConvertUTF16toUTF8(wordText).get());
    if (dontCheckWord)
      printf(" (not checking)");
    printf("\n");
#endif

    ErrorResult erv;
    // see if there is a spellcheck range that already intersects the word
    // and remove it. We only need to remove old ranges, so don't bother if
    // there were no ranges when we started out.
    if (originalRangeCount > 0) {
      // likewise, if this word is inside new text, we won't bother testing
      if (!aStatus->mCreatedRange ||
          !aStatus->mCreatedRange->IsPointInRange(*beginNode, beginOffset, erv)) {
        nsTArray<RefPtr<nsRange>> ranges;
        aSpellCheckSelection->GetRangesForInterval(*beginNode, beginOffset,
                                                   *endNode, endOffset,
                                                   true, ranges, erv);
        ENSURE_SUCCESS(erv, erv.StealNSResult());
        for (uint32_t i = 0; i < ranges.Length(); i++)
          RemoveRange(aSpellCheckSelection, ranges[i]);
      }
    }

    // some words are special and don't need checking
    if (dontCheckWord)
      continue;

    // some nodes we don't spellcheck
    if (!ShouldSpellCheckNode(textEditor, beginNode)) {
      continue;
    }

    // Don't check spelling if we're inside the noCheckRange. This needs to
    // be done after we clear any old selection because the excluded word
    // might have been previously marked.
    //
    // We do a simple check to see if the beginning of our word is in the
    // exclusion range. Because the exclusion range is a multiple of a word,
    // this is sufficient.
    if (aStatus->mNoCheckRange &&
        aStatus->mNoCheckRange->IsPointInRange(*beginNode, beginOffset, erv)) {
      continue;
    }

    // check spelling and add to selection if misspelled
    bool isMisspelled;
    mozInlineSpellWordUtil::NormalizeWord(wordText);
    nsresult rv = mSpellCheck->CheckCurrentWordNoSuggest(wordText,
                                                         &isMisspelled);
    if (NS_FAILED(rv))
      continue;

    wordsChecked++;
    if (isMisspelled) {
      // misspelled words count extra toward the max
      RefPtr<nsRange> wordRange;
      // If we somehow can't make a range for this word, just ignore it.
      if(NS_SUCCEEDED(aWordUtil.MakeRange(wordNodeOffsetRange.Begin(),
                                          wordNodeOffsetRange.End(),
                                          getter_AddRefs(wordRange)))) {
        AddRange(aSpellCheckSelection, wordRange);
        aStatus->mWordCount++;
        if (aStatus->mWordCount >= mMaxMisspellingsPerCheck ||
            SpellCheckSelectionIsFull()) {
          break;
        }
      }
    }
  }

  return NS_OK;
}

// An RAII helper that calls ChangeNumPendingSpellChecks on destruction.
class AutoChangeNumPendingSpellChecks
{
public:
  AutoChangeNumPendingSpellChecks(mozInlineSpellChecker* aSpellChecker,
                                  int32_t aDelta)
    : mSpellChecker(aSpellChecker), mDelta(aDelta) {}

  ~AutoChangeNumPendingSpellChecks()
  {
    mSpellChecker->ChangeNumPendingSpellChecks(mDelta);
  }

private:
  RefPtr<mozInlineSpellChecker> mSpellChecker;
  int32_t mDelta;
};

// mozInlineSpellChecker::ResumeCheck
//
//    Called by the resume event when it fires. We will try to pick up where
//    the last resume left off.

nsresult
mozInlineSpellChecker::ResumeCheck(UniquePtr<mozInlineSpellStatus>&& aStatus)
{
  // Observers should be notified that spell check has ended only after spell
  // check is done below, but since there are many early returns in this method
  // and the number of pending spell checks must be decremented regardless of
  // whether the spell check actually happens, use this RAII object.
  AutoChangeNumPendingSpellChecks autoChangeNumPending(this, -1);

  if (aStatus->IsFullSpellCheck()) {
    // Allow posting new spellcheck resume events from inside
    // ResumeCheck, now that we're actually firing.
    NS_ASSERTION(mFullSpellCheckScheduled,
                 "How could this be false?  The full spell check is "
                 "calling us!!");
    mFullSpellCheckScheduled = false;
  }

  if (! mSpellCheck)
    return NS_OK; // spell checking has been turned off

  if (!mTextEditor) {
    return NS_OK;
  }

  mozInlineSpellWordUtil wordUtil;
  nsresult rv = wordUtil.Init(mTextEditor);
  if (NS_FAILED(rv))
    return NS_OK; // editor doesn't like us, don't assert

  RefPtr<Selection> spellCheckSelection = GetSpellCheckSelection();
  if (NS_WARN_IF(!spellCheckSelection)) {
    return NS_ERROR_FAILURE;
  }

  nsAutoString currentDictionary;
  rv = mSpellCheck->GetCurrentDictionary(currentDictionary);
  if (NS_FAILED(rv)) {
    // no active dictionary
    int32_t count = spellCheckSelection->RangeCount();
    for (int32_t index = count - 1; index >= 0; index--) {
      nsRange *checkRange = spellCheckSelection->GetRangeAt(index);
      if (checkRange) {
        RemoveRange(spellCheckSelection, checkRange);
      }
    }
    return NS_OK;
  }

  CleanupRangesInSelection(spellCheckSelection);

  rv = aStatus->FinishInitOnEvent(wordUtil);
  NS_ENSURE_SUCCESS(rv, rv);
  if (! aStatus->mRange)
    return NS_OK; // empty range, nothing to do

  bool doneChecking = true;
  if (aStatus->mOp == mozInlineSpellStatus::eOpSelection)
    rv = DoSpellCheckSelection(wordUtil, spellCheckSelection);
  else
    rv = DoSpellCheck(wordUtil, spellCheckSelection, aStatus, &doneChecking);
  NS_ENSURE_SUCCESS(rv, rv);

  if (! doneChecking)
    rv = ScheduleSpellCheck(std::move(aStatus));
  return rv;
}

// mozInlineSpellChecker::IsPointInSelection
//
//    Determines if a given (node,offset) point is inside the given
//    selection. If so, the specific range of the selection that
//    intersects is places in *aRange. (There may be multiple disjoint
//    ranges in a selection.)
//
//    If there is no intersection, *aRange will be nullptr.

nsresult
mozInlineSpellChecker::IsPointInSelection(Selection& aSelection,
                                          nsINode *aNode,
                                          int32_t aOffset,
                                          nsRange** aRange)
{
  *aRange = nullptr;

  nsTArray<nsRange*> ranges;
  nsresult rv = aSelection.GetRangesForIntervalArray(aNode, aOffset,
                                                     aNode, aOffset,
                                                     true, &ranges);
  NS_ENSURE_SUCCESS(rv, rv);

  if (ranges.Length() == 0)
    return NS_OK; // no matches

  // there may be more than one range returned, and we don't know what do
  // do with that, so just get the first one
  NS_ADDREF(*aRange = ranges[0]);
  return NS_OK;
}

nsresult
mozInlineSpellChecker::CleanupRangesInSelection(Selection *aSelection)
{
  // integrity check - remove ranges that have collapsed to nothing. This
  // can happen if the node containing a highlighted word was removed.
  if (!aSelection)
    return NS_ERROR_FAILURE;

  int32_t count = aSelection->RangeCount();

  for (int32_t index = 0; index < count; index++)
  {
    nsRange *checkRange = aSelection->GetRangeAt(index);
    if (checkRange)
    {
      if (checkRange->Collapsed())
      {
        RemoveRange(aSelection, checkRange);
        index--;
        count--;
      }
    }
  }

  return NS_OK;
}


// mozInlineSpellChecker::RemoveRange
//
//    For performance reasons, we have an upper bound on the number of word
//    ranges  in the spell check selection. When removing a range from the
//    selection, we need to decrement mNumWordsInSpellSelection

nsresult
mozInlineSpellChecker::RemoveRange(Selection *aSpellCheckSelection,
                                   nsRange *aRange)
{
  NS_ENSURE_ARG_POINTER(aSpellCheckSelection);
  NS_ENSURE_ARG_POINTER(aRange);

  ErrorResult rv;
  aSpellCheckSelection->RemoveRange(*aRange, rv);
  if (!rv.Failed() && mNumWordsInSpellSelection)
    mNumWordsInSpellSelection--;

  return rv.StealNSResult();
}


// mozInlineSpellChecker::AddRange
//
//    For performance reasons, we have an upper bound on the number of word
//    ranges we'll add to the spell check selection. Once we reach that upper
//    bound, stop adding the ranges

nsresult
mozInlineSpellChecker::AddRange(Selection* aSpellCheckSelection,
                                nsRange* aRange)
{
  NS_ENSURE_ARG_POINTER(aSpellCheckSelection);
  NS_ENSURE_ARG_POINTER(aRange);

  nsresult rv = NS_OK;

  if (!SpellCheckSelectionIsFull())
  {
    IgnoredErrorResult err;
    aSpellCheckSelection->AddRange(*aRange, err);
    if (err.Failed()) {
      rv = err.StealNSResult();
    } else {
      mNumWordsInSpellSelection++;
    }
  }

  return rv;
}

already_AddRefed<Selection>
mozInlineSpellChecker::GetSpellCheckSelection()
{
  if (NS_WARN_IF(!mTextEditor)) {
    return nullptr;
  }
  RefPtr<Selection> selection =
    mTextEditor->GetSelection(SelectionType::eSpellCheck);
  if (!selection) {
    return nullptr;
  }
  return selection.forget();
}

nsresult
mozInlineSpellChecker::SaveCurrentSelectionPosition()
{
  if (NS_WARN_IF(!mTextEditor)) {
    return NS_OK; // XXX Why NS_OK?
  }

  // figure out the old caret position based on the current selection
  RefPtr<Selection> selection = mTextEditor->GetSelection();
  if (NS_WARN_IF(!selection)) {
    return NS_ERROR_FAILURE;
  }

  mCurrentSelectionAnchorNode = selection->GetFocusNode();
  mCurrentSelectionOffset = selection->FocusOffset();

  return NS_OK;
}

// This is a copy of nsContentUtils::ContentIsDescendantOf. Another crime
// for XPCOM's rap sheet
bool // static
ContentIsDescendantOf(nsINode* aPossibleDescendant,
                      nsINode* aPossibleAncestor)
{
  MOZ_ASSERT(aPossibleDescendant, "The possible descendant is null!");
  MOZ_ASSERT(aPossibleAncestor, "The possible ancestor is null!");

  do {
    if (aPossibleDescendant == aPossibleAncestor)
      return true;
    aPossibleDescendant = aPossibleDescendant->GetParentNode();
  } while (aPossibleDescendant);

  return false;
}

// mozInlineSpellChecker::HandleNavigationEvent
//
//    Acts upon mouse clicks and keyboard navigation changes, spell checking
//    the previous word if the new navigation location moves us to another
//    word.
//
//    This is complicated by the fact that our mouse events are happening after
//    selection has been changed to account for the mouse click. But keyboard
//    events are happening before the caret selection has changed. Working
//    around this by letting keyboard events setting forceWordSpellCheck to
//    true. aNewPositionOffset also tries to work around this for the
//    DOM_VK_RIGHT and DOM_VK_LEFT cases.

nsresult
mozInlineSpellChecker::HandleNavigationEvent(bool aForceWordSpellCheck,
                                             int32_t aNewPositionOffset)
{
  nsresult rv;

  // If we already handled the navigation event and there is no possibility
  // anything has changed since then, we don't have to do anything. This
  // optimization makes a noticeable difference when you hold down a navigation
  // key like Page Down.
  if (! mNeedsCheckAfterNavigation)
    return NS_OK;

  nsCOMPtr<nsINode> currentAnchorNode = mCurrentSelectionAnchorNode;
  uint32_t currentAnchorOffset = mCurrentSelectionOffset;

  // now remember the new focus position resulting from the event
  rv = SaveCurrentSelectionPosition();
  NS_ENSURE_SUCCESS(rv, rv);

  bool shouldPost;
  auto status = MakeUnique<mozInlineSpellStatus>(this);
  rv = status->InitForNavigation(aForceWordSpellCheck, aNewPositionOffset,
                                 currentAnchorNode, currentAnchorOffset,
                                 mCurrentSelectionAnchorNode,
                                 mCurrentSelectionOffset,
                                 &shouldPost);
  NS_ENSURE_SUCCESS(rv, rv);
  if (shouldPost) {
    rv = ScheduleSpellCheck(std::move(status));
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return NS_OK;
}

NS_IMETHODIMP
mozInlineSpellChecker::HandleEvent(Event* aEvent)
{
  nsAutoString eventType;
  aEvent->GetType(eventType);

  if (eventType.EqualsLiteral("blur")) {
    return OnBlur(aEvent);
  }
  if (eventType.EqualsLiteral("click")) {
    return OnMouseClick(aEvent);
  }
  if (eventType.EqualsLiteral("keypress")) {
    return OnKeyPress(aEvent);
  }

  return NS_OK;
}

nsresult
mozInlineSpellChecker::OnBlur(Event* aEvent)
{
  // force spellcheck on blur, for instance when tabbing out of a textbox
  HandleNavigationEvent(true);
  return NS_OK;
}

nsresult
mozInlineSpellChecker::OnMouseClick(Event *aMouseEvent)
{
  MouseEvent* mouseEvent = aMouseEvent->AsMouseEvent();
  NS_ENSURE_TRUE(mouseEvent, NS_OK);

  // ignore any errors from HandleNavigationEvent as we don't want to prevent
  // anyone else from seeing this event.
  HandleNavigationEvent(mouseEvent->Button() != 0);
  return NS_OK;
}

nsresult
mozInlineSpellChecker::OnKeyPress(Event* aKeyEvent)
{
  RefPtr<KeyboardEvent> keyEvent = aKeyEvent->AsKeyboardEvent();
  NS_ENSURE_TRUE(keyEvent, NS_OK);

  uint32_t keyCode = keyEvent->KeyCode();

  // we only care about navigation keys that moved selection
  switch (keyCode)
  {
    case KeyboardEventBinding::DOM_VK_RIGHT:
    case KeyboardEventBinding::DOM_VK_LEFT:
      HandleNavigationEvent(false,
                            keyCode == KeyboardEventBinding::DOM_VK_RIGHT ?
                              1 : -1);
      break;
    case KeyboardEventBinding::DOM_VK_UP:
    case KeyboardEventBinding::DOM_VK_DOWN:
    case KeyboardEventBinding::DOM_VK_HOME:
    case KeyboardEventBinding::DOM_VK_END:
    case KeyboardEventBinding::DOM_VK_PAGE_UP:
    case KeyboardEventBinding::DOM_VK_PAGE_DOWN:
      HandleNavigationEvent(true /* force a spelling correction */);
      break;
  }

  return NS_OK;
}

// Used as the nsIEditorSpellCheck::UpdateCurrentDictionary callback.
class UpdateCurrentDictionaryCallback final : public nsIEditorSpellCheckCallback
{
public:
  NS_DECL_ISUPPORTS

  explicit UpdateCurrentDictionaryCallback(mozInlineSpellChecker* aSpellChecker,
                                           uint32_t aDisabledAsyncToken)
    : mSpellChecker(aSpellChecker), mDisabledAsyncToken(aDisabledAsyncToken) {}

  NS_IMETHOD EditorSpellCheckDone() override
  {
    // Ignore this callback if SetEnableRealTimeSpell(false) was called after
    // the UpdateCurrentDictionary call that triggered it.
    return mSpellChecker->mDisabledAsyncToken > mDisabledAsyncToken ?
           NS_OK :
           mSpellChecker->CurrentDictionaryUpdated();
  }

private:
  ~UpdateCurrentDictionaryCallback() {}

  RefPtr<mozInlineSpellChecker> mSpellChecker;
  uint32_t mDisabledAsyncToken;
};
NS_IMPL_ISUPPORTS(UpdateCurrentDictionaryCallback, nsIEditorSpellCheckCallback)

NS_IMETHODIMP mozInlineSpellChecker::UpdateCurrentDictionary()
{
  // mSpellCheck is null and mPendingSpellCheck is nonnull while the spell
  // checker is being initialized.  Calling UpdateCurrentDictionary on
  // mPendingSpellCheck simply queues the dictionary update after the init.
  RefPtr<EditorSpellCheck> spellCheck =
    mSpellCheck ? mSpellCheck : mPendingSpellCheck;
  if (!spellCheck) {
    return NS_OK;
  }

  RefPtr<UpdateCurrentDictionaryCallback> cb =
    new UpdateCurrentDictionaryCallback(this, mDisabledAsyncToken);
  NS_ENSURE_STATE(cb);
  nsresult rv = spellCheck->UpdateCurrentDictionary(cb);
  if (NS_FAILED(rv)) {
    cb = nullptr;
    return rv;
  }
  mNumPendingUpdateCurrentDictionary++;
  ChangeNumPendingSpellChecks(1);

  return NS_OK;
}

// Called when nsIEditorSpellCheck::UpdateCurrentDictionary completes.
nsresult mozInlineSpellChecker::CurrentDictionaryUpdated()
{
  mNumPendingUpdateCurrentDictionary--;
  NS_ASSERTION(mNumPendingUpdateCurrentDictionary >= 0,
               "CurrentDictionaryUpdated called without corresponding "
               "UpdateCurrentDictionary call!");
  ChangeNumPendingSpellChecks(-1);

  nsresult rv = SpellCheckRange(nullptr);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

NS_IMETHODIMP
mozInlineSpellChecker::GetSpellCheckPending(bool* aPending)
{
  *aPending = mNumPendingSpellChecks > 0;
  return NS_OK;
}
