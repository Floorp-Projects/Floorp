/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 et tw=78: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_TextTrackRegionList_h
#define mozilla_dom_TextTrackRegionList_h

#include "nsAutoPtr.h"
#include "nsCycleCollectionParticipant.h"
#include "nsTArray.h"
#include "nsWrapperCache.h"

namespace mozilla {
namespace dom {

class TextTrackRegion;

class TextTrackRegionList MOZ_FINAL : public nsISupports,
                                      public nsWrapperCache
{
public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(TextTrackRegionList)

  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aScope) MOZ_OVERRIDE;

  TextTrackRegionList(nsISupports* aGlobal);

  nsISupports* GetParentObject() const
  {
    return mParent;
  }

  /** WebIDL Methods. */
  uint32_t Length() const
  {
    return mTextTrackRegions.Length();
  }

  TextTrackRegion* IndexedGetter(uint32_t aIndex, bool& aFound);

  TextTrackRegion* GetRegionById(const nsAString& aId);

  /** end WebIDL Methods. */

  void AddTextTrackRegion(TextTrackRegion* aRegion);

  void RemoveTextTrackRegion(const TextTrackRegion& aRegion);

private:
  nsCOMPtr<nsISupports> mParent;
  nsTArray<nsRefPtr<TextTrackRegion> > mTextTrackRegions;
};

} //namespace dom
} //namespace mozilla

#endif //mozilla_dom_TextTrackRegionList_h
