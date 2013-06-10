/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 et tw=78: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_TextTrackCueList_h
#define mozilla_dom_TextTrackCueList_h

#include "mozilla/dom/TextTrackCue.h"
#include "nsCOMPtr.h"
#include "nsCycleCollectionParticipant.h"
#include "nsWrapperCache.h"

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

  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aScope) MOZ_OVERRIDE;

  nsISupports* GetParentObject() const
  {
    return mParent;
  }

  uint32_t Length() const
  {
    return mList.Length();
  }

  // Time is in seconds.
  void Update(double aTime);

  TextTrackCue* IndexedGetter(uint32_t aIndex, bool& aFound);
  TextTrackCue* GetCueById(const nsAString& aId);

  void AddCue(TextTrackCue& cue);
  void RemoveCue(TextTrackCue& cue);

private:
  nsCOMPtr<nsISupports> mParent;

  nsTArray< nsRefPtr<TextTrackCue> > mList;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_TextTrackCueList_h
