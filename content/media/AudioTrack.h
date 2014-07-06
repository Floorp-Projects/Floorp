/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 et tw=78: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_AudioTrack_h
#define mozilla_dom_AudioTrack_h

#include "MediaTrack.h"

namespace mozilla {
namespace dom {

class AudioTrackList;

class AudioTrack : public MediaTrack
{
public:
  AudioTrack(const nsAString& aId,
             const nsAString& aKind,
             const nsAString& aLabel,
             const nsAString& aLanguage,
             bool aEnabled);

  virtual JSObject* WrapObject(JSContext* aCx) MOZ_OVERRIDE;

  virtual AudioTrack* AsAudioTrack() MOZ_OVERRIDE
  {
    return this;
  }

  virtual void SetEnabledInternal(bool aEnabled, int aFlags) MOZ_OVERRIDE;

  // WebIDL
  bool Enabled() const
  {
    return mEnabled;
  }

  void SetEnabled(bool aEnabled);

private:
  bool mEnabled;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_AudioTrack_h
