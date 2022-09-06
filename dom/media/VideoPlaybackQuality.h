/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_VideoPlaybackQuality_h_
#define mozilla_dom_VideoPlaybackQuality_h_

#include "mozilla/dom/HTMLMediaElement.h"
#include "nsCycleCollectionParticipant.h"
#include "nsDOMNavigationTiming.h"
#include "nsWrapperCache.h"

namespace mozilla::dom {

class VideoPlaybackQuality final : public nsWrapperCache {
 public:
  NS_INLINE_DECL_CYCLE_COLLECTING_NATIVE_REFCOUNTING(VideoPlaybackQuality)
  NS_DECL_CYCLE_COLLECTION_NATIVE_WRAPPERCACHE_CLASS(VideoPlaybackQuality)

  VideoPlaybackQuality(HTMLMediaElement* aElement,
                       DOMHighResTimeStamp aCreationTime, uint32_t aTotalFrames,
                       uint32_t aDroppedFrames);

  HTMLMediaElement* GetParentObject() const;

  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override;

  DOMHighResTimeStamp CreationTime() const { return mCreationTime; }

  uint32_t TotalVideoFrames() const { return mTotalFrames; }

  uint32_t DroppedVideoFrames() const { return mDroppedFrames; }

 private:
  ~VideoPlaybackQuality() = default;

  RefPtr<HTMLMediaElement> mElement;
  DOMHighResTimeStamp mCreationTime;
  uint32_t mTotalFrames;
  uint32_t mDroppedFrames;
};

}  // namespace mozilla::dom

#endif  // mozilla_dom_VideoPlaybackQuality_h_
