/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 et tw=78: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_TextTrackCueList_h
#define mozilla_dom_TextTrackCueList_h

#include "nsTArray.h"
#include "nsCOMPtr.h"
#include "nsCycleCollectionParticipant.h"
#include "nsWrapperCache.h"
#include "mozilla/ErrorResult.h"
#include "Intervals.h"

namespace mozilla {
namespace dom {

class TextTrackCue;

class TextTrackCueList final : public nsISupports
                             , public nsWrapperCache
{
public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(TextTrackCueList)

  // TextTrackCueList WebIDL
  explicit TextTrackCueList(nsISupports* aParent);

  JSObject* WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

  nsISupports* GetParentObject() const
  {
    return mParent;
  }

  uint32_t Length() const
  {
    return mList.Length();
  }

  bool IsEmpty() const
  {
    return mList.Length() == 0;
  }

  TextTrackCue* IndexedGetter(uint32_t aIndex, bool& aFound);
  TextTrackCue* operator[](uint32_t aIndex);
  TextTrackCue* GetCueById(const nsAString& aId);
  TextTrackCueList& operator=(const TextTrackCueList& aOther);
  // Adds a cue to mList by performing an insertion sort on mList.
  // We expect most files to already be sorted, so an insertion sort starting
  // from the end of the current array should be more efficient than a general
  // sort step after all cues are loaded.
  void AddCue(TextTrackCue& aCue);
  void RemoveCue(TextTrackCue& aCue);
  void RemoveCue(TextTrackCue& aCue, ErrorResult& aRv);
  void RemoveCueAt(uint32_t aIndex);
  void RemoveAll();
  void GetArray(nsTArray<RefPtr<TextTrackCue> >& aCues);

  void SetCuesInactive();

  already_AddRefed<TextTrackCueList>
  GetCueListByTimeInterval(media::Interval<double>& aInterval);
  void NotifyCueUpdated(TextTrackCue *aCue);
  bool IsCueExist(TextTrackCue *aCue);

private:
  ~TextTrackCueList();

  nsCOMPtr<nsISupports> mParent;

  // A sorted list of TextTrackCues sorted by earliest start time. If the start
  // times are equal then it will be sorted by end time, earliest first.
  nsTArray< RefPtr<TextTrackCue> > mList;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_TextTrackCueList_h
