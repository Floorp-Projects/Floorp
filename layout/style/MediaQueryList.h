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
#include "nsTArray.h"
#include "prclist.h"
#include "mozilla/Attributes.h"
#include "nsWrapperCache.h"
#include "mozilla/dom/MediaQueryListBinding.h"

class nsPresContext;
class nsMediaList;

namespace mozilla {
namespace dom {

class MediaQueryList MOZ_FINAL : public nsISupports,
                                 public nsWrapperCache,
                                 public PRCList
{
public:
  // The caller who constructs is responsible for calling Evaluate
  // before calling any other methods.
  MediaQueryList(nsPresContext *aPresContext,
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

  typedef FallibleTArray< nsRefPtr<mozilla::dom::MediaQueryListListener> > CallbackList;
  typedef FallibleTArray<HandleChangeData> NotifyList;

  // Appends listeners that need notification to aListenersToNotify
  void MediumFeaturesChanged(NotifyList &aListenersToNotify);

  bool HasListeners() const { return !mCallbacks.IsEmpty(); }

  void RemoveAllListeners();

  JSObject* WrapObject(JSContext* aCx, JS::Handle<JSObject*> aScope) MOZ_OVERRIDE;

  // WebIDL methods
  void GetMedia(nsAString& aMedia);
  bool Matches();
  void AddListener(mozilla::dom::MediaQueryListListener& aListener);
  void RemoveListener(mozilla::dom::MediaQueryListListener& aListener);

private:
  void RecomputeMatches();

  // We only need a pointer to the pres context to support lazy
  // reevaluation following dynamic changes.  However, this lazy
  // reevaluation is perhaps somewhat important, since some usage
  // patterns may involve the creation of large numbers of
  // MediaQueryList objects which almost immediately become garbage
  // (after a single call to the .matches getter).
  //
  // This pointer does make us a little more dependent on cycle
  // collection.
  //
  // We have a non-null mPresContext for our entire lifetime except
  // after cycle collection unlinking.  Having a non-null mPresContext
  // is equivalent to being in that pres context's mDOMMediaQueryLists
  // linked list.
  nsRefPtr<nsPresContext> mPresContext;

  nsRefPtr<nsMediaList> mMediaList;
  bool mMatches;
  bool mMatchesValid;
  CallbackList mCallbacks;
};

} // namespace dom
} // namespace mozilla

#endif /* !defined(mozilla_dom_MediaQueryList_h) */
