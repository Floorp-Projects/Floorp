/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SubBufferDecoder.h"
#include "MediaSourceDecoder.h"
#include "MediaDecoderReader.h"
#include "mozilla/dom/TimeRanges.h"

#ifdef PR_LOGGING
extern PRLogModuleInfo* GetMediaSourceLog();
extern PRLogModuleInfo* GetMediaSourceAPILog();

#define MSE_DEBUG(...) PR_LOG(GetMediaSourceLog(), PR_LOG_DEBUG, (__VA_ARGS__))
#define MSE_API(...) PR_LOG(GetMediaSourceAPILog(), PR_LOG_DEBUG, (__VA_ARGS__))
#else
#define MSE_DEBUG(...)
#define MSE_API(...)
#endif

namespace mozilla {

class ReentrantMonitor;

namespace layers {

class ImageContainer;

} // namespace layers


ReentrantMonitor&
SubBufferDecoder::GetReentrantMonitor()
{
  return mParentDecoder->GetReentrantMonitor();
}

bool
SubBufferDecoder::OnStateMachineThread() const
{
  return mParentDecoder->OnStateMachineThread();
}

bool
SubBufferDecoder::OnDecodeThread() const
{
  return mParentDecoder->OnDecodeThread();
}

SourceBufferResource*
SubBufferDecoder::GetResource() const
{
  return static_cast<SourceBufferResource*>(mResource.get());
}

void
SubBufferDecoder::NotifyDecodedFrames(uint32_t aParsed, uint32_t aDecoded)
{
  return mParentDecoder->NotifyDecodedFrames(aParsed, aDecoded);
}

void
SubBufferDecoder::SetMediaDuration(int64_t aDuration)
{
  mMediaDuration = aDuration;
}

void
SubBufferDecoder::UpdateEstimatedMediaDuration(int64_t aDuration)
{
  MSE_DEBUG("SubBufferDecoder(%p)::UpdateEstimatedMediaDuration(aDuration=%lld)", this, aDuration);
}

void
SubBufferDecoder::SetMediaSeekable(bool aMediaSeekable)
{
  MSE_DEBUG("SubBufferDecoder(%p)::SetMediaSeekable(aMediaSeekable=%d)", this, aMediaSeekable);
}

layers::ImageContainer*
SubBufferDecoder::GetImageContainer()
{
  return mParentDecoder->GetImageContainer();
}

MediaDecoderOwner*
SubBufferDecoder::GetOwner()
{
  return mParentDecoder->GetOwner();
}

void
SubBufferDecoder::NotifyDataArrived(const char* aBuffer, uint32_t aLength, int64_t aOffset)
{
  mReader->NotifyDataArrived(aBuffer, aLength, aOffset);

  // XXX: Params make no sense to parent decoder as it relates to a
  // specific SubBufferDecoder's data stream.  Pass bogus values here to
  // force parent decoder's state machine to recompute end time for
  // infinite length media.
  mParentDecoder->NotifyDataArrived(nullptr, 0, 0);
}

nsresult
SubBufferDecoder::GetBuffered(dom::TimeRanges* aBuffered)
{
  // XXX: Need mStartTime (from StateMachine) instead of passing 0.
  return mReader->GetBuffered(aBuffered, 0);
}

int64_t
SubBufferDecoder::ConvertToByteOffset(double aTime)
{
  int64_t readerOffset = mReader->GetEvictionOffset(aTime);
  if (readerOffset >= 0) {
    return readerOffset;
  }

  // Uses a conversion based on (aTime/duration) * length.  For the
  // purposes of eviction this should be adequate since we have the
  // byte threshold as well to ensure data actually gets evicted and
  // we ensure we don't evict before the current playable point.
  if (mMediaDuration <= 0) {
    return -1;
  }
  int64_t length = GetResource()->GetLength();
  MOZ_ASSERT(length > 0);
  int64_t offset = (aTime / (double(mMediaDuration) / USECS_PER_S)) * length;
  return offset;
}

bool
SubBufferDecoder::ContainsTime(double aTime)
{
  ErrorResult dummy;
  nsRefPtr<dom::TimeRanges> ranges = new dom::TimeRanges();
  nsresult rv = GetBuffered(ranges);
  if (NS_FAILED(rv) || ranges->Length() == 0) {
    return false;
  }
  return ranges->Find(aTime) != dom::TimeRanges::NoIndex;
}

} // namespace mozilla
