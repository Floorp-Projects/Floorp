/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "VideoPlaybackQuality.h"

#include "mozilla/dom/HTMLMediaElement.h"
#include "mozilla/dom/VideoPlaybackQualityBinding.h"
#include "nsCycleCollectionParticipant.h"
#include "nsWrapperCache.h"

namespace mozilla {
namespace dom {

VideoPlaybackQuality::VideoPlaybackQuality(HTMLMediaElement* aElement,
                                           DOMHighResTimeStamp aCreationTime,
                                           uint32_t aTotalFrames,
                                           uint32_t aDroppedFrames,
                                           uint32_t aCorruptedFrames)
  : mElement(aElement)
  , mCreationTime(aCreationTime)
  , mTotalFrames(aTotalFrames)
  , mDroppedFrames(aDroppedFrames)
  , mCorruptedFrames(aCorruptedFrames)
{
}

HTMLMediaElement*
VideoPlaybackQuality::GetParentObject() const
{
  return mElement;
}

JSObject*
VideoPlaybackQuality::WrapObject(JSContext *aCx, JS::Handle<JSObject*> aGivenProto)
{
  return VideoPlaybackQuality_Binding::Wrap(aCx, this, aGivenProto);
}

NS_IMPL_CYCLE_COLLECTION_ROOT_NATIVE(VideoPlaybackQuality, AddRef)
NS_IMPL_CYCLE_COLLECTION_UNROOT_NATIVE(VideoPlaybackQuality, Release)

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(VideoPlaybackQuality, mElement)

} // namespace dom
} // namespace mozilla
