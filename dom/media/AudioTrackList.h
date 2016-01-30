/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 et tw=78: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_AudioTrackList_h
#define mozilla_dom_AudioTrackList_h

#include "MediaTrack.h"
#include "MediaTrackList.h"

namespace mozilla {
namespace dom {

class AudioTrack;

class AudioTrackList : public MediaTrackList
{
public:
  AudioTrackList(nsPIDOMWindowInner* aOwnerWindow, HTMLMediaElement* aMediaElement)
    : MediaTrackList(aOwnerWindow, aMediaElement) {}

  JSObject* WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

  AudioTrack* operator[](uint32_t aIndex);

  // WebIDL
  AudioTrack* IndexedGetter(uint32_t aIndex, bool& aFound);

  AudioTrack* GetTrackById(const nsAString& aId);

protected:
  AudioTrackList* AsAudioTrackList() override { return this; }
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_AudioTrackList_h
