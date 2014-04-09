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

namespace mozilla {
namespace dom {

class TextTrackCue;

class TextTrackCueList MOZ_FINAL : public nsISupports
                                 , public nsWrapperCache
{
public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(TextTrackCueList)

  // TextTrackCueList WebIDL
  TextTrackCueList(nsISupports* aParent);

  virtual JSObject* WrapObject(JSContext* aCx) MOZ_OVERRIDE;

  nsISupports* GetParentObject() const
  {
    return mParent;
  }

  uint32_t Length() const
  {
    return mList.Length();
  }

  TextTrackCue* IndexedGetter(uint32_t aIndex, bool& aFound);
  TextTrackCue* operator[](uint32_t aIndex);
  TextTrackCue* GetCueById(const nsAString& aId);

  // Adds a cue to mList by performing an insertion sort on mList.
  // We expect most files to already be sorted, so an insertion sort starting
  // from the end of the current array should be more efficient than a general
  // sort step after all cues are loaded.
  void AddCue(TextTrackCue& aCue);
  void RemoveCue(TextTrackCue& aCue, ErrorResult& aRv);
  void RemoveCueAt(uint32_t aIndex);
  void RemoveAll();
  void GetArray(nsTArray<nsRefPtr<TextTrackCue> >& aCues);

private:
  nsCOMPtr<nsISupports> mParent;

  // A sorted list of TextTrackCues sorted by earliest start time. If the start
  // times are equal then it will be sorted by end time, earliest first.
  nsTArray< nsRefPtr<TextTrackCue> > mList;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_TextTrackCueList_h
