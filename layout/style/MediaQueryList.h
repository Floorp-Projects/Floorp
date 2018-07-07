/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* implements DOM interface for querying and observing media queries */

#ifndef mozilla_dom_MediaQueryList_h
#define mozilla_dom_MediaQueryList_h

#include "nsISupports.h"
#include "nsCycleCollectionParticipant.h"
#include "nsCOMPtr.h"
#include "nsTArray.h"
#include "mozilla/LinkedList.h"
#include "mozilla/Attributes.h"
#include "nsWrapperCache.h"
#include "mozilla/DOMEventTargetHelper.h"
#include "mozilla/dom/MediaQueryListBinding.h"

class nsIDocument;

namespace mozilla {
namespace dom {

class MediaList;

class MediaQueryList final : public DOMEventTargetHelper,
                             public mozilla::LinkedListElement<MediaQueryList>
{
public:
  // The caller who constructs is responsible for calling Evaluate
  // before calling any other methods.
  MediaQueryList(nsIDocument* aDocument,
                 const nsAString& aMediaQueryList,
                 mozilla::dom::CallerType aCallerType);
private:
  ~MediaQueryList();

public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(MediaQueryList, DOMEventTargetHelper)

  nsISupports* GetParentObject() const;

  void MaybeNotify();

  JSObject* WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

  // WebIDL methods
  void GetMedia(nsAString& aMedia);
  bool Matches();
  void AddListener(EventListener* aListener, ErrorResult& aRv);
  void RemoveListener(EventListener* aListener, ErrorResult& aRv);

  using DOMEventTargetHelper::EventListenerAdded;
  void EventListenerAdded(nsAtom* aType) override;

  IMPL_EVENT_HANDLER(change)

  bool HasListeners();

  void Disconnect();

  size_t SizeOfExcludingThis(MallocSizeOf aMallocSizeOf) const;

private:
  void LastRelease() final
  {
    auto listElement = static_cast<LinkedListElement<MediaQueryList>*>(this);
    if (listElement->isInList()) {
      listElement->remove();
    }
  }

  void RecomputeMatches();

  // We only need a pointer to the document to support lazy
  // reevaluation following dynamic changes.  However, this lazy
  // reevaluation is perhaps somewhat important, since some usage
  // patterns may involve the creation of large numbers of
  // MediaQueryList objects which almost immediately become garbage
  // (after a single call to the .matches getter).
  //
  // This pointer does make us a little more dependent on cycle
  // collection.
  //
  // We have a non-null mDocument for our entire lifetime except
  // after cycle collection unlinking.  Having a non-null mDocument
  // is equivalent to being in that document's mDOMMediaQueryLists
  // linked list.
  nsCOMPtr<nsIDocument> mDocument;

  RefPtr<MediaList> mMediaList;
  bool mMatches;
  bool mMatchesValid;
};

} // namespace dom
} // namespace mozilla

#endif /* !defined(mozilla_dom_MediaQueryList_h) */
