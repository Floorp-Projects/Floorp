/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "NotificationController.h"

#include "DocAccessible-inl.h"
#include "DocAccessibleChild.h"
#include "TextLeafAccessible.h"
#include "TextUpdater.h"

#include "mozilla/dom/TabChild.h"
#include "mozilla/dom/Element.h"
#include "mozilla/Telemetry.h"

using namespace mozilla;
using namespace mozilla::a11y;

////////////////////////////////////////////////////////////////////////////////
// NotificationCollector
////////////////////////////////////////////////////////////////////////////////

NotificationController::NotificationController(DocAccessible* aDocument,
                                               nsIPresShell* aPresShell) :
  EventQueue(aDocument), mObservingState(eNotObservingRefresh),
  mPresShell(aPresShell)
{
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
  for (auto it = tmp->mContentInsertions.ConstIter(); !it.Done(); it.Next()) {
    NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(cb, "mContentInsertions key");
    cb.NoteXPCOMChild(it.Key());
    nsTArray<nsCOMPtr<nsIContent>>* list = it.UserData();
    for (uint32_t i = 0; i < list->Length(); i++) {
      NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(cb,
                                         "mContentInsertions value item");
      cb.NoteXPCOMChild(list->ElementAt(i));
    }
  }
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mEvents)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mRelocations)
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
  mRelocations.Clear();
  mEventTree.Clear();
}

EventTree*
NotificationController::QueueMutation(Accessible* aContainer)
{
  EventTree* tree = mEventTree.FindOrInsert(aContainer);
  if (tree) {
    ScheduleProcessing();
  }
  return tree;
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
  nsTArray<nsCOMPtr<nsIContent>>* list =
    mContentInsertions.LookupOrAdd(aContainer);

  bool needsProcessing = false;
  nsIContent* node = aStartChildNode;
  while (node != aEndChildNode) {
    // Notification triggers for content insertion even if no content was
    // actually inserted, check if the given content has a frame to discard
    // this case early.
    if (node->GetPrimaryFrame()) {
      if (list->AppendElement(node))
        needsProcessing = true;
    }
    node = node->GetNextSibling();
  }

  if (needsProcessing) {
    ScheduleProcessing();
  }
}

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

////////////////////////////////////////////////////////////////////////////////
// NotificationCollector: protected

bool
NotificationController::IsUpdatePending()
{
  return mPresShell->IsLayoutFlushObserver() ||
    mObservingState == eRefreshProcessingForUpdate ||
    mContentInsertions.Count() != 0 || mNotifications.Length() != 0 ||
    mTextHash.Count() != 0 ||
    !mDocument->HasLoadState(DocAccessible::eTreeConstructed);
}

////////////////////////////////////////////////////////////////////////////////
// NotificationCollector: private

void
NotificationController::WillRefresh(mozilla::TimeStamp aTime)
{
  PROFILER_LABEL_FUNC(js::ProfileEntry::Category::OTHER);
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

    NS_ASSERTION(mContentInsertions.Count() == 0,
                 "Pending content insertions while initial accessible tree isn't created!");
  }

  // Initialize scroll support if needed.
  if (!(mDocument->mDocFlags & DocAccessible::eScrollInitialized))
    mDocument->AddScrollListener();

  // Process rendered text change notifications.
  for (auto iter = mTextHash.Iter(); !iter.Done(); iter.Next()) {
    nsCOMPtrHashKey<nsIContent>* entry = iter.Get();
    nsIContent* textNode = entry->GetKey();
    Accessible* textAcc = mDocument->GetAccessible(textNode);

    // If the text node is not in tree or doesn't have frame then this case should
    // have been handled already by content removal notifications.
    nsINode* containerNode = textNode->GetParentNode();
    if (!containerNode) {
      NS_ASSERTION(!textAcc,
                   "Text node was removed but accessible is kept alive!");
      continue;
    }

    nsIFrame* textFrame = textNode->GetPrimaryFrame();
    if (!textFrame) {
      NS_ASSERTION(!textAcc,
                   "Text node isn't rendered but accessible is kept alive!");
      continue;
    }

    nsIContent* containerElm = containerNode->IsElement() ?
      containerNode->AsElement() : nullptr;

    nsIFrame::RenderedText text = textFrame->GetRenderedText(0,
        UINT32_MAX, nsIFrame::TextOffsetType::OFFSETS_IN_CONTENT_TEXT,
        nsIFrame::TrailingWhitespace::DONT_TRIM_TRAILING_WHITESPACE);

    // Remove text accessible if rendered text is empty.
    if (textAcc) {
      if (text.mString.IsEmpty()) {
  #ifdef A11Y_LOG
        if (logging::IsEnabled(logging::eTree | logging::eText)) {
          logging::MsgBegin("TREE", "text node lost its content; doc: %p", mDocument);
          logging::Node("container", containerElm);
          logging::Node("content", textNode);
          logging::MsgEnd();
        }
  #endif

        mDocument->ContentRemoved(containerElm, textNode);
        continue;
      }

      // Update text of the accessible and fire text change events.
  #ifdef A11Y_LOG
      if (logging::IsEnabled(logging::eText)) {
        logging::MsgBegin("TEXT", "text may be changed; doc: %p", mDocument);
        logging::Node("container", containerElm);
        logging::Node("content", textNode);
        logging::MsgEntry("old text '%s'",
                          NS_ConvertUTF16toUTF8(textAcc->AsTextLeaf()->Text()).get());
        logging::MsgEntry("new text: '%s'",
                          NS_ConvertUTF16toUTF8(text.mString).get());
        logging::MsgEnd();
      }
  #endif

      TextUpdater::Run(mDocument, textAcc->AsTextLeaf(), text.mString);
      continue;
    }

    // Append an accessible if rendered text is not empty.
    if (!text.mString.IsEmpty()) {
  #ifdef A11Y_LOG
      if (logging::IsEnabled(logging::eTree | logging::eText)) {
        logging::MsgBegin("TREE", "text node gains new content; doc: %p", mDocument);
        logging::Node("container", containerElm);
        logging::Node("content", textNode);
        logging::MsgEnd();
      }
  #endif

      Accessible* container = mDocument->AccessibleOrTrueContainer(containerNode);
      MOZ_ASSERT(container,
                 "Text node having rendered text hasn't accessible document!");
      if (container) {
        nsTArray<nsCOMPtr<nsIContent>>* list =
          mContentInsertions.LookupOrAdd(container);
        list->AppendElement(textNode);
      }
    }
  }
  mTextHash.Clear();

  // Process content inserted notifications to update the tree.
  for (auto iter = mContentInsertions.ConstIter(); !iter.Done(); iter.Next()) {
    mDocument->ProcessContentInserted(iter.Key(), iter.UserData());
    if (!mDocument) {
      return;
    }
  }
  mContentInsertions.Clear();

  // Bind hanging child documents.
  uint32_t hangingDocCnt = mHangingChildDocuments.Length();
  nsTArray<RefPtr<DocAccessible>> newChildDocs;
  for (uint32_t idx = 0; idx < hangingDocCnt; idx++) {
    DocAccessible* childDoc = mHangingChildDocuments[idx];
    if (childDoc->IsDefunct())
      continue;

    nsIContent* ownerContent = mDocument->DocumentNode()->
      FindContentForSubDocument(childDoc->DocumentNode());
    if (ownerContent) {
      Accessible* outerDocAcc = mDocument->GetAccessible(ownerContent);
      if (outerDocAcc && outerDocAcc->AppendChild(childDoc)) {
        if (mDocument->AppendChildDocument(childDoc)) {
          newChildDocs.AppendElement(Move(mHangingChildDocuments[idx]));
          continue;
        }

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
  nsTArray < RefPtr<Notification> > notifications;
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

  // We cannot rely on DOM tree to keep aria-owns relations updated. Make
  // a validation to remove dead links.
  mDocument->ValidateARIAOwned();

  // Process relocation list.
  for (uint32_t idx = 0; idx < mRelocations.Length(); idx++) {
    if (mRelocations[idx]->IsInDocument()) {
      mDocument->DoARIAOwnsRelocation(mRelocations[idx]);
    }
  }
  mRelocations.Clear();

  // If a generic notification occurs after this point then we may be allowed to
  // process it synchronously.  However we do not want to reenter if fireing
  // events causes script to run.
  mObservingState = eRefreshProcessing;

  RefPtr<DocAccessible> deathGrip(mDocument);
  mEventTree.Process(deathGrip);
  deathGrip = nullptr;

  ProcessEventQueue();

  if (IPCAccessibilityActive()) {
    size_t newDocCount = newChildDocs.Length();
    for (size_t i = 0; i < newDocCount; i++) {
      DocAccessible* childDoc = newChildDocs[i];
      Accessible* parent = childDoc->Parent();
      DocAccessibleChild* parentIPCDoc = mDocument->IPCDoc();
      uint64_t id = reinterpret_cast<uintptr_t>(parent->UniqueID());
      MOZ_ASSERT(id);
      DocAccessibleChild* ipcDoc = childDoc->IPCDoc();
      if (ipcDoc) {
        parentIPCDoc->SendBindChildDoc(ipcDoc, id);
        continue;
      }

      ipcDoc = new DocAccessibleChild(childDoc);
      childDoc->SetIPCDoc(ipcDoc);
      nsCOMPtr<nsITabChild> tabChild =
        do_GetInterface(mDocument->DocumentNode()->GetDocShell());
      if (tabChild) {
        static_cast<TabChild*>(tabChild.get())->
          SendPDocAccessibleConstructor(ipcDoc, parentIPCDoc, id);
      }
    }
  }

  mObservingState = eRefreshObserving;
  if (!mDocument)
    return;

  // Stop further processing if there are no new notifications of any kind or
  // events and document load is processed.
  if (mContentInsertions.Count() == 0 && mNotifications.IsEmpty() &&
      mEvents.IsEmpty() && mTextHash.Count() == 0 &&
      mHangingChildDocuments.IsEmpty() &&
      mDocument->HasLoadState(DocAccessible::eCompletelyLoaded) &&
      mPresShell->RemoveRefreshObserver(this, Flush_Display)) {
    mObservingState = eNotObservingRefresh;
  }
}
