/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DecoderFactory.h"

#include "nsMimeTypes.h"
#include "mozilla/RefPtr.h"

#include "AnimationSurfaceProvider.h"
#include "Decoder.h"
#include "DecodedSurfaceProvider.h"
#include "IDecodingTask.h"
#include "ImageOps.h"
#include "nsPNGDecoder.h"
#include "nsGIFDecoder2.h"
#include "nsJPEGDecoder.h"
#include "nsBMPDecoder.h"
#include "nsICODecoder.h"
#include "nsIconDecoder.h"
#include "nsWebPDecoder.h"
#ifdef MOZ_AV1
#  include "nsAVIFDecoder.h"
#endif
#ifdef MOZ_JXL
#  include "nsJXLDecoder.h"
#endif

namespace mozilla {

using namespace gfx;

namespace image {

/* static */
DecoderType DecoderFactory::GetDecoderType(const char* aMimeType) {
  // By default we don't know.
  DecoderType type = DecoderType::UNKNOWN;

  // PNG
  if (!strcmp(aMimeType, IMAGE_PNG)) {
    type = DecoderType::PNG;
  } else if (!strcmp(aMimeType, IMAGE_X_PNG)) {
    type = DecoderType::PNG;
  } else if (!strcmp(aMimeType, IMAGE_APNG)) {
    type = DecoderType::PNG;

    // GIF
  } else if (!strcmp(aMimeType, IMAGE_GIF)) {
    type = DecoderType::GIF;

    // JPEG
  } else if (!strcmp(aMimeType, IMAGE_JPEG)) {
    type = DecoderType::JPEG;
  } else if (!strcmp(aMimeType, IMAGE_PJPEG)) {
    type = DecoderType::JPEG;
  } else if (!strcmp(aMimeType, IMAGE_JPG)) {
    type = DecoderType::JPEG;

    // BMP
  } else if (!strcmp(aMimeType, IMAGE_BMP)) {
    type = DecoderType::BMP;
  } else if (!strcmp(aMimeType, IMAGE_BMP_MS)) {
    type = DecoderType::BMP;

    // BMP_CLIPBOARD
  } else if (!strcmp(aMimeType, IMAGE_BMP_MS_CLIPBOARD)) {
    type = DecoderType::BMP_CLIPBOARD;

    // ICO
  } else if (!strcmp(aMimeType, IMAGE_ICO)) {
    type = DecoderType::ICO;
  } else if (!strcmp(aMimeType, IMAGE_ICO_MS)) {
    type = DecoderType::ICO;

    // Icon
  } else if (!strcmp(aMimeType, IMAGE_ICON_MS)) {
    type = DecoderType::ICON;

    // WebP
  } else if (!strcmp(aMimeType, IMAGE_WEBP)) {
    type = DecoderType::WEBP;

    // AVIF
  }
#ifdef MOZ_AV1
  else if (!strcmp(aMimeType, IMAGE_AVIF) &&
           StaticPrefs::image_avif_enabled()) {
    type = DecoderType::AVIF;
  }
#endif
#ifdef MOZ_JXL
  else if (!strcmp(aMimeType, IMAGE_JXL) && StaticPrefs::image_jxl_enabled()) {
    type = DecoderType::JXL;
  }
#endif

  return type;
}

/* static */
DecoderFlags DecoderFactory::GetDefaultDecoderFlagsForType(DecoderType aType) {
  auto flags = DefaultDecoderFlags();

#ifdef MOZ_AV1
  if (aType == DecoderType::AVIF) {
    if (StaticPrefs::image_avif_sequence_enabled()) {
      flags |= DecoderFlags::AVIF_SEQUENCES_ENABLED;
    }
    if (StaticPrefs::image_avif_sequence_animate_avif_major_branded_images()) {
      flags |= DecoderFlags::AVIF_ANIMATE_AVIF_MAJOR;
    }
  }
#endif

  return flags;
}

/* static */
already_AddRefed<Decoder> DecoderFactory::GetDecoder(DecoderType aType,
                                                     RasterImage* aImage,
                                                     bool aIsRedecode) {
  RefPtr<Decoder> decoder;

  switch (aType) {
    case DecoderType::PNG:
      decoder = new nsPNGDecoder(aImage);
      break;
    case DecoderType::GIF:
      decoder = new nsGIFDecoder2(aImage);
      break;
    case DecoderType::JPEG:
      // If we have all the data we don't want to waste cpu time doing
      // a progressive decode.
      decoder = new nsJPEGDecoder(
          aImage, aIsRedecode ? Decoder::SEQUENTIAL : Decoder::PROGRESSIVE);
      break;
    case DecoderType::BMP:
      decoder = new nsBMPDecoder(aImage);
      break;
    case DecoderType::BMP_CLIPBOARD:
      decoder = new nsBMPDecoder(aImage, /* aForClipboard */ true);
      break;
    case DecoderType::ICO:
      decoder = new nsICODecoder(aImage);
      break;
    case DecoderType::ICON:
      decoder = new nsIconDecoder(aImage);
      break;
    case DecoderType::WEBP:
      decoder = new nsWebPDecoder(aImage);
      break;
#ifdef MOZ_AV1
    case DecoderType::AVIF:
      decoder = new nsAVIFDecoder(aImage);
      break;
#endif
#ifdef MOZ_JXL
    case DecoderType::JXL:
      decoder = new nsJXLDecoder(aImage);
      break;
#endif
    default:
      MOZ_ASSERT_UNREACHABLE("Unknown decoder type");
  }

  return decoder.forget();
}

/* static */
nsresult DecoderFactory::CreateDecoder(
    DecoderType aType, NotNull<RasterImage*> aImage,
    NotNull<SourceBuffer*> aSourceBuffer, const IntSize& aIntrinsicSize,
    const IntSize& aOutputSize, DecoderFlags aDecoderFlags,
    SurfaceFlags aSurfaceFlags, IDecodingTask** aOutTask) {
  if (aType == DecoderType::UNKNOWN) {
    return NS_ERROR_INVALID_ARG;
  }

  // Create an anonymous decoder. Interaction with the SurfaceCache and the
  // owning RasterImage will be mediated by DecodedSurfaceProvider.
  RefPtr<Decoder> decoder = GetDecoder(
      aType, nullptr, bool(aDecoderFlags & DecoderFlags::IS_REDECODE));
  MOZ_ASSERT(decoder, "Should have a decoder now");

  // Initialize the decoder.
  decoder->SetMetadataDecode(false);
  decoder->SetIterator(aSourceBuffer->Iterator());
  decoder->SetOutputSize(OrientedIntSize::FromUnknownSize(aOutputSize));
  decoder->SetDecoderFlags(aDecoderFlags | DecoderFlags::FIRST_FRAME_ONLY);
  decoder->SetSurfaceFlags(aSurfaceFlags);

  nsresult rv = decoder->Init();
  if (NS_FAILED(rv)) {
    return NS_ERROR_FAILURE;
  }

  // Create a DecodedSurfaceProvider which will manage the decoding process and
  // make this decoder's output available in the surface cache.
  SurfaceKey surfaceKey =
      RasterSurfaceKey(aOutputSize, aSurfaceFlags, PlaybackType::eStatic);
  auto provider = MakeNotNull<RefPtr<DecodedSurfaceProvider>>(
      aImage, surfaceKey, WrapNotNull(decoder));
  if (aDecoderFlags & DecoderFlags::CANNOT_SUBSTITUTE) {
    provider->Availability().SetCannotSubstitute();
  }

  // Attempt to insert the surface provider into the surface cache right away so
  // we won't trigger any more decoders with the same parameters.
  switch (SurfaceCache::Insert(provider)) {
    case InsertOutcome::SUCCESS:
      break;
    case InsertOutcome::FAILURE_ALREADY_PRESENT:
      return NS_ERROR_ALREADY_INITIALIZED;
    default:
      return NS_ERROR_FAILURE;
  }

  // Return the surface provider in its IDecodingTask guise.
  RefPtr<IDecodingTask> task = provider.get();
  task.forget(aOutTask);
  return NS_OK;
}

/* static */
nsresult DecoderFactory::CreateAnimationDecoder(
    DecoderType aType, NotNull<RasterImage*> aImage,
    NotNull<SourceBuffer*> aSourceBuffer, const IntSize& aIntrinsicSize,
    DecoderFlags aDecoderFlags, SurfaceFlags aSurfaceFlags,
    size_t aCurrentFrame, IDecodingTask** aOutTask) {
  if (aType == DecoderType::UNKNOWN) {
    return NS_ERROR_INVALID_ARG;
  }

  MOZ_ASSERT(aType == DecoderType::GIF || aType == DecoderType::PNG ||
                 aType == DecoderType::WEBP || aType == DecoderType::AVIF,
             "Calling CreateAnimationDecoder for non-animating DecoderType");

  // Create an anonymous decoder. Interaction with the SurfaceCache and the
  // owning RasterImage will be mediated by AnimationSurfaceProvider.
  RefPtr<Decoder> decoder =
      GetDecoder(aType, nullptr, /* aIsRedecode = */ true);
  MOZ_ASSERT(decoder, "Should have a decoder now");

  // Initialize the decoder.
  decoder->SetMetadataDecode(false);
  decoder->SetIterator(aSourceBuffer->Iterator());
  decoder->SetDecoderFlags(aDecoderFlags | DecoderFlags::IS_REDECODE);
  decoder->SetSurfaceFlags(aSurfaceFlags);

  nsresult rv = decoder->Init();
  if (NS_FAILED(rv)) {
    return NS_ERROR_FAILURE;
  }

  // Create an AnimationSurfaceProvider which will manage the decoding process
  // and make this decoder's output available in the surface cache.
  SurfaceKey surfaceKey =
      RasterSurfaceKey(aIntrinsicSize, aSurfaceFlags, PlaybackType::eAnimated);
  auto provider = MakeNotNull<RefPtr<AnimationSurfaceProvider>>(
      aImage, surfaceKey, WrapNotNull(decoder), aCurrentFrame);

  // Attempt to insert the surface provider into the surface cache right away so
  // we won't trigger any more decoders with the same parameters.
  switch (SurfaceCache::Insert(provider)) {
    case InsertOutcome::SUCCESS:
      break;
    case InsertOutcome::FAILURE_ALREADY_PRESENT:
      return NS_ERROR_ALREADY_INITIALIZED;
    default:
      return NS_ERROR_FAILURE;
  }

  // Return the surface provider in its IDecodingTask guise.
  RefPtr<IDecodingTask> task = provider.get();
  task.forget(aOutTask);
  return NS_OK;
}

/* static */
already_AddRefed<Decoder> DecoderFactory::CloneAnimationDecoder(
    Decoder* aDecoder) {
  MOZ_ASSERT(aDecoder);

  // In an ideal world, we would assert aDecoder->HasAnimation() but we cannot.
  // The decoder may not have detected it is animated yet (e.g. it did not even
  // get scheduled yet, or it has only decoded the first frame and has yet to
  // rediscover it is animated).
  DecoderType type = aDecoder->GetType();
  MOZ_ASSERT(type == DecoderType::GIF || type == DecoderType::PNG ||
                 type == DecoderType::WEBP || type == DecoderType::AVIF,
             "Calling CloneAnimationDecoder for non-animating DecoderType");

  RefPtr<Decoder> decoder = GetDecoder(type, nullptr, /* aIsRedecode = */ true);
  MOZ_ASSERT(decoder, "Should have a decoder now");

  // Initialize the decoder.
  decoder->SetMetadataDecode(false);
  decoder->SetIterator(aDecoder->GetSourceBuffer()->Iterator());
  decoder->SetDecoderFlags(aDecoder->GetDecoderFlags());
  decoder->SetSurfaceFlags(aDecoder->GetSurfaceFlags());
  decoder->SetFrameRecycler(aDecoder->GetFrameRecycler());

  if (NS_FAILED(decoder->Init())) {
    return nullptr;
  }

  return decoder.forget();
}

/* static */
already_AddRefed<IDecodingTask> DecoderFactory::CreateMetadataDecoder(
    DecoderType aType, NotNull<RasterImage*> aImage, DecoderFlags aFlags,
    NotNull<SourceBuffer*> aSourceBuffer) {
  if (aType == DecoderType::UNKNOWN) {
    return nullptr;
  }

  RefPtr<Decoder> decoder =
      GetDecoder(aType, aImage, /* aIsRedecode = */ false);
  MOZ_ASSERT(decoder, "Should have a decoder now");

  // Initialize the decoder.
  decoder->SetMetadataDecode(true);
  decoder->SetDecoderFlags(aFlags);
  decoder->SetIterator(aSourceBuffer->Iterator());

  if (NS_FAILED(decoder->Init())) {
    return nullptr;
  }

  RefPtr<IDecodingTask> task = new MetadataDecodingTask(WrapNotNull(decoder));
  return task.forget();
}

/* static */
already_AddRefed<Decoder> DecoderFactory::CreateDecoderForICOResource(
    DecoderType aType, SourceBufferIterator&& aIterator,
    NotNull<nsICODecoder*> aICODecoder, bool aIsMetadataDecode,
    const Maybe<OrientedIntSize>& aExpectedSize,
    const Maybe<uint32_t>& aDataOffset
    /* = Nothing() */) {
  // Create the decoder.
  RefPtr<Decoder> decoder;
  switch (aType) {
    case DecoderType::BMP:
      MOZ_ASSERT(aDataOffset);
      decoder =
          new nsBMPDecoder(aICODecoder->GetImageMaybeNull(), *aDataOffset);
      break;

    case DecoderType::PNG:
      MOZ_ASSERT(!aDataOffset);
      decoder = new nsPNGDecoder(aICODecoder->GetImageMaybeNull());
      break;

    default:
      MOZ_ASSERT_UNREACHABLE("Invalid ICO resource decoder type");
      return nullptr;
  }

  MOZ_ASSERT(decoder);

  // Initialize the decoder, copying settings from @aICODecoder.
  decoder->SetMetadataDecode(aIsMetadataDecode);
  decoder->SetIterator(std::forward<SourceBufferIterator>(aIterator));
  if (!aIsMetadataDecode) {
    decoder->SetOutputSize(aICODecoder->OutputSize());
  }
  if (aExpectedSize) {
    decoder->SetExpectedSize(*aExpectedSize);
  }
  decoder->SetDecoderFlags(aICODecoder->GetDecoderFlags());
  decoder->SetSurfaceFlags(aICODecoder->GetSurfaceFlags());
  decoder->SetFinalizeFrames(false);

  if (NS_FAILED(decoder->Init())) {
    return nullptr;
  }

  return decoder.forget();
}

/* static */
already_AddRefed<Decoder> DecoderFactory::CreateAnonymousDecoder(
    DecoderType aType, NotNull<SourceBuffer*> aSourceBuffer,
    const Maybe<IntSize>& aOutputSize, DecoderFlags aDecoderFlags,
    SurfaceFlags aSurfaceFlags) {
  if (aType == DecoderType::UNKNOWN) {
    return nullptr;
  }

  RefPtr<Decoder> decoder =
      GetDecoder(aType, /* aImage = */ nullptr, /* aIsRedecode = */ false);
  MOZ_ASSERT(decoder, "Should have a decoder now");

  // Initialize the decoder.
  decoder->SetMetadataDecode(false);
  decoder->SetIterator(aSourceBuffer->Iterator());

  // Anonymous decoders are always transient; we don't want to optimize surfaces
  // or do any other expensive work that might be wasted.
  DecoderFlags decoderFlags = DecoderFlags::IMAGE_IS_TRANSIENT;

  decoder->SetDecoderFlags(aDecoderFlags | decoderFlags);
  decoder->SetSurfaceFlags(aSurfaceFlags);

  // Set an output size for downscale-during-decode if requested.
  if (aOutputSize) {
    decoder->SetOutputSize(OrientedIntSize::FromUnknownSize(*aOutputSize));
  }

  if (NS_FAILED(decoder->Init())) {
    return nullptr;
  }

  return decoder.forget();
}

/* static */
already_AddRefed<Decoder> DecoderFactory::CreateAnonymousMetadataDecoder(
    DecoderType aType, NotNull<SourceBuffer*> aSourceBuffer) {
  if (aType == DecoderType::UNKNOWN) {
    return nullptr;
  }

  RefPtr<Decoder> decoder =
      GetDecoder(aType, /* aImage = */ nullptr, /* aIsRedecode = */ false);
  MOZ_ASSERT(decoder, "Should have a decoder now");

  // Initialize the decoder.
  decoder->SetMetadataDecode(true);
  decoder->SetIterator(aSourceBuffer->Iterator());
  decoder->SetDecoderFlags(DecoderFlags::FIRST_FRAME_ONLY);

  if (NS_FAILED(decoder->Init())) {
    return nullptr;
  }

  return decoder.forget();
}

}  // namespace image
}  // namespace mozilla
