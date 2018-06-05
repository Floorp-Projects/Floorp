/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#if !defined(AutoplayPolicy_h_)
#define AutoplayPolicy_h_

#include "mozilla/NotNull.h"

class nsIDocument;

namespace mozilla {
namespace dom {

class HTMLMediaElement;
class AudioContext;

/**
 * AutoplayPolicy is used to manage autoplay logic for all kinds of media,
 * including MediaElement, Web Audio and Web Speech.
 *
 * Autoplay could be disable by turn off the pref "media.autoplay.enabled".
 * Once user disable autoplay, media could only be played if one of following
 * conditions is true.
 * 1) Owner document is activated by user gestures
 *    We restrict user gestures to "mouse click", "keyboard press" and "touch".
 * 2) Muted media content or video without audio content.
 * 3) Document's origin has the "autoplay-media" permission.
 */
class AutoplayPolicy
{
public:
  static bool IsMediaElementAllowedToPlay(NotNull<HTMLMediaElement*> aElement);
  static bool IsAudioContextAllowedToPlay(NotNull<AudioContext*> aContext);
private:
  static bool IsDocumentAllowedToPlay(nsIDocument* aDoc);
};

} // namespace dom
} // namespace mozilla

#endif
