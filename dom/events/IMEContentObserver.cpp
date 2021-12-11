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
#include "mozilla/PresShell.h"
#include "mozilla/TextComposition.h"
#include "mozilla/TextEvents.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/Selection.h"
#include "nsContentUtils.h"
#include "nsGkAtoms.h"
#include "nsAtom.h"
#include "nsDocShell.h"
#include "nsIContent.h"
#include "mozilla/dom/Document.h"
#include "nsIFrame.h"
#include "nsINode.h"
#include "nsISelectionController.h"
#include "nsISupports.h"
#include "nsIWeakReferenceUtils.h"
#include "nsIWidget.h"
#include "nsPresContext.h"
#include "nsRange.h"
#include "nsRefreshDriver.h"
#include "WritingModes.h"

namespace mozilla {

using NodePosition = ContentEventHandler::NodePosition;
using NodePositionBefore = ContentEventHandler::NodePositionBefore;

using namespace dom;
using namespace widget;

LazyLogModule sIMECOLog("IMEContentObserver");

static const char* ToChar(bool aBool) { return aBool ? "true" : "false"; }

// This method determines the node to use for the point before the current node.
// If you have the following aContent and aContainer, and want to represent the
// following point for `NodePosition` or `RangeBoundary`:
//
// <parent> {node} {node} | {node} </parent>
//  ^                     ^     ^
// aContainer           point  aContent
//
// This function will shift `aContent` to the left into the format which
// `NodePosition` and `RangeBoundary` use:
//
// <parent> {node} {node} | {node} </parent>
//  ^               ^     ^
// aContainer    result  point
static nsIContent* PointBefore(nsINode* aContainer, nsIContent* aContent) {
  if (aContent) {
    return aContent->GetPreviousSibling();
  }
  return aContainer->GetLastChild();
}

/******************************************************************************
 * mozilla::IMEContentObserver
 ******************************************************************************/

NS_IMPL_CYCLE_COLLECTION_CLASS(IMEContentObserver)

// Note that we don't need to add mFirstAddedContainer nor
// mLastAddedContainer to cycle collection because they are non-null only
// during short time and shouldn't be touched while they are non-null.

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(IMEContentObserver)
  nsAutoScriptBlocker scriptBlocker;

  tmp->NotifyIMEOfBlur();
  tmp->UnregisterObservers();

  NS_IMPL_CYCLE_COLLECTION_UNLINK(mSelection)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mRootContent)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mEditableNode)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mDocShell)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mEditorBase)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mDocumentObserver)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mEndOfAddedTextCache.mContainerNode)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mStartOfRemovingTextRangeCache.mContainerNode)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_WEAK_REFERENCE

  tmp->mIMENotificationRequests = nullptr;
  tmp->mESM = nullptr;
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(IMEContentObserver)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mWidget)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mFocusedWidget)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mSelection)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mRootContent)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mEditableNode)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mDocShell)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mEditorBase)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mDocumentObserver)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mEndOfAddedTextCache.mContainerNode)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(
      mStartOfRemovingTextRangeCache.mContainerNode)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(IMEContentObserver)
  NS_INTERFACE_MAP_ENTRY(nsIMutationObserver)
  NS_INTERFACE_MAP_ENTRY(nsIReflowObserver)
  NS_INTERFACE_MAP_ENTRY(nsIScrollObserver)
  NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIReflowObserver)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(IMEContentObserver)
NS_IMPL_CYCLE_COLLECTING_RELEASE(IMEContentObserver)

IMEContentObserver::IMEContentObserver()
    : mESM(nullptr),
      mIMENotificationRequests(nullptr),
      mSuppressNotifications(0),
      mPreCharacterDataChangeLength(-1),
      mSendingNotification(NOTIFY_IME_OF_NOTHING),
      mIsObserving(false),
      mIMEHasFocus(false),
      mNeedsToNotifyIMEOfFocusSet(false),
      mNeedsToNotifyIMEOfTextChange(false),
      mNeedsToNotifyIMEOfSelectionChange(false),
      mNeedsToNotifyIMEOfPositionChange(false),
      mNeedsToNotifyIMEOfCompositionEventHandled(false),
      mIsHandlingQueryContentEvent(false) {
#ifdef DEBUG
  mTextChangeData.Test();
#endif
}

void IMEContentObserver::Init(nsIWidget& aWidget, nsPresContext& aPresContext,
                              nsIContent* aContent, EditorBase& aEditorBase) {
  State state = GetState();
  if (NS_WARN_IF(state == eState_Observing)) {
    return;  // Nothing to do.
  }

  bool firstInitialization = state != eState_StoppedObserving;
  if (!firstInitialization) {
    // If this is now trying to initialize with new contents, all observers
    // should be registered again for simpler implementation.
    UnregisterObservers();
    Clear();
  }

  mESM = aPresContext.EventStateManager();
  mESM->OnStartToObserveContent(this);

  mWidget = &aWidget;
  mIMENotificationRequests = &mWidget->IMENotificationRequestsRef();

  if (!InitWithEditor(aPresContext, aContent, aEditorBase)) {
    MOZ_LOG(sIMECOLog, LogLevel::Debug,
            ("0x%p IMEContentObserver::Init() FAILED, due to InitWithEditor() "
             "failure",
             this));
    Clear();
    return;
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

void IMEContentObserver::OnIMEReceivedFocus() {
  // While Init() notifies IME of focus, pending layout may be flushed
  // because the notification may cause querying content.  Then, recursive
  // call of Init() with the latest content may occur.  In such case, we
  // shouldn't keep first initialization which notified IME of focus.
  if (GetState() != eState_Initializing) {
    MOZ_LOG(sIMECOLog, LogLevel::Debug,
            ("0x%p IMEContentObserver::OnIMEReceivedFocus(), "
             "but the state is not \"initializing\", so does nothing",
             this));
    return;
  }

  // NOTIFY_IME_OF_FOCUS might cause recreating IMEContentObserver
  // instance via IMEStateManager::UpdateIMEState().  So, this
  // instance might already have been destroyed, check it.
  if (!mRootContent) {
    MOZ_LOG(sIMECOLog, LogLevel::Debug,
            ("0x%p IMEContentObserver::OnIMEReceivedFocus(), "
             "but mRootContent has already been cleared, so does nothing",
             this));
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

bool IMEContentObserver::InitWithEditor(nsPresContext& aPresContext,
                                        nsIContent* aContent,
                                        EditorBase& aEditorBase) {
  mEditableNode = IMEStateManager::GetRootEditableNode(&aPresContext, aContent);
  if (NS_WARN_IF(!mEditableNode)) {
    return false;
  }

  mEditorBase = &aEditorBase;

  RefPtr<PresShell> presShell = aPresContext.GetPresShell();

  // get selection and root content
  nsCOMPtr<nsISelectionController> selCon;
  if (mEditableNode->IsContent()) {
    nsIFrame* frame = mEditableNode->AsContent()->GetPrimaryFrame();
    if (NS_WARN_IF(!frame)) {
      return false;
    }

    frame->GetSelectionController(&aPresContext, getter_AddRefs(selCon));
  } else {
    // mEditableNode is a document
    selCon = presShell;
  }

  if (NS_WARN_IF(!selCon)) {
    return false;
  }

  mSelection = selCon->GetSelection(nsISelectionController::SELECTION_NORMAL);
  if (NS_WARN_IF(!mSelection)) {
    return false;
  }

  if (const nsRange* selRange = mSelection->GetRangeAt(0)) {
    if (NS_WARN_IF(!selRange->GetStartContainer())) {
      return false;
    }

    nsCOMPtr<nsINode> startContainer = selRange->GetStartContainer();
    mRootContent = startContainer->GetSelectionRootContent(presShell);
  } else {
    nsCOMPtr<nsINode> editableNode = mEditableNode;
    mRootContent = editableNode->GetSelectionRootContent(presShell);
  }
  if (!mRootContent && mEditableNode->IsDocument()) {
    // The document node is editable, but there are no contents, this document
    // is not editable.
    return false;
  }

  if (NS_WARN_IF(!mRootContent)) {
    return false;
  }

  mDocShell = aPresContext.GetDocShell();
  if (NS_WARN_IF(!mDocShell)) {
    return false;
  }

  mDocumentObserver = new DocumentObserver(*this);

  return true;
}

void IMEContentObserver::Clear() {
  mEditorBase = nullptr;
  mSelection = nullptr;
  mEditableNode = nullptr;
  mRootContent = nullptr;
  mDocShell = nullptr;
  // Should be safe to clear mDocumentObserver here even though it grabs
  // this instance in most cases because this is called by Init() or Destroy().
  // The callers of Init() grab this instance with local RefPtr.
  // The caller of Destroy() also grabs this instance with local RefPtr.
  // So, this won't cause refcount of this instance become 0.
  mDocumentObserver = nullptr;
}

void IMEContentObserver::ObserveEditableNode() {
  MOZ_RELEASE_ASSERT(mSelection);
  MOZ_RELEASE_ASSERT(mRootContent);
  MOZ_RELEASE_ASSERT(GetState() != eState_Observing);

  // If this is called before sending NOTIFY_IME_OF_FOCUS (it's possible when
  // the editor is reframed before sending NOTIFY_IME_OF_FOCUS asynchronously),
  // the notification requests of mWidget may be different from after the widget
  // receives NOTIFY_IME_OF_FOCUS.   So, this should be called again by
  // OnIMEReceivedFocus() which is called after sending NOTIFY_IME_OF_FOCUS.
  if (!mIMEHasFocus) {
    MOZ_ASSERT(!mWidget || mNeedsToNotifyIMEOfFocusSet ||
                   mSendingNotification == NOTIFY_IME_OF_FOCUS,
               "Wow, OnIMEReceivedFocus() won't be called?");
    return;
  }

  mIsObserving = true;
  if (mEditorBase) {
    mEditorBase->SetIMEContentObserver(this);
  }

  mRootContent->AddMutationObserver(this);
  // If it's in a document (should be so), we can use document observer to
  // reduce redundant computation of text change offsets.
  Document* doc = mRootContent->GetComposedDoc();
  if (doc) {
    RefPtr<DocumentObserver> documentObserver = mDocumentObserver;
    documentObserver->Observe(doc);
  }

  if (mDocShell) {
    // Add scroll position listener and reflow observer to detect position
    // and size changes
    mDocShell->AddWeakScrollObserver(this);
    mDocShell->AddWeakReflowObserver(this);
  }
}

void IMEContentObserver::NotifyIMEOfBlur() {
  // Prevent any notifications to be sent IME.
  nsCOMPtr<nsIWidget> widget;
  mWidget.swap(widget);
  mIMENotificationRequests = nullptr;

  // If we hasn't been set focus, we shouldn't send blur notification to IME.
  if (!mIMEHasFocus) {
    return;
  }

  // mWidget must have been non-nullptr if IME has focus.
  MOZ_RELEASE_ASSERT(widget);

  RefPtr<IMEContentObserver> kungFuDeathGrip(this);

  MOZ_LOG(sIMECOLog, LogLevel::Info,
          ("0x%p IMEContentObserver::NotifyIMEOfBlur(), "
           "sending NOTIFY_IME_OF_BLUR",
           this));

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
           "sent NOTIFY_IME_OF_BLUR",
           this));
}

void IMEContentObserver::UnregisterObservers() {
  if (!mIsObserving) {
    return;
  }
  mIsObserving = false;

  if (mEditorBase) {
    mEditorBase->SetIMEContentObserver(nullptr);
  }

  if (mSelection) {
    mSelectionData.Clear();
    mFocusedWidget = nullptr;
  }

  if (mRootContent) {
    mRootContent->RemoveMutationObserver(this);
  }

  if (mDocumentObserver) {
    RefPtr<DocumentObserver> documentObserver = mDocumentObserver;
    documentObserver->StopObserving();
  }

  if (mDocShell) {
    mDocShell->RemoveWeakScrollObserver(this);
    mDocShell->RemoveWeakReflowObserver(this);
  }
}

nsPresContext* IMEContentObserver::GetPresContext() const {
  return mESM ? mESM->GetPresContext() : nullptr;
}

void IMEContentObserver::Destroy() {
  // WARNING: When you change this method, you have to check Unlink() too.

  // Note that don't send any notifications later from here.  I.e., notify
  // IMEStateManager of the blur synchronously because IMEStateManager needs to
  // stop notifying the main process if this is requested by the main process.
  NotifyIMEOfBlur();
  UnregisterObservers();
  Clear();

  mWidget = nullptr;
  mIMENotificationRequests = nullptr;

  if (mESM) {
    mESM->OnStopObservingContent(this);
    mESM = nullptr;
  }
}

bool IMEContentObserver::Destroyed() const { return !mWidget; }

void IMEContentObserver::DisconnectFromEventStateManager() { mESM = nullptr; }

bool IMEContentObserver::MaybeReinitialize(nsIWidget& aWidget,
                                           nsPresContext& aPresContext,
                                           nsIContent* aContent,
                                           EditorBase& aEditorBase) {
  if (!IsObservingContent(&aPresContext, aContent)) {
    return false;
  }

  if (GetState() == eState_StoppedObserving) {
    Init(aWidget, aPresContext, aContent, aEditorBase);
  }
  return IsManaging(&aPresContext, aContent);
}

bool IMEContentObserver::IsManaging(nsPresContext* aPresContext,
                                    nsIContent* aContent) const {
  return GetState() == eState_Observing &&
         IsObservingContent(aPresContext, aContent);
}

bool IMEContentObserver::IsBeingInitializedFor(nsPresContext* aPresContext,
                                               nsIContent* aContent) const {
  return GetState() == eState_Initializing &&
         IsObservingContent(aPresContext, aContent);
}

bool IMEContentObserver::IsManaging(const TextComposition* aComposition) const {
  if (GetState() != eState_Observing) {
    return false;
  }
  nsPresContext* presContext = aComposition->GetPresContext();
  if (NS_WARN_IF(!presContext)) {
    return false;
  }
  if (presContext != GetPresContext()) {
    return false;  // observing different document
  }
  nsINode* targetNode = aComposition->GetEventTargetNode();
  nsIContent* targetContent =
      targetNode && targetNode->IsContent() ? targetNode->AsContent() : nullptr;
  return IsObservingContent(presContext, targetContent);
}

IMEContentObserver::State IMEContentObserver::GetState() const {
  if (!mSelection || !mRootContent || !mEditableNode) {
    return eState_NotObserving;  // failed to initialize or finalized.
  }
  if (!mRootContent->IsInComposedDoc()) {
    // the focused editor has already been reframed.
    return eState_StoppedObserving;
  }
  return mIsObserving ? eState_Observing : eState_Initializing;
}

bool IMEContentObserver::IsObservingContent(nsPresContext* aPresContext,
                                            nsIContent* aContent) const {
  return mEditableNode ==
         IMEStateManager::GetRootEditableNode(aPresContext, aContent);
}

bool IMEContentObserver::IsEditorHandlingEventForComposition() const {
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

bool IMEContentObserver::IsEditorComposing() const {
  // Note that don't use TextComposition here. The important thing is,
  // whether the editor already started to handle composition because
  // web contents can change selection, text content and/or something from
  // compositionstart event listener which is run before EditorBase handles it.
  if (NS_WARN_IF(!mEditorBase)) {
    return false;
  }
  return mEditorBase->IsIMEComposing();
}

nsresult IMEContentObserver::GetSelectionAndRoot(
    Selection** aSelection, nsIContent** aRootContent) const {
  if (!mEditableNode || !mSelection) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  NS_ASSERTION(mSelection && mRootContent, "uninitialized content observer");
  NS_ADDREF(*aSelection = mSelection);
  NS_ADDREF(*aRootContent = mRootContent);
  return NS_OK;
}

void IMEContentObserver::OnSelectionChange(Selection& aSelection) {
  if (!mIsObserving) {
    return;
  }

  if (aSelection.RangeCount() && mWidget) {
    bool causedByComposition = IsEditorHandlingEventForComposition();
    bool causedBySelectionEvent = TextComposition::IsHandlingSelectionEvent();
    bool duringComposition = IsEditorComposing();
    MaybeNotifyIMEOfSelectionChange(causedByComposition, causedBySelectionEvent,
                                    duringComposition);
  }
}

void IMEContentObserver::ScrollPositionChanged() {
  if (!NeedsPositionChangeNotification()) {
    return;
  }

  MaybeNotifyIMEOfPositionChange();
}

NS_IMETHODIMP
IMEContentObserver::Reflow(DOMHighResTimeStamp aStart,
                           DOMHighResTimeStamp aEnd) {
  if (!NeedsPositionChangeNotification()) {
    return NS_OK;
  }

  MaybeNotifyIMEOfPositionChange();
  return NS_OK;
}

NS_IMETHODIMP
IMEContentObserver::ReflowInterruptible(DOMHighResTimeStamp aStart,
                                        DOMHighResTimeStamp aEnd) {
  if (!NeedsPositionChangeNotification()) {
    return NS_OK;
  }

  MaybeNotifyIMEOfPositionChange();
  return NS_OK;
}

nsresult IMEContentObserver::HandleQueryContentEvent(
    WidgetQueryContentEvent* aEvent) {
  // If the instance has normal selection cache and the query event queries
  // normal selection's range, it should use the cached selection which was
  // sent to the widget.  However, if this instance has already received new
  // selection change notification but hasn't updated the cache yet (i.e.,
  // not sending selection change notification to IME, don't use the cached
  // value.  Note that don't update selection cache here since if you update
  // selection cache here, IMENotificationSender won't notify IME of selection
  // change because it looks like that the selection isn't actually changed.
  bool isSelectionCacheAvailable = aEvent->mUseNativeLineBreak &&
                                   mSelectionData.IsValid() &&
                                   !mNeedsToNotifyIMEOfSelectionChange;
  if (isSelectionCacheAvailable && aEvent->mMessage == eQuerySelectedText &&
      aEvent->mInput.mSelectionType == SelectionType::eNormal) {
    aEvent->EmplaceReply();
    aEvent->mReply->mOffsetAndData.emplace(mSelectionData.mOffset,
                                           mSelectionData.String(),
                                           OffsetAndDataFor::SelectedString);
    aEvent->mReply->mContentsRoot = mRootContent;
    aEvent->mReply->mHasSelection = !mSelectionData.IsCollapsed();
    aEvent->mReply->mWritingMode = mSelectionData.GetWritingMode();
    aEvent->mReply->mReversed = mSelectionData.mReversed;
    MOZ_LOG(sIMECOLog, LogLevel::Debug,
            ("0x%p IMEContentObserver::HandleQueryContentEvent(aEvent={ "
             "mMessage=%s, mReply=%s })",
             this, ToChar(aEvent->mMessage), ToString(aEvent->mReply).c_str()));
    return NS_OK;
  }

  MOZ_LOG(sIMECOLog, LogLevel::Debug,
          ("0x%p IMEContentObserver::HandleQueryContentEvent(aEvent={ "
           "mMessage=%s })",
           this, ToChar(aEvent->mMessage)));

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
  if (NS_WARN_IF(Destroyed())) {
    // If this has already destroyed during querying the content, the query
    // is outdated even if it's succeeded.  So, make the query fail.
    aEvent->mReply.reset();
    MOZ_LOG(sIMECOLog, LogLevel::Warning,
            ("0x%p IMEContentObserver::HandleQueryContentEvent(), WARNING, "
             "IMEContentObserver has been destroyed during the query, "
             "making the query fail",
             this));
    return rv;
  }

  if (aEvent->Succeeded() &&
      NS_WARN_IF(aEvent->mReply->mContentsRoot != mRootContent)) {
    // Focus has changed unexpectedly, so make the query fail.
    aEvent->mReply.reset();
  }
  return rv;
}

bool IMEContentObserver::OnMouseButtonEvent(nsPresContext* aPresContext,
                                            WidgetMouseEvent* aMouseEvent) {
  if (!mIMENotificationRequests ||
      !mIMENotificationRequests->WantMouseButtonEventOnChar()) {
    return false;
  }
  if (!aMouseEvent->IsTrusted() || aMouseEvent->DefaultPrevented() ||
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

  WidgetQueryContentEvent queryCharAtPointEvent(true, eQueryCharacterAtPoint,
                                                aMouseEvent->mWidget);
  queryCharAtPointEvent.mRefPoint = aMouseEvent->mRefPoint;
  ContentEventHandler handler(aPresContext);
  handler.OnQueryCharacterAtPoint(&queryCharAtPointEvent);
  if (NS_WARN_IF(queryCharAtPointEvent.Failed()) ||
      queryCharAtPointEvent.DidNotFindChar()) {
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
    queryCharAtPointEvent.mReply->mRect.MoveBy(
        topLevelWidget->WidgetToScreenOffset() -
        mWidget->WidgetToScreenOffset());
  }
  // The refPt is relative to its widget.
  // We should notify it with offset in the widget.
  if (aMouseEvent->mWidget != mWidget) {
    queryCharAtPointEvent.mRefPoint +=
        aMouseEvent->mWidget->WidgetToScreenOffset() -
        mWidget->WidgetToScreenOffset();
  }

  IMENotification notification(NOTIFY_IME_OF_MOUSE_BUTTON_EVENT);
  notification.mMouseButtonEventData.mEventMessage = aMouseEvent->mMessage;
  notification.mMouseButtonEventData.mOffset =
      queryCharAtPointEvent.mReply->StartOffset();
  notification.mMouseButtonEventData.mCursorPos =
      queryCharAtPointEvent.mRefPoint;
  notification.mMouseButtonEventData.mCharRect =
      queryCharAtPointEvent.mReply->mRect;
  notification.mMouseButtonEventData.mButton = aMouseEvent->mButton;
  notification.mMouseButtonEventData.mButtons = aMouseEvent->mButtons;
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

void IMEContentObserver::CharacterDataWillChange(
    nsIContent* aContent, const CharacterDataChangeInfo& aInfo) {
  if (!aContent->IsText()) {
    return;  // Ignore if it's a comment node or something other invisible data
             // node.
  }
  MOZ_ASSERT(mPreCharacterDataChangeLength < 0,
             "CharacterDataChanged() should've reset "
             "mPreCharacterDataChangeLength");

  if (!NeedsTextChangeNotification() ||
      !nsContentUtils::IsInSameAnonymousTree(mRootContent, aContent)) {
    return;
  }

  mEndOfAddedTextCache.Clear();
  mStartOfRemovingTextRangeCache.Clear();

  // Although we don't assume this change occurs while this is storing
  // the range of added consecutive nodes, if it actually happens, we need to
  // flush them since this change may occur before or in the range.  So, it's
  // safe to flush pending computation of mTextChangeData before handling this.
  MaybeNotifyIMEOfAddedTextDuringDocumentChange();

  mPreCharacterDataChangeLength = ContentEventHandler::GetNativeTextLength(
      *aContent->AsText(), aInfo.mChangeStart, aInfo.mChangeEnd);
  MOZ_ASSERT(
      mPreCharacterDataChangeLength >= aInfo.mChangeEnd - aInfo.mChangeStart,
      "The computed length must be same as or larger than XP length");
}

void IMEContentObserver::CharacterDataChanged(
    nsIContent* aContent, const CharacterDataChangeInfo& aInfo) {
  if (!aContent->IsText()) {
    return;  // Ignore if it's a comment node or something other invisible data
             // node.
  }

  if (!NeedsTextChangeNotification() ||
      !nsContentUtils::IsInSameAnonymousTree(mRootContent, aContent)) {
    return;
  }

  mEndOfAddedTextCache.Clear();
  mStartOfRemovingTextRangeCache.Clear();
  MOZ_ASSERT(
      !HasAddedNodesDuringDocumentChange(),
      "The stored range should be flushed before actually the data is changed");

  int64_t removedLength = mPreCharacterDataChangeLength;
  mPreCharacterDataChangeLength = -1;

  MOZ_ASSERT(removedLength >= 0,
             "mPreCharacterDataChangeLength should've been set by "
             "CharacterDataWillChange()");

  uint32_t offset = 0;
  // get offsets of change and fire notification
  nsresult rv = ContentEventHandler::GetFlatTextLengthInRange(
      NodePosition(mRootContent, 0), NodePosition(aContent, aInfo.mChangeStart),
      mRootContent, &offset, LINE_BREAK_TYPE_NATIVE);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return;
  }

  uint32_t newLength = ContentEventHandler::GetNativeTextLength(
      *aContent->AsText(), aInfo.mChangeStart,
      aInfo.mChangeStart + aInfo.mReplaceLength);

  uint32_t oldEnd = offset + static_cast<uint32_t>(removedLength);
  uint32_t newEnd = offset + newLength;

  TextChangeData data(offset, oldEnd, newEnd,
                      IsEditorHandlingEventForComposition(),
                      IsEditorComposing());
  MaybeNotifyIMEOfTextChange(data);
}

void IMEContentObserver::NotifyContentAdded(nsINode* aContainer,
                                            nsIContent* aFirstContent,
                                            nsIContent* aLastContent) {
  if (!NeedsTextChangeNotification() ||
      !nsContentUtils::IsInSameAnonymousTree(mRootContent, aFirstContent)) {
    return;
  }

  MOZ_ASSERT_IF(aFirstContent, aFirstContent->GetParentNode() == aContainer);
  MOZ_ASSERT_IF(aLastContent, aLastContent->GetParentNode() == aContainer);

  mStartOfRemovingTextRangeCache.Clear();

  // If it's in a document change, nodes are added consecutively.  Therefore,
  // if we cache the first node and the last node, we need to compute the
  // range once.
  // FYI: This is not true if the change caused by an operation in the editor.
  if (IsInDocumentChange()) {
    // Now, mEndOfAddedTextCache may be invalid if node is added before
    // the last node in mEndOfAddedTextCache.  Clear it.
    mEndOfAddedTextCache.Clear();
    if (!HasAddedNodesDuringDocumentChange()) {
      mFirstAddedContainer = mLastAddedContainer = aContainer;
      mFirstAddedContent = aFirstContent;
      mLastAddedContent = aLastContent;
      MOZ_LOG(sIMECOLog, LogLevel::Debug,
              ("0x%p IMEContentObserver::NotifyContentAdded(), starts to store "
               "consecutive added nodes",
               this));
      return;
    }
    // If first node being added is not next node of the last node,
    // notify IME of the previous range first, then, restart to cache the
    // range.
    if (NS_WARN_IF(!IsNextNodeOfLastAddedNode(aContainer, aFirstContent))) {
      // Flush the old range first.
      MaybeNotifyIMEOfAddedTextDuringDocumentChange();
      mFirstAddedContainer = aContainer;
      mFirstAddedContent = aFirstContent;
      MOZ_LOG(sIMECOLog, LogLevel::Debug,
              ("0x%p IMEContentObserver::NotifyContentAdded(), starts to store "
               "consecutive added nodes",
               this));
    }
    mLastAddedContainer = aContainer;
    mLastAddedContent = aLastContent;
    return;
  }
  MOZ_ASSERT(!HasAddedNodesDuringDocumentChange(),
             "The cache should be cleared when document change finished");

  uint32_t offset = 0;
  nsresult rv = NS_OK;
  if (!mEndOfAddedTextCache.Match(aContainer,
                                  aFirstContent->GetPreviousSibling())) {
    mEndOfAddedTextCache.Clear();
    rv = ContentEventHandler::GetFlatTextLengthInRange(
        NodePosition(mRootContent, 0),
        NodePositionBefore(aContainer, PointBefore(aContainer, aFirstContent)),
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
      NodePositionBefore(aContainer, PointBefore(aContainer, aFirstContent)),
      NodePosition(aContainer, aLastContent), mRootContent, &addingLength,
      LINE_BREAK_TYPE_NATIVE);
  if (NS_WARN_IF(NS_FAILED((rv)))) {
    mEndOfAddedTextCache.Clear();
    return;
  }

  // If multiple lines are being inserted in an HTML editor, next call of
  // NotifyContentAdded() is for adding next node.  Therefore, caching the text
  // length can skip to compute the text length before the adding node and
  // before of it.
  mEndOfAddedTextCache.Cache(aContainer, aLastContent, offset + addingLength);

  if (!addingLength) {
    return;
  }

  TextChangeData data(offset, offset, offset + addingLength,
                      IsEditorHandlingEventForComposition(),
                      IsEditorComposing());
  MaybeNotifyIMEOfTextChange(data);
}

void IMEContentObserver::ContentAppended(nsIContent* aFirstNewContent) {
  nsIContent* parent = aFirstNewContent->GetParent();
  MOZ_ASSERT(parent);
  NotifyContentAdded(parent, aFirstNewContent, parent->GetLastChild());
}

void IMEContentObserver::ContentInserted(nsIContent* aChild) {
  MOZ_ASSERT(aChild);
  NotifyContentAdded(aChild->GetParentNode(), aChild, aChild);
}

void IMEContentObserver::ContentRemoved(nsIContent* aChild,
                                        nsIContent* aPreviousSibling) {
  if (!NeedsTextChangeNotification() ||
      !nsContentUtils::IsInSameAnonymousTree(mRootContent, aChild)) {
    return;
  }

  mEndOfAddedTextCache.Clear();
  MaybeNotifyIMEOfAddedTextDuringDocumentChange();

  nsINode* containerNode = aChild->GetParentNode();
  MOZ_ASSERT(containerNode);

  uint32_t offset = 0;
  nsresult rv = NS_OK;
  if (!mStartOfRemovingTextRangeCache.Match(containerNode, aPreviousSibling)) {
    // At removing a child node of aContainer, we need the line break caused
    // by open tag of aContainer.  Be careful when aPreviousSibling is nullptr.

    rv = ContentEventHandler::GetFlatTextLengthInRange(
        NodePosition(mRootContent, 0),
        NodePosition(containerNode, aPreviousSibling), mRootContent, &offset,
        LINE_BREAK_TYPE_NATIVE);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      mStartOfRemovingTextRangeCache.Clear();
      return;
    }
    mStartOfRemovingTextRangeCache.Cache(containerNode, aPreviousSibling,
                                         offset);
  } else {
    offset = mStartOfRemovingTextRangeCache.mFlatTextLength;
  }

  // get offset at the end of the deleted node
  uint32_t textLength = 0;
  if (const Text* textNode = Text::FromNode(aChild)) {
    textLength = ContentEventHandler::GetNativeTextLength(*textNode);
  } else {
    uint32_t nodeLength = static_cast<int32_t>(aChild->GetChildCount());
    rv = ContentEventHandler::GetFlatTextLengthInRange(
        NodePositionBefore(aChild, 0), NodePosition(aChild, nodeLength),
        mRootContent, &textLength, LINE_BREAK_TYPE_NATIVE, true);
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

void IMEContentObserver::ClearAddedNodesDuringDocumentChange() {
  mFirstAddedContainer = mLastAddedContainer = nullptr;
  mFirstAddedContent = mLastAddedContent = nullptr;
  MOZ_LOG(sIMECOLog, LogLevel::Debug,
          ("0x%p IMEContentObserver::ClearAddedNodesDuringDocumentChange()"
           ", finished storing consecutive nodes",
           this));
}

bool IMEContentObserver::IsNextNodeOfLastAddedNode(nsINode* aParent,
                                                   nsIContent* aChild) const {
  MOZ_ASSERT(aParent);
  MOZ_ASSERT(aChild && aChild->GetParentNode() == aParent);
  MOZ_ASSERT(mRootContent);
  MOZ_ASSERT(HasAddedNodesDuringDocumentChange());

  // If the parent node isn't changed, we can check that mLastAddedContent has
  // aChild as its next sibling.
  if (aParent == mLastAddedContainer) {
    if (NS_WARN_IF(mLastAddedContent->GetNextSibling() != aChild)) {
      return false;
    }
    return true;
  }

  // If the parent node is changed, that means that the recorded last added node
  // shouldn't have a sibling.
  if (NS_WARN_IF(mLastAddedContent->GetNextSibling())) {
    return false;
  }

  // If the node is aParent is a descendant of mLastAddedContainer,
  // aChild should be the first child in the new container.
  if (mLastAddedContainer == aParent->GetParent()) {
    if (NS_WARN_IF(aChild->GetPreviousSibling())) {
      return false;
    }
    return true;
  }

  // Otherwise, we need to check it even with slow path.
  nsIContent* nextContentOfLastAddedContent =
      mLastAddedContent->GetNextNode(mRootContent->GetParentNode());
  if (NS_WARN_IF(!nextContentOfLastAddedContent)) {
    return false;
  }
  if (NS_WARN_IF(nextContentOfLastAddedContent != aChild)) {
    return false;
  }
  return true;
}

void IMEContentObserver::MaybeNotifyIMEOfAddedTextDuringDocumentChange() {
  if (!HasAddedNodesDuringDocumentChange()) {
    return;
  }

  MOZ_LOG(sIMECOLog, LogLevel::Debug,
          ("0x%p "
           "IMEContentObserver::MaybeNotifyIMEOfAddedTextDuringDocumentChange()"
           ", flushing stored consecutive nodes",
           this));

  // Notify IME of text change which is caused by added nodes now.

  // First, compute offset of start of first added node from start of the
  // editor.
  uint32_t offset;
  nsresult rv = ContentEventHandler::GetFlatTextLengthInRange(
      NodePosition(mRootContent, 0),
      NodePosition(mFirstAddedContainer,
                   PointBefore(mFirstAddedContainer, mFirstAddedContent)),
      mRootContent, &offset, LINE_BREAK_TYPE_NATIVE);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    ClearAddedNodesDuringDocumentChange();
    return;
  }

  // Next, compute the text length of added nodes.
  uint32_t length;
  rv = ContentEventHandler::GetFlatTextLengthInRange(
      NodePosition(mFirstAddedContainer,
                   PointBefore(mFirstAddedContainer, mFirstAddedContent)),
      NodePosition(mLastAddedContainer, mLastAddedContent), mRootContent,
      &length, LINE_BREAK_TYPE_NATIVE);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    ClearAddedNodesDuringDocumentChange();
    return;
  }

  // Finally, try to notify IME of the range.
  TextChangeData data(offset, offset, offset + length,
                      IsEditorHandlingEventForComposition(),
                      IsEditorComposing());
  MaybeNotifyIMEOfTextChange(data);
  ClearAddedNodesDuringDocumentChange();
}

void IMEContentObserver::BeginDocumentUpdate() {
  MOZ_LOG(sIMECOLog, LogLevel::Debug,
          ("0x%p IMEContentObserver::BeginDocumentUpdate(), "
           "HasAddedNodesDuringDocumentChange()=%s",
           this, ToChar(HasAddedNodesDuringDocumentChange())));

  // If we're not in a nested document update, this will return early,
  // otherwise, it will handle flusing any changes currently pending before
  // entering a nested document update.
  MaybeNotifyIMEOfAddedTextDuringDocumentChange();
}

void IMEContentObserver::EndDocumentUpdate() {
  MOZ_LOG(sIMECOLog, LogLevel::Debug,
          ("0x%p IMEContentObserver::EndDocumentUpdate(), "
           "HasAddedNodesDuringDocumentChange()=%s",
           this, ToChar(HasAddedNodesDuringDocumentChange())));

  MaybeNotifyIMEOfAddedTextDuringDocumentChange();
}

void IMEContentObserver::SuppressNotifyingIME() {
  mSuppressNotifications++;

  MOZ_LOG(sIMECOLog, LogLevel::Debug,
          ("0x%p IMEContentObserver::SuppressNotifyingIME(), "
           "mSuppressNotifications=%u",
           this, mSuppressNotifications));
}

void IMEContentObserver::UnsuppressNotifyingIME() {
  MOZ_LOG(sIMECOLog, LogLevel::Debug,
          ("0x%p IMEContentObserver::UnsuppressNotifyingIME(), "
           "mSuppressNotifications=%u",
           this, mSuppressNotifications));

  if (!mSuppressNotifications || --mSuppressNotifications) {
    return;
  }
  FlushMergeableNotifications();
}

void IMEContentObserver::OnEditActionHandled() {
  MOZ_LOG(sIMECOLog, LogLevel::Debug,
          ("0x%p IMEContentObserver::EditAction()", this));

  mEndOfAddedTextCache.Clear();
  mStartOfRemovingTextRangeCache.Clear();
  FlushMergeableNotifications();
}

void IMEContentObserver::BeforeEditAction() {
  MOZ_LOG(sIMECOLog, LogLevel::Debug,
          ("0x%p IMEContentObserver::BeforeEditAction()", this));

  mEndOfAddedTextCache.Clear();
  mStartOfRemovingTextRangeCache.Clear();
}

void IMEContentObserver::CancelEditAction() {
  MOZ_LOG(sIMECOLog, LogLevel::Debug,
          ("0x%p IMEContentObserver::CancelEditAction()", this));

  mEndOfAddedTextCache.Clear();
  mStartOfRemovingTextRangeCache.Clear();
  FlushMergeableNotifications();
}

void IMEContentObserver::PostFocusSetNotification() {
  MOZ_LOG(sIMECOLog, LogLevel::Debug,
          ("0x%p IMEContentObserver::PostFocusSetNotification()", this));

  mNeedsToNotifyIMEOfFocusSet = true;
}

void IMEContentObserver::PostTextChangeNotification() {
  MOZ_LOG(sIMECOLog, LogLevel::Debug,
          ("0x%p IMEContentObserver::PostTextChangeNotification("
           "mTextChangeData=%s)",
           this, ToString(mTextChangeData).c_str()));

  MOZ_ASSERT(mTextChangeData.IsValid(),
             "mTextChangeData must have text change data");
  mNeedsToNotifyIMEOfTextChange = true;
}

void IMEContentObserver::PostSelectionChangeNotification() {
  MOZ_LOG(
      sIMECOLog, LogLevel::Debug,
      ("0x%p IMEContentObserver::PostSelectionChangeNotification(), "
       "mSelectionData={ mCausedByComposition=%s, mCausedBySelectionEvent=%s }",
       this, ToChar(mSelectionData.mCausedByComposition),
       ToChar(mSelectionData.mCausedBySelectionEvent)));

  mNeedsToNotifyIMEOfSelectionChange = true;
}

void IMEContentObserver::MaybeNotifyIMEOfFocusSet() {
  MOZ_LOG(sIMECOLog, LogLevel::Debug,
          ("0x%p IMEContentObserver::MaybeNotifyIMEOfFocusSet()", this));

  PostFocusSetNotification();
  FlushMergeableNotifications();
}

void IMEContentObserver::MaybeNotifyIMEOfTextChange(
    const TextChangeDataBase& aTextChangeData) {
  MOZ_LOG(sIMECOLog, LogLevel::Debug,
          ("0x%p IMEContentObserver::MaybeNotifyIMEOfTextChange("
           "aTextChangeData=%s)",
           this, ToString(aTextChangeData).c_str()));

  mTextChangeData += aTextChangeData;
  PostTextChangeNotification();
  FlushMergeableNotifications();
}

void IMEContentObserver::CancelNotifyingIMEOfTextChange() {
  MOZ_LOG(sIMECOLog, LogLevel::Debug,
          ("0x%p IMEContentObserver::CancelNotifyingIMEOfTextChange()", this));
  mTextChangeData.Clear();
  mNeedsToNotifyIMEOfTextChange = false;
}

void IMEContentObserver::MaybeNotifyIMEOfSelectionChange(
    bool aCausedByComposition, bool aCausedBySelectionEvent,
    bool aOccurredDuringComposition) {
  MOZ_LOG(
      sIMECOLog, LogLevel::Debug,
      ("0x%p IMEContentObserver::MaybeNotifyIMEOfSelectionChange("
       "aCausedByComposition=%s, aCausedBySelectionEvent=%s, "
       "aOccurredDuringComposition)",
       this, ToChar(aCausedByComposition), ToChar(aCausedBySelectionEvent)));

  mSelectionData.AssignReason(aCausedByComposition, aCausedBySelectionEvent,
                              aOccurredDuringComposition);
  PostSelectionChangeNotification();
  FlushMergeableNotifications();
}

void IMEContentObserver::MaybeNotifyIMEOfPositionChange() {
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
             "NOTIY_IME_OF_POSITION_CHANGE",
             this));
    return;
  }
  PostPositionChangeNotification();
  FlushMergeableNotifications();
}

void IMEContentObserver::CancelNotifyingIMEOfPositionChange() {
  MOZ_LOG(sIMECOLog, LogLevel::Debug,
          ("0x%p IMEContentObserver::CancelNotifyIMEOfPositionChange()", this));
  mNeedsToNotifyIMEOfPositionChange = false;
}

void IMEContentObserver::MaybeNotifyCompositionEventHandled() {
  MOZ_LOG(
      sIMECOLog, LogLevel::Debug,
      ("0x%p IMEContentObserver::MaybeNotifyCompositionEventHandled()", this));

  PostCompositionEventHandledNotification();
  FlushMergeableNotifications();
}

bool IMEContentObserver::UpdateSelectionCache(bool aRequireFlush /* = true */) {
  MOZ_ASSERT(IsSafeToNotifyIME());

  mSelectionData.ClearSelectionData();

  // XXX Cannot we cache some information for reducing the cost to compute
  //     selection offset and writing mode?
  WidgetQueryContentEvent querySelectedTextEvent(true, eQuerySelectedText,
                                                 mWidget);
  querySelectedTextEvent.mNeedsToFlushLayout = aRequireFlush;
  ContentEventHandler handler(GetPresContext());
  handler.OnQuerySelectedText(&querySelectedTextEvent);
  if (NS_WARN_IF(querySelectedTextEvent.DidNotFindSelection()) ||
      NS_WARN_IF(querySelectedTextEvent.mReply->mContentsRoot !=
                 mRootContent)) {
    return false;
  }
  MOZ_ASSERT(querySelectedTextEvent.mReply->mOffsetAndData.isSome());

  mFocusedWidget = querySelectedTextEvent.mReply->mFocusedWidget;
  mSelectionData.mOffset = querySelectedTextEvent.mReply->StartOffset();
  *mSelectionData.mString = querySelectedTextEvent.mReply->DataRef();
  mSelectionData.SetWritingMode(
      querySelectedTextEvent.mReply->WritingModeRef());
  mSelectionData.mReversed = querySelectedTextEvent.mReply->mReversed;

  // WARNING: Don't modify the reason of selection change here.

  MOZ_LOG(sIMECOLog, LogLevel::Debug,
          ("0x%p IMEContentObserver::UpdateSelectionCache(), "
           "mSelectionData=%s",
           this, ToString(mSelectionData).c_str()));

  return mSelectionData.IsValid();
}

void IMEContentObserver::PostPositionChangeNotification() {
  MOZ_LOG(sIMECOLog, LogLevel::Debug,
          ("0x%p IMEContentObserver::PostPositionChangeNotification()", this));

  mNeedsToNotifyIMEOfPositionChange = true;
}

void IMEContentObserver::PostCompositionEventHandledNotification() {
  MOZ_LOG(sIMECOLog, LogLevel::Debug,
          ("0x%p IMEContentObserver::"
           "PostCompositionEventHandledNotification()",
           this));

  mNeedsToNotifyIMEOfCompositionEventHandled = true;
}

bool IMEContentObserver::IsReflowLocked() const {
  nsPresContext* presContext = GetPresContext();
  if (NS_WARN_IF(!presContext)) {
    return false;
  }
  PresShell* presShell = presContext->GetPresShell();
  if (NS_WARN_IF(!presShell)) {
    return false;
  }
  // During reflow, we shouldn't notify IME because IME may query content
  // synchronously.  Then, it causes ContentEventHandler will try to flush
  // pending notifications during reflow.
  return presShell->IsReflowLocked();
}

bool IMEContentObserver::IsSafeToNotifyIME() const {
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
  if (mEditorBase && mEditorBase->IsInEditSubAction()) {
    return false;
  }

  return true;
}

void IMEContentObserver::FlushMergeableNotifications() {
  if (!IsSafeToNotifyIME()) {
    // So, if this is already called, this should do nothing.
    MOZ_LOG(sIMECOLog, LogLevel::Debug,
            ("0x%p IMEContentObserver::FlushMergeableNotifications(), "
             "FAILED, due to unsafe to notify IME",
             this));
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
             "FAILED, due to already flushing pending notifications",
             this));
    return;
  }

  // If text change notification and/or position change notification becomes
  // unnecessary, let's cancel them.
  if (mNeedsToNotifyIMEOfTextChange && !NeedsTextChangeNotification()) {
    CancelNotifyingIMEOfTextChange();
  }
  if (mNeedsToNotifyIMEOfPositionChange && !NeedsPositionChangeNotification()) {
    CancelNotifyingIMEOfPositionChange();
  }

  if (!NeedsToNotifyIMEOfSomething()) {
    MOZ_LOG(sIMECOLog, LogLevel::Debug,
            ("0x%p   IMEContentObserver::FlushMergeableNotifications(), "
             "FAILED, due to no pending notifications",
             this));
    return;
  }

  // NOTE: Reset each pending flag because sending notification may cause
  //       another change.

  MOZ_LOG(sIMECOLog, LogLevel::Debug,
          ("0x%p IMEContentObserver::FlushMergeableNotifications(), "
           "creating IMENotificationSender...",
           this));

  // If contents in selection range is modified, the selection range still
  // has removed node from the tree.  In such case, ContentIterator won't
  // work well.  Therefore, we shouldn't use AddScriptRunnder() here since
  // it may kick runnable event immediately after DOM tree is changed but
  // the selection range isn't modified yet.
  mQueuedSender = new IMENotificationSender(this);
  mQueuedSender->Dispatch(mDocShell);
  MOZ_LOG(sIMECOLog, LogLevel::Debug,
          ("0x%p IMEContentObserver::FlushMergeableNotifications(), "
           "finished",
           this));
}

void IMEContentObserver::TryToFlushPendingNotifications(bool aAllowAsync) {
  // If a sender instance is sending notifications, we shouldn't try to create
  // a new sender again because the sender will recreate by itself if there are
  // new pending notifications.
  if (mSendingNotification != NOTIFY_IME_OF_NOTHING) {
    return;
  }

  // When the caller allows to put off notifying IME, we can wait the next
  // call of this method or to run the queued sender.
  if (mQueuedSender && XRE_IsContentProcess() && aAllowAsync) {
    return;
  }

  if (!mQueuedSender) {
    // If it was not safe to dispatch notifications when the pending
    // notifications are posted, this may not have IMENotificationSender
    // instance because it couldn't dispatch it, e.g., when an edit sub-action
    // is being handled in the editor, we shouldn't do it even if it's safe to
    // run script.  Therefore, we need to create the sender instance here in the
    // case.
    if (!NeedsToNotifyIMEOfSomething()) {
      return;
    }
    mQueuedSender = new IMENotificationSender(this);
  }

  MOZ_LOG(sIMECOLog, LogLevel::Debug,
          ("0x%p IMEContentObserver::TryToFlushPendingNotifications(), "
           "performing queued IMENotificationSender forcibly",
           this));
  RefPtr<IMENotificationSender> queuedSender = mQueuedSender;
  queuedSender->Run();
}

/******************************************************************************
 * mozilla::IMEContentObserver::AChangeEvent
 ******************************************************************************/

bool IMEContentObserver::AChangeEvent::CanNotifyIME(
    ChangeEventType aChangeEventType) const {
  RefPtr<IMEContentObserver> observer = GetObserver();
  if (NS_WARN_IF(!observer)) {
    return false;
  }

  if (aChangeEventType == eChangeEventType_CompositionEventHandled) {
    return observer->mWidget != nullptr;
  }
  State state = observer->GetState();
  // If it's not initialized, we should do nothing.
  if (state == eState_NotObserving) {
    return false;
  }
  // If setting focus, just check the state.
  if (aChangeEventType == eChangeEventType_Focus) {
    return !NS_WARN_IF(observer->mIMEHasFocus);
  }
  // If we've not notified IME of focus yet, we shouldn't notify anything.
  if (!observer->mIMEHasFocus) {
    return false;
  }

  // If IME has focus, IMEContentObserver must hold the widget.
  MOZ_ASSERT(observer->mWidget);

  return true;
}

bool IMEContentObserver::AChangeEvent::IsSafeToNotifyIME(
    ChangeEventType aChangeEventType) const {
  if (NS_WARN_IF(!nsContentUtils::IsSafeToRunScript())) {
    return false;
  }

  RefPtr<IMEContentObserver> observer = GetObserver();
  if (!observer) {
    return false;
  }

  // While we're sending a notification, we shouldn't send another notification
  // recursively.
  if (observer->mSendingNotification != NOTIFY_IME_OF_NOTHING) {
    MOZ_LOG(
        sIMECOLog, LogLevel::Debug,
        ("0x%p   IMEContentObserver::AChangeEvent::IsSafeToNotifyIME(), "
         "putting off sending notification due to detecting recursive call, "
         "mIMEContentObserver={ mSendingNotification=%s }",
         this, ToChar(observer->mSendingNotification)));
    return false;
  }
  State state = observer->GetState();
  if (aChangeEventType == eChangeEventType_Focus) {
    if (NS_WARN_IF(state != eState_Initializing && state != eState_Observing)) {
      return false;
    }
  } else if (aChangeEventType == eChangeEventType_CompositionEventHandled) {
    // It doesn't need to check the observing status.
  } else if (state != eState_Observing) {
    return false;
  }
  return observer->IsSafeToNotifyIME();
}

/******************************************************************************
 * mozilla::IMEContentObserver::IMENotificationSender
 ******************************************************************************/

void IMEContentObserver::IMENotificationSender::Dispatch(
    nsIDocShell* aDocShell) {
  if (XRE_IsContentProcess() && aDocShell) {
    RefPtr<nsPresContext> presContext = aDocShell->GetPresContext();
    if (presContext) {
      nsRefreshDriver* refreshDriver = presContext->RefreshDriver();
      if (refreshDriver) {
        refreshDriver->AddEarlyRunner(this);
        return;
      }
    }
  }

  nsIScriptGlobalObject* globalObject =
      aDocShell ? aDocShell->GetScriptGlobalObject() : nullptr;
  if (globalObject) {
    RefPtr<IMENotificationSender> queuedSender = this;
    globalObject->Dispatch(TaskCategory::Other, queuedSender.forget());
  } else {
    NS_DispatchToCurrentThread(this);
  }
}

NS_IMETHODIMP
IMEContentObserver::IMENotificationSender::Run() {
  if (NS_WARN_IF(mIsRunning)) {
    MOZ_LOG(sIMECOLog, LogLevel::Error,
            ("0x%p IMEContentObserver::IMENotificationSender::Run(), FAILED, "
             "called recursively",
             this));
    return NS_OK;
  }

  RefPtr<IMEContentObserver> observer = GetObserver();
  if (!observer) {
    return NS_OK;
  }

  AutoRestore<bool> running(mIsRunning);
  mIsRunning = true;

  // This instance was already performed forcibly.
  if (observer->mQueuedSender != this) {
    return NS_OK;
  }

  // NOTE: Reset each pending flag because sending notification may cause
  //       another change.

  if (observer->mNeedsToNotifyIMEOfFocusSet) {
    observer->mNeedsToNotifyIMEOfFocusSet = false;
    SendFocusSet();
    observer->mQueuedSender = nullptr;
    // If it's not safe to notify IME of focus, SendFocusSet() sets
    // mNeedsToNotifyIMEOfFocusSet true again.  For guaranteeing to send the
    // focus notification later,  we should put a new sender into the queue but
    // this case must be rare.  Note that if mIMEContentObserver is already
    // destroyed, mNeedsToNotifyIMEOfFocusSet is never set true again.
    if (observer->mNeedsToNotifyIMEOfFocusSet) {
      MOZ_ASSERT(!observer->mIMEHasFocus);
      MOZ_LOG(sIMECOLog, LogLevel::Debug,
              ("0x%p IMEContentObserver::IMENotificationSender::Run(), "
               "posting IMENotificationSender to current thread",
               this));
      observer->mQueuedSender = new IMENotificationSender(observer);
      observer->mQueuedSender->Dispatch(observer->mDocShell);
      return NS_OK;
    }
    // This is the first notification to IME. So, we don't need to notify
    // anymore since IME starts to query content after it gets focus.
    observer->ClearPendingNotifications();
    return NS_OK;
  }

  if (observer->mNeedsToNotifyIMEOfTextChange) {
    observer->mNeedsToNotifyIMEOfTextChange = false;
    SendTextChange();
    // Even if the observer hasn't received selection change, let's try to send
    // selection change notification to IME because selection start offset may
    // be changed if the previous contents of selection start are changed.  For
    // example, when previous `<p>` element of another `<p>` element which
    // contains caret is removed by a DOM mutation, selection change event
    // won't be fired, but selection start offset should be decreased by the
    // length of removed `<p>` element.
    observer->mNeedsToNotifyIMEOfSelectionChange = true;
  }

  // If a text change notification causes another text change again, we should
  // notify IME of that before sending a selection change notification.
  if (!observer->mNeedsToNotifyIMEOfTextChange) {
    // Be aware, PuppetWidget depends on the order of this. A selection change
    // notification should not be sent before a text change notification because
    // PuppetWidget shouldn't query new text content every selection change.
    if (observer->mNeedsToNotifyIMEOfSelectionChange) {
      observer->mNeedsToNotifyIMEOfSelectionChange = false;
      SendSelectionChange();
    }
  }

  // If a text change notification causes another text change again or a
  // selection change notification causes either a text change or another
  // selection change, we should notify IME of those before sending a position
  // change notification.
  if (!observer->mNeedsToNotifyIMEOfTextChange &&
      !observer->mNeedsToNotifyIMEOfSelectionChange) {
    if (observer->mNeedsToNotifyIMEOfPositionChange) {
      observer->mNeedsToNotifyIMEOfPositionChange = false;
      SendPositionChange();
    }
  }

  // Composition event handled notification should be sent after all the
  // other notifications because this notifies widget of finishing all pending
  // events are handled completely.
  if (!observer->mNeedsToNotifyIMEOfTextChange &&
      !observer->mNeedsToNotifyIMEOfSelectionChange &&
      !observer->mNeedsToNotifyIMEOfPositionChange) {
    if (observer->mNeedsToNotifyIMEOfCompositionEventHandled) {
      observer->mNeedsToNotifyIMEOfCompositionEventHandled = false;
      SendCompositionEventHandled();
    }
  }

  observer->mQueuedSender = nullptr;

  // If notifications caused some new change, we should notify them now.
  if (observer->NeedsToNotifyIMEOfSomething()) {
    if (observer->GetState() == eState_StoppedObserving) {
      MOZ_LOG(sIMECOLog, LogLevel::Debug,
              ("0x%p IMEContentObserver::IMENotificationSender::Run(), "
               "waiting IMENotificationSender to be reinitialized",
               this));
    } else {
      MOZ_LOG(sIMECOLog, LogLevel::Debug,
              ("0x%p IMEContentObserver::IMENotificationSender::Run(), "
               "posting IMENotificationSender to current thread",
               this));
      observer->mQueuedSender = new IMENotificationSender(observer);
      observer->mQueuedSender->Dispatch(observer->mDocShell);
    }
  }
  return NS_OK;
}

void IMEContentObserver::IMENotificationSender::SendFocusSet() {
  RefPtr<IMEContentObserver> observer = GetObserver();
  if (!observer) {
    return;
  }

  if (!CanNotifyIME(eChangeEventType_Focus)) {
    // If IMEContentObserver has already gone, we don't need to notify IME of
    // focus.
    MOZ_LOG(sIMECOLog, LogLevel::Debug,
            ("0x%p IMEContentObserver::IMENotificationSender::"
             "SendFocusSet(), FAILED, due to impossible to notify IME of focus",
             this));
    observer->ClearPendingNotifications();
    return;
  }

  if (!IsSafeToNotifyIME(eChangeEventType_Focus)) {
    MOZ_LOG(sIMECOLog, LogLevel::Debug,
            ("0x%p   IMEContentObserver::IMENotificationSender::"
             "SendFocusSet(), retrying to send NOTIFY_IME_OF_FOCUS...",
             this));
    observer->PostFocusSetNotification();
    return;
  }

  observer->mIMEHasFocus = true;
  // Initialize selection cache with the first selection data.
#ifdef XP_MACOSX
  // We need to flush layout only on macOS because character coordinates are
  // cached by cocoa with this call, but we don't have a way to update them
  // after that.  Therefore, we need the latest layout information right now.
  observer->UpdateSelectionCache(true);
#else
  // We avoid flushing for focus in the general case.
  observer->UpdateSelectionCache(false);
#endif  // #ifdef XP_MACOSX #else
  MOZ_LOG(sIMECOLog, LogLevel::Info,
          ("0x%p IMEContentObserver::IMENotificationSender::"
           "SendFocusSet(), sending NOTIFY_IME_OF_FOCUS...",
           this));

  MOZ_RELEASE_ASSERT(observer->mSendingNotification == NOTIFY_IME_OF_NOTHING);
  observer->mSendingNotification = NOTIFY_IME_OF_FOCUS;
  IMEStateManager::NotifyIME(IMENotification(NOTIFY_IME_OF_FOCUS),
                             observer->mWidget);
  observer->mSendingNotification = NOTIFY_IME_OF_NOTHING;

  // IMENotificationRequests referred by ObserveEditableNode() may be different
  // before or after widget receives NOTIFY_IME_OF_FOCUS.  Therefore, we need
  // to guarantee to call ObserveEditableNode() after sending
  // NOTIFY_IME_OF_FOCUS.
  observer->OnIMEReceivedFocus();

  MOZ_LOG(sIMECOLog, LogLevel::Debug,
          ("0x%p IMEContentObserver::IMENotificationSender::"
           "SendFocusSet(), sent NOTIFY_IME_OF_FOCUS",
           this));
}

void IMEContentObserver::IMENotificationSender::SendSelectionChange() {
  RefPtr<IMEContentObserver> observer = GetObserver();
  if (!observer) {
    return;
  }

  if (!CanNotifyIME(eChangeEventType_Selection)) {
    MOZ_LOG(
        sIMECOLog, LogLevel::Debug,
        ("0x%p IMEContentObserver::IMENotificationSender::"
         "SendSelectionChange(), FAILED, due to impossible to notify IME of "
         "selection change",
         this));
    return;
  }

  if (!IsSafeToNotifyIME(eChangeEventType_Selection)) {
    MOZ_LOG(sIMECOLog, LogLevel::Debug,
            ("0x%p   IMEContentObserver::IMENotificationSender::"
             "SendSelectionChange(), retrying to send "
             "NOTIFY_IME_OF_SELECTION_CHANGE...",
             this));
    observer->PostSelectionChangeNotification();
    return;
  }

  SelectionChangeData lastSelChangeData = observer->mSelectionData;
  if (NS_WARN_IF(!observer->UpdateSelectionCache())) {
    MOZ_LOG(
        sIMECOLog, LogLevel::Error,
        ("0x%p IMEContentObserver::IMENotificationSender::"
         "SendSelectionChange(), FAILED, due to UpdateSelectionCache() failure",
         this));
    return;
  }

  // The state may be changed since querying content causes flushing layout.
  if (!CanNotifyIME(eChangeEventType_Selection)) {
    MOZ_LOG(
        sIMECOLog, LogLevel::Debug,
        ("0x%p IMEContentObserver::IMENotificationSender::"
         "SendSelectionChange(), FAILED, due to flushing layout having changed "
         "something",
         this));
    return;
  }

  // If the selection isn't changed actually, we shouldn't notify IME of
  // selection change.
  SelectionChangeData& newSelChangeData = observer->mSelectionData;
  if (lastSelChangeData.IsValid() &&
      lastSelChangeData.mOffset == newSelChangeData.mOffset &&
      lastSelChangeData.String() == newSelChangeData.String() &&
      lastSelChangeData.GetWritingMode() == newSelChangeData.GetWritingMode() &&
      lastSelChangeData.mReversed == newSelChangeData.mReversed) {
    MOZ_LOG(sIMECOLog, LogLevel::Debug,
            ("0x%p IMEContentObserver::IMENotificationSender::"
             "SendSelectionChange(), not notifying IME of "
             "NOTIFY_IME_OF_SELECTION_CHANGE due to not changed actually",
             this));
    return;
  }

  MOZ_LOG(sIMECOLog, LogLevel::Info,
          ("0x%p IMEContentObserver::IMENotificationSender::"
           "SendSelectionChange(), sending NOTIFY_IME_OF_SELECTION_CHANGE... "
           "newSelChangeData=%s",
           this, ToString(newSelChangeData).c_str()));

  IMENotification notification(NOTIFY_IME_OF_SELECTION_CHANGE);
  notification.SetData(observer->mSelectionData);

  MOZ_RELEASE_ASSERT(observer->mSendingNotification == NOTIFY_IME_OF_NOTHING);
  observer->mSendingNotification = NOTIFY_IME_OF_SELECTION_CHANGE;
  IMEStateManager::NotifyIME(notification, observer->mWidget);
  observer->mSendingNotification = NOTIFY_IME_OF_NOTHING;

  MOZ_LOG(sIMECOLog, LogLevel::Debug,
          ("0x%p IMEContentObserver::IMENotificationSender::"
           "SendSelectionChange(), sent NOTIFY_IME_OF_SELECTION_CHANGE",
           this));
}

void IMEContentObserver::IMENotificationSender::SendTextChange() {
  RefPtr<IMEContentObserver> observer = GetObserver();
  if (!observer) {
    return;
  }

  if (!CanNotifyIME(eChangeEventType_Text)) {
    MOZ_LOG(
        sIMECOLog, LogLevel::Debug,
        ("0x%p IMEContentObserver::IMENotificationSender::"
         "SendTextChange(), FAILED, due to impossible to notify IME of text "
         "change",
         this));
    return;
  }

  if (!IsSafeToNotifyIME(eChangeEventType_Text)) {
    MOZ_LOG(sIMECOLog, LogLevel::Debug,
            ("0x%p   IMEContentObserver::IMENotificationSender::"
             "SendTextChange(), retrying to send NOTIFY_IME_OF_TEXT_CHANGE...",
             this));
    observer->PostTextChangeNotification();
    return;
  }

  // If text change notification is unnecessary anymore, just cancel it.
  if (!observer->NeedsTextChangeNotification()) {
    MOZ_LOG(sIMECOLog, LogLevel::Warning,
            ("0x%p   IMEContentObserver::IMENotificationSender::"
             "SendTextChange(), canceling sending NOTIFY_IME_OF_TEXT_CHANGE",
             this));
    observer->CancelNotifyingIMEOfTextChange();
    return;
  }

  MOZ_LOG(sIMECOLog, LogLevel::Info,
          ("0x%p IMEContentObserver::IMENotificationSender::"
           "SendTextChange(), sending NOTIFY_IME_OF_TEXT_CHANGE... "
           "mIMEContentObserver={ mTextChangeData=%s }",
           this, ToString(observer->mTextChangeData).c_str()));

  IMENotification notification(NOTIFY_IME_OF_TEXT_CHANGE);
  notification.SetData(observer->mTextChangeData);
  observer->mTextChangeData.Clear();

  MOZ_RELEASE_ASSERT(observer->mSendingNotification == NOTIFY_IME_OF_NOTHING);
  observer->mSendingNotification = NOTIFY_IME_OF_TEXT_CHANGE;
  IMEStateManager::NotifyIME(notification, observer->mWidget);
  observer->mSendingNotification = NOTIFY_IME_OF_NOTHING;

  MOZ_LOG(sIMECOLog, LogLevel::Debug,
          ("0x%p IMEContentObserver::IMENotificationSender::"
           "SendTextChange(), sent NOTIFY_IME_OF_TEXT_CHANGE",
           this));
}

void IMEContentObserver::IMENotificationSender::SendPositionChange() {
  RefPtr<IMEContentObserver> observer = GetObserver();
  if (!observer) {
    return;
  }

  if (!CanNotifyIME(eChangeEventType_Position)) {
    MOZ_LOG(sIMECOLog, LogLevel::Debug,
            ("0x%p IMEContentObserver::IMENotificationSender::"
             "SendPositionChange(), FAILED, due to impossible to notify IME of "
             "position change",
             this));
    return;
  }

  if (!IsSafeToNotifyIME(eChangeEventType_Position)) {
    MOZ_LOG(sIMECOLog, LogLevel::Debug,
            ("0x%p   IMEContentObserver::IMENotificationSender::"
             "SendPositionChange(), retrying to send "
             "NOTIFY_IME_OF_POSITION_CHANGE...",
             this));
    observer->PostPositionChangeNotification();
    return;
  }

  // If position change notification is unnecessary anymore, just cancel it.
  if (!observer->NeedsPositionChangeNotification()) {
    MOZ_LOG(sIMECOLog, LogLevel::Warning,
            ("0x%p   IMEContentObserver::IMENotificationSender::"
             "SendPositionChange(), canceling sending "
             "NOTIFY_IME_OF_POSITION_CHANGE",
             this));
    observer->CancelNotifyingIMEOfPositionChange();
    return;
  }

  MOZ_LOG(sIMECOLog, LogLevel::Info,
          ("0x%p IMEContentObserver::IMENotificationSender::"
           "SendPositionChange(), sending NOTIFY_IME_OF_POSITION_CHANGE...",
           this));

  MOZ_RELEASE_ASSERT(observer->mSendingNotification == NOTIFY_IME_OF_NOTHING);
  observer->mSendingNotification = NOTIFY_IME_OF_POSITION_CHANGE;
  IMEStateManager::NotifyIME(IMENotification(NOTIFY_IME_OF_POSITION_CHANGE),
                             observer->mWidget);
  observer->mSendingNotification = NOTIFY_IME_OF_NOTHING;

  MOZ_LOG(sIMECOLog, LogLevel::Debug,
          ("0x%p IMEContentObserver::IMENotificationSender::"
           "SendPositionChange(), sent NOTIFY_IME_OF_POSITION_CHANGE",
           this));
}

void IMEContentObserver::IMENotificationSender::SendCompositionEventHandled() {
  RefPtr<IMEContentObserver> observer = GetObserver();
  if (!observer) {
    return;
  }

  if (!CanNotifyIME(eChangeEventType_CompositionEventHandled)) {
    MOZ_LOG(
        sIMECOLog, LogLevel::Debug,
        ("0x%p IMEContentObserver::IMENotificationSender::"
         "SendCompositionEventHandled(), FAILED, due to impossible to notify "
         "IME of composition event handled",
         this));
    return;
  }

  if (!IsSafeToNotifyIME(eChangeEventType_CompositionEventHandled)) {
    MOZ_LOG(sIMECOLog, LogLevel::Debug,
            ("0x%p   IMEContentObserver::IMENotificationSender::"
             "SendCompositionEventHandled(), retrying to send "
             "NOTIFY_IME_OF_POSITION_CHANGE...",
             this));
    observer->PostCompositionEventHandledNotification();
    return;
  }

  MOZ_LOG(sIMECOLog, LogLevel::Info,
          ("0x%p IMEContentObserver::IMENotificationSender::"
           "SendCompositionEventHandled(), sending "
           "NOTIFY_IME_OF_COMPOSITION_EVENT_HANDLED...",
           this));

  MOZ_RELEASE_ASSERT(observer->mSendingNotification == NOTIFY_IME_OF_NOTHING);
  observer->mSendingNotification = NOTIFY_IME_OF_COMPOSITION_EVENT_HANDLED;
  IMEStateManager::NotifyIME(
      IMENotification(NOTIFY_IME_OF_COMPOSITION_EVENT_HANDLED),
      observer->mWidget);
  observer->mSendingNotification = NOTIFY_IME_OF_NOTHING;

  MOZ_LOG(sIMECOLog, LogLevel::Debug,
          ("0x%p IMEContentObserver::IMENotificationSender::"
           "SendCompositionEventHandled(), sent "
           "NOTIFY_IME_OF_COMPOSITION_EVENT_HANDLED",
           this));
}

/******************************************************************************
 * mozilla::IMEContentObserver::DocumentObservingHelper
 ******************************************************************************/

NS_IMPL_CYCLE_COLLECTION_CLASS(IMEContentObserver::DocumentObserver)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(IMEContentObserver::DocumentObserver)
  // StopObserving() releases mIMEContentObserver and mDocument.
  tmp->StopObserving();
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(IMEContentObserver::DocumentObserver)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mIMEContentObserver)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mDocument)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(IMEContentObserver::DocumentObserver)
  NS_INTERFACE_MAP_ENTRY(nsIDocumentObserver)
  NS_INTERFACE_MAP_ENTRY(nsIMutationObserver)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(IMEContentObserver::DocumentObserver)
NS_IMPL_CYCLE_COLLECTING_RELEASE(IMEContentObserver::DocumentObserver)

void IMEContentObserver::DocumentObserver::Observe(Document* aDocument) {
  MOZ_ASSERT(aDocument);

  // Guarantee that aDocument won't be destroyed during a call of
  // StopObserving().
  RefPtr<Document> newDocument = aDocument;

  StopObserving();

  mDocument = std::move(newDocument);
  mDocument->AddObserver(this);
}

void IMEContentObserver::DocumentObserver::StopObserving() {
  if (!IsObserving()) {
    return;
  }

  // Grab IMEContentObserver which could be destroyed during method calls.
  RefPtr<IMEContentObserver> observer = std::move(mIMEContentObserver);

  // Stop observing the document first.
  RefPtr<Document> document = std::move(mDocument);
  document->RemoveObserver(this);

  // Notify IMEContentObserver of ending of document updates if this already
  // notified it of beginning of document updates.
  for (; IsUpdating(); --mDocumentUpdating) {
    // FYI: IsUpdating() returns true until mDocumentUpdating becomes 0.
    //      However, IsObserving() returns false now because mDocument was
    //      already cleared above.  Therefore, this method won't be called
    //      recursively.
    observer->EndDocumentUpdate();
  }
}

void IMEContentObserver::DocumentObserver::Destroy() {
  StopObserving();
  mIMEContentObserver = nullptr;
}

void IMEContentObserver::DocumentObserver::BeginUpdate(Document* aDocument) {
  if (NS_WARN_IF(Destroyed()) || NS_WARN_IF(!IsObserving())) {
    return;
  }
  mDocumentUpdating++;
  mIMEContentObserver->BeginDocumentUpdate();
}

void IMEContentObserver::DocumentObserver::EndUpdate(Document* aDocument) {
  if (NS_WARN_IF(Destroyed()) || NS_WARN_IF(!IsObserving()) ||
      NS_WARN_IF(!IsUpdating())) {
    return;
  }
  mDocumentUpdating--;
  mIMEContentObserver->EndDocumentUpdate();
}

}  // namespace mozilla
