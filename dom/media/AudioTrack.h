/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 et tw=78: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_AudioTrack_h
#define mozilla_dom_AudioTrack_h

#include "MediaTrack.h"

namespace mozilla::dom {

class AudioStreamTrack;

class AudioTrack : public MediaTrack {
 public:
  AudioTrack(nsIGlobalObject* aOwnerGlobal, const nsAString& aId,
             const nsAString& aKind, const nsAString& aLabel,
             const nsAString& aLanguage, bool aEnabled,
             AudioStreamTrack* aStreamTrack = nullptr);

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(AudioTrack, MediaTrack)

  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override;

  AudioTrack* AsAudioTrack() override { return this; }

  void SetEnabledInternal(bool aEnabled, int aFlags) override;

  // Get associated audio stream track when the audio track comes from
  // MediaStream. This might be nullptr when the src of owning HTMLMediaElement
  // is not MediaStream.
  AudioStreamTrack* GetAudioStreamTrack() { return mAudioStreamTrack; }

  // WebIDL
  bool Enabled() const { return mEnabled; }

  void SetEnabled(bool aEnabled);

 private:
  virtual ~AudioTrack();

  bool mEnabled;
  RefPtr<AudioStreamTrack> mAudioStreamTrack;
};

}  // namespace mozilla::dom

#endif  // mozilla_dom_AudioTrack_h
