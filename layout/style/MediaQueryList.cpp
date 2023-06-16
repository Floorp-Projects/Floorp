/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* implements DOM interface for querying and observing media queries */

#include "mozilla/dom/MediaQueryList.h"
#include "mozilla/dom/MediaQueryListEvent.h"
#include "mozilla/dom/MediaList.h"
#include "mozilla/dom/EventTarget.h"
#include "mozilla/dom/EventTargetBinding.h"
#include "nsPresContext.h"
#include "mozilla/dom/Document.h"

#define ONCHANGE_STRING u"change"_ns

namespace mozilla::dom {

MediaQueryList::MediaQueryList(Document* aDocument,
                               const nsACString& aMediaQueryList,
                               CallerType aCallerType)
    : DOMEventTargetHelper(aDocument->GetInnerWindow()),
      mDocument(aDocument),
      mMediaList(MediaList::Create(aMediaQueryList, aCallerType)),
      mViewportDependent(mMediaList->IsViewportDependent()) {
  KeepAliveIfHasListenersFor(nsGkAtoms::onchange);
}

MediaQueryList::~MediaQueryList() = default;

NS_IMPL_CYCLE_COLLECTION_CLASS(MediaQueryList)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(MediaQueryList,
                                                  DOMEventTargetHelper)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mDocument)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(MediaQueryList,
                                                DOMEventTargetHelper)
  if (tmp->mDocument) {
    static_cast<LinkedListElement<MediaQueryList>*>(tmp)->remove();
    NS_IMPL_CYCLE_COLLECTION_UNLINK(mDocument)
  }
  tmp->Disconnect();
  NS_IMPL_CYCLE_COLLECTION_UNLINK_PRESERVED_WRAPPER
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(MediaQueryList)
NS_INTERFACE_MAP_END_INHERITING(DOMEventTargetHelper)

NS_IMPL_ADDREF_INHERITED(MediaQueryList, DOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(MediaQueryList, DOMEventTargetHelper)

void MediaQueryList::GetMedia(nsACString& aMedia) const {
  mMediaList->GetText(aMedia);
}

bool MediaQueryList::Matches() {
  if (mViewportDependent &&
      mDocument->StyleOrLayoutObservablyDependsOnParentDocumentLayout()) {
    RefPtr<Document> doc = mDocument;
    // This is enough to trigger media query updates in the current doc, and
    // will flush the parent document layout if appropriate.
    doc->FlushPendingNotifications(FlushType::Layout);
  }
  if (!mMatchesValid) {
    MOZ_ASSERT(!HasListeners(),
               "when listeners present, must keep mMatches current");
    RecomputeMatches();
  }
  return mMatches;
}

void MediaQueryList::AddListener(EventListener* aListener, ErrorResult& aRv) {
  if (!aListener) {
    return;
  }

  AddEventListenerOptionsOrBoolean options;
  options.SetAsBoolean() = false;

  AddEventListener(ONCHANGE_STRING, aListener, options, Nullable<bool>(), aRv);
}

void MediaQueryList::EventListenerAdded(nsAtom* aType) {
  // HasListeners() might still be false if the added thing wasn't a
  // listener we care about.
  if (!mMatchesValid && HasListeners()) {
    RecomputeMatches();
  }

  DOMEventTargetHelper::EventListenerAdded(aType);
}

void MediaQueryList::RemoveListener(EventListener* aListener,
                                    ErrorResult& aRv) {
  if (!aListener) {
    return;
  }

  EventListenerOptionsOrBoolean options;
  options.SetAsBoolean() = false;

  RemoveEventListener(ONCHANGE_STRING, aListener, options, aRv);
}

bool MediaQueryList::HasListeners() const {
  return HasListenersFor(ONCHANGE_STRING);
}

void MediaQueryList::Disconnect() {
  DisconnectFromOwner();

  IgnoreKeepAliveIfHasListenersFor(nsGkAtoms::onchange);
}

void MediaQueryList::RecomputeMatches() {
  mMatches = false;

  if (!mDocument) {
    return;
  }

  mMatches = mMediaList->Matches(*mDocument);
  mMatchesValid = true;
}

nsISupports* MediaQueryList::GetParentObject() const {
  return ToSupports(mDocument);
}

JSObject* MediaQueryList::WrapObject(JSContext* aCx,
                                     JS::Handle<JSObject*> aGivenProto) {
  return MediaQueryList_Binding::Wrap(aCx, this, aGivenProto);
}

bool MediaQueryList::MediaFeatureValuesChanged() {
  mMatchesValid = false;

  if (!HasListeners()) {
    return false;  // No need to recompute or notify if we have no listeners.
  }

  bool oldMatches = mMatches;
  RecomputeMatches();

  return mMatches != oldMatches;
}

void MediaQueryList::FireChangeEvent() {
  MediaQueryListEventInit init;
  init.mBubbles = false;
  init.mCancelable = false;
  init.mMatches = mMatches;
  mMediaList->GetText(init.mMedia);

  RefPtr<MediaQueryListEvent> event =
      MediaQueryListEvent::Constructor(this, ONCHANGE_STRING, init);
  event->SetTrusted(true);

  DispatchEvent(*event);
}

size_t MediaQueryList::SizeOfExcludingThis(MallocSizeOf aMallocSizeOf) const {
  size_t n = 0;
  // mMediaList is reference counted, but it's created and primarily owned
  // by this MediaQueryList object.
  n += mMediaList->SizeOfIncludingThis(aMallocSizeOf);
  return n;
}

}  // namespace mozilla::dom
