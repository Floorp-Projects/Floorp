/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "NotificationController.h"

#include "Accessible-inl.h"
#include "DocAccessible-inl.h"
#include "TextLeafAccessible.h"
#include "TextUpdater.h"

#include "mozilla/dom/Element.h"
#include "mozilla/Telemetry.h"

using namespace mozilla;
using namespace mozilla::a11y;

// Defines the number of selection add/remove events in the queue when they
// aren't packed into single selection within event.
const unsigned int kSelChangeCountToPack = 5;

////////////////////////////////////////////////////////////////////////////////
// NotificationCollector
////////////////////////////////////////////////////////////////////////////////

NotificationController::NotificationController(DocAccessible* aDocument,
                                               nsIPresShell* aPresShell) :
  EventQueue(aDocument), mObservingState(eNotObservingRefresh),
  mPresShell(aPresShell)
{
  mTextHash.Init();

  // Schedule initial accessible tree construction.
  ScheduleProcessing();
}

NotificationController::~NotificationController()
{
  NS_ASSERTION(!mDocument, "Controller wasn't shutdown properly!");
  if (mDocument)
    Shutdown();
}

////////////////////////////////////////////////////////////////////////////////
// NotificationCollector: AddRef/Release and cycle collection

NS_IMPL_CYCLE_COLLECTING_NATIVE_ADDREF(NotificationController)
NS_IMPL_CYCLE_COLLECTING_NATIVE_RELEASE(NotificationController)

NS_IMPL_CYCLE_COLLECTION_CLASS(NotificationController)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(NotificationController)
  if (tmp->mDocument)
    tmp->Shutdown();
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(NotificationController)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mHangingChildDocuments)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mContentInsertions)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mEvents)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_ROOT_NATIVE(NotificationController, AddRef)
NS_IMPL_CYCLE_COLLECTION_UNROOT_NATIVE(NotificationController, Release)

////////////////////////////////////////////////////////////////////////////////
// NotificationCollector: public

void
NotificationController::Shutdown()
{
  if (mObservingState != eNotObservingRefresh &&
      mPresShell->RemoveRefreshObserver(this, Flush_Display)) {
    mObservingState = eNotObservingRefresh;
  }

  // Shutdown handling child documents.
  int32_t childDocCount = mHangingChildDocuments.Length();
  for (int32_t idx = childDocCount - 1; idx >= 0; idx--) {
    if (!mHangingChildDocuments[idx]->IsDefunct())
      mHangingChildDocuments[idx]->Shutdown();
  }

  mHangingChildDocuments.Clear();

  mDocument = nullptr;
  mPresShell = nullptr;

  mTextHash.Clear();
  mContentInsertions.Clear();
  mNotifications.Clear();
  mEvents.Clear();
}

void
NotificationController::ScheduleChildDocBinding(DocAccessible* aDocument)
{
  // Schedule child document binding to the tree.
  mHangingChildDocuments.AppendElement(aDocument);
  ScheduleProcessing();
}

void
NotificationController::ScheduleContentInsertion(Accessible* aContainer,
                                                 nsIContent* aStartChildNode,
                                                 nsIContent* aEndChildNode)
{
  nsRefPtr<ContentInsertion> insertion = new ContentInsertion(mDocument,
                                                              aContainer);
  if (insertion && insertion->InitChildList(aStartChildNode, aEndChildNode) &&
      mContentInsertions.AppendElement(insertion)) {
    ScheduleProcessing();
  }
}

////////////////////////////////////////////////////////////////////////////////
// NotificationCollector: protected

void
NotificationController::ScheduleProcessing()
{
  // If notification flush isn't planed yet start notification flush
  // asynchronously (after style and layout).
  if (mObservingState == eNotObservingRefresh) {
    if (mPresShell->AddRefreshObserver(this, Flush_Display))
      mObservingState = eRefreshObserving;
  }
}

bool
NotificationController::IsUpdatePending()
{
  return mPresShell->IsLayoutFlushObserver() ||
    mObservingState == eRefreshProcessingForUpdate ||
    mContentInsertions.Length() != 0 || mNotifications.Length() != 0 ||
    mTextHash.Count() != 0 ||
    !mDocument->HasLoadState(DocAccessible::eTreeConstructed);
}

////////////////////////////////////////////////////////////////////////////////
// NotificationCollector: private

void
NotificationController::WillRefresh(mozilla::TimeStamp aTime)
{
  Telemetry::AutoTimer<Telemetry::A11Y_UPDATE_TIME> updateTimer;

  // If the document accessible that notification collector was created for is
  // now shut down, don't process notifications anymore.
  NS_ASSERTION(mDocument,
               "The document was shut down while refresh observer is attached!");
  if (!mDocument)
    return;

  if (mObservingState == eRefreshProcessing ||
      mObservingState == eRefreshProcessingForUpdate)
    return;

  // Any generic notifications should be queued if we're processing content
  // insertions or generic notifications.
  mObservingState = eRefreshProcessingForUpdate;

  // Initial accessible tree construction.
  if (!mDocument->HasLoadState(DocAccessible::eTreeConstructed)) {
    // If document is not bound to parent at this point then the document is not
    // ready yet (process notifications later).
    if (!mDocument->IsBoundToParent()) {
      mObservingState = eRefreshObserving;
      return;
    }

#ifdef A11Y_LOG
    if (logging::IsEnabled(logging::eTree)) {
      logging::MsgBegin("TREE", "initial tree created");
      logging::Address("document", mDocument);
      logging::MsgEnd();
    }
#endif

    mDocument->DoInitialUpdate();

    NS_ASSERTION(mContentInsertions.Length() == 0,
                 "Pending content insertions while initial accessible tree isn't created!");
  }

  // Initialize scroll support if needed.
  if (!(mDocument->mDocFlags & DocAccessible::eScrollInitialized))
    mDocument->AddScrollListener();

  // Process content inserted notifications to update the tree. Process other
  // notifications like DOM events and then flush event queue. If any new
  // notifications are queued during this processing then they will be processed
  // on next refresh. If notification processing queues up new events then they
  // are processed in this refresh. If events processing queues up new events
  // then new events are processed on next refresh.
  // Note: notification processing or event handling may shut down the owning
  // document accessible.

  // Process only currently queued content inserted notifications.
  nsTArray<nsRefPtr<ContentInsertion> > contentInsertions;
  contentInsertions.SwapElements(mContentInsertions);

  uint32_t insertionCount = contentInsertions.Length();
  for (uint32_t idx = 0; idx < insertionCount; idx++) {
    contentInsertions[idx]->Process();
    if (!mDocument)
      return;
  }

  // Process rendered text change notifications.
  mTextHash.EnumerateEntries(TextEnumerator, mDocument);
  mTextHash.Clear();

  // Bind hanging child documents.
  uint32_t hangingDocCnt = mHangingChildDocuments.Length();
  for (uint32_t idx = 0; idx < hangingDocCnt; idx++) {
    DocAccessible* childDoc = mHangingChildDocuments[idx];
    if (childDoc->IsDefunct())
      continue;

    nsIContent* ownerContent = mDocument->DocumentNode()->
      FindContentForSubDocument(childDoc->DocumentNode());
    if (ownerContent) {
      Accessible* outerDocAcc = mDocument->GetAccessible(ownerContent);
      if (outerDocAcc && outerDocAcc->AppendChild(childDoc)) {
        if (mDocument->AppendChildDocument(childDoc))
          continue;

        outerDocAcc->RemoveChild(childDoc);
      }

      // Failed to bind the child document, destroy it.
      childDoc->Shutdown();
    }
  }
  mHangingChildDocuments.Clear();

  // If the document is ready and all its subdocuments are completely loaded
  // then process the document load.
  if (mDocument->HasLoadState(DocAccessible::eReady) &&
      !mDocument->HasLoadState(DocAccessible::eCompletelyLoaded) &&
      hangingDocCnt == 0) {
    uint32_t childDocCnt = mDocument->ChildDocumentCount(), childDocIdx = 0;
    for (; childDocIdx < childDocCnt; childDocIdx++) {
      DocAccessible* childDoc = mDocument->GetChildDocumentAt(childDocIdx);
      if (!childDoc->HasLoadState(DocAccessible::eCompletelyLoaded))
        break;
    }

    if (childDocIdx == childDocCnt) {
      mDocument->ProcessLoad();
      if (!mDocument)
        return;
    }
  }

  // Process only currently queued generic notifications.
  nsTArray < nsRefPtr<Notification> > notifications;
  notifications.SwapElements(mNotifications);

  uint32_t notificationCount = notifications.Length();
  for (uint32_t idx = 0; idx < notificationCount; idx++) {
    notifications[idx]->Process();
    if (!mDocument)
      return;
  }

  // Process invalidation list of the document after all accessible tree
  // modification are done.
  mDocument->ProcessInvalidationList();

  // If a generic notification occurs after this point then we may be allowed to
  // process it synchronously.  However we do not want to reenter if fireing
  // events causes script to run.
  mObservingState = eRefreshProcessing;

  ProcessEventQueue();
  mObservingState = eRefreshObserving;
  if (!mDocument)
    return;

  // Stop further processing if there are no new notifications of any kind or
  // events and document load is processed.
  if (mContentInsertions.IsEmpty() && mNotifications.IsEmpty() &&
      mEvents.IsEmpty() && mTextHash.Count() == 0 &&
      mHangingChildDocuments.IsEmpty() &&
      mDocument->HasLoadState(DocAccessible::eCompletelyLoaded) &&
      mPresShell->RemoveRefreshObserver(this, Flush_Display)) {
    mObservingState = eNotObservingRefresh;
  }
}

////////////////////////////////////////////////////////////////////////////////
// Notification controller: text leaf accessible text update

PLDHashOperator
NotificationController::TextEnumerator(nsCOMPtrHashKey<nsIContent>* aEntry,
                                       void* aUserArg)
{
  DocAccessible* document = static_cast<DocAccessible*>(aUserArg);
  nsIContent* textNode = aEntry->GetKey();
  Accessible* textAcc = document->GetAccessible(textNode);

  // If the text node is not in tree or doesn't have frame then this case should
  // have been handled already by content removal notifications.
  nsINode* containerNode = textNode->GetParentNode();
  if (!containerNode) {
    NS_ASSERTION(!textAcc,
                 "Text node was removed but accessible is kept alive!");
    return PL_DHASH_NEXT;
  }

  nsIFrame* textFrame = textNode->GetPrimaryFrame();
  if (!textFrame) {
    NS_ASSERTION(!textAcc,
                 "Text node isn't rendered but accessible is kept alive!");
    return PL_DHASH_NEXT;
  }

  nsIContent* containerElm = containerNode->IsElement() ?
    containerNode->AsElement() : nullptr;

  nsAutoString text;
  textFrame->GetRenderedText(&text);

  // Remove text accessible if rendered text is empty.
  if (textAcc) {
    if (text.IsEmpty()) {
#ifdef A11Y_LOG
      if (logging::IsEnabled(logging::eTree | logging::eText)) {
        logging::MsgBegin("TREE", "text node lost its content");
        logging::Node("container", containerElm);
        logging::Node("content", textNode);
        logging::MsgEnd();
      }
#endif

      document->ContentRemoved(containerElm, textNode);
      return PL_DHASH_NEXT;
    }

    // Update text of the accessible and fire text change events.
#ifdef A11Y_LOG
    if (logging::IsEnabled(logging::eText)) {
      logging::MsgBegin("TEXT", "text may be changed");
      logging::Node("container", containerElm);
      logging::Node("content", textNode);
      logging::MsgEntry("old text '%s'",
                        NS_ConvertUTF16toUTF8(textAcc->AsTextLeaf()->Text()).get());
      logging::MsgEntry("new text: '%s'",
                        NS_ConvertUTF16toUTF8(text).get());
      logging::MsgEnd();
    }
#endif

    TextUpdater::Run(document, textAcc->AsTextLeaf(), text);
    return PL_DHASH_NEXT;
  }

  // Append an accessible if rendered text is not empty.
  if (!text.IsEmpty()) {
#ifdef A11Y_LOG
    if (logging::IsEnabled(logging::eTree | logging::eText)) {
      logging::MsgBegin("TREE", "text node gains new content");
      logging::Node("container", containerElm);
      logging::Node("content", textNode);
      logging::MsgEnd();
    }
#endif

    // Make sure the text node is in accessible document still.
    Accessible* container = document->GetAccessibleOrContainer(containerNode);
    NS_ASSERTION(container,
                 "Text node having rendered text hasn't accessible document!");
    if (container) {
      nsTArray<nsCOMPtr<nsIContent> > insertedContents;
      insertedContents.AppendElement(textNode);
      document->ProcessContentInserted(container, &insertedContents);
    }
  }

  return PL_DHASH_NEXT;
}


////////////////////////////////////////////////////////////////////////////////
// NotificationController: content inserted notification

NotificationController::ContentInsertion::
  ContentInsertion(DocAccessible* aDocument, Accessible* aContainer) :
  mDocument(aDocument), mContainer(aContainer)
{
}

bool
NotificationController::ContentInsertion::
  InitChildList(nsIContent* aStartChildNode, nsIContent* aEndChildNode)
{
  bool haveToUpdate = false;

  nsIContent* node = aStartChildNode;
  while (node != aEndChildNode) {
    // Notification triggers for content insertion even if no content was
    // actually inserted, check if the given content has a frame to discard
    // this case early.
    if (node->GetPrimaryFrame()) {
      if (mInsertedContent.AppendElement(node))
        haveToUpdate = true;
    }

    node = node->GetNextSibling();
  }

  return haveToUpdate;
}

NS_IMPL_CYCLE_COLLECTION_1(NotificationController::ContentInsertion,
                           mContainer)

NS_IMPL_CYCLE_COLLECTION_ROOT_NATIVE(NotificationController::ContentInsertion,
                                     AddRef)
NS_IMPL_CYCLE_COLLECTION_UNROOT_NATIVE(NotificationController::ContentInsertion,
                                       Release)

void
NotificationController::ContentInsertion::Process()
{
  mDocument->ProcessContentInserted(mContainer, &mInsertedContent);

  mDocument = nullptr;
  mContainer = nullptr;
  mInsertedContent.Clear();
}

