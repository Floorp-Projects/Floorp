/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 et tw=78: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_VideoTrackList_h
#define mozilla_dom_VideoTrackList_h

#include "MediaTrack.h"
#include "MediaTrackList.h"

namespace mozilla {
namespace dom {

class VideoTrack;

class VideoTrackList : public MediaTrackList
{
public:
  VideoTrackList(nsPIDOMWindowInner* aOwnerWindow,
                 HTMLMediaElement* aMediaElement)
    : MediaTrackList(aOwnerWindow, aMediaElement)
    , mSelectedIndex(-1)
  {}

  JSObject* WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

  VideoTrack* operator[](uint32_t aIndex);

  void RemoveTrack(const RefPtr<MediaTrack>& aTrack) override;

  void EmptyTracks() override;

  VideoTrack* GetSelectedTrack();

  // WebIDL
  int32_t SelectedIndex() const
  {
    return mSelectedIndex;
  }

  VideoTrack* IndexedGetter(uint32_t aIndex, bool& aFound);

  VideoTrack* GetTrackById(const nsAString& aId);

  friend class VideoTrack;

protected:
  VideoTrackList* AsVideoTrackList() override { return this; }

private:
  int32_t mSelectedIndex;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_VideoTrackList_h
