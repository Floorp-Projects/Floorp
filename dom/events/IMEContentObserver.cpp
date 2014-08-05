/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 sw=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ContentEventHandler.h"
#include "IMEContentObserver.h"
#include "mozilla/AsyncEventDispatcher.h"
#include "mozilla/EventStateManager.h"
#include "mozilla/IMEStateManager.h"
#include "mozilla/TextComposition.h"
#include "mozilla/dom/Element.h"
#include "nsAutoPtr.h"
#include "nsContentUtils.h"
#include "nsGkAtoms.h"
#include "nsIAtom.h"
#include "nsIContent.h"
#include "nsIDocument.h"
#include "nsIDOMDocument.h"
#include "nsIDOMRange.h"
#include "nsIFrame.h"
#include "nsINode.h"
#include "nsIPresShell.h"
#include "nsISelectionController.h"
#include "nsISelectionPrivate.h"
#include "nsISupports.h"
#include "nsITextControlElement.h"
#include "nsIWidget.h"
#include "nsPresContext.h"
#include "nsThreadUtils.h"
#include "nsWeakReference.h"

namespace mozilla {

using namespace widget;

NS_IMPL_CYCLE_COLLECTION_CLASS(IMEContentObserver)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(IMEContentObserver)
  nsAutoScriptBlocker scriptBlocker;

  tmp->UnregisterObservers(true);

  NS_IMPL_CYCLE_COLLECTION_UNLINK(mWidget)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mSelection)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mRootContent)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mEditableNode)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mDocShell)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mEditor)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mEndOfAddedTextCache.mContainerNode)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mStartOfRemovingTextRangeCache.mContainerNode)

  tmp->mUpdatePreference.mWantUpdates = nsIMEUpdatePreference::NOTIFY_NOTHING;
  tmp->mESM = nullptr;
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(IMEContentObserver)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mWidget)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mSelection)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mRootContent)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mEditableNode)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mDocShell)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mEditor)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mEndOfAddedTextCache.mContainerNode)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(
    mStartOfRemovingTextRangeCache.mContainerNode)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(IMEContentObserver)
 NS_INTERFACE_MAP_ENTRY(nsISelectionListener)
 NS_INTERFACE_MAP_ENTRY(nsIMutationObserver)
 NS_INTERFACE_MAP_ENTRY(nsIReflowObserver)
 NS_INTERFACE_MAP_ENTRY(nsIScrollObserver)
 NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
 NS_INTERFACE_MAP_ENTRY(nsIEditorObserver)
 NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsISelectionListener)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(IMEContentObserver)
NS_IMPL_CYCLE_COLLECTING_RELEASE(IMEContentObserver)

IMEContentObserver::IMEContentObserver()
  : mESM(nullptr)
  , mPreCharacterDataChangeLength(-1)
  , mIsEditorInTransaction(false)
  , mIsSelectionChangeEventPending(false)
  , mSelectionChangeCausedOnlyByComposition(false)
  , mIsPositionChangeEventPending(false)
{
#ifdef DEBUG
  TestMergingTextChangeData();
#endif
}

void
IMEContentObserver::Init(nsIWidget* aWidget,
                         nsPresContext* aPresContext,
                         nsIContent* aContent)
{
  mESM = aPresContext->EventStateManager();
  mESM->OnStartToObserveContent(this);

  mWidget = aWidget;
  mEditableNode = IMEStateManager::GetRootEditableNode(aPresContext, aContent);
  if (!mEditableNode) {
    return;
  }

  nsCOMPtr<nsITextControlElement> textControlElement =
    do_QueryInterface(mEditableNode);
  if (textControlElement) {
    // This may fail. For example, <input type="button" contenteditable>
    mEditor = textControlElement->GetTextEditor();
    if (!mEditor && mEditableNode->IsContent()) {
      // The element must be an editing host.
      nsIContent* editingHost = mEditableNode->AsContent()->GetEditingHost();
      MOZ_ASSERT(editingHost == mEditableNode,
                 "found editing host should be mEditableNode");
      if (editingHost == mEditableNode) {
        mEditor = nsContentUtils::GetHTMLEditor(aPresContext);
      }
    }
  } else {
    mEditor = nsContentUtils::GetHTMLEditor(aPresContext);
  }
  MOZ_ASSERT(mEditor, "Failed to get editor");
  if (mEditor) {
    mEditor->AddEditorObserver(this);
  }

  nsIPresShell* presShell = aPresContext->PresShell();

  // get selection and root content
  nsCOMPtr<nsISelectionController> selCon;
  if (mEditableNode->IsNodeOfType(nsINode::eCONTENT)) {
    nsIFrame* frame =
      static_cast<nsIContent*>(mEditableNode.get())->GetPrimaryFrame();
    NS_ENSURE_TRUE_VOID(frame);

    frame->GetSelectionController(aPresContext,
                                  getter_AddRefs(selCon));
  } else {
    // mEditableNode is a document
    selCon = do_QueryInterface(presShell);
  }
  NS_ENSURE_TRUE_VOID(selCon);

  selCon->GetSelection(nsISelectionController::SELECTION_NORMAL,
                       getter_AddRefs(mSelection));
  NS_ENSURE_TRUE_VOID(mSelection);

  nsCOMPtr<nsIDOMRange> selDomRange;
  if (NS_SUCCEEDED(mSelection->GetRangeAt(0, getter_AddRefs(selDomRange)))) {
    nsRange* selRange = static_cast<nsRange*>(selDomRange.get());
    NS_ENSURE_TRUE_VOID(selRange && selRange->GetStartParent());

    mRootContent = selRange->GetStartParent()->
                     GetSelectionRootContent(presShell);
  } else {
    mRootContent = mEditableNode->GetSelectionRootContent(presShell);
  }
  if (!mRootContent && mEditableNode->IsNodeOfType(nsINode::eDOCUMENT)) {
    // The document node is editable, but there are no contents, this document
    // is not editable.
    return;
  }
  NS_ENSURE_TRUE_VOID(mRootContent);

  if (IMEStateManager::IsTestingIME()) {
    nsIDocument* doc = aPresContext->Document();
    (new AsyncEventDispatcher(doc, NS_LITERAL_STRING("MozIMEFocusIn"),
                              false, false))->RunDOMEventWhenSafe();
  }

  aWidget->NotifyIME(IMENotification(NOTIFY_IME_OF_FOCUS));

  // NOTIFY_IME_OF_FOCUS might cause recreating IMEContentObserver
  // instance via IMEStateManager::UpdateIMEState().  So, this
  // instance might already have been destroyed, check it.
  if (!mRootContent) {
    return;
  }

  mDocShell = aPresContext->GetDocShell();

  ObserveEditableNode();
}

void
IMEContentObserver::ObserveEditableNode()
{
  MOZ_ASSERT(mSelection);
  MOZ_ASSERT(mRootContent);

  mUpdatePreference = mWidget->GetIMEUpdatePreference();
  if (mUpdatePreference.WantSelectionChange()) {
    // add selection change listener
    nsCOMPtr<nsISelectionPrivate> selPrivate(do_QueryInterface(mSelection));
    NS_ENSURE_TRUE_VOID(selPrivate);
    nsresult rv = selPrivate->AddSelectionListener(this);
    NS_ENSURE_SUCCESS_VOID(rv);
  }

  if (mUpdatePreference.WantTextChange()) {
    // add text change observer
    mRootContent->AddMutationObserver(this);
  }

  if (mUpdatePreference.WantPositionChanged() && mDocShell) {
    // Add scroll position listener and reflow observer to detect position and
    // size changes
    mDocShell->AddWeakScrollObserver(this);
    mDocShell->AddWeakReflowObserver(this);
  }
}

void
IMEContentObserver::UnregisterObservers(bool aPostEvent)
{
  if (mEditor) {
    mEditor->RemoveEditorObserver(this);
  }

  // If CreateTextStateManager failed, mRootContent will be null, then, we
  // should not call NotifyIME(IMENotification(NOTIFY_IME_OF_BLUR))
  if (mRootContent && mWidget) {
    if (IMEStateManager::IsTestingIME() && mEditableNode) {
      nsIDocument* doc = mEditableNode->OwnerDoc();
      if (doc) {
        nsRefPtr<AsyncEventDispatcher> dispatcher =
          new AsyncEventDispatcher(doc, NS_LITERAL_STRING("MozIMEFocusOut"),
                                   false, false);
        if (aPostEvent) {
          dispatcher->PostDOMEvent();
        } else {
          dispatcher->RunDOMEventWhenSafe();
        }
      }
    }
    // A test event handler might destroy the widget.
    if (mWidget) {
      mWidget->NotifyIME(IMENotification(NOTIFY_IME_OF_BLUR));
    }
  }

  if (mUpdatePreference.WantSelectionChange() && mSelection) {
    nsCOMPtr<nsISelectionPrivate> selPrivate(do_QueryInterface(mSelection));
    if (selPrivate) {
      selPrivate->RemoveSelectionListener(this);
    }
  }

  if (mUpdatePreference.WantTextChange() && mRootContent) {
    mRootContent->RemoveMutationObserver(this);
  }

  if (mUpdatePreference.WantPositionChanged() && mDocShell) {
    mDocShell->RemoveWeakScrollObserver(this);
    mDocShell->RemoveWeakReflowObserver(this);
  }
}

void
IMEContentObserver::Destroy()
{
  // WARNING: When you change this method, you have to check Unlink() too.

  UnregisterObservers(false);

  mEditor = nullptr;
  // Even if there are some pending notification, it'll never notify the widget.
  mWidget = nullptr;
  mSelection = nullptr;
  mRootContent = nullptr;
  mEditableNode = nullptr;
  mDocShell = nullptr;
  mUpdatePreference.mWantUpdates = nsIMEUpdatePreference::NOTIFY_NOTHING;

  if (mESM) {
    mESM->OnStopObservingContent(this);
    mESM = nullptr;
  }
}

void
IMEContentObserver::DisconnectFromEventStateManager()
{
  mESM = nullptr;
}

bool
IMEContentObserver::IsManaging(nsPresContext* aPresContext,
                               nsIContent* aContent)
{
  if (!mSelection || !mRootContent || !mEditableNode) {
    return false; // failed to initialize.
  }
  if (!mRootContent->IsInComposedDoc()) {
    return false; // the focused editor has already been reframed.
  }
  return mEditableNode == IMEStateManager::GetRootEditableNode(aPresContext,
                                                               aContent);
}

bool
IMEContentObserver::IsEditorHandlingEventForComposition() const
{
  if (!mWidget) {
    return false;
  }
  nsRefPtr<TextComposition> composition =
    IMEStateManager::GetTextCompositionFor(mWidget);
  if (!composition) {
    return false;
  }
  return composition->IsEditorHandlingEvent();
}

nsresult
IMEContentObserver::GetSelectionAndRoot(nsISelection** aSelection,
                                        nsIContent** aRootContent) const
{
  if (!mEditableNode || !mSelection) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  NS_ASSERTION(mSelection && mRootContent, "uninitialized content observer");
  NS_ADDREF(*aSelection = mSelection);
  NS_ADDREF(*aRootContent = mRootContent);
  return NS_OK;
}

// Helper class, used for selection change notification
class SelectionChangeEvent : public nsRunnable
{
public:
  SelectionChangeEvent(IMEContentObserver* aDispatcher,
                       bool aCausedByComposition)
    : mDispatcher(aDispatcher)
    , mCausedByComposition(aCausedByComposition)
  {
    MOZ_ASSERT(mDispatcher);
  }

  NS_IMETHOD Run()
  {
    if (mDispatcher->GetWidget()) {
      IMENotification notification(NOTIFY_IME_OF_SELECTION_CHANGE);
      notification.mSelectionChangeData.mCausedByComposition =
         mCausedByComposition;
      mDispatcher->GetWidget()->NotifyIME(notification);
    }
    return NS_OK;
  }

private:
  nsRefPtr<IMEContentObserver> mDispatcher;
  bool mCausedByComposition;
};

nsresult
IMEContentObserver::NotifySelectionChanged(nsIDOMDocument* aDOMDocument,
                                           nsISelection* aSelection,
                                           int16_t aReason)
{
  bool causedByComposition = IsEditorHandlingEventForComposition();
  if (causedByComposition &&
      !mUpdatePreference.WantChangesCausedByComposition()) {
    return NS_OK;
  }

  int32_t count = 0;
  nsresult rv = aSelection->GetRangeCount(&count);
  NS_ENSURE_SUCCESS(rv, rv);
  if (count > 0 && mWidget) {
    MaybeNotifyIMEOfSelectionChange(causedByComposition);
  }
  return NS_OK;
}

// Helper class, used for position change notification
class PositionChangeEvent MOZ_FINAL : public nsRunnable
{
public:
  PositionChangeEvent(IMEContentObserver* aDispatcher)
    : mDispatcher(aDispatcher)
  {
    MOZ_ASSERT(mDispatcher);
  }

  NS_IMETHOD Run()
  {
    if (mDispatcher->GetWidget()) {
      mDispatcher->GetWidget()->NotifyIME(
        IMENotification(NOTIFY_IME_OF_POSITION_CHANGE));
    }
    return NS_OK;
  }

private:
  nsRefPtr<IMEContentObserver> mDispatcher;
};

void
IMEContentObserver::ScrollPositionChanged()
{
  MaybeNotifyIMEOfPositionChange();
}

NS_IMETHODIMP
IMEContentObserver::Reflow(DOMHighResTimeStamp aStart,
                           DOMHighResTimeStamp aEnd)
{
  MaybeNotifyIMEOfPositionChange();
  return NS_OK;
}

NS_IMETHODIMP
IMEContentObserver::ReflowInterruptible(DOMHighResTimeStamp aStart,
                                        DOMHighResTimeStamp aEnd)
{
  MaybeNotifyIMEOfPositionChange();
  return NS_OK;
}

// Helper class, used for text change notification
class TextChangeEvent : public nsRunnable
{
public:
  TextChangeEvent(IMEContentObserver* aDispatcher,
                  const IMEContentObserver::TextChangeData& aData)
    : mDispatcher(aDispatcher)
    , mData(aData)
  {
    MOZ_ASSERT(mDispatcher);
    MOZ_ASSERT(mData.mStored);
  }

  NS_IMETHOD Run()
  {
    if (mDispatcher->GetWidget()) {
      IMENotification notification(NOTIFY_IME_OF_TEXT_CHANGE);
      notification.mTextChangeData.mStartOffset = mData.mStartOffset;
      notification.mTextChangeData.mOldEndOffset = mData.mRemovedEndOffset;
      notification.mTextChangeData.mNewEndOffset = mData.mAddedEndOffset;
      notification.mTextChangeData.mCausedByComposition =
        mData.mCausedOnlyByComposition;
      mDispatcher->GetWidget()->NotifyIME(notification);
    }
    return NS_OK;
  }

private:
  nsRefPtr<IMEContentObserver> mDispatcher;
  IMEContentObserver::TextChangeData mData;
};

void
IMEContentObserver::StoreTextChangeData(const TextChangeData& aTextChangeData)
{
  MOZ_ASSERT(aTextChangeData.mStartOffset <= aTextChangeData.mRemovedEndOffset,
             "end of removed text must be same or larger than start");
  MOZ_ASSERT(aTextChangeData.mStartOffset <= aTextChangeData.mAddedEndOffset,
             "end of added text must be same or larger than start");

  if (!mTextChangeData.mStored) {
    mTextChangeData = aTextChangeData;
    MOZ_ASSERT(mTextChangeData.mStored, "Why mStored is false?");
    return;
  }

  // |mTextChangeData| should represent all modified text ranges and all
  // inserted text ranges.
  // |mStartOffset| and |mRemovedEndOffset| represent all replaced or removed
  // text ranges.  I.e., mStartOffset should be the smallest offset of all
  // modified text ranges in old text.  |mRemovedEndOffset| should be the
  // largest end offset in old text of all modified text ranges.
  // |mAddedEndOffset| represents the end offset of all inserted text ranges.
  // I.e., only this is an offset in new text.
  // In other words, between mStartOffset and |mRemovedEndOffset| of the
  // premodified text was already removed.  And some text whose length is
  // |mAddedEndOffset - mStartOffset| is inserted to |mStartOffset|.  I.e.,
  // this allows IME to mark dirty the modified text range with |mStartOffset|
  // and |mRemovedEndOffset| if IME stores all text of the focused editor and
  // to compute new text length with |mAddedEndOffset| and |mRemovedEndOffset|.
  // Additionally, IME can retrieve only the text between |mStartOffset| and
  // |mAddedEndOffset| for updating stored text.

  // For comparing new and old |mStartOffset|/|mRemovedEndOffset| values, they
  // should be adjusted to be in same text. The |newData.mStartOffset| and
  // |newData.mRemovedEndOffset| should be computed as in old text because
  // |mStartOffset| and |mRemovedEndOffset| represent the modified text range
  // in the old text but even if some text before the values of the newData
  // has already been modified, the values don't include the changes.

  // For comparing new and old |mAddedEndOffset| values, they should be
  // adjusted to be in same text.  The |oldData.mAddedEndOffset| should be
  // computed as in the new text because |mAddedEndOffset| indicates the end
  // offset of inserted text in the new text but |oldData.mAddedEndOffset|
  // doesn't include any changes of the text before |newData.mAddedEndOffset|.

  const TextChangeData& newData = aTextChangeData;
  const TextChangeData oldData = mTextChangeData;

  mTextChangeData.mCausedOnlyByComposition =
    newData.mCausedOnlyByComposition && oldData.mCausedOnlyByComposition;

  if (newData.mStartOffset >= oldData.mAddedEndOffset) {
    // Case 1:
    // If new start is after old end offset of added text, it means that text
    // after the modified range is modified.  Like:
    // added range of old change:             +----------+
    // removed range of new change:                           +----------+
    // So, the old start offset is always the smaller offset.
    mTextChangeData.mStartOffset = oldData.mStartOffset;
    // The new end offset of removed text is moved by the old change and we
    // need to cancel the move of the old change for comparing the offsets in
    // same text because it doesn't make sensce to compare offsets in different
    // text.
    uint32_t newRemovedEndOffsetInOldText =
      newData.mRemovedEndOffset - oldData.Difference();
    mTextChangeData.mRemovedEndOffset =
      std::max(newRemovedEndOffsetInOldText, oldData.mRemovedEndOffset);
    // The new end offset of added text is always the larger offset.
    mTextChangeData.mAddedEndOffset = newData.mAddedEndOffset;
    return;
  }

  if (newData.mStartOffset >= oldData.mStartOffset) {
    // If new start is in the modified range, it means that new data changes
    // a part or all of the range.
    mTextChangeData.mStartOffset = oldData.mStartOffset;
    if (newData.mRemovedEndOffset >= oldData.mAddedEndOffset) {
      // Case 2:
      // If new end of removed text is greater than old end of added text, it
      // means that all or a part of modified range modified again and text
      // after the modified range is also modified.  Like:
      // added range of old change:             +----------+
      // removed range of new change:                   +----------+
      // So, the new removed end offset is moved by the old change and we need
      // to cancel the move of the old change for comparing the offsets in the
      // same text because it doesn't make sense to compare the offsets in
      // different text.
      uint32_t newRemovedEndOffsetInOldText =
        newData.mRemovedEndOffset - oldData.Difference();
      mTextChangeData.mRemovedEndOffset =
        std::max(newRemovedEndOffsetInOldText, oldData.mRemovedEndOffset);
      // The old end of added text is replaced by new change. So, it should be
      // same as the new start.  On the other hand, the new added end offset is
      // always same or larger.  Therefore, the merged end offset of added
      // text should be the new end offset of added text.
      mTextChangeData.mAddedEndOffset = newData.mAddedEndOffset;
      return;
    }

    // Case 3:
    // If new end of removed text is less than old end of added text, it means
    // that only a part of the modified range is modified again.  Like:
    // added range of old change:             +------------+
    // removed range of new change:               +-----+
    // So, the new end offset of removed text should be same as the old end
    // offset of removed text.  Therefore, the merged end offset of removed
    // text should be the old text change's |mRemovedEndOffset|.
    mTextChangeData.mRemovedEndOffset = oldData.mRemovedEndOffset;
    // The old end of added text is moved by new change.  So, we need to cancel
    // the move of the new change for comparing the offsets in same text.
    uint32_t oldAddedEndOffsetInNewText =
      oldData.mAddedEndOffset + newData.Difference();
    mTextChangeData.mAddedEndOffset =
      std::max(newData.mAddedEndOffset, oldAddedEndOffsetInNewText);
    return;
  }

  if (newData.mRemovedEndOffset >= oldData.mStartOffset) {
    // If new end of removed text is greater than old start (and new start is
    // less than old start), it means that a part of modified range is modified
    // again and some new text before the modified range is also modified.
    MOZ_ASSERT(newData.mStartOffset < oldData.mStartOffset,
      "new start offset should be less than old one here");
    mTextChangeData.mStartOffset = newData.mStartOffset;
    if (newData.mRemovedEndOffset >= oldData.mAddedEndOffset) {
      // Case 4:
      // If new end of removed text is greater than old end of added text, it
      // means that all modified text and text after the modified range is
      // modified.  Like:
      // added range of old change:             +----------+
      // removed range of new change:        +------------------+
      // So, the new end of removed text is moved by the old change.  Therefore,
      // we need to cancel the move of the old change for comparing the offsets
      // in same text because it doesn't make sense to compare the offsets in
      // different text.
      uint32_t newRemovedEndOffsetInOldText =
        newData.mRemovedEndOffset - oldData.Difference();
      mTextChangeData.mRemovedEndOffset =
        std::max(newRemovedEndOffsetInOldText, oldData.mRemovedEndOffset);
      // The old end of added text is replaced by new change.  So, the old end
      // offset of added text is same as new text change's start offset.  Then,
      // new change's end offset of added text is always same or larger than
      // it.  Therefore, merged end offset of added text is always the new end
      // offset of added text.
      mTextChangeData.mAddedEndOffset = newData.mAddedEndOffset;
      return;
    }

    // Case 5:
    // If new end of removed text is less than old end of added text, it
    // means that only a part of the modified range is modified again.  Like:
    // added range of old change:             +----------+
    // removed range of new change:      +----------+
    // So, the new end of removed text should be same as old end of removed
    // text for preventing end of removed text to be modified.  Therefore,
    // merged end offset of removed text is always the old end offset of removed
    // text.
    mTextChangeData.mRemovedEndOffset = oldData.mRemovedEndOffset;
    // The old end of added text is moved by this change.  So, we need to
    // cancel the move of the new change for comparing the offsets in same text
    // because it doesn't make sense to compare the offsets in different text.
    uint32_t oldAddedEndOffsetInNewText =
      oldData.mAddedEndOffset + newData.Difference();
    mTextChangeData.mAddedEndOffset =
      std::max(newData.mAddedEndOffset, oldAddedEndOffsetInNewText);
    return;
  }

  // Case 6:
  // Otherwise, i.e., both new end of added text and new start are less than
  // old start, text before the modified range is modified.  Like:
  // added range of old change:                  +----------+
  // removed range of new change: +----------+
  MOZ_ASSERT(newData.mStartOffset < oldData.mStartOffset,
    "new start offset should be less than old one here");
  mTextChangeData.mStartOffset = newData.mStartOffset;
  MOZ_ASSERT(newData.mRemovedEndOffset < oldData.mRemovedEndOffset,
     "new removed end offset should be less than old one here");
  mTextChangeData.mRemovedEndOffset = oldData.mRemovedEndOffset;
  // The end of added text should be adjusted with the new difference.
  uint32_t oldAddedEndOffsetInNewText =
    oldData.mAddedEndOffset + newData.Difference();
  mTextChangeData.mAddedEndOffset =
    std::max(newData.mAddedEndOffset, oldAddedEndOffsetInNewText);
}

void
IMEContentObserver::CharacterDataWillChange(nsIDocument* aDocument,
                                            nsIContent* aContent,
                                            CharacterDataChangeInfo* aInfo)
{
  NS_ASSERTION(aContent->IsNodeOfType(nsINode::eTEXT),
               "character data changed for non-text node");
  MOZ_ASSERT(mPreCharacterDataChangeLength < 0,
             "CharacterDataChanged() should've reset "
             "mPreCharacterDataChangeLength");

  mEndOfAddedTextCache.Clear();
  mStartOfRemovingTextRangeCache.Clear();

  bool causedByComposition = IsEditorHandlingEventForComposition();
  if (!mTextChangeData.mStored && causedByComposition &&
      !mUpdatePreference.WantChangesCausedByComposition()) {
    return;
  }

  mPreCharacterDataChangeLength =
    ContentEventHandler::GetNativeTextLength(aContent, aInfo->mChangeStart,
                                             aInfo->mChangeEnd);
  MOZ_ASSERT(mPreCharacterDataChangeLength >=
               aInfo->mChangeEnd - aInfo->mChangeStart,
             "The computed length must be same as or larger than XP length");
}

void
IMEContentObserver::CharacterDataChanged(nsIDocument* aDocument,
                                         nsIContent* aContent,
                                         CharacterDataChangeInfo* aInfo)
{
  NS_ASSERTION(aContent->IsNodeOfType(nsINode::eTEXT),
               "character data changed for non-text node");

  mEndOfAddedTextCache.Clear();
  mStartOfRemovingTextRangeCache.Clear();

  int64_t removedLength = mPreCharacterDataChangeLength;
  mPreCharacterDataChangeLength = -1;

  bool causedByComposition = IsEditorHandlingEventForComposition();
  if (!mTextChangeData.mStored && causedByComposition &&
      !mUpdatePreference.WantChangesCausedByComposition()) {
    return;
  }

  MOZ_ASSERT(removedLength >= 0,
             "mPreCharacterDataChangeLength should've been set by "
             "CharacterDataWillChange()");

  uint32_t offset = 0;
  // get offsets of change and fire notification
  nsresult rv =
    ContentEventHandler::GetFlatTextOffsetOfRange(mRootContent, aContent,
                                                  aInfo->mChangeStart,
                                                  &offset,
                                                  LINE_BREAK_TYPE_NATIVE);
  NS_ENSURE_SUCCESS_VOID(rv);

  uint32_t newLength =
    ContentEventHandler::GetNativeTextLength(aContent, aInfo->mChangeStart,
                                             aInfo->mChangeStart +
                                               aInfo->mReplaceLength);

  uint32_t oldEnd = offset + static_cast<uint32_t>(removedLength);
  uint32_t newEnd = offset + newLength;

  TextChangeData data(offset, oldEnd, newEnd, causedByComposition);
  MaybeNotifyIMEOfTextChange(data);
}

void
IMEContentObserver::NotifyContentAdded(nsINode* aContainer,
                                       int32_t aStartIndex,
                                       int32_t aEndIndex)
{
  mStartOfRemovingTextRangeCache.Clear();

  bool causedByComposition = IsEditorHandlingEventForComposition();
  if (!mTextChangeData.mStored && causedByComposition &&
      !mUpdatePreference.WantChangesCausedByComposition()) {
    return;
  }

  uint32_t offset = 0;
  nsresult rv = NS_OK;
  if (!mEndOfAddedTextCache.Match(aContainer, aStartIndex)) {
    mEndOfAddedTextCache.Clear();
    rv = ContentEventHandler::GetFlatTextOffsetOfRange(mRootContent, aContainer,
                                                       aStartIndex, &offset,
                                                       LINE_BREAK_TYPE_NATIVE);
    if (NS_WARN_IF(NS_FAILED((rv)))) {
      return;
    }
  } else {
    offset = mEndOfAddedTextCache.mFlatTextLength;
  }

  // get offset at the end of the last added node
  nsIContent* childAtStart = aContainer->GetChildAt(aStartIndex);
  uint32_t addingLength = 0;
  rv = ContentEventHandler::GetFlatTextOffsetOfRange(childAtStart, aContainer,
                                                     aEndIndex, &addingLength,
                                                     LINE_BREAK_TYPE_NATIVE);
  if (NS_WARN_IF(NS_FAILED((rv)))) {
    mEndOfAddedTextCache.Clear();
    return;
  }

  // If multiple lines are being inserted in an HTML editor, next call of
  // NotifyContentAdded() is for adding next node.  Therefore, caching the text
  // length can skip to compute the text length before the adding node and
  // before of it.
  mEndOfAddedTextCache.Cache(aContainer, aEndIndex, offset + addingLength);

  if (!addingLength) {
    return;
  }

  TextChangeData data(offset, offset, offset + addingLength,
                      causedByComposition);
  MaybeNotifyIMEOfTextChange(data);
}

void
IMEContentObserver::ContentAppended(nsIDocument* aDocument,
                                    nsIContent* aContainer,
                                    nsIContent* aFirstNewContent,
                                    int32_t aNewIndexInContainer)
{
  NotifyContentAdded(aContainer, aNewIndexInContainer,
                     aContainer->GetChildCount());
}

void
IMEContentObserver::ContentInserted(nsIDocument* aDocument,
                                    nsIContent* aContainer,
                                    nsIContent* aChild,
                                    int32_t aIndexInContainer)
{
  NotifyContentAdded(NODE_FROM(aContainer, aDocument),
                     aIndexInContainer, aIndexInContainer + 1);
}

void
IMEContentObserver::ContentRemoved(nsIDocument* aDocument,
                                   nsIContent* aContainer,
                                   nsIContent* aChild,
                                   int32_t aIndexInContainer,
                                   nsIContent* aPreviousSibling)
{
  mEndOfAddedTextCache.Clear();

  bool causedByComposition = IsEditorHandlingEventForComposition();
  if (!mTextChangeData.mStored && causedByComposition &&
      !mUpdatePreference.WantChangesCausedByComposition()) {
    return;
  }

  nsINode* containerNode = NODE_FROM(aContainer, aDocument);

  uint32_t offset = 0;
  nsresult rv = NS_OK;
  if (!mStartOfRemovingTextRangeCache.Match(containerNode, aIndexInContainer)) {
    rv =
      ContentEventHandler::GetFlatTextOffsetOfRange(mRootContent, containerNode,
                                                    aIndexInContainer, &offset,
                                                    LINE_BREAK_TYPE_NATIVE);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      mStartOfRemovingTextRangeCache.Clear();
      return;
    }
    mStartOfRemovingTextRangeCache.Cache(containerNode, aIndexInContainer,
                                         offset);
  } else {
    offset = mStartOfRemovingTextRangeCache.mFlatTextLength;
  }

  // get offset at the end of the deleted node
  int32_t nodeLength =
    aChild->IsNodeOfType(nsINode::eTEXT) ?
      static_cast<int32_t>(aChild->TextLength()) :
      std::max(static_cast<int32_t>(aChild->GetChildCount()), 1);
  MOZ_ASSERT(nodeLength >= 0, "The node length is out of range");
  uint32_t textLength = 0;
  rv = ContentEventHandler::GetFlatTextOffsetOfRange(aChild, aChild,
                                                     nodeLength, &textLength,
                                                     LINE_BREAK_TYPE_NATIVE);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    mStartOfRemovingTextRangeCache.Clear();
    return;
  }

  if (!textLength) {
    return;
  }

  TextChangeData data(offset, offset + textLength, offset, causedByComposition);
  MaybeNotifyIMEOfTextChange(data);
}

static nsIContent*
GetContentBR(dom::Element* aElement)
{
  if (!aElement->IsNodeOfType(nsINode::eCONTENT)) {
    return nullptr;
  }
  nsIContent* content = static_cast<nsIContent*>(aElement);
  return content->IsHTML(nsGkAtoms::br) ? content : nullptr;
}

void
IMEContentObserver::AttributeWillChange(nsIDocument* aDocument,
                                        dom::Element* aElement,
                                        int32_t aNameSpaceID,
                                        nsIAtom* aAttribute,
                                        int32_t aModType)
{
  nsIContent *content = GetContentBR(aElement);
  mPreAttrChangeLength = content ?
    ContentEventHandler::GetNativeTextLength(content) : 0;
}

void
IMEContentObserver::AttributeChanged(nsIDocument* aDocument,
                                     dom::Element* aElement,
                                     int32_t aNameSpaceID,
                                     nsIAtom* aAttribute,
                                     int32_t aModType)
{
  mEndOfAddedTextCache.Clear();
  mStartOfRemovingTextRangeCache.Clear();

  bool causedByComposition = IsEditorHandlingEventForComposition();
  if (!mTextChangeData.mStored && causedByComposition &&
      !mUpdatePreference.WantChangesCausedByComposition()) {
    return;
  }

  nsIContent *content = GetContentBR(aElement);
  if (!content) {
    return;
  }

  uint32_t postAttrChangeLength =
    ContentEventHandler::GetNativeTextLength(content);
  if (postAttrChangeLength == mPreAttrChangeLength) {
    return;
  }
  uint32_t start;
  nsresult rv =
    ContentEventHandler::GetFlatTextOffsetOfRange(mRootContent, content,
                                                  0, &start,
                                                  LINE_BREAK_TYPE_NATIVE);
  NS_ENSURE_SUCCESS_VOID(rv);

  TextChangeData data(start, start + mPreAttrChangeLength,
                      start + postAttrChangeLength, causedByComposition);
  MaybeNotifyIMEOfTextChange(data);
}

NS_IMETHODIMP
IMEContentObserver::EditAction()
{
  mIsEditorInTransaction = false;
  mEndOfAddedTextCache.Clear();
  mStartOfRemovingTextRangeCache.Clear();
  FlushMergeableNotifications();
  return NS_OK;
}

NS_IMETHODIMP
IMEContentObserver::BeforeEditAction()
{
  mIsEditorInTransaction = true;
  mEndOfAddedTextCache.Clear();
  mStartOfRemovingTextRangeCache.Clear();
  return NS_OK;
}

NS_IMETHODIMP
IMEContentObserver::CancelEditAction()
{
  mIsEditorInTransaction = false;
  mEndOfAddedTextCache.Clear();
  mStartOfRemovingTextRangeCache.Clear();
  FlushMergeableNotifications();
  return NS_OK;
}

void
IMEContentObserver::MaybeNotifyIMEOfTextChange(const TextChangeData& aData)
{
  StoreTextChangeData(aData);
  MOZ_ASSERT(mTextChangeData.mStored,
             "mTextChangeData must have text change data");
  FlushMergeableNotifications();
}

void
IMEContentObserver::MaybeNotifyIMEOfSelectionChange(bool aCausedByComposition)
{
  if (!mIsSelectionChangeEventPending) {
    mSelectionChangeCausedOnlyByComposition = aCausedByComposition;
  } else {
    mSelectionChangeCausedOnlyByComposition =
      mSelectionChangeCausedOnlyByComposition && aCausedByComposition;
  }
  mIsSelectionChangeEventPending = true;
  FlushMergeableNotifications();
}

void
IMEContentObserver::MaybeNotifyIMEOfPositionChange()
{
  mIsPositionChangeEventPending = true;
  FlushMergeableNotifications();
}

void
IMEContentObserver::FlushMergeableNotifications()
{
  if (mIsEditorInTransaction || !mWidget) {
    return;
  }

  if (mTextChangeData.mStored) {
    nsContentUtils::AddScriptRunner(new TextChangeEvent(this, mTextChangeData));
    mTextChangeData.mStored = false;
  }

  if (mIsSelectionChangeEventPending) {
    nsContentUtils::AddScriptRunner(
      new SelectionChangeEvent(this, mSelectionChangeCausedOnlyByComposition));
    mIsSelectionChangeEventPending = false;
  }

  if (mIsPositionChangeEventPending) {
    nsContentUtils::AddScriptRunner(new PositionChangeEvent(this));
    mIsPositionChangeEventPending = false;
  }
}

#ifdef DEBUG
// Let's test the code of merging multiple text change data in debug build
// and crash if one of them fails because this feature is very complex but
// cannot be tested with mochitest.
void
IMEContentObserver::TestMergingTextChangeData()
{
  static bool gTestTextChangeEvent = true;
  if (!gTestTextChangeEvent) {
    return;
  }
  gTestTextChangeEvent = false;

  /****************************************************************************
   * Case 1
   ****************************************************************************/

  // Appending text
  StoreTextChangeData(TextChangeData(10, 10, 20, false));
  StoreTextChangeData(TextChangeData(20, 20, 35, false));
  MOZ_ASSERT(mTextChangeData.mStartOffset == 10,
    "Test 1-1-1: mStartOffset should be the first offset");
  MOZ_ASSERT(mTextChangeData.mRemovedEndOffset == 10, // 20 - (20 - 10)
    "Test 1-1-2: mRemovedEndOffset should be the first end of removed text");
  MOZ_ASSERT(mTextChangeData.mAddedEndOffset == 35,
    "Test 1-1-3: mAddedEndOffset should be the last end of added text");
  mTextChangeData.mStored = false;

  // Removing text (longer line -> shorter line)
  StoreTextChangeData(TextChangeData(10, 20, 10, false));
  StoreTextChangeData(TextChangeData(10, 30, 10, false));
  MOZ_ASSERT(mTextChangeData.mStartOffset == 10,
    "Test 1-2-1: mStartOffset should be the first offset");
  MOZ_ASSERT(mTextChangeData.mRemovedEndOffset == 40, // 30 + (10 - 20)
    "Test 1-2-2: mRemovedEndOffset should be the the last end of removed text "
    "with already removed length");
  MOZ_ASSERT(mTextChangeData.mAddedEndOffset == 10,
    "Test 1-2-3: mAddedEndOffset should be the last end of added text");
  mTextChangeData.mStored = false;

  // Removing text (shorter line -> longer line)
  StoreTextChangeData(TextChangeData(10, 20, 10, false));
  StoreTextChangeData(TextChangeData(10, 15, 10, false));
  MOZ_ASSERT(mTextChangeData.mStartOffset == 10,
    "Test 1-3-1: mStartOffset should be the first offset");
  MOZ_ASSERT(mTextChangeData.mRemovedEndOffset == 25, // 15 + (10 - 20)
    "Test 1-3-2: mRemovedEndOffset should be the the last end of removed text "
    "with already removed length");
  MOZ_ASSERT(mTextChangeData.mAddedEndOffset == 10,
    "Test 1-3-3: mAddedEndOffset should be the last end of added text");
  mTextChangeData.mStored = false;

  // Appending text at different point (not sure if actually occurs)
  StoreTextChangeData(TextChangeData(10, 10, 20, false));
  StoreTextChangeData(TextChangeData(55, 55, 60, false));
  MOZ_ASSERT(mTextChangeData.mStartOffset == 10,
    "Test 1-4-1: mStartOffset should be the smallest offset");
  MOZ_ASSERT(mTextChangeData.mRemovedEndOffset == 45, // 55 - (10 - 20)
    "Test 1-4-2: mRemovedEndOffset should be the the largest end of removed "
    "text without already added length");
  MOZ_ASSERT(mTextChangeData.mAddedEndOffset == 60,
    "Test 1-4-3: mAddedEndOffset should be the last end of added text");
  mTextChangeData.mStored = false;

  // Removing text at different point (not sure if actually occurs)
  StoreTextChangeData(TextChangeData(10, 20, 10, false));
  StoreTextChangeData(TextChangeData(55, 68, 55, false));
  MOZ_ASSERT(mTextChangeData.mStartOffset == 10,
    "Test 1-5-1: mStartOffset should be the smallest offset");
  MOZ_ASSERT(mTextChangeData.mRemovedEndOffset == 78, // 68 - (10 - 20)
    "Test 1-5-2: mRemovedEndOffset should be the the largest end of removed "
    "text with already removed length");
  MOZ_ASSERT(mTextChangeData.mAddedEndOffset == 55,
    "Test 1-5-3: mAddedEndOffset should be the largest end of added text");
  mTextChangeData.mStored = false;

  // Replacing text and append text (becomes longer)
  StoreTextChangeData(TextChangeData(30, 35, 32, false));
  StoreTextChangeData(TextChangeData(32, 32, 40, false));
  MOZ_ASSERT(mTextChangeData.mStartOffset == 30,
    "Test 1-6-1: mStartOffset should be the smallest offset");
  MOZ_ASSERT(mTextChangeData.mRemovedEndOffset == 35, // 32 - (32 - 35)
    "Test 1-6-2: mRemovedEndOffset should be the the first end of removed "
    "text");
  MOZ_ASSERT(mTextChangeData.mAddedEndOffset == 40,
    "Test 1-6-3: mAddedEndOffset should be the last end of added text");
  mTextChangeData.mStored = false;

  // Replacing text and append text (becomes shorter)
  StoreTextChangeData(TextChangeData(30, 35, 32, false));
  StoreTextChangeData(TextChangeData(32, 32, 33, false));
  MOZ_ASSERT(mTextChangeData.mStartOffset == 30,
    "Test 1-7-1: mStartOffset should be the smallest offset");
  MOZ_ASSERT(mTextChangeData.mRemovedEndOffset == 35, // 32 - (32 - 35)
    "Test 1-7-2: mRemovedEndOffset should be the the first end of removed "
    "text");
  MOZ_ASSERT(mTextChangeData.mAddedEndOffset == 33,
    "Test 1-7-3: mAddedEndOffset should be the last end of added text");
  mTextChangeData.mStored = false;

  // Removing text and replacing text after first range (not sure if actually
  // occurs)
  StoreTextChangeData(TextChangeData(30, 35, 30, false));
  StoreTextChangeData(TextChangeData(32, 34, 48, false));
  MOZ_ASSERT(mTextChangeData.mStartOffset == 30,
    "Test 1-8-1: mStartOffset should be the smallest offset");
  MOZ_ASSERT(mTextChangeData.mRemovedEndOffset == 39, // 34 - (30 - 35)
    "Test 1-8-2: mRemovedEndOffset should be the the first end of removed text "
    "without already removed text");
  MOZ_ASSERT(mTextChangeData.mAddedEndOffset == 48,
    "Test 1-8-3: mAddedEndOffset should be the last end of added text");
  mTextChangeData.mStored = false;

  // Removing text and replacing text after first range (not sure if actually
  // occurs)
  StoreTextChangeData(TextChangeData(30, 35, 30, false));
  StoreTextChangeData(TextChangeData(32, 38, 36, false));
  MOZ_ASSERT(mTextChangeData.mStartOffset == 30,
    "Test 1-9-1: mStartOffset should be the smallest offset");
  MOZ_ASSERT(mTextChangeData.mRemovedEndOffset == 43, // 38 - (30 - 35)
    "Test 1-9-2: mRemovedEndOffset should be the the first end of removed text "
    "without already removed text");
  MOZ_ASSERT(mTextChangeData.mAddedEndOffset == 36,
    "Test 1-9-3: mAddedEndOffset should be the last end of added text");
  mTextChangeData.mStored = false;

  /****************************************************************************
   * Case 2
   ****************************************************************************/

  // Replacing text in around end of added text (becomes shorter) (not sure
  // if actually occurs)
  StoreTextChangeData(TextChangeData(50, 50, 55, false));
  StoreTextChangeData(TextChangeData(53, 60, 54, false));
  MOZ_ASSERT(mTextChangeData.mStartOffset == 50,
    "Test 2-1-1: mStartOffset should be the smallest offset");
  MOZ_ASSERT(mTextChangeData.mRemovedEndOffset == 55, // 60 - (55 - 50)
    "Test 2-1-2: mRemovedEndOffset should be the the last end of removed text "
    "without already added text length");
  MOZ_ASSERT(mTextChangeData.mAddedEndOffset == 54,
    "Test 2-1-3: mAddedEndOffset should be the last end of added text");
  mTextChangeData.mStored = false;

  // Replacing text around end of added text (becomes longer) (not sure
  // if actually occurs)
  StoreTextChangeData(TextChangeData(50, 50, 55, false));
  StoreTextChangeData(TextChangeData(54, 62, 68, false));
  MOZ_ASSERT(mTextChangeData.mStartOffset == 50,
    "Test 2-2-1: mStartOffset should be the smallest offset");
  MOZ_ASSERT(mTextChangeData.mRemovedEndOffset == 57, // 62 - (55 - 50)
    "Test 2-2-2: mRemovedEndOffset should be the the last end of removed text "
    "without already added text length");
  MOZ_ASSERT(mTextChangeData.mAddedEndOffset == 68,
    "Test 2-2-3: mAddedEndOffset should be the last end of added text");
  mTextChangeData.mStored = false;

  // Replacing text around end of replaced text (became shorter) (not sure if
  // actually occurs)
  StoreTextChangeData(TextChangeData(36, 48, 45, false));
  StoreTextChangeData(TextChangeData(43, 50, 49, false));
  MOZ_ASSERT(mTextChangeData.mStartOffset == 36,
    "Test 2-3-1: mStartOffset should be the smallest offset");
  MOZ_ASSERT(mTextChangeData.mRemovedEndOffset == 53, // 50 - (45 - 48)
    "Test 2-3-2: mRemovedEndOffset should be the the last end of removed text "
    "without already removed text length");
  MOZ_ASSERT(mTextChangeData.mAddedEndOffset == 49,
    "Test 2-3-3: mAddedEndOffset should be the last end of added text");
  mTextChangeData.mStored = false;

  // Replacing text around end of replaced text (became longer) (not sure if
  // actually occurs)
  StoreTextChangeData(TextChangeData(36, 52, 53, false));
  StoreTextChangeData(TextChangeData(43, 68, 61, false));
  MOZ_ASSERT(mTextChangeData.mStartOffset == 36,
    "Test 2-4-1: mStartOffset should be the smallest offset");
  MOZ_ASSERT(mTextChangeData.mRemovedEndOffset == 67, // 68 - (53 - 52)
    "Test 2-4-2: mRemovedEndOffset should be the the last end of removed text "
    "without already added text length");
  MOZ_ASSERT(mTextChangeData.mAddedEndOffset == 61,
    "Test 2-4-3: mAddedEndOffset should be the last end of added text");
  mTextChangeData.mStored = false;

  /****************************************************************************
   * Case 3
   ****************************************************************************/

  // Appending text in already added text (not sure if actually occurs)
  StoreTextChangeData(TextChangeData(10, 10, 20, false));
  StoreTextChangeData(TextChangeData(15, 15, 30, false));
  MOZ_ASSERT(mTextChangeData.mStartOffset == 10,
    "Test 3-1-1: mStartOffset should be the smallest offset");
  MOZ_ASSERT(mTextChangeData.mRemovedEndOffset == 10,
    "Test 3-1-2: mRemovedEndOffset should be the the first end of removed text");
  MOZ_ASSERT(mTextChangeData.mAddedEndOffset == 35, // 20 + (30 - 15)
    "Test 3-1-3: mAddedEndOffset should be the first end of added text with "
    "added text length by the new change");
  mTextChangeData.mStored = false;

  // Replacing text in added text (not sure if actually occurs)
  StoreTextChangeData(TextChangeData(50, 50, 55, false));
  StoreTextChangeData(TextChangeData(52, 53, 56, false));
  MOZ_ASSERT(mTextChangeData.mStartOffset == 50,
    "Test 3-2-1: mStartOffset should be the smallest offset");
  MOZ_ASSERT(mTextChangeData.mRemovedEndOffset == 50,
    "Test 3-2-2: mRemovedEndOffset should be the the first end of removed text");
  MOZ_ASSERT(mTextChangeData.mAddedEndOffset == 58, // 55 + (56 - 53)
    "Test 3-2-3: mAddedEndOffset should be the first end of added text with "
    "added text length by the new change");
  mTextChangeData.mStored = false;

  // Replacing text in replaced text (became shorter) (not sure if actually
  // occurs)
  StoreTextChangeData(TextChangeData(36, 48, 45, false));
  StoreTextChangeData(TextChangeData(37, 38, 50, false));
  MOZ_ASSERT(mTextChangeData.mStartOffset == 36,
    "Test 3-3-1: mStartOffset should be the smallest offset");
  MOZ_ASSERT(mTextChangeData.mRemovedEndOffset == 48,
    "Test 3-3-2: mRemovedEndOffset should be the the first end of removed text");
  MOZ_ASSERT(mTextChangeData.mAddedEndOffset == 57, // 45 + (50 - 38)
    "Test 3-3-3: mAddedEndOffset should be the first end of added text with "
    "added text length by the new change");
  mTextChangeData.mStored = false;

  // Replacing text in replaced text (became longer) (not sure if actually
  // occurs)
  StoreTextChangeData(TextChangeData(32, 48, 53, false));
  StoreTextChangeData(TextChangeData(43, 50, 52, false));
  MOZ_ASSERT(mTextChangeData.mStartOffset == 32,
    "Test 3-4-1: mStartOffset should be the smallest offset");
  MOZ_ASSERT(mTextChangeData.mRemovedEndOffset == 48,
    "Test 3-4-2: mRemovedEndOffset should be the the last end of removed text "
    "without already added text length");
  MOZ_ASSERT(mTextChangeData.mAddedEndOffset == 55, // 53 + (52 - 50)
    "Test 3-4-3: mAddedEndOffset should be the first end of added text with "
    "added text length by the new change");
  mTextChangeData.mStored = false;

  // Replacing text in replaced text (became shorter) (not sure if actually
  // occurs)
  StoreTextChangeData(TextChangeData(36, 48, 50, false));
  StoreTextChangeData(TextChangeData(37, 49, 47, false));
  MOZ_ASSERT(mTextChangeData.mStartOffset == 36,
    "Test 3-5-1: mStartOffset should be the smallest offset");
  MOZ_ASSERT(mTextChangeData.mRemovedEndOffset == 48,
    "Test 3-5-2: mRemovedEndOffset should be the the first end of removed "
    "text");
  MOZ_ASSERT(mTextChangeData.mAddedEndOffset == 48, // 50 + (47 - 49)
    "Test 3-5-3: mAddedEndOffset should be the first end of added text without "
    "removed text length by the new change");
  mTextChangeData.mStored = false;

  // Replacing text in replaced text (became longer) (not sure if actually
  // occurs)
  StoreTextChangeData(TextChangeData(32, 48, 53, false));
  StoreTextChangeData(TextChangeData(43, 50, 47, false));
  MOZ_ASSERT(mTextChangeData.mStartOffset == 32,
    "Test 3-6-1: mStartOffset should be the smallest offset");
  MOZ_ASSERT(mTextChangeData.mRemovedEndOffset == 48,
    "Test 3-6-2: mRemovedEndOffset should be the the last end of removed text "
    "without already added text length");
  MOZ_ASSERT(mTextChangeData.mAddedEndOffset == 50, // 53 + (47 - 50)
    "Test 3-6-3: mAddedEndOffset should be the first end of added text without "
    "removed text length by the new change");
  mTextChangeData.mStored = false;

  /****************************************************************************
   * Case 4
   ****************************************************************************/

  // Replacing text all of already append text (not sure if actually occurs)
  StoreTextChangeData(TextChangeData(50, 50, 55, false));
  StoreTextChangeData(TextChangeData(44, 66, 68, false));
  MOZ_ASSERT(mTextChangeData.mStartOffset == 44,
    "Test 4-1-1: mStartOffset should be the smallest offset");
  MOZ_ASSERT(mTextChangeData.mRemovedEndOffset == 61, // 66 - (55 - 50)
    "Test 4-1-2: mRemovedEndOffset should be the the last end of removed text "
    "without already added text length");
  MOZ_ASSERT(mTextChangeData.mAddedEndOffset == 68,
    "Test 4-1-3: mAddedEndOffset should be the last end of added text");
  mTextChangeData.mStored = false;

  // Replacing text around a point in which text was removed (not sure if
  // actually occurs)
  StoreTextChangeData(TextChangeData(50, 62, 50, false));
  StoreTextChangeData(TextChangeData(44, 66, 68, false));
  MOZ_ASSERT(mTextChangeData.mStartOffset == 44,
    "Test 4-2-1: mStartOffset should be the smallest offset");
  MOZ_ASSERT(mTextChangeData.mRemovedEndOffset == 78, // 66 - (50 - 62)
    "Test 4-2-2: mRemovedEndOffset should be the the last end of removed text "
    "without already removed text length");
  MOZ_ASSERT(mTextChangeData.mAddedEndOffset == 68,
    "Test 4-2-3: mAddedEndOffset should be the last end of added text");
  mTextChangeData.mStored = false;

  // Replacing text all replaced text (became shorter) (not sure if actually
  // occurs)
  StoreTextChangeData(TextChangeData(50, 62, 60, false));
  StoreTextChangeData(TextChangeData(49, 128, 130, false));
  MOZ_ASSERT(mTextChangeData.mStartOffset == 49,
    "Test 4-3-1: mStartOffset should be the smallest offset");
  MOZ_ASSERT(mTextChangeData.mRemovedEndOffset == 130, // 128 - (60 - 62)
    "Test 4-3-2: mRemovedEndOffset should be the the last end of removed text "
    "without already removed text length");
  MOZ_ASSERT(mTextChangeData.mAddedEndOffset == 130,
    "Test 4-3-3: mAddedEndOffset should be the last end of added text");
  mTextChangeData.mStored = false;

  // Replacing text all replaced text (became longer) (not sure if actually
  // occurs)
  StoreTextChangeData(TextChangeData(50, 61, 73, false));
  StoreTextChangeData(TextChangeData(44, 100, 50, false));
  MOZ_ASSERT(mTextChangeData.mStartOffset == 44,
    "Test 4-4-1: mStartOffset should be the smallest offset");
  MOZ_ASSERT(mTextChangeData.mRemovedEndOffset == 88, // 100 - (73 - 61)
    "Test 4-4-2: mRemovedEndOffset should be the the last end of removed text "
    "with already added text length");
  MOZ_ASSERT(mTextChangeData.mAddedEndOffset == 50,
    "Test 4-4-3: mAddedEndOffset should be the last end of added text");
  mTextChangeData.mStored = false;

  /****************************************************************************
   * Case 5
   ****************************************************************************/

  // Replacing text around start of added text (not sure if actually occurs)
  StoreTextChangeData(TextChangeData(50, 50, 55, false));
  StoreTextChangeData(TextChangeData(48, 52, 49, false));
  MOZ_ASSERT(mTextChangeData.mStartOffset == 48,
    "Test 5-1-1: mStartOffset should be the smallest offset");
  MOZ_ASSERT(mTextChangeData.mRemovedEndOffset == 50,
    "Test 5-1-2: mRemovedEndOffset should be the the first end of removed "
    "text");
  MOZ_ASSERT(mTextChangeData.mAddedEndOffset == 52, // 55 + (52 - 49)
    "Test 5-1-3: mAddedEndOffset should be the first end of added text with "
    "added text length by the new change");
  mTextChangeData.mStored = false;

  // Replacing text around start of replaced text (became shorter) (not sure if
  // actually occurs)
  StoreTextChangeData(TextChangeData(50, 60, 58, false));
  StoreTextChangeData(TextChangeData(43, 50, 48, false));
  MOZ_ASSERT(mTextChangeData.mStartOffset == 43,
    "Test 5-2-1: mStartOffset should be the smallest offset");
  MOZ_ASSERT(mTextChangeData.mRemovedEndOffset == 60,
    "Test 5-2-2: mRemovedEndOffset should be the the first end of removed "
    "text");
  MOZ_ASSERT(mTextChangeData.mAddedEndOffset == 56, // 58 + (48 - 50)
    "Test 5-2-3: mAddedEndOffset should be the first end of added text without "
    "removed text length by the new change");
  mTextChangeData.mStored = false;

  // Replacing text around start of replaced text (became longer) (not sure if
  // actually occurs)
  StoreTextChangeData(TextChangeData(50, 60, 68, false));
  StoreTextChangeData(TextChangeData(43, 55, 53, false));
  MOZ_ASSERT(mTextChangeData.mStartOffset == 43,
    "Test 5-3-1: mStartOffset should be the smallest offset");
  MOZ_ASSERT(mTextChangeData.mRemovedEndOffset == 60,
    "Test 5-3-2: mRemovedEndOffset should be the the first end of removed "
    "text");
  MOZ_ASSERT(mTextChangeData.mAddedEndOffset == 66, // 68 + (53 - 55)
    "Test 5-3-3: mAddedEndOffset should be the first end of added text without "
    "removed text length by the new change");
  mTextChangeData.mStored = false;

  // Replacing text around start of replaced text (became shorter) (not sure if
  // actually occurs)
  StoreTextChangeData(TextChangeData(50, 60, 58, false));
  StoreTextChangeData(TextChangeData(43, 50, 128, false));
  MOZ_ASSERT(mTextChangeData.mStartOffset == 43,
    "Test 5-4-1: mStartOffset should be the smallest offset");
  MOZ_ASSERT(mTextChangeData.mRemovedEndOffset == 60,
    "Test 5-4-2: mRemovedEndOffset should be the the first end of removed "
    "text");
  MOZ_ASSERT(mTextChangeData.mAddedEndOffset == 136, // 58 + (128 - 50)
    "Test 5-4-3: mAddedEndOffset should be the first end of added text with "
    "added text length by the new change");
  mTextChangeData.mStored = false;

  // Replacing text around start of replaced text (became longer) (not sure if
  // actually occurs)
  StoreTextChangeData(TextChangeData(50, 60, 68, false));
  StoreTextChangeData(TextChangeData(43, 55, 65, false));
  MOZ_ASSERT(mTextChangeData.mStartOffset == 43,
    "Test 5-5-1: mStartOffset should be the smallest offset");
  MOZ_ASSERT(mTextChangeData.mRemovedEndOffset == 60,
    "Test 5-5-2: mRemovedEndOffset should be the the first end of removed "
    "text");
  MOZ_ASSERT(mTextChangeData.mAddedEndOffset == 78, // 68 + (65 - 55)
    "Test 5-5-3: mAddedEndOffset should be the first end of added text with "
    "added text length by the new change");
  mTextChangeData.mStored = false;

  /****************************************************************************
   * Case 6
   ****************************************************************************/

  // Appending text before already added text (not sure if actually occurs)
  StoreTextChangeData(TextChangeData(30, 30, 45, false));
  StoreTextChangeData(TextChangeData(10, 10, 20, false));
  MOZ_ASSERT(mTextChangeData.mStartOffset == 10,
    "Test 6-1-1: mStartOffset should be the smallest offset");
  MOZ_ASSERT(mTextChangeData.mRemovedEndOffset == 30,
    "Test 6-1-2: mRemovedEndOffset should be the the largest end of removed "
    "text");
  MOZ_ASSERT(mTextChangeData.mAddedEndOffset == 55, // 45 + (20 - 10)
    "Test 6-1-3: mAddedEndOffset should be the first end of added text with "
    "added text length by the new change");
  mTextChangeData.mStored = false;

  // Removing text before already removed text (not sure if actually occurs)
  StoreTextChangeData(TextChangeData(30, 35, 30, false));
  StoreTextChangeData(TextChangeData(10, 25, 10, false));
  MOZ_ASSERT(mTextChangeData.mStartOffset == 10,
    "Test 6-2-1: mStartOffset should be the smallest offset");
  MOZ_ASSERT(mTextChangeData.mRemovedEndOffset == 35,
    "Test 6-2-2: mRemovedEndOffset should be the the largest end of removed "
    "text");
  MOZ_ASSERT(mTextChangeData.mAddedEndOffset == 15, // 30 - (25 - 10)
    "Test 6-2-3: mAddedEndOffset should be the first end of added text with "
    "removed text length by the new change");
  mTextChangeData.mStored = false;

  // Replacing text before already replaced text (not sure if actually occurs)
  StoreTextChangeData(TextChangeData(50, 65, 70, false));
  StoreTextChangeData(TextChangeData(13, 24, 15, false));
  MOZ_ASSERT(mTextChangeData.mStartOffset == 13,
    "Test 6-3-1: mStartOffset should be the smallest offset");
  MOZ_ASSERT(mTextChangeData.mRemovedEndOffset == 65,
    "Test 6-3-2: mRemovedEndOffset should be the the largest end of removed "
    "text");
  MOZ_ASSERT(mTextChangeData.mAddedEndOffset == 61, // 70 + (15 - 24)
    "Test 6-3-3: mAddedEndOffset should be the first end of added text without "
    "removed text length by the new change");
  mTextChangeData.mStored = false;

  // Replacing text before already replaced text (not sure if actually occurs)
  StoreTextChangeData(TextChangeData(50, 65, 70, false));
  StoreTextChangeData(TextChangeData(13, 24, 36, false));
  MOZ_ASSERT(mTextChangeData.mStartOffset == 13,
    "Test 6-4-1: mStartOffset should be the smallest offset");
  MOZ_ASSERT(mTextChangeData.mRemovedEndOffset == 65,
    "Test 6-4-2: mRemovedEndOffset should be the the largest end of removed "
    "text");
  MOZ_ASSERT(mTextChangeData.mAddedEndOffset == 82, // 70 + (36 - 24)
    "Test 6-4-3: mAddedEndOffset should be the first end of added text without "
    "removed text length by the new change");
  mTextChangeData.mStored = false;
}
#endif // #ifdef DEBUG

} // namespace mozilla
