/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebMWriter.h"
#include "EbmlComposer.h"
#include "mozilla/ProfilerLabels.h"
#include "OpusTrackEncoder.h"

namespace mozilla {

WebMWriter::WebMWriter() : mEbmlComposer(new EbmlComposer()) {}

WebMWriter::~WebMWriter() {
  // Out-of-line dtor so mEbmlComposer UniquePtr can delete a complete type.
}

nsresult WebMWriter::WriteEncodedTrack(
    const nsTArray<RefPtr<EncodedFrame>>& aData, uint32_t aFlags) {
  AUTO_PROFILER_LABEL("WebMWriter::WriteEncodedTrack", OTHER);
  for (uint32_t i = 0; i < aData.Length(); i++) {
    nsresult rv = mEbmlComposer->WriteSimpleBlock(aData.ElementAt(i).get());
    NS_ENSURE_SUCCESS(rv, rv);
  }
  return NS_OK;
}

nsresult WebMWriter::GetContainerData(nsTArray<nsTArray<uint8_t>>* aOutputBufs,
                                      uint32_t aFlags) {
  AUTO_PROFILER_LABEL("WebMWriter::GetContainerData", OTHER);
  mEbmlComposer->ExtractBuffer(aOutputBufs, aFlags);
  if (aFlags & ContainerWriter::FLUSH_NEEDED) {
    mIsWritingComplete = true;
  }
  return NS_OK;
}

nsresult WebMWriter::SetMetadata(
    const nsTArray<RefPtr<TrackMetadataBase>>& aMetadata) {
  AUTO_PROFILER_LABEL("WebMWriter::SetMetadata", OTHER);
  MOZ_DIAGNOSTIC_ASSERT(!aMetadata.IsEmpty());

  // Integrity checks
  bool bad = false;
  for (const RefPtr<TrackMetadataBase>& metadata : aMetadata) {
    MOZ_ASSERT(metadata);

    if (metadata->GetKind() == TrackMetadataBase::METADATA_VP8) {
      VP8Metadata* meta = static_cast<VP8Metadata*>(metadata.get());
      if (meta->mWidth == 0 || meta->mHeight == 0 || meta->mDisplayWidth == 0 ||
          meta->mDisplayHeight == 0) {
        bad = true;
      }
    }

    if (metadata->GetKind() == TrackMetadataBase::METADATA_VORBIS) {
      VorbisMetadata* meta = static_cast<VorbisMetadata*>(metadata.get());
      if (meta->mSamplingFrequency == 0 || meta->mChannels == 0 ||
          meta->mData.IsEmpty()) {
        bad = true;
      }
    }

    if (metadata->GetKind() == TrackMetadataBase::METADATA_OPUS) {
      OpusMetadata* meta = static_cast<OpusMetadata*>(metadata.get());
      if (meta->mSamplingFrequency == 0 || meta->mChannels == 0 ||
          meta->mIdHeader.IsEmpty()) {
        bad = true;
      }
    }
  }
  if (bad) {
    return NS_ERROR_FAILURE;
  }

  // Storing
  DebugOnly<bool> hasAudio = false;
  DebugOnly<bool> hasVideo = false;
  for (const RefPtr<TrackMetadataBase>& metadata : aMetadata) {
    MOZ_ASSERT(metadata);

    if (metadata->GetKind() == TrackMetadataBase::METADATA_VP8) {
      MOZ_ASSERT(!hasVideo);
      VP8Metadata* meta = static_cast<VP8Metadata*>(metadata.get());
      mEbmlComposer->SetVideoConfig(meta->mWidth, meta->mHeight,
                                    meta->mDisplayWidth, meta->mDisplayHeight);
      hasVideo = true;
    }

    if (metadata->GetKind() == TrackMetadataBase::METADATA_VORBIS) {
      MOZ_ASSERT(!hasAudio);
      VorbisMetadata* meta = static_cast<VorbisMetadata*>(metadata.get());
      mEbmlComposer->SetAudioConfig(meta->mSamplingFrequency, meta->mChannels);
      mEbmlComposer->SetAudioCodecPrivateData(meta->mData);
      hasAudio = true;
    }

    if (metadata->GetKind() == TrackMetadataBase::METADATA_OPUS) {
      MOZ_ASSERT(!hasAudio);
      OpusMetadata* meta = static_cast<OpusMetadata*>(metadata.get());
      mEbmlComposer->SetAudioConfig(meta->mSamplingFrequency, meta->mChannels);
      mEbmlComposer->SetAudioCodecPrivateData(meta->mIdHeader);
      hasAudio = true;
    }
  }
  mEbmlComposer->GenerateHeader();
  return NS_OK;
}

}  // namespace mozilla
