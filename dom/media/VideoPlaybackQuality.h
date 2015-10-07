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

namespace mozilla {
namespace dom {

class VideoPlaybackQuality final : public nsWrapperCache
{
public:
  NS_INLINE_DECL_CYCLE_COLLECTING_NATIVE_REFCOUNTING(VideoPlaybackQuality)
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_NATIVE_CLASS(VideoPlaybackQuality)

  VideoPlaybackQuality(HTMLMediaElement* aElement, DOMHighResTimeStamp aCreationTime,
                       uint64_t aTotalFrames, uint64_t aDroppedFrames,
                       uint64_t aCorruptedFrames);

  HTMLMediaElement* GetParentObject() const;

  JSObject* WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

  DOMHighResTimeStamp CreationTime() const
  {
    return mCreationTime;
  }

  uint64_t TotalVideoFrames()
  {
    return mTotalFrames;
  }

  uint64_t DroppedVideoFrames()
  {
    return mDroppedFrames;
  }

  uint64_t CorruptedVideoFrames()
  {
    return mCorruptedFrames;
  }

private:
  ~VideoPlaybackQuality() {}

  nsRefPtr<HTMLMediaElement> mElement;
  DOMHighResTimeStamp mCreationTime;
  uint64_t mTotalFrames;
  uint64_t mDroppedFrames;
  uint64_t mCorruptedFrames;
};

} // namespace dom
} // namespace mozilla

#endif /* mozilla_dom_VideoPlaybackQuality_h_ */
