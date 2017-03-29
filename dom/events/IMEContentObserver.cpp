/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Logging.h"

#include "ContentEventHandler.h"
#include "IMEContentObserver.h"
#include "mozilla/AsyncEventDispatcher.h"
#include "mozilla/AutoRestore.h"
#include "mozilla/EventStateManager.h"
#include "mozilla/IMEStateManager.h"
#include "mozilla/MouseEvents.h"
#include "mozilla/TextComposition.h"
#include "mozilla/TextEvents.h"
#include "mozilla/dom/Element.h"
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
#include "nsIWidget.h"
#include "nsPresContext.h"
#include "nsWeakReference.h"
#include "WritingModes.h"

namespace mozilla {

using namespace widget;

LazyLogModule sIMECOLog("IMEContentObserver");

static const char*
ToChar(bool aBool)
{
  return aBool ? "true" : "false";
}

class WritingModeToString final : public nsAutoCString
{
public:
  explicit WritingModeToString(const WritingMode& aWritingMode)
  {
    if (!aWritingMode.IsVertical()) {
      AssignLiteral("Horizontal");
      return;
    }
    if (aWritingMode.IsVerticalLR()) {
      AssignLiteral("Vertical (LR)");
      return;
    }
    AssignLiteral("Vertical (RL)");
  }
  virtual ~WritingModeToString() {}
};

class SelectionChangeDataToString final : public nsAutoCString
{
public:
  explicit SelectionChangeDataToString(
             const IMENotification::SelectionChangeDataBase& aData)
  {
    if (!aData.IsValid()) {
      AppendLiteral("{ IsValid()=false }");
      return;
    }
    AppendPrintf("{ mOffset=%u, ", aData.mOffset);
    if (aData.mString->Length() > 20) {
      AppendPrintf("mString.Length()=%u, ", aData.mString->Length());
    } else {
      AppendPrintf("mString=\"%s\" (Length()=%u), ",
                   NS_ConvertUTF16toUTF8(*aData.mString).get(),
                   aData.mString->Length());
    }
    AppendPrintf("GetWritingMode()=%s, mReversed=%s, mCausedByComposition=%s, "
                 "mCausedBySelectionEvent=%s }",
                 WritingModeToString(aData.GetWritingMode()).get(),
                 ToChar(aData.mReversed),
                 ToChar(aData.mCausedByComposition),
                 ToChar(aData.mCausedBySelectionEvent));
  }
  virtual ~SelectionChangeDataToString() {}
};

class TextChangeDataToString final : public nsAutoCString
{
public:
  explicit TextChangeDataToString(
             const IMENotification::TextChangeDataBase& aData)
  {
    if (!aData.IsValid()) {
      AppendLiteral("{ IsValid()=false }");
      return;
    }
    AppendPrintf("{ mStartOffset=%u, mRemovedEndOffset=%u, mAddedEndOffset=%u, "
                 "mCausedOnlyByComposition=%s, "
                 "mIncludingChangesDuringComposition=%s, "
                 "mIncludingChangesWithoutComposition=%s }",
                 aData.mStartOffset, aData.mRemovedEndOffset,
                 aData.mAddedEndOffset,
                 ToChar(aData.mCausedOnlyByComposition),
                 ToChar(aData.mIncludingChangesDuringComposition),
                 ToChar(aData.mIncludingChangesWithoutComposition));
  }
  virtual ~TextChangeDataToString() {}
};

/******************************************************************************
 * mozilla::IMEContentObserver
 ******************************************************************************/

NS_IMPL_CYCLE_COLLECTION_CLASS(IMEContentObserver)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(IMEContentObserver)
  nsAutoScriptBlocker scriptBlocker;

  tmp->NotifyIMEOfBlur();
  tmp->UnregisterObservers();

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
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mFocusedWidget)
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
  , mSuppressNotifications(0)
  , mPreCharacterDataChangeLength(-1)
  , mSendingNotification(NOTIFY_IME_OF_NOTHING)
  , mIsObserving(false)
  , mIMEHasFocus(false)
  , mNeedsToNotifyIMEOfFocusSet(false)
  , mNeedsToNotifyIMEOfTextChange(false)
  , mNeedsToNotifyIMEOfSelectionChange(false)
  , mNeedsToNotifyIMEOfPositionChange(false)
  , mNeedsToNotifyIMEOfCompositionEventHandled(false)
  , mIsHandlingQueryContentEvent(false)
{
#ifdef DEBUG
  mTextChangeData.Test();
#endif
}

void
IMEContentObserver::Init(nsIWidget* aWidget,
                         nsPresContext* aPresContext,
                         nsIContent* aContent,
                         nsIEditor* aEditor)
{
  State state = GetState();
  if (NS_WARN_IF(state == eState_Observing)) {
    return; // Nothing to do.
  }

  bool firstInitialization = state != eState_StoppedObserving;
  if (!firstInitialization) {
    // If this is now trying to initialize with new contents, all observers
    // should be registered again for simpler implementation.
    UnregisterObservers();
    // Clear members which may not be initialized again.
    Clear();
  }

  mESM = aPresContext->EventStateManager();
  mESM->OnStartToObserveContent(this);

  mWidget = aWidget;

  if (aWidget->GetInputContext().mIMEState.mEnabled == IMEState::PLUGIN) {
    if (!InitWithPlugin(aPresContext, aContent)) {
      Clear();
      return;
    }
  } else {
    if (!InitWithEditor(aPresContext, aContent, aEditor)) {
      Clear();
      return;
    }
  }

  if (firstInitialization) {
    // Now, try to send NOTIFY_IME_OF_FOCUS to IME via the widget.
    MaybeNotifyIMEOfFocusSet();
    // When this is called first time, IME has not received NOTIFY_IME_OF_FOCUS
    // yet since NOTIFY_IME_OF_FOCUS will be sent to widget asynchronously.
    // So, we need to do nothing here.  After NOTIFY_IME_OF_FOCUS has been
    // sent, OnIMEReceivedFocus() will be called and content, selection and/or
    // position changes will be observed
    return;
  }

  // When this is called after editor reframing (i.e., the root editable node
  // is also recreated), IME has usually received NOTIFY_IME_OF_FOCUS.  In this
  // case, we need to restart to observe content, selection and/or position
  // changes in new root editable node.
  ObserveEditableNode();

  if (!NeedsToNotifyIMEOfSomething()) {
    return;
  }

  // Some change events may wait to notify IME because this was being
  // initialized.  It is the time to flush them.
  FlushMergeableNotifications();
}

void
IMEContentObserver::OnIMEReceivedFocus()
{
  // While Init() notifies IME of focus, pending layout may be flushed
  // because the notification may cause querying content.  Then, recursive
  // call of Init() with the latest content may occur.  In such case, we
  // shouldn't keep first initialization which notified IME of focus.
  if (GetState() != eState_Initializing) {
    return;
  }

  // NOTIFY_IME_OF_FOCUS might cause recreating IMEContentObserver
  // instance via IMEStateManager::UpdateIMEState().  So, this
  // instance might already have been destroyed, check it.
  if (!mRootContent) {
    return;
  }

  // Start to observe which is needed by IME when IME actually has focus.
  ObserveEditableNode();

  if (!NeedsToNotifyIMEOfSomething()) {
    return;
  }

  // Some change events may wait to notify IME because this was being
  // initialized.  It is the time to flush them.
  FlushMergeableNotifications();
}

bool
IMEContentObserver::InitWithEditor(nsPresContext* aPresContext,
                                   nsIContent* aContent,
                                   nsIEditor* aEditor)
{
  MOZ_ASSERT(aEditor);

  mEditableNode =
    IMEStateManager::GetRootEditableNode(aPresContext, aContent);
  if (NS_WARN_IF(!mEditableNode)) {
    return false;
  }

  mEditor = aEditor;
  if (NS_WARN_IF(!mEditor)) {
    return false;
  }

  nsIPresShell* presShell = aPresContext->PresShell();

  // get selection and root content
  nsCOMPtr<nsISelectionController> selCon;
  if (mEditableNode->IsNodeOfType(nsINode::eCONTENT)) {
    nsIFrame* frame =
      static_cast<nsIContent*>(mEditableNode.get())->GetPrimaryFrame();
    if (NS_WARN_IF(!frame)) {
      return false;
    }

    frame->GetSelectionController(aPresContext,
                                  getter_AddRefs(selCon));
  } else {
    // mEditableNode is a document
    selCon = do_QueryInterface(presShell);
  }

  if (NS_WARN_IF(!selCon)) {
    return false;
  }

  selCon->GetSelection(nsISelectionController::SELECTION_NORMAL,
                       getter_AddRefs(mSelection));
  if (NS_WARN_IF(!mSelection)) {
    return false;
  }

  nsCOMPtr<nsIDOMRange> selDomRange;
  if (NS_SUCCEEDED(mSelection->GetRangeAt(0, getter_AddRefs(selDomRange)))) {
    nsRange* selRange = static_cast<nsRange*>(selDomRange.get());
    if (NS_WARN_IF(!selRange) || NS_WARN_IF(!selRange->GetStartParent())) {
      return false;
    }

    mRootContent = selRange->GetStartParent()->
                     GetSelectionRootContent(presShell);
  } else {
    mRootContent = mEditableNode->GetSelectionRootContent(presShell);
  }
  if (!mRootContent && mEditableNode->IsNodeOfType(nsINode::eDOCUMENT)) {
    // The document node is editable, but there are no contents, this document
    // is not editable.
    return false;
  }

  if (NS_WARN_IF(!mRootContent)) {
    return false;
  }

  mDocShell = aPresContext->GetDocShell();
  if (NS_WARN_IF(!mDocShell)) {
    return false;
  }

  MOZ_ASSERT(!WasInitializedWithPlugin());

  return true;
}

bool
IMEContentObserver::InitWithPlugin(nsPresContext* aPresContext,
                                   nsIContent* aContent)
{
  if (NS_WARN_IF(!aContent) ||
      NS_WARN_IF(aContent->GetDesiredIMEState().mEnabled != IMEState::PLUGIN)) {
    return false;
  }
  nsIFrame* frame = aContent->GetPrimaryFrame();
  if (NS_WARN_IF(!frame)) {
    return false;
  }
  nsCOMPtr<nsISelectionController> selCon;
  frame->GetSelectionController(aPresContext, getter_AddRefs(selCon));
  if (NS_WARN_IF(!selCon)) {
    return false;
  }
  selCon->GetSelection(nsISelectionController::SELECTION_NORMAL,
                       getter_AddRefs(mSelection));
  if (NS_WARN_IF(!mSelection)) {
    return false;
  }

  mEditor = nullptr;
  mEditableNode = aContent;
  mRootContent = aContent;

  mDocShell = aPresContext->GetDocShell();
  if (NS_WARN_IF(!mDocShell)) {
    return false;
  }

  MOZ_ASSERT(WasInitializedWithPlugin());

  return true;
}

bool
IMEContentObserver::WasInitializedWithPlugin() const
{
  return mDocShell && !mEditor;
}

void
IMEContentObserver::Clear()
{
  mEditor = nullptr;
  mSelection = nullptr;
  mEditableNode = nullptr;
  mRootContent = nullptr;
  mDocShell = nullptr;
}

void
IMEContentObserver::ObserveEditableNode()
{
  MOZ_RELEASE_ASSERT(mSelection);
  MOZ_RELEASE_ASSERT(mRootContent);
  MOZ_RELEASE_ASSERT(GetState() != eState_Observing);

  // If this is called before sending NOTIFY_IME_OF_FOCUS (it's possible when
  // the editor is reframed before sending NOTIFY_IME_OF_FOCUS asynchronously),
  // the update preference of mWidget may be different from after the widget
  // receives NOTIFY_IME_OF_FOCUS.   So, this should be called again by
  // OnIMEReceivedFocus() which is called after sending NOTIFY_IME_OF_FOCUS.
  if (!mIMEHasFocus) {
    MOZ_ASSERT(!mWidget || mNeedsToNotifyIMEOfFocusSet ||
               mSendingNotification == NOTIFY_IME_OF_FOCUS,
               "Wow, OnIMEReceivedFocus() won't be called?");
    return;
  }

  mIsObserving = true;
  if (mEditor) {
    mEditor->AddEditorObserver(this);
  }

  mUpdatePreference = mWidget->GetIMEUpdatePreference();
  if (!WasInitializedWithPlugin()) {
    // Add selection change listener only when this starts to observe
    // non-plugin content since we cannot detect selection changes in
    // plugins.
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
IMEContentObserver::NotifyIMEOfBlur()
{
  // Prevent any notifications to be sent IME.
  nsCOMPtr<nsIWidget> widget;
  mWidget.swap(widget);

  // If we hasn't been set focus, we shouldn't send blur notification to IME.
  if (!mIMEHasFocus) {
    return;
  }

  // mWidget must have been non-nullptr if IME has focus.
  MOZ_RELEASE_ASSERT(widget);

  RefPtr<IMEContentObserver> kungFuDeathGrip(this);

  MOZ_LOG(sIMECOLog, LogLevel::Info,
    ("0x%p IMEContentObserver::NotifyIMEOfBlur(), "
     "sending NOTIFY_IME_OF_BLUR", this));

  // For now, we need to send blur notification in any condition because
  // we don't have any simple ways to send blur notification asynchronously.
  // After this call, Destroy() or Unlink() will stop observing the content
  // and forget everything.  Therefore, if it's not safe to send notification
  // when script blocker is unlocked, we cannot send blur notification after
  // that and before next focus notification.
  // Anyway, as far as we know, IME doesn't try to query content when it loses
  // focus.  So, this may not cause any problem.
  mIMEHasFocus = false;
  IMEStateManager::NotifyIME(IMENotification(NOTIFY_IME_OF_BLUR), widget);

  MOZ_LOG(sIMECOLog, LogLevel::Debug,
    ("0x%p IMEContentObserver::NotifyIMEOfBlur(), "
     "sent NOTIFY_IME_OF_BLUR", this));
}

void
IMEContentObserver::UnregisterObservers()
{
  if (!mIsObserving) {
    return;
  }
  mIsObserving = false;

  if (mEditor) {
    mEditor->RemoveEditorObserver(this);
  }

  if (mSelection) {
    nsCOMPtr<nsISelectionPrivate> selPrivate(do_QueryInterface(mSelection));
    if (selPrivate) {
      selPrivate->RemoveSelectionListener(this);
    }
    mSelectionData.Clear();
    mFocusedWidget = nullptr;
  }

  if (mUpdatePreference.WantTextChange() && mRootContent) {
    mRootContent->RemoveMutationObserver(this);
  }

  if (mUpdatePreference.WantPositionChanged() && mDocShell) {
    mDocShell->RemoveWeakScrollObserver(this);
    mDocShell->RemoveWeakReflowObserver(this);
  }
}

nsPresContext*
IMEContentObserver::GetPresContext() const
{
  return mESM ? mESM->GetPresContext() : nullptr;
}

void
IMEContentObserver::Destroy()
{
  // WARNING: When you change this method, you have to check Unlink() too.

  NotifyIMEOfBlur();
  UnregisterObservers();
  Clear();

  mWidget = nullptr;
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
IMEContentObserver::MaybeReinitialize(nsIWidget* aWidget,
                                      nsPresContext* aPresContext,
                                      nsIContent* aContent,
                                      nsIEditor* aEditor)
{
  if (!IsObservingContent(aPresContext, aContent)) {
    return false;
  }

  if (GetState() == eState_StoppedObserving) {
    Init(aWidget, aPresContext, aContent, aEditor);
  }
  return IsManaging(aPresContext, aContent);
}

bool
IMEContentObserver::IsManaging(nsPresContext* aPresContext,
                               nsIContent* aContent) const
{
  return GetState() == eState_Observing &&
         IsObservingContent(aPresContext, aContent);
}

bool
IMEContentObserver::IsManaging(const TextComposition* aComposition) const
{
  if (GetState() != eState_Observing) {
    return false;
  }
  nsPresContext* presContext = aComposition->GetPresContext();
  if (NS_WARN_IF(!presContext)) {
    return false;
  }
  if (presContext != GetPresContext()) {
    return false; // observing different document
  }
  nsINode* targetNode = aComposition->GetEventTargetNode();
  nsIContent* targetContent =
    targetNode && targetNode->IsContent() ? targetNode->AsContent() : nullptr;
  return IsObservingContent(presContext, targetContent);
}

IMEContentObserver::State
IMEContentObserver::GetState() const
{
  if (!mSelection || !mRootContent || !mEditableNode) {
    return eState_NotObserving; // failed to initialize or finalized.
  }
  if (!mRootContent->IsInComposedDoc()) {
    // the focused editor has already been reframed.
    return eState_StoppedObserving;
  }
  return mIsObserving ? eState_Observing : eState_Initializing;
}

bool
IMEContentObserver::IsObservingContent(nsPresContext* aPresContext,
                                       nsIContent* aContent) const
{
  return IsInitializedWithPlugin() ?
    mRootContent == aContent && mRootContent != nullptr :
    mEditableNode == IMEStateManager::GetRootEditableNode(aPresContext,
                                                          aContent);
}

bool
IMEContentObserver::IsEditorHandlingEventForComposition() const
{
  if (!mWidget) {
    return false;
  }
  RefPtr<TextComposition> composition =
    IMEStateManager::GetTextCompositionFor(mWidget);
  if (!composition) {
    return false;
  }
  return composition->IsEditorHandlingEvent();
}

bool
IMEContentObserver::IsEditorComposing() const
{
  // Note that don't use TextComposition here. The important thing is,
  // whether the editor already started to handle composition because
  // web contents can change selection, text content and/or something from
  // compositionstart event listener which is run before EditorBase handles it.
  if (NS_WARN_IF(!mEditor)) {
    return false;
  }
  bool isComposing = false;
  nsresult rv = mEditor->GetComposing(&isComposing);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return false;
  }
  return isComposing;
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

nsresult
IMEContentObserver::NotifySelectionChanged(nsIDOMDocument* aDOMDocument,
                                           nsISelection* aSelection,
                                           int16_t aReason)
{
  int32_t count = 0;
  nsresult rv = aSelection->GetRangeCount(&count);
  NS_ENSURE_SUCCESS(rv, rv);
  if (count > 0 && mWidget) {
    bool causedByComposition = IsEditorHandlingEventForComposition();
    bool causedBySelectionEvent = TextComposition::IsHandlingSelectionEvent();
    bool duringComposition = IsEditorComposing();
    MaybeNotifyIMEOfSelectionChange(causedByComposition,
                                    causedBySelectionEvent,
                                    duringComposition);
  }
  return NS_OK;
}

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

nsresult
IMEContentObserver::HandleQueryContentEvent(WidgetQueryContentEvent* aEvent)
{
  // If the instance has normal selection cache and the query event queries
  // normal selection's range, it should use the cached selection which was
  // sent to the widget.  However, if this instance has already received new
  // selection change notification but hasn't updated the cache yet (i.e.,
  // not sending selection change notification to IME, don't use the cached
  // value.  Note that don't update selection cache here since if you update
  // selection cache here, IMENotificationSender won't notify IME of selection
  // change because it looks like that the selection isn't actually changed.
  bool isSelectionCacheAvailable =
    aEvent->mUseNativeLineBreak && mSelectionData.IsValid() &&
    !mNeedsToNotifyIMEOfSelectionChange;
  if (isSelectionCacheAvailable &&
      aEvent->mMessage == eQuerySelectedText &&
      aEvent->mInput.mSelectionType == SelectionType::eNormal) {
    aEvent->mReply.mContentsRoot = mRootContent;
    aEvent->mReply.mHasSelection = !mSelectionData.IsCollapsed();
    aEvent->mReply.mOffset = mSelectionData.mOffset;
    aEvent->mReply.mString = mSelectionData.String();
    aEvent->mReply.mWritingMode = mSelectionData.GetWritingMode();
    aEvent->mReply.mReversed = mSelectionData.mReversed;
    aEvent->mSucceeded = true;
    MOZ_LOG(sIMECOLog, LogLevel::Debug,
      ("0x%p IMEContentObserver::HandleQueryContentEvent(aEvent={ "
       "mMessage=%s })", this, ToChar(aEvent->mMessage)));
    return NS_OK;
  }

  MOZ_LOG(sIMECOLog, LogLevel::Debug,
    ("0x%p IMEContentObserver::HandleQueryContentEvent(aEvent={ "
     "mMessage=%s })", this, ToChar(aEvent->mMessage)));

  // If we can make the event's input offset absolute with TextComposition or
  // mSelection, we should set it here for reducing the cost of computing
  // selection start offset.  If ContentEventHandler receives a
  // WidgetQueryContentEvent whose input offset is relative to insertion point,
  // it computes current selection start offset (this may be expensive) and
  // make the offset absolute value itself.
  // Note that calling MakeOffsetAbsolute() makes the event a query event with
  // absolute offset.  So, ContentEventHandler doesn't pay any additional cost
  // after calling MakeOffsetAbsolute() here.
  if (aEvent->mInput.mRelativeToInsertionPoint &&
      aEvent->mInput.IsValidEventMessage(aEvent->mMessage)) {
    RefPtr<TextComposition> composition =
      IMEStateManager::GetTextCompositionFor(aEvent->mWidget);
    if (composition) {
      uint32_t compositionStart = composition->NativeOffsetOfStartComposition();
      if (NS_WARN_IF(!aEvent->mInput.MakeOffsetAbsolute(compositionStart))) {
        return NS_ERROR_FAILURE;
      }
    } else if (isSelectionCacheAvailable) {
      uint32_t selectionStart = mSelectionData.mOffset;
      if (NS_WARN_IF(!aEvent->mInput.MakeOffsetAbsolute(selectionStart))) {
        return NS_ERROR_FAILURE;
      }
    }
  }

  AutoRestore<bool> handling(mIsHandlingQueryContentEvent);
  mIsHandlingQueryContentEvent = true;
  ContentEventHandler handler(GetPresContext());
  nsresult rv = handler.HandleQueryContentEvent(aEvent);

  if (!IsInitializedWithPlugin() &&
      NS_WARN_IF(aEvent->mReply.mContentsRoot != mRootContent)) {
    // Focus has changed unexpectedly, so make the query fail.
    aEvent->mSucceeded = false;
  }
  return rv;
}

bool
IMEContentObserver::OnMouseButtonEvent(nsPresContext* aPresContext,
                                       WidgetMouseEvent* aMouseEvent)
{
  if (!mUpdatePreference.WantMouseButtonEventOnChar()) {
    return false;
  }
  if (!aMouseEvent->IsTrusted() ||
      aMouseEvent->DefaultPrevented() ||
      !aMouseEvent->mWidget) {
    return false;
  }
  // Now, we need to notify only mouse down and mouse up event.
  switch (aMouseEvent->mMessage) {
    case eMouseUp:
    case eMouseDown:
      break;
    default:
      return false;
  }
  if (NS_WARN_IF(!mWidget) || NS_WARN_IF(mWidget->Destroyed())) {
    return false;
  }

  RefPtr<IMEContentObserver> kungFuDeathGrip(this);

  WidgetQueryContentEvent charAtPt(true, eQueryCharacterAtPoint,
                                   aMouseEvent->mWidget);
  charAtPt.mRefPoint = aMouseEvent->mRefPoint;
  ContentEventHandler handler(aPresContext);
  handler.OnQueryCharacterAtPoint(&charAtPt);
  if (NS_WARN_IF(!charAtPt.mSucceeded) ||
      charAtPt.mReply.mOffset == WidgetQueryContentEvent::NOT_FOUND) {
    return false;
  }

  // The widget might be destroyed during querying the content since it
  // causes flushing layout.
  if (!mWidget || NS_WARN_IF(mWidget->Destroyed())) {
    return false;
  }

  // The result character rect is relative to the top level widget.
  // We should notify it with offset in the widget.
  nsIWidget* topLevelWidget = mWidget->GetTopLevelWidget();
  if (topLevelWidget && topLevelWidget != mWidget) {
    charAtPt.mReply.mRect.MoveBy(
      topLevelWidget->WidgetToScreenOffset() -
        mWidget->WidgetToScreenOffset());
  }
  // The refPt is relative to its widget.
  // We should notify it with offset in the widget.
  if (aMouseEvent->mWidget != mWidget) {
    charAtPt.mRefPoint += aMouseEvent->mWidget->WidgetToScreenOffset() -
      mWidget->WidgetToScreenOffset();
  }

  IMENotification notification(NOTIFY_IME_OF_MOUSE_BUTTON_EVENT);
  notification.mMouseButtonEventData.mEventMessage = aMouseEvent->mMessage;
  notification.mMouseButtonEventData.mOffset = charAtPt.mReply.mOffset;
  notification.mMouseButtonEventData.mCursorPos.Set(
    charAtPt.mRefPoint.ToUnknownPoint());
  notification.mMouseButtonEventData.mCharRect.Set(
    charAtPt.mReply.mRect.ToUnknownRect());
  notification.mMouseButtonEventData.mButton = aMouseEvent->button;
  notification.mMouseButtonEventData.mButtons = aMouseEvent->buttons;
  notification.mMouseButtonEventData.mModifiers = aMouseEvent->mModifiers;

  nsresult rv = IMEStateManager::NotifyIME(notification, mWidget);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return false;
  }

  bool consumed = (rv == NS_SUCCESS_EVENT_CONSUMED);
  if (consumed) {
    aMouseEvent->PreventDefault();
  }
  return consumed;
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

  MOZ_ASSERT(removedLength >= 0,
             "mPreCharacterDataChangeLength should've been set by "
             "CharacterDataWillChange()");

  uint32_t offset = 0;
  // get offsets of change and fire notification
  nsresult rv =
    ContentEventHandler::GetFlatTextLengthInRange(
                           NodePosition(mRootContent, 0),
                           NodePosition(aContent, aInfo->mChangeStart),
                           mRootContent, &offset, LINE_BREAK_TYPE_NATIVE);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return;
  }

  uint32_t newLength =
    ContentEventHandler::GetNativeTextLength(aContent, aInfo->mChangeStart,
                                             aInfo->mChangeStart +
                                               aInfo->mReplaceLength);

  uint32_t oldEnd = offset + static_cast<uint32_t>(removedLength);
  uint32_t newEnd = offset + newLength;

  TextChangeData data(offset, oldEnd, newEnd,
                      IsEditorHandlingEventForComposition(),
                      IsEditorComposing());
  MaybeNotifyIMEOfTextChange(data);
}

void
IMEContentObserver::NotifyContentAdded(nsINode* aContainer,
                                       int32_t aStartIndex,
                                       int32_t aEndIndex)
{
  mStartOfRemovingTextRangeCache.Clear();

  uint32_t offset = 0;
  nsresult rv = NS_OK;
  if (!mEndOfAddedTextCache.Match(aContainer, aStartIndex)) {
    mEndOfAddedTextCache.Clear();
    rv = ContentEventHandler::GetFlatTextLengthInRange(
                                NodePosition(mRootContent, 0),
                                NodePositionBefore(aContainer, aStartIndex),
                                mRootContent, &offset, LINE_BREAK_TYPE_NATIVE);
    if (NS_WARN_IF(NS_FAILED((rv)))) {
      return;
    }
  } else {
    offset = mEndOfAddedTextCache.mFlatTextLength;
  }

  // get offset at the end of the last added node
  uint32_t addingLength = 0;
  rv = ContentEventHandler::GetFlatTextLengthInRange(
                              NodePositionBefore(aContainer, aStartIndex),
                              NodePosition(aContainer, aEndIndex),
                              mRootContent, &addingLength,
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
                      IsEditorHandlingEventForComposition(),
                      IsEditorComposing());
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

  nsINode* containerNode = NODE_FROM(aContainer, aDocument);

  uint32_t offset = 0;
  nsresult rv = NS_OK;
  if (!mStartOfRemovingTextRangeCache.Match(containerNode, aIndexInContainer)) {
    // At removing a child node of aContainer, we need the line break caused
    // by open tag of aContainer.  Be careful when aIndexInContainer is 0.
    rv = ContentEventHandler::GetFlatTextLengthInRange(
                                NodePosition(mRootContent, 0),
                                NodePosition(containerNode, aIndexInContainer),
                                mRootContent, &offset, LINE_BREAK_TYPE_NATIVE);
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
  uint32_t textLength = 0;
  if (aChild->IsNodeOfType(nsINode::eTEXT)) {
    textLength = ContentEventHandler::GetNativeTextLength(aChild);
  } else {
    uint32_t nodeLength = static_cast<int32_t>(aChild->GetChildCount());
    rv = ContentEventHandler::GetFlatTextLengthInRange(
                                NodePositionBefore(aChild, 0),
                                NodePosition(aChild, nodeLength),
                                mRootContent, &textLength,
                                LINE_BREAK_TYPE_NATIVE, true);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      mStartOfRemovingTextRangeCache.Clear();
      return;
    }
  }

  if (!textLength) {
    return;
  }

  TextChangeData data(offset, offset + textLength, offset,
                      IsEditorHandlingEventForComposition(),
                      IsEditorComposing());
  MaybeNotifyIMEOfTextChange(data);
}

void
IMEContentObserver::AttributeWillChange(nsIDocument* aDocument,
                                        dom::Element* aElement,
                                        int32_t aNameSpaceID,
                                        nsIAtom* aAttribute,
                                        int32_t aModType,
                                        const nsAttrValue* aNewValue)
{
  mPreAttrChangeLength =
    ContentEventHandler::GetNativeTextLengthBefore(aElement, mRootContent);
}

void
IMEContentObserver::AttributeChanged(nsIDocument* aDocument,
                                     dom::Element* aElement,
                                     int32_t aNameSpaceID,
                                     nsIAtom* aAttribute,
                                     int32_t aModType,
                                     const nsAttrValue* aOldValue)
{
  mEndOfAddedTextCache.Clear();
  mStartOfRemovingTextRangeCache.Clear();

  uint32_t postAttrChangeLength =
    ContentEventHandler::GetNativeTextLengthBefore(aElement, mRootContent);
  if (postAttrChangeLength == mPreAttrChangeLength) {
    return;
  }
  uint32_t start;
  nsresult rv =
    ContentEventHandler::GetFlatTextLengthInRange(
                           NodePosition(mRootContent, 0),
                           NodePositionBefore(aElement, 0),
                           mRootContent, &start, LINE_BREAK_TYPE_NATIVE);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return;
  }

  TextChangeData data(start, start + mPreAttrChangeLength,
                      start + postAttrChangeLength,
                      IsEditorHandlingEventForComposition(),
                      IsEditorComposing());
  MaybeNotifyIMEOfTextChange(data);
}

void
IMEContentObserver::SuppressNotifyingIME()
{
  mSuppressNotifications++;

  MOZ_LOG(sIMECOLog, LogLevel::Debug,
    ("0x%p IMEContentObserver::SuppressNotifyingIME(), "
     "mSuppressNotifications=%u", this, mSuppressNotifications));
}

void
IMEContentObserver::UnsuppressNotifyingIME()
{
  MOZ_LOG(sIMECOLog, LogLevel::Debug,
    ("0x%p IMEContentObserver::UnsuppressNotifyingIME(), "
     "mSuppressNotifications=%u", this, mSuppressNotifications));

  if (!mSuppressNotifications || --mSuppressNotifications) {
    return;
  }
  FlushMergeableNotifications();
}

NS_IMETHODIMP
IMEContentObserver::EditAction()
{
  MOZ_LOG(sIMECOLog, LogLevel::Debug,
    ("0x%p IMEContentObserver::EditAction()", this));

  mEndOfAddedTextCache.Clear();
  mStartOfRemovingTextRangeCache.Clear();
  FlushMergeableNotifications();
  return NS_OK;
}

NS_IMETHODIMP
IMEContentObserver::BeforeEditAction()
{
  MOZ_LOG(sIMECOLog, LogLevel::Debug,
    ("0x%p IMEContentObserver::BeforeEditAction()", this));

  mEndOfAddedTextCache.Clear();
  mStartOfRemovingTextRangeCache.Clear();
  return NS_OK;
}

NS_IMETHODIMP
IMEContentObserver::CancelEditAction()
{
  MOZ_LOG(sIMECOLog, LogLevel::Debug,
    ("0x%p IMEContentObserver::CancelEditAction()", this));

  mEndOfAddedTextCache.Clear();
  mStartOfRemovingTextRangeCache.Clear();
  FlushMergeableNotifications();
  return NS_OK;
}

void
IMEContentObserver::PostFocusSetNotification()
{
  MOZ_LOG(sIMECOLog, LogLevel::Debug,
    ("0x%p IMEContentObserver::PostFocusSetNotification()", this));

  mNeedsToNotifyIMEOfFocusSet = true;
}

void
IMEContentObserver::PostTextChangeNotification()
{
  MOZ_LOG(sIMECOLog, LogLevel::Debug,
    ("0x%p IMEContentObserver::PostTextChangeNotification("
     "mTextChangeData=%s)",
     this, TextChangeDataToString(mTextChangeData).get()));

  MOZ_ASSERT(mTextChangeData.IsValid(),
             "mTextChangeData must have text change data");
  mNeedsToNotifyIMEOfTextChange = true;
}

void
IMEContentObserver::PostSelectionChangeNotification()
{
  MOZ_LOG(sIMECOLog, LogLevel::Debug,
    ("0x%p IMEContentObserver::PostSelectionChangeNotification(), "
     "mSelectionData={ mCausedByComposition=%s, mCausedBySelectionEvent=%s }",
     this, ToChar(mSelectionData.mCausedByComposition),
     ToChar(mSelectionData.mCausedBySelectionEvent)));

  mNeedsToNotifyIMEOfSelectionChange = true;
}

void
IMEContentObserver::MaybeNotifyIMEOfFocusSet()
{
  MOZ_LOG(sIMECOLog, LogLevel::Debug,
    ("0x%p IMEContentObserver::MaybeNotifyIMEOfFocusSet()", this));

  PostFocusSetNotification();
  FlushMergeableNotifications();
}

void
IMEContentObserver::MaybeNotifyIMEOfTextChange(
                      const TextChangeDataBase& aTextChangeData)
{
  MOZ_LOG(sIMECOLog, LogLevel::Debug,
    ("0x%p IMEContentObserver::MaybeNotifyIMEOfTextChange("
     "aTextChangeData=%s)",
     this, TextChangeDataToString(aTextChangeData).get()));

  mTextChangeData += aTextChangeData;
  PostTextChangeNotification();
  FlushMergeableNotifications();
}

void
IMEContentObserver::MaybeNotifyIMEOfSelectionChange(
                      bool aCausedByComposition,
                      bool aCausedBySelectionEvent,
                      bool aOccurredDuringComposition)
{
  MOZ_LOG(sIMECOLog, LogLevel::Debug,
    ("0x%p IMEContentObserver::MaybeNotifyIMEOfSelectionChange("
     "aCausedByComposition=%s, aCausedBySelectionEvent=%s, "
     "aOccurredDuringComposition)",
     this, ToChar(aCausedByComposition), ToChar(aCausedBySelectionEvent)));

  mSelectionData.AssignReason(aCausedByComposition,
                              aCausedBySelectionEvent,
                              aOccurredDuringComposition);
  PostSelectionChangeNotification();
  FlushMergeableNotifications();
}

void
IMEContentObserver::MaybeNotifyIMEOfPositionChange()
{
  MOZ_LOG(sIMECOLog, LogLevel::Debug,
    ("0x%p IMEContentObserver::MaybeNotifyIMEOfPositionChange()", this));
  // If reflow is caused by ContentEventHandler during PositionChangeEvent
  // sending NOTIFY_IME_OF_POSITION_CHANGE, we don't need to notify IME of it
  // again since ContentEventHandler returns the result including this reflow's
  // result.
  if (mIsHandlingQueryContentEvent &&
      mSendingNotification == NOTIFY_IME_OF_POSITION_CHANGE) {
    MOZ_LOG(sIMECOLog, LogLevel::Debug,
      ("0x%p   IMEContentObserver::MaybeNotifyIMEOfPositionChange(), "
       "ignored since caused by ContentEventHandler during sending "
       "NOTIY_IME_OF_POSITION_CHANGE", this));
    return;
  }
  PostPositionChangeNotification();
  FlushMergeableNotifications();
}

void
IMEContentObserver::MaybeNotifyCompositionEventHandled()
{
  MOZ_LOG(sIMECOLog, LogLevel::Debug,
    ("0x%p IMEContentObserver::MaybeNotifyCompositionEventHandled()",
     this));

  PostCompositionEventHandledNotification();
  FlushMergeableNotifications();
}

bool
IMEContentObserver::UpdateSelectionCache()
{
  MOZ_ASSERT(IsSafeToNotifyIME());

  if (WasInitializedWithPlugin()) {
    return false;
  }

  mSelectionData.ClearSelectionData();

  // XXX Cannot we cache some information for reducing the cost to compute
  //     selection offset and writing mode?
  WidgetQueryContentEvent selection(true, eQuerySelectedText, mWidget);
  ContentEventHandler handler(GetPresContext());
  handler.OnQuerySelectedText(&selection);
  if (NS_WARN_IF(!selection.mSucceeded) ||
      NS_WARN_IF(selection.mReply.mContentsRoot != mRootContent)) {
    return false;
  }

  mFocusedWidget = selection.mReply.mFocusedWidget;
  mSelectionData.mOffset = selection.mReply.mOffset;
  *mSelectionData.mString = selection.mReply.mString;
  mSelectionData.SetWritingMode(selection.GetWritingMode());
  mSelectionData.mReversed = selection.mReply.mReversed;

  // WARNING: Don't modify the reason of selection change here.

  MOZ_LOG(sIMECOLog, LogLevel::Debug,
    ("0x%p IMEContentObserver::UpdateSelectionCache(), "
     "mSelectionData=%s",
     this, SelectionChangeDataToString(mSelectionData).get()));

  return mSelectionData.IsValid();
}

void
IMEContentObserver::PostPositionChangeNotification()
{
  MOZ_LOG(sIMECOLog, LogLevel::Debug,
    ("0x%p IMEContentObserver::PostPositionChangeNotification()", this));

  mNeedsToNotifyIMEOfPositionChange = true;
}

void
IMEContentObserver::PostCompositionEventHandledNotification()
{
  MOZ_LOG(sIMECOLog, LogLevel::Debug,
    ("0x%p IMEContentObserver::"
     "PostCompositionEventHandledNotification()", this));

  mNeedsToNotifyIMEOfCompositionEventHandled = true;
}

bool
IMEContentObserver::IsReflowLocked() const
{
  nsPresContext* presContext = GetPresContext();
  if (NS_WARN_IF(!presContext)) {
    return false;
  }
  nsIPresShell* presShell = presContext->GetPresShell();
  if (NS_WARN_IF(!presShell)) {
    return false;
  }
  // During reflow, we shouldn't notify IME because IME may query content
  // synchronously.  Then, it causes ContentEventHandler will try to flush
  // pending notifications during reflow.
  return presShell->IsReflowLocked();
}

bool
IMEContentObserver::IsSafeToNotifyIME() const
{
  // If this is already detached from the widget, this doesn't need to notify
  // anything.
  if (!mWidget) {
    return false;
  }

  // Don't notify IME of anything if it's not good time to do it.
  if (mSuppressNotifications) {
    return false;
  }

  if (!mESM || NS_WARN_IF(!GetPresContext())) {
    return false;
  }

  // If it's in reflow, we should wait to finish the reflow.
  // FYI: This should be called again from Reflow() or ReflowInterruptible().
  if (IsReflowLocked()) {
    return false;
  }

  // If we're in handling an edit action, this method will be called later.
  bool isInEditAction = false;
  if (mEditor && NS_SUCCEEDED(mEditor->GetIsInEditAction(&isInEditAction)) &&
      isInEditAction) {
    return false;
  }

  return true;
}

void
IMEContentObserver::FlushMergeableNotifications()
{
  if (!IsSafeToNotifyIME()) {
    // So, if this is already called, this should do nothing.
    MOZ_LOG(sIMECOLog, LogLevel::Debug,
      ("0x%p IMEContentObserver::FlushMergeableNotifications(), "
       "FAILED, due to unsafe to notify IME", this));
    return;
  }

  // Notifying something may cause nested call of this method.  For example,
  // when somebody notified one of the notifications may dispatch query content
  // event. Then, it causes flushing layout which may cause another layout
  // change notification.

  if (mQueuedSender) {
    // So, if this is already called, this should do nothing.
    MOZ_LOG(sIMECOLog, LogLevel::Debug,
      ("0x%p   IMEContentObserver::FlushMergeableNotifications(), "
       "FAILED, due to already flushing pending notifications", this));
    return;
  }

  if (!NeedsToNotifyIMEOfSomething()) {
    MOZ_LOG(sIMECOLog, LogLevel::Debug,
      ("0x%p   IMEContentObserver::FlushMergeableNotifications(), "
       "FAILED, due to no pending notifications", this));
    return;
  }

  // NOTE: Reset each pending flag because sending notification may cause
  //       another change.

  MOZ_LOG(sIMECOLog, LogLevel::Debug,
    ("0x%p IMEContentObserver::FlushMergeableNotifications(), "
     "creating IMENotificationSender...", this));

  // If contents in selection range is modified, the selection range still
  // has removed node from the tree.  In such case, nsContentIterator won't
  // work well.  Therefore, we shouldn't use AddScriptRunnder() here since
  // it may kick runnable event immediately after DOM tree is changed but
  // the selection range isn't modified yet.
  mQueuedSender = new IMENotificationSender(this);
  NS_DispatchToCurrentThread(mQueuedSender);

  MOZ_LOG(sIMECOLog, LogLevel::Debug,
    ("0x%p IMEContentObserver::FlushMergeableNotifications(), "
     "finished", this));
}

void
IMEContentObserver::TryToFlushPendingNotifications()
{
  if (!mQueuedSender || mSendingNotification != NOTIFY_IME_OF_NOTHING) {
    return;
  }

  MOZ_LOG(sIMECOLog, LogLevel::Debug,
    ("0x%p IMEContentObserver::TryToFlushPendingNotifications(), "
     "performing queued IMENotificationSender forcibly", this));
  RefPtr<IMENotificationSender> queuedSender = mQueuedSender;
  queuedSender->Run();
}

/******************************************************************************
 * mozilla::IMEContentObserver::AChangeEvent
 ******************************************************************************/

bool
IMEContentObserver::AChangeEvent::CanNotifyIME(
                                    ChangeEventType aChangeEventType) const
{
  if (NS_WARN_IF(!mIMEContentObserver)) {
    return false;
  }
  if (aChangeEventType == eChangeEventType_CompositionEventHandled) {
    return mIMEContentObserver->mWidget != nullptr;
  }
  State state = mIMEContentObserver->GetState();
  // If it's not initialized, we should do nothing.
  if (state == eState_NotObserving) {
    return false;
  }
  // If setting focus, just check the state.
  if (aChangeEventType == eChangeEventType_Focus) {
    return !NS_WARN_IF(mIMEContentObserver->mIMEHasFocus);
  }
  // If we've not notified IME of focus yet, we shouldn't notify anything.
  if (!mIMEContentObserver->mIMEHasFocus) {
    return false;
  }

  // If IME has focus, IMEContentObserver must hold the widget.
  MOZ_ASSERT(mIMEContentObserver->mWidget);

  return true;
}

bool
IMEContentObserver::AChangeEvent::IsSafeToNotifyIME(
                                    ChangeEventType aChangeEventType) const
{
  if (NS_WARN_IF(!nsContentUtils::IsSafeToRunScript())) {
    return false;
  }
  // While we're sending a notification, we shouldn't send another notification
  // recursively.
  if (mIMEContentObserver->mSendingNotification != NOTIFY_IME_OF_NOTHING) {
    MOZ_LOG(sIMECOLog, LogLevel::Debug,
      ("0x%p   IMEContentObserver::AChangeEvent::IsSafeToNotifyIME(), "
       "putting off sending notification due to detecting recursive call, "
       "mIMEContentObserver={ mSendingNotification=%s }",
       this, ToChar(mIMEContentObserver->mSendingNotification)));
    return false;
  }
  State state = mIMEContentObserver->GetState();
  if (aChangeEventType == eChangeEventType_Focus) {
    if (NS_WARN_IF(state != eState_Initializing && state != eState_Observing)) {
      return false;
    }
  } else if (aChangeEventType == eChangeEventType_CompositionEventHandled) {
    // It doesn't need to check the observing status.
  } else if (state != eState_Observing) {
    return false;
  }
  return mIMEContentObserver->IsSafeToNotifyIME();
}

/******************************************************************************
 * mozilla::IMEContentObserver::IMENotificationSender
 ******************************************************************************/
 
NS_IMETHODIMP
IMEContentObserver::IMENotificationSender::Run()
{
  if (NS_WARN_IF(mIsRunning)) {
    MOZ_LOG(sIMECOLog, LogLevel::Error,
      ("0x%p IMEContentObserver::IMENotificationSender::Run(), FAILED, "
       "called recursively", this));
    return NS_OK;
  }

  AutoRestore<bool> running(mIsRunning);
  mIsRunning = true;

  // This instance was already performed forcibly.
  if (mIMEContentObserver->mQueuedSender != this) {
    return NS_OK;
  }

  // NOTE: Reset each pending flag because sending notification may cause
  //       another change.

  if (mIMEContentObserver->mNeedsToNotifyIMEOfFocusSet) {
    mIMEContentObserver->mNeedsToNotifyIMEOfFocusSet = false;
    SendFocusSet();
    mIMEContentObserver->mQueuedSender = nullptr;
    // If it's not safe to notify IME of focus, SendFocusSet() sets
    // mNeedsToNotifyIMEOfFocusSet true again.  For guaranteeing to send the
    // focus notification later,  we should put a new sender into the queue but
    // this case must be rare.  Note that if mIMEContentObserver is already
    // destroyed, mNeedsToNotifyIMEOfFocusSet is never set true again.
    if (mIMEContentObserver->mNeedsToNotifyIMEOfFocusSet) {
      MOZ_ASSERT(!mIMEContentObserver->mIMEHasFocus);
      MOZ_LOG(sIMECOLog, LogLevel::Debug,
        ("0x%p IMEContentObserver::IMENotificationSender::Run(), "
         "posting IMENotificationSender to current thread", this));
      mIMEContentObserver->mQueuedSender =
        new IMENotificationSender(mIMEContentObserver);
      NS_DispatchToCurrentThread(mIMEContentObserver->mQueuedSender);
      return NS_OK;
    }
    // This is the first notification to IME. So, we don't need to notify
    // anymore since IME starts to query content after it gets focus.
    mIMEContentObserver->ClearPendingNotifications();
    return NS_OK;
  }

  if (mIMEContentObserver->mNeedsToNotifyIMEOfTextChange) {
    mIMEContentObserver->mNeedsToNotifyIMEOfTextChange = false;
    SendTextChange();
  }

  // If a text change notification causes another text change again, we should
  // notify IME of that before sending a selection change notification.
  if (!mIMEContentObserver->mNeedsToNotifyIMEOfTextChange) {
    // Be aware, PuppetWidget depends on the order of this. A selection change
    // notification should not be sent before a text change notification because
    // PuppetWidget shouldn't query new text content every selection change.
    if (mIMEContentObserver->mNeedsToNotifyIMEOfSelectionChange) {
      mIMEContentObserver->mNeedsToNotifyIMEOfSelectionChange = false;
      SendSelectionChange();
    }
  }

  // If a text change notification causes another text change again or a
  // selection change notification causes either a text change or another
  // selection change, we should notify IME of those before sending a position
  // change notification.
  if (!mIMEContentObserver->mNeedsToNotifyIMEOfTextChange &&
      !mIMEContentObserver->mNeedsToNotifyIMEOfSelectionChange) {
    if (mIMEContentObserver->mNeedsToNotifyIMEOfPositionChange) {
      mIMEContentObserver->mNeedsToNotifyIMEOfPositionChange = false;
      SendPositionChange();
    }
  }

  // Composition event handled notification should be sent after all the
  // other notifications because this notifies widget of finishing all pending
  // events are handled completely.
  if (!mIMEContentObserver->mNeedsToNotifyIMEOfTextChange &&
      !mIMEContentObserver->mNeedsToNotifyIMEOfSelectionChange &&
      !mIMEContentObserver->mNeedsToNotifyIMEOfPositionChange) {
    if (mIMEContentObserver->mNeedsToNotifyIMEOfCompositionEventHandled) {
      mIMEContentObserver->mNeedsToNotifyIMEOfCompositionEventHandled = false;
      SendCompositionEventHandled();
    }
  }

  mIMEContentObserver->mQueuedSender = nullptr;

  // If notifications caused some new change, we should notify them now.
  if (mIMEContentObserver->NeedsToNotifyIMEOfSomething()) {
    if (mIMEContentObserver->GetState() == eState_StoppedObserving) {
      MOZ_LOG(sIMECOLog, LogLevel::Debug,
        ("0x%p IMEContentObserver::IMENotificationSender::Run(), "
         "waiting IMENotificationSender to be reinitialized", this));
    } else {
      MOZ_LOG(sIMECOLog, LogLevel::Debug,
        ("0x%p IMEContentObserver::IMENotificationSender::Run(), "
         "posting IMENotificationSender to current thread", this));
      mIMEContentObserver->mQueuedSender =
        new IMENotificationSender(mIMEContentObserver);
      NS_DispatchToCurrentThread(mIMEContentObserver->mQueuedSender);
    }
  }
  return NS_OK;
}

void
IMEContentObserver::IMENotificationSender::SendFocusSet()
{
  if (!CanNotifyIME(eChangeEventType_Focus)) {
    // If IMEContentObserver has already gone, we don't need to notify IME of
    // focus.
    MOZ_LOG(sIMECOLog, LogLevel::Debug,
      ("0x%p IMEContentObserver::IMENotificationSender::"
       "SendFocusSet(), FAILED, due to impossible to notify IME of focus",
       this));
    mIMEContentObserver->ClearPendingNotifications();
    return;
  }

  if (!IsSafeToNotifyIME(eChangeEventType_Focus)) {
    MOZ_LOG(sIMECOLog, LogLevel::Debug,
      ("0x%p   IMEContentObserver::IMENotificationSender::"
       "SendFocusSet(), retrying to send NOTIFY_IME_OF_FOCUS...", this));
    mIMEContentObserver->PostFocusSetNotification();
    return;
  }

  mIMEContentObserver->mIMEHasFocus = true;
  // Initialize selection cache with the first selection data.
  mIMEContentObserver->UpdateSelectionCache();

  MOZ_LOG(sIMECOLog, LogLevel::Info,
    ("0x%p IMEContentObserver::IMENotificationSender::"
     "SendFocusSet(), sending NOTIFY_IME_OF_FOCUS...", this));

  MOZ_RELEASE_ASSERT(mIMEContentObserver->mSendingNotification ==
                       NOTIFY_IME_OF_NOTHING);
  mIMEContentObserver->mSendingNotification = NOTIFY_IME_OF_FOCUS;
  IMEStateManager::NotifyIME(IMENotification(NOTIFY_IME_OF_FOCUS),
                             mIMEContentObserver->mWidget);
  mIMEContentObserver->mSendingNotification = NOTIFY_IME_OF_NOTHING;

  // nsIMEUpdatePreference referred by ObserveEditableNode() may be different
  // before or after widget receives NOTIFY_IME_OF_FOCUS.  Therefore, we need
  // to guarantee to call ObserveEditableNode() after sending
  // NOTIFY_IME_OF_FOCUS.
  mIMEContentObserver->OnIMEReceivedFocus();

  MOZ_LOG(sIMECOLog, LogLevel::Debug,
    ("0x%p IMEContentObserver::IMENotificationSender::"
     "SendFocusSet(), sent NOTIFY_IME_OF_FOCUS", this));
}

void
IMEContentObserver::IMENotificationSender::SendSelectionChange()
{
  if (!CanNotifyIME(eChangeEventType_Selection)) {
    MOZ_LOG(sIMECOLog, LogLevel::Debug,
      ("0x%p IMEContentObserver::IMENotificationSender::"
       "SendSelectionChange(), FAILED, due to impossible to notify IME of "
       "selection change", this));
    return;
  }

  if (!IsSafeToNotifyIME(eChangeEventType_Selection)) {
    MOZ_LOG(sIMECOLog, LogLevel::Debug,
      ("0x%p   IMEContentObserver::IMENotificationSender::"
       "SendSelectionChange(), retrying to send "
       "NOTIFY_IME_OF_SELECTION_CHANGE...", this));
    mIMEContentObserver->PostSelectionChangeNotification();
    return;
  }

  SelectionChangeData lastSelChangeData = mIMEContentObserver->mSelectionData;
  if (NS_WARN_IF(!mIMEContentObserver->UpdateSelectionCache())) {
    MOZ_LOG(sIMECOLog, LogLevel::Error,
      ("0x%p IMEContentObserver::IMENotificationSender::"
       "SendSelectionChange(), FAILED, due to UpdateSelectionCache() failure",
       this));
    return;
  }

  // The state may be changed since querying content causes flushing layout.
  if (!CanNotifyIME(eChangeEventType_Selection)) {
    MOZ_LOG(sIMECOLog, LogLevel::Debug,
      ("0x%p IMEContentObserver::IMENotificationSender::"
       "SendSelectionChange(), FAILED, due to flushing layout having changed "
       "something", this));
    return;
  }

  // If the selection isn't changed actually, we shouldn't notify IME of
  // selection change.
  SelectionChangeData& newSelChangeData = mIMEContentObserver->mSelectionData;
  if (lastSelChangeData.IsValid() &&
      lastSelChangeData.mOffset == newSelChangeData.mOffset &&
      lastSelChangeData.String() == newSelChangeData.String() &&
      lastSelChangeData.GetWritingMode() == newSelChangeData.GetWritingMode() &&
      lastSelChangeData.mReversed == newSelChangeData.mReversed) {
    MOZ_LOG(sIMECOLog, LogLevel::Debug,
      ("0x%p IMEContentObserver::IMENotificationSender::"
       "SendSelectionChange(), not notifying IME of "
       "NOTIFY_IME_OF_SELECTION_CHANGE due to not changed actually", this));
    return;
  }

  MOZ_LOG(sIMECOLog, LogLevel::Info,
    ("0x%p IMEContentObserver::IMENotificationSender::"
     "SendSelectionChange(), sending NOTIFY_IME_OF_SELECTION_CHANGE... "
     "newSelChangeData=%s",
     this, SelectionChangeDataToString(newSelChangeData).get()));

  IMENotification notification(NOTIFY_IME_OF_SELECTION_CHANGE);
  notification.SetData(mIMEContentObserver->mSelectionData);

  MOZ_RELEASE_ASSERT(mIMEContentObserver->mSendingNotification ==
                       NOTIFY_IME_OF_NOTHING);
  mIMEContentObserver->mSendingNotification = NOTIFY_IME_OF_SELECTION_CHANGE;
  IMEStateManager::NotifyIME(notification, mIMEContentObserver->mWidget);
  mIMEContentObserver->mSendingNotification = NOTIFY_IME_OF_NOTHING;

  MOZ_LOG(sIMECOLog, LogLevel::Debug,
    ("0x%p IMEContentObserver::IMENotificationSender::"
     "SendSelectionChange(), sent NOTIFY_IME_OF_SELECTION_CHANGE", this));
}

void
IMEContentObserver::IMENotificationSender::SendTextChange()
{
  if (!CanNotifyIME(eChangeEventType_Text)) {
    MOZ_LOG(sIMECOLog, LogLevel::Debug,
      ("0x%p IMEContentObserver::IMENotificationSender::"
       "SendTextChange(), FAILED, due to impossible to notify IME of text "
       "change", this));
    return;
  }

  if (!IsSafeToNotifyIME(eChangeEventType_Text)) {
    MOZ_LOG(sIMECOLog, LogLevel::Debug,
      ("0x%p   IMEContentObserver::IMENotificationSender::"
       "SendTextChange(), retrying to send NOTIFY_IME_OF_TEXT_CHANGE...",
       this));
    mIMEContentObserver->PostTextChangeNotification();
    return;
  }

  MOZ_LOG(sIMECOLog, LogLevel::Info,
    ("0x%p IMEContentObserver::IMENotificationSender::"
     "SendTextChange(), sending NOTIFY_IME_OF_TEXT_CHANGE... "
     "mIMEContentObserver={ mTextChangeData=%s }",
     this, TextChangeDataToString(mIMEContentObserver->mTextChangeData).get()));

  IMENotification notification(NOTIFY_IME_OF_TEXT_CHANGE);
  notification.SetData(mIMEContentObserver->mTextChangeData);
  mIMEContentObserver->mTextChangeData.Clear();

  MOZ_RELEASE_ASSERT(mIMEContentObserver->mSendingNotification ==
                       NOTIFY_IME_OF_NOTHING);
  mIMEContentObserver->mSendingNotification = NOTIFY_IME_OF_TEXT_CHANGE;
  IMEStateManager::NotifyIME(notification, mIMEContentObserver->mWidget);
  mIMEContentObserver->mSendingNotification = NOTIFY_IME_OF_NOTHING;

  MOZ_LOG(sIMECOLog, LogLevel::Debug,
    ("0x%p IMEContentObserver::IMENotificationSender::"
     "SendTextChange(), sent NOTIFY_IME_OF_TEXT_CHANGE", this));
}

void
IMEContentObserver::IMENotificationSender::SendPositionChange()
{
  if (!CanNotifyIME(eChangeEventType_Position)) {
    MOZ_LOG(sIMECOLog, LogLevel::Debug,
      ("0x%p IMEContentObserver::IMENotificationSender::"
       "SendPositionChange(), FAILED, due to impossible to notify IME of "
       "position change", this));
    return;
  }

  if (!IsSafeToNotifyIME(eChangeEventType_Position)) {
    MOZ_LOG(sIMECOLog, LogLevel::Debug,
      ("0x%p   IMEContentObserver::IMENotificationSender::"
       "SendPositionChange(), retrying to send "
       "NOTIFY_IME_OF_POSITION_CHANGE...", this));
    mIMEContentObserver->PostPositionChangeNotification();
    return;
  }

  MOZ_LOG(sIMECOLog, LogLevel::Info,
    ("0x%p IMEContentObserver::IMENotificationSender::"
     "SendPositionChange(), sending NOTIFY_IME_OF_POSITION_CHANGE...", this));

  MOZ_RELEASE_ASSERT(mIMEContentObserver->mSendingNotification ==
                       NOTIFY_IME_OF_NOTHING);
  mIMEContentObserver->mSendingNotification = NOTIFY_IME_OF_POSITION_CHANGE;
  IMEStateManager::NotifyIME(IMENotification(NOTIFY_IME_OF_POSITION_CHANGE),
                             mIMEContentObserver->mWidget);
  mIMEContentObserver->mSendingNotification = NOTIFY_IME_OF_NOTHING;

  MOZ_LOG(sIMECOLog, LogLevel::Debug,
    ("0x%p IMEContentObserver::IMENotificationSender::"
     "SendPositionChange(), sent NOTIFY_IME_OF_POSITION_CHANGE", this));
}

void
IMEContentObserver::IMENotificationSender::SendCompositionEventHandled()
{
  if (!CanNotifyIME(eChangeEventType_CompositionEventHandled)) {
    MOZ_LOG(sIMECOLog, LogLevel::Debug,
      ("0x%p IMEContentObserver::IMENotificationSender::"
       "SendCompositionEventHandled(), FAILED, due to impossible to notify "
       "IME of composition event handled", this));
    return;
  }

  if (!IsSafeToNotifyIME(eChangeEventType_CompositionEventHandled)) {
    MOZ_LOG(sIMECOLog, LogLevel::Debug,
      ("0x%p   IMEContentObserver::IMENotificationSender::"
       "SendCompositionEventHandled(), retrying to send "
       "NOTIFY_IME_OF_POSITION_CHANGE...", this));
    mIMEContentObserver->PostCompositionEventHandledNotification();
    return;
  }

  MOZ_LOG(sIMECOLog, LogLevel::Info,
    ("0x%p IMEContentObserver::IMENotificationSender::"
     "SendCompositionEventHandled(), sending "
     "NOTIFY_IME_OF_COMPOSITION_EVENT_HANDLED...", this));

  MOZ_RELEASE_ASSERT(mIMEContentObserver->mSendingNotification ==
                       NOTIFY_IME_OF_NOTHING);
  mIMEContentObserver->mSendingNotification =
                         NOTIFY_IME_OF_COMPOSITION_EVENT_HANDLED;
  IMEStateManager::NotifyIME(
                     IMENotification(NOTIFY_IME_OF_COMPOSITION_EVENT_HANDLED),
                     mIMEContentObserver->mWidget);
  mIMEContentObserver->mSendingNotification = NOTIFY_IME_OF_NOTHING;

  MOZ_LOG(sIMECOLog, LogLevel::Debug,
    ("0x%p IMEContentObserver::IMENotificationSender::"
     "SendCompositionEventHandled(), sent "
     "NOTIFY_IME_OF_COMPOSITION_EVENT_HANDLED", this));
}

} // namespace mozilla
