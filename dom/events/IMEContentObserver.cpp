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
#include "nsIWidget.h"
#include "nsPresContext.h"
#include "nsThreadUtils.h"
#include "nsWeakReference.h"

namespace mozilla {

using namespace widget;

NS_IMPL_CYCLE_COLLECTION(IMEContentObserver,
                         mWidget, mSelection,
                         mRootContent, mEditableNode, mDocShell)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(IMEContentObserver)
 NS_INTERFACE_MAP_ENTRY(nsISelectionListener)
 NS_INTERFACE_MAP_ENTRY(nsIMutationObserver)
 NS_INTERFACE_MAP_ENTRY(nsIReflowObserver)
 NS_INTERFACE_MAP_ENTRY(nsIScrollObserver)
 NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
 NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsISelectionListener)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(IMEContentObserver)
NS_IMPL_CYCLE_COLLECTING_RELEASE(IMEContentObserver)

IMEContentObserver::IMEContentObserver()
  : mESM(nullptr)
{
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
IMEContentObserver::Destroy()
{
  // If CreateTextStateManager failed, mRootContent will be null,
  // and we should not call NotifyIME(IMENotification(NOTIFY_IME_OF_BLUR))
  if (mRootContent) {
    if (IMEStateManager::IsTestingIME() && mEditableNode) {
      nsIDocument* doc = mEditableNode->OwnerDoc();
      (new AsyncEventDispatcher(doc, NS_LITERAL_STRING("MozIMEFocusOut"),
                                false, false))->RunDOMEventWhenSafe();
    }
    mWidget->NotifyIME(IMENotification(NOTIFY_IME_OF_BLUR));
  }
  // Even if there are some pending notification, it'll never notify the widget.
  mWidget = nullptr;
  if (mUpdatePreference.WantSelectionChange() && mSelection) {
    nsCOMPtr<nsISelectionPrivate> selPrivate(do_QueryInterface(mSelection));
    if (selPrivate) {
      selPrivate->RemoveSelectionListener(this);
    }
  }
  mSelection = nullptr;
  if (mUpdatePreference.WantTextChange() && mRootContent) {
    mRootContent->RemoveMutationObserver(this);
  }
  if (mUpdatePreference.WantPositionChanged() && mDocShell) {
    mDocShell->RemoveWeakScrollObserver(this);
    mDocShell->RemoveWeakReflowObserver(this);
  }
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
  if (!mRootContent->IsInDoc()) {
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
    nsContentUtils::AddScriptRunner(
      new SelectionChangeEvent(this, causedByComposition));
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
  if (mWidget) {
    nsContentUtils::AddScriptRunner(new PositionChangeEvent(this));
  }
}

NS_IMETHODIMP
IMEContentObserver::Reflow(DOMHighResTimeStamp aStart,
                           DOMHighResTimeStamp aEnd)
{
  if (mWidget) {
    nsContentUtils::AddScriptRunner(new PositionChangeEvent(this));
  }
  return NS_OK;
}

NS_IMETHODIMP
IMEContentObserver::ReflowInterruptible(DOMHighResTimeStamp aStart,
                                        DOMHighResTimeStamp aEnd)
{
  if (mWidget) {
    nsContentUtils::AddScriptRunner(new PositionChangeEvent(this));
  }
  return NS_OK;
}

// Helper class, used for text change notification
class TextChangeEvent : public nsRunnable
{
public:
  TextChangeEvent(IMEContentObserver* aDispatcher,
                  uint32_t aStart, uint32_t aOldEnd, uint32_t aNewEnd,
                  bool aCausedByComposition)
    : mDispatcher(aDispatcher)
    , mStart(aStart)
    , mOldEnd(aOldEnd)
    , mNewEnd(aNewEnd)
    , mCausedByComposition(aCausedByComposition)
  {
    MOZ_ASSERT(mDispatcher);
  }

  NS_IMETHOD Run()
  {
    if (mDispatcher->GetWidget()) {
      IMENotification notification(NOTIFY_IME_OF_TEXT_CHANGE);
      notification.mTextChangeData.mStartOffset = mStart;
      notification.mTextChangeData.mOldEndOffset = mOldEnd;
      notification.mTextChangeData.mNewEndOffset = mNewEnd;
      notification.mTextChangeData.mCausedByComposition = mCausedByComposition;
      mDispatcher->GetWidget()->NotifyIME(notification);
    }
    return NS_OK;
  }

private:
  nsRefPtr<IMEContentObserver> mDispatcher;
  uint32_t mStart, mOldEnd, mNewEnd;
  bool mCausedByComposition;
};

void
IMEContentObserver::CharacterDataChanged(nsIDocument* aDocument,
                                         nsIContent* aContent,
                                         CharacterDataChangeInfo* aInfo)
{
  NS_ASSERTION(aContent->IsNodeOfType(nsINode::eTEXT),
               "character data changed for non-text node");

  bool causedByComposition = IsEditorHandlingEventForComposition();
  if (causedByComposition &&
      !mUpdatePreference.WantChangesCausedByComposition()) {
    return;
  }

  uint32_t offset = 0;
  // get offsets of change and fire notification
  nsresult rv =
    ContentEventHandler::GetFlatTextOffsetOfRange(mRootContent, aContent,
                                                  aInfo->mChangeStart,
                                                  &offset,
                                                  LINE_BREAK_TYPE_NATIVE);
  NS_ENSURE_SUCCESS_VOID(rv);

  uint32_t oldEnd = offset + aInfo->mChangeEnd - aInfo->mChangeStart;
  uint32_t newEnd = offset + aInfo->mReplaceLength;

  nsContentUtils::AddScriptRunner(
    new TextChangeEvent(this, offset, oldEnd, newEnd, causedByComposition));
}

void
IMEContentObserver::NotifyContentAdded(nsINode* aContainer,
                                       int32_t aStartIndex,
                                       int32_t aEndIndex)
{
  bool causedByComposition = IsEditorHandlingEventForComposition();
  if (causedByComposition &&
      !mUpdatePreference.WantChangesCausedByComposition()) {
    return;
  }

  uint32_t offset = 0;
  nsresult rv =
    ContentEventHandler::GetFlatTextOffsetOfRange(mRootContent, aContainer,
                                                  aStartIndex, &offset,
                                                  LINE_BREAK_TYPE_NATIVE);
  NS_ENSURE_SUCCESS_VOID(rv);

  // get offset at the end of the last added node
  nsIContent* childAtStart = aContainer->GetChildAt(aStartIndex);
  uint32_t addingLength = 0;
  rv = ContentEventHandler::GetFlatTextOffsetOfRange(childAtStart, aContainer,
                                                     aEndIndex, &addingLength,
                                                     LINE_BREAK_TYPE_NATIVE);
  NS_ENSURE_SUCCESS_VOID(rv);

  if (!addingLength) {
    return;
  }

  nsContentUtils::AddScriptRunner(
    new TextChangeEvent(this, offset, offset, offset + addingLength,
                        causedByComposition));
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
  bool causedByComposition = IsEditorHandlingEventForComposition();
  if (causedByComposition &&
      !mUpdatePreference.WantChangesCausedByComposition()) {
    return;
  }

  uint32_t offset = 0;
  nsresult rv =
    ContentEventHandler::GetFlatTextOffsetOfRange(mRootContent,
                                                  NODE_FROM(aContainer,
                                                            aDocument),
                                                  aIndexInContainer, &offset,
                                                  LINE_BREAK_TYPE_NATIVE);
  NS_ENSURE_SUCCESS_VOID(rv);

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
  NS_ENSURE_SUCCESS_VOID(rv);

  if (!textLength) {
    return;
  }

  nsContentUtils::AddScriptRunner(
    new TextChangeEvent(this, offset, offset + textLength, offset,
                        causedByComposition));
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
  bool causedByComposition = IsEditorHandlingEventForComposition();
  if (causedByComposition &&
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

  nsContentUtils::AddScriptRunner(
    new TextChangeEvent(this, start, start + mPreAttrChangeLength,
                        start + postAttrChangeLength, causedByComposition));
}

} // namespace mozilla
