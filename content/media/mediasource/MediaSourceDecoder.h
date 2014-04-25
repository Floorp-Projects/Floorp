/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_MEDIASOURCEDECODER_H_
#define MOZILLA_MEDIASOURCEDECODER_H_

#include "MediaCache.h"
#include "MediaDecoder.h"
#include "mozilla/Attributes.h"
#include "mozilla/ReentrantMonitor.h"
#include "nsCOMPtr.h"
#include "nsError.h"
#include "nsStringGlue.h"
#include "nsTArray.h"

class nsIStreamListener;

namespace mozilla {

class MediaResource;
class MediaDecoderReader;
class MediaDecoderStateMachine;
class SubBufferDecoder;
class MediaSourceReader;

namespace dom {

class HTMLMediaElement;
class MediaSource;

} // namespace dom

class MediaSourceDecoder : public MediaDecoder
{
public:
  MediaSourceDecoder(dom::HTMLMediaElement* aElement);

  virtual MediaDecoder* Clone() MOZ_OVERRIDE;
  virtual MediaDecoderStateMachine* CreateStateMachine() MOZ_OVERRIDE;
  virtual nsresult Load(nsIStreamListener**, MediaDecoder*) MOZ_OVERRIDE;
  virtual nsresult GetSeekable(dom::TimeRanges* aSeekable) MOZ_OVERRIDE;

  static already_AddRefed<MediaResource> CreateResource();

  void AttachMediaSource(dom::MediaSource* aMediaSource);
  void DetachMediaSource();

  already_AddRefed<SubBufferDecoder> CreateSubDecoder(const nsACString& aType);

  nsresult EnqueueDecoderInitialization();

private:
  // The owning MediaSource holds a strong reference to this decoder, and
  // calls Attach/DetachMediaSource on this decoder to set and clear
  // mMediaSource.
  dom::MediaSource* mMediaSource;
};

} // namespace mozilla

#endif /* MOZILLA_MEDIASOURCEDECODER_H_ */
