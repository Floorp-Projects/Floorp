/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <time.h>
#include "nsAutoPtr.h"
#include "ISOControl.h"
#include "ISOMediaBoxes.h"
#include "EncodedFrameContainer.h"

namespace mozilla {

// For MP4 creation_time and modification_time offset from January 1, 1904 to
// January 1, 1970.
#define iso_time_offset 2082844800

FragmentBuffer::FragmentBuffer(uint32_t aTrackType, uint32_t aFragDuration,
                               TrackMetadataBase* aMetadata)
  : mTrackType(aTrackType)
  , mFragDuration(aFragDuration)
  , mMediaStartTime(0)
  , mFragmentNumber(0)
  , mLastFrameTimeOfLastFragment(0)
  , mEOS(false)
{
  mFragArray.AppendElement();
  if (mTrackType == Audio_Track) {
    nsRefPtr<AACTrackMetadata> audMeta = static_cast<AACTrackMetadata*>(aMetadata);
    MOZ_ASSERT(audMeta);
  } else {
    nsRefPtr<AVCTrackMetadata> vidMeta = static_cast<AVCTrackMetadata*>(aMetadata);
    MOZ_ASSERT(vidMeta);
  }
  MOZ_COUNT_CTOR(FragmentBuffer);
}

FragmentBuffer::~FragmentBuffer()
{
  MOZ_COUNT_DTOR(FragmentBuffer);
}

bool
FragmentBuffer::HasEnoughData()
{
  // Audio or video frame is enough to form a moof.
  return (mFragArray.Length() > 1);
}

nsresult
FragmentBuffer::GetCSD(nsTArray<uint8_t>& aCSD)
{
  if (!mCSDFrame) {
    return NS_ERROR_FAILURE;
  }
  aCSD.AppendElements(mCSDFrame->GetFrameData().Elements(),
                      mCSDFrame->GetFrameData().Length());

  return NS_OK;
}

nsresult
FragmentBuffer::AddFrame(EncodedFrame* aFrame)
{
  // already EOS, it rejects all new data.
  if (mEOS) {
    MOZ_ASSERT(0);
    return NS_OK;
  }

  EncodedFrame::FrameType type = aFrame->GetFrameType();
  if (type == EncodedFrame::AAC_CSD || type == EncodedFrame::AVC_CSD) {
    mCSDFrame = aFrame;
    // Ue CSD's timestamp as the start time. Encoder should send CSD frame first
    // and data frames.
    mMediaStartTime = aFrame->GetTimeStamp();
    mFragmentNumber = 1;
    return NS_OK;
  }

  // if the timestamp is incorrect, abort it.
  if (aFrame->GetTimeStamp() < mMediaStartTime) {
    MOZ_ASSERT(false);
    return NS_ERROR_FAILURE;
  }

  mFragArray.LastElement().AppendElement(aFrame);

  // check if current fragment is reach the fragment duration.
  if ((aFrame->GetTimeStamp() - mMediaStartTime) > (mFragDuration * mFragmentNumber)) {
    mFragArray.AppendElement();
    mFragmentNumber++;
  }

  return NS_OK;
}

nsresult
FragmentBuffer::GetFirstFragment(nsTArray<nsRefPtr<EncodedFrame>>& aFragment,
                                 bool aFlush)
{
  // It should be called only if there is a complete fragment in mFragArray.
  if (mFragArray.Length() <= 1 && !mEOS) {
    MOZ_ASSERT(false);
    return NS_ERROR_FAILURE;
  }

  if (aFlush) {
    aFragment.SwapElements(mFragArray.ElementAt(0));
    mFragArray.RemoveElementAt(0);
  } else {
    aFragment.AppendElements(mFragArray.ElementAt(0));
  }
  return NS_OK;
}

uint32_t
FragmentBuffer::GetFirstFragmentSampleNumber()
{
  return mFragArray.ElementAt(0).Length();
}

uint32_t
FragmentBuffer::GetFirstFragmentSampleSize()
{
  uint32_t size = 0;
  uint32_t len = mFragArray.ElementAt(0).Length();
  for (uint32_t i = 0; i < len; i++) {
    size += mFragArray.ElementAt(0).ElementAt(i)->GetFrameData().Length();
  }
  return size;
}

ISOControl::ISOControl()
  : mAudioFragmentBuffer(nullptr)
  , mVideoFragmentBuffer(nullptr)
  , mFragNum(0)
  , mOutputSize(0)
  , mBitCount(0)
  , mBit(0)
{
  // Create a data array for first mp4 Box, ftyp.
  mOutBuffers.SetLength(1);
  MOZ_COUNT_CTOR(ISOControl);
}

ISOControl::~ISOControl()
{
  MOZ_COUNT_DTOR(ISOControl);
}

uint32_t
ISOControl::GetNextTrackID()
{
  return (mMetaArray.Length() + 1);
}

uint32_t
ISOControl::GetTrackID(uint32_t aTrackType)
{
  TrackMetadataBase::MetadataKind kind;
  if (aTrackType == Audio_Track) {
    kind = TrackMetadataBase::METADATA_AAC;
  } else {
    kind = TrackMetadataBase::METADATA_AVC;
  }

  for (uint32_t i = 0; i < mMetaArray.Length(); i++) {
    if (mMetaArray[i]->GetKind() == kind) {
      return (i + 1);
    }
  }

  return 0;
}

nsresult
ISOControl::SetMetadata(TrackMetadataBase* aTrackMeta)
{
  if (aTrackMeta->GetKind() == TrackMetadataBase::METADATA_AAC ||
      aTrackMeta->GetKind() == TrackMetadataBase::METADATA_AVC) {
    mMetaArray.AppendElement(aTrackMeta);
    return NS_OK;
  }
  return NS_ERROR_FAILURE;
}

nsresult
ISOControl::GetAudioMetadata(nsRefPtr<AACTrackMetadata>& aAudMeta)
{
  for (uint32_t i = 0; i < mMetaArray.Length() ; i++) {
    if (mMetaArray[i]->GetKind() == TrackMetadataBase::METADATA_AAC) {
      aAudMeta = static_cast<AACTrackMetadata*>(mMetaArray[i].get());
      return NS_OK;
    }
  }
  return NS_ERROR_FAILURE;
}

nsresult
ISOControl::GetVideoMetadata(nsRefPtr<AVCTrackMetadata>& aVidMeta)
{
  for (uint32_t i = 0; i < mMetaArray.Length() ; i++) {
    if (mMetaArray[i]->GetKind() == TrackMetadataBase::METADATA_AVC) {
      aVidMeta = static_cast<AVCTrackMetadata*>(mMetaArray[i].get());
      return NS_OK;
    }
  }

  return NS_ERROR_FAILURE;
}

bool
ISOControl::HasAudioTrack()
{
  nsRefPtr<AACTrackMetadata> audMeta;
  GetAudioMetadata(audMeta);
  return audMeta;
}

bool
ISOControl::HasVideoTrack()
{
  nsRefPtr<AVCTrackMetadata> vidMeta;
  GetVideoMetadata(vidMeta);
  return vidMeta;
}

nsresult
ISOControl::SetFragment(FragmentBuffer* aFragment)
{
  if (aFragment->GetType() == Audio_Track) {
    mAudioFragmentBuffer = aFragment;
  } else {
    mVideoFragmentBuffer = aFragment;
  }
  return NS_OK;
}

FragmentBuffer*
ISOControl::GetFragment(uint32_t aType)
{
  if (aType == Audio_Track) {
    return mAudioFragmentBuffer;
  } else if (aType == Video_Track){
    return mVideoFragmentBuffer;
  }
  MOZ_ASSERT(0);
  return nullptr;
}

nsresult
ISOControl::GetBufs(nsTArray<nsTArray<uint8_t>>* aOutputBufs)
{
  uint32_t len = mOutBuffers.Length();
  for (uint32_t i = 0; i < len; i++) {
    mOutBuffers[i].SwapElements(*aOutputBufs->AppendElement());
    mOutputSize += mOutBuffers[i].Length();
  }
  return FlushBuf();
}

nsresult
ISOControl::FlushBuf()
{
  mOutBuffers.SetLength(1);
  mLastWrittenBoxPos = 0;
  return NS_OK;
}

uint32_t
ISOControl::WriteAVData(nsTArray<uint8_t>& aArray)
{
  MOZ_ASSERT(!mBitCount);

  uint32_t len = aArray.Length();
  if (!len) {
    return 0;
  }

  // The last element already has data, allocated a new element for pointer
  // swapping.
  if (mOutBuffers.LastElement().Length()) {
    mOutBuffers.AppendElement();
  }
  // Swap the video/audio data pointer.
  mOutBuffers.LastElement().SwapElements(aArray);
  // Following data could be boxes, so appending a new uint8_t array here.
  mOutBuffers.AppendElement();

  return len;
}

uint32_t
ISOControl::WriteBits(uint64_t aBits, size_t aNumBits)
{
  uint8_t output_byte = 0;

  MOZ_ASSERT(aNumBits <= 64);
  // TODO: rewritten following with bitset?
  for (size_t i = aNumBits; i > 0; i--) {
    mBit |= (((aBits >> (i - 1)) & 1) << (8 - ++mBitCount));
    if (mBitCount == 8) {
      Write(&mBit, sizeof(uint8_t));
      mBit = 0;
      mBitCount = 0;
      output_byte++;
    }
  }
  return output_byte;
}

uint32_t
ISOControl::Write(uint8_t* aBuf, uint32_t aSize)
{
  mOutBuffers.LastElement().AppendElements(aBuf, aSize);
  return aSize;
}

uint32_t
ISOControl::Write(uint8_t aData)
{
  MOZ_ASSERT(!mBitCount);
  Write((uint8_t*)&aData, sizeof(uint8_t));
  return sizeof(uint8_t);
}

uint32_t
ISOControl::GetBufPos()
{
  uint32_t len = mOutBuffers.Length();
  uint32_t pos = 0;
  for (uint32_t i = 0; i < len; i++) {
    pos += mOutBuffers.ElementAt(i).Length();
  }
  return pos;
}

uint32_t
ISOControl::WriteFourCC(const char* aType)
{
  // Bit operation should be aligned to byte before writing any byte data.
  MOZ_ASSERT(!mBitCount);

  uint32_t size = strlen(aType);
  if (size == 4) {
    return Write((uint8_t*)aType, size);
  }

  return 0;
}

nsresult
ISOControl::GenerateFtyp()
{
  nsresult rv;
  uint32_t size;
  nsAutoPtr<FileTypeBox> type_box(new FileTypeBox(this));
  rv = type_box->Generate(&size);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = type_box->Write();
  NS_ENSURE_SUCCESS(rv, rv);
  return NS_OK;
}

nsresult
ISOControl::GenerateMoov()
{
  nsresult rv;
  uint32_t size;
  nsAutoPtr<MovieBox> moov_box(new MovieBox(this));
  rv = moov_box->Generate(&size);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = moov_box->Write();
  NS_ENSURE_SUCCESS(rv, rv);
  return NS_OK;
}

nsresult
ISOControl::GenerateMoof(uint32_t aTrackType)
{
  mFragNum++;

  nsresult rv;
  uint32_t size;
  uint64_t first_sample_offset = mOutputSize + mLastWrittenBoxPos;
  nsAutoPtr<MovieFragmentBox> moof_box(new MovieFragmentBox(aTrackType, this));
  nsAutoPtr<MediaDataBox> mdat_box(new MediaDataBox(aTrackType, this));

  rv = moof_box->Generate(&size);
  NS_ENSURE_SUCCESS(rv, rv);
  first_sample_offset += size;
  rv = mdat_box->Generate(&size);
  NS_ENSURE_SUCCESS(rv, rv);
  first_sample_offset += mdat_box->FirstSampleOffsetInMediaDataBox();

  // correct offset info
  nsTArray<nsRefPtr<MuxerOperation>> tfhds;
  rv = moof_box->Find(NS_LITERAL_CSTRING("tfhd"), tfhds);
  NS_ENSURE_SUCCESS(rv, rv);
  uint32_t len = tfhds.Length();
  for (uint32_t i = 0; i < len; i++) {
    TrackFragmentHeaderBox* tfhd = (TrackFragmentHeaderBox*) tfhds.ElementAt(i).get();
    rv = tfhd->UpdateBaseDataOffset(first_sample_offset);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  rv = moof_box->Write();
  NS_ENSURE_SUCCESS(rv, rv);
  rv = mdat_box->Write();
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

uint32_t
ISOControl::GetTime()
{
  return (uint64_t)time(nullptr) + iso_time_offset;
}

}
