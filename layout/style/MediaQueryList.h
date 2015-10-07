/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set shiftwidth=2 tabstop=8 autoindent cindent expandtab: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* implements DOM interface for querying and observing media queries */

#ifndef mozilla_dom_MediaQueryList_h
#define mozilla_dom_MediaQueryList_h

#include "nsISupports.h"
#include "nsCycleCollectionParticipant.h"
#include "nsAutoPtr.h"
#include "nsCOMPtr.h"
#include "nsTArray.h"
#include "prclist.h"
#include "mozilla/Attributes.h"
#include "nsWrapperCache.h"
#include "mozilla/dom/MediaQueryListBinding.h"

class nsIDocument;
class nsMediaList;

namespace mozilla {
namespace dom {

class MediaQueryList final : public nsISupports,
                             public nsWrapperCache,
                             public PRCList
{
public:
  // The caller who constructs is responsible for calling Evaluate
  // before calling any other methods.
  MediaQueryList(nsIDocument *aDocument,
                 const nsAString &aMediaQueryList);
private:
  ~MediaQueryList();

public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(MediaQueryList)

  nsISupports* GetParentObject() const;

  struct HandleChangeData {
    nsRefPtr<MediaQueryList> mql;
    nsRefPtr<mozilla::dom::MediaQueryListListener> callback;
  };

  // Appends listeners that need notification to aListenersToNotify
  void MediumFeaturesChanged(nsTArray<HandleChangeData>& aListenersToNotify);

  bool HasListeners() const { return !mCallbacks.IsEmpty(); }

  void RemoveAllListeners();

  JSObject* WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

  // WebIDL methods
  void GetMedia(nsAString& aMedia);
  bool Matches();
  void AddListener(mozilla::dom::MediaQueryListListener& aListener);
  void RemoveListener(mozilla::dom::MediaQueryListListener& aListener);

private:
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

  nsRefPtr<nsMediaList> mMediaList;
  bool mMatches;
  bool mMatchesValid;
  nsTArray<nsRefPtr<mozilla::dom::MediaQueryListListener>> mCallbacks;
};

} // namespace dom
} // namespace mozilla

#endif /* !defined(mozilla_dom_MediaQueryList_h) */
