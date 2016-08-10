/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DecoderFactory.h"

#include "nsMimeTypes.h"
#include "mozilla/RefPtr.h"

#include "Decoder.h"
#include "IDecodingTask.h"
#include "nsPNGDecoder.h"
#include "nsGIFDecoder2.h"
#include "nsJPEGDecoder.h"
#include "nsBMPDecoder.h"
#include "nsICODecoder.h"
#include "nsIconDecoder.h"

namespace mozilla {

using namespace gfx;

namespace image {

/* static */ DecoderType
DecoderFactory::GetDecoderType(const char* aMimeType)
{
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

  // ICO
  } else if (!strcmp(aMimeType, IMAGE_ICO)) {
    type = DecoderType::ICO;
  } else if (!strcmp(aMimeType, IMAGE_ICO_MS)) {
    type = DecoderType::ICO;

  // Icon
  } else if (!strcmp(aMimeType, IMAGE_ICON_MS)) {
    type = DecoderType::ICON;
  }

  return type;
}

/* static */ already_AddRefed<Decoder>
DecoderFactory::GetDecoder(DecoderType aType,
                           RasterImage* aImage,
                           bool aIsRedecode)
{
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
      decoder = new nsJPEGDecoder(aImage,
                                  aIsRedecode ? Decoder::SEQUENTIAL
                                              : Decoder::PROGRESSIVE);
      break;
    case DecoderType::BMP:
      decoder = new nsBMPDecoder(aImage);
      break;
    case DecoderType::ICO:
      decoder = new nsICODecoder(aImage);
      break;
    case DecoderType::ICON:
      decoder = new nsIconDecoder(aImage);
      break;
    default:
      MOZ_ASSERT_UNREACHABLE("Unknown decoder type");
  }

  return decoder.forget();
}

/* static */ already_AddRefed<IDecodingTask>
DecoderFactory::CreateDecoder(DecoderType aType,
                              NotNull<RasterImage*> aImage,
                              NotNull<SourceBuffer*> aSourceBuffer,
                              const IntSize& aIntrinsicSize,
                              const IntSize& aOutputSize,
                              DecoderFlags aDecoderFlags,
                              SurfaceFlags aSurfaceFlags,
                              int aSampleSize)
{
  if (aType == DecoderType::UNKNOWN) {
    return nullptr;
  }

  // Create an anonymous decoder. Interaction with the SurfaceCache and the
  // owning RasterImage will be mediated by DecodedSurfaceProvider.
  RefPtr<Decoder> decoder =
    GetDecoder(aType, nullptr, bool(aDecoderFlags & DecoderFlags::IS_REDECODE));
  MOZ_ASSERT(decoder, "Should have a decoder now");

  // Initialize the decoder.
  decoder->SetMetadataDecode(false);
  decoder->SetIterator(aSourceBuffer->Iterator());
  decoder->SetOutputSize(aOutputSize);
  decoder->SetDecoderFlags(aDecoderFlags | DecoderFlags::FIRST_FRAME_ONLY);
  decoder->SetSurfaceFlags(aSurfaceFlags);
  decoder->SetSampleSize(aSampleSize);

  if (NS_FAILED(decoder->Init())) {
    return nullptr;
  }

  // Create a DecodedSurfaceProvider which will manage the decoding process and
  // make this decoder's output available in the surface cache.
  SurfaceKey surfaceKey =
    RasterSurfaceKey(aOutputSize, aSurfaceFlags, /* aFrameNum = */ 0);
  NotNull<RefPtr<DecodedSurfaceProvider>> provider =
    WrapNotNull(new DecodedSurfaceProvider(aImage,
                                           WrapNotNull(decoder),
                                           surfaceKey));

  // Attempt to insert the surface provider into the surface cache right away so
  // we won't trigger any more decoders with the same parameters.
  InsertOutcome outcome =
    SurfaceCache::Insert(provider, ImageKey(aImage.get()), surfaceKey);
  if (outcome != InsertOutcome::SUCCESS) {
    return nullptr;
  }

  // Return the surface provider in its IDecodingTask guise.
  RefPtr<IDecodingTask> task = provider.get();
  return task.forget();
}

/* static */ already_AddRefed<IDecodingTask>
DecoderFactory::CreateAnimationDecoder(DecoderType aType,
                                       NotNull<RasterImage*> aImage,
                                       NotNull<SourceBuffer*> aSourceBuffer,
                                       const IntSize& aIntrinsicSize,
                                       DecoderFlags aDecoderFlags,
                                       SurfaceFlags aSurfaceFlags)
{
  if (aType == DecoderType::UNKNOWN) {
    return nullptr;
  }

  MOZ_ASSERT(aType == DecoderType::GIF || aType == DecoderType::PNG,
             "Calling CreateAnimationDecoder for non-animating DecoderType");

  RefPtr<Decoder> decoder =
    GetDecoder(aType, aImage, /* aIsRedecode = */ true);
  MOZ_ASSERT(decoder, "Should have a decoder now");

  // Initialize the decoder.
  decoder->SetMetadataDecode(false);
  decoder->SetIterator(aSourceBuffer->Iterator());
  decoder->SetDecoderFlags(aDecoderFlags | DecoderFlags::IS_REDECODE);
  decoder->SetSurfaceFlags(aSurfaceFlags);

  if (NS_FAILED(decoder->Init())) {
    return nullptr;
  }

  // Add a placeholder for the first frame to the SurfaceCache so we won't
  // trigger any more decoders with the same parameters.
  SurfaceKey surfaceKey =
    RasterSurfaceKey(aIntrinsicSize, aSurfaceFlags, /* aFrameNum = */ 0);
  InsertOutcome outcome =
    SurfaceCache::InsertPlaceholder(ImageKey(aImage.get()), surfaceKey);
  if (outcome != InsertOutcome::SUCCESS) {
    return nullptr;
  }

  RefPtr<IDecodingTask> task = new AnimationDecodingTask(WrapNotNull(decoder));
  return task.forget();
}

/* static */ already_AddRefed<IDecodingTask>
DecoderFactory::CreateMetadataDecoder(DecoderType aType,
                                      NotNull<RasterImage*> aImage,
                                      NotNull<SourceBuffer*> aSourceBuffer,
                                      int aSampleSize)
{
  if (aType == DecoderType::UNKNOWN) {
    return nullptr;
  }

  RefPtr<Decoder> decoder =
    GetDecoder(aType, aImage, /* aIsRedecode = */ false);
  MOZ_ASSERT(decoder, "Should have a decoder now");

  // Initialize the decoder.
  decoder->SetMetadataDecode(true);
  decoder->SetIterator(aSourceBuffer->Iterator());
  decoder->SetSampleSize(aSampleSize);

  if (NS_FAILED(decoder->Init())) {
    return nullptr;
  }

  RefPtr<IDecodingTask> task = new MetadataDecodingTask(WrapNotNull(decoder));
  return task.forget();
}

/* static */ already_AddRefed<Decoder>
DecoderFactory::CreateDecoderForICOResource(DecoderType aType,
                                            NotNull<SourceBuffer*> aSourceBuffer,
                                            NotNull<nsICODecoder*> aICODecoder,
                                            const Maybe<uint32_t>& aDataOffset
                                              /* = Nothing() */)
{
  // Create the decoder.
  RefPtr<Decoder> decoder;
  switch (aType) {
    case DecoderType::BMP:
      MOZ_ASSERT(aDataOffset);
      decoder = new nsBMPDecoder(aICODecoder->GetImageMaybeNull(), *aDataOffset);
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
  MOZ_ASSERT(!aICODecoder->IsMetadataDecode());
  decoder->SetMetadataDecode(aICODecoder->IsMetadataDecode());
  decoder->SetIterator(aSourceBuffer->Iterator());
  decoder->SetOutputSize(aICODecoder->OutputSize());
  decoder->SetDecoderFlags(aICODecoder->GetDecoderFlags());
  decoder->SetSurfaceFlags(aICODecoder->GetSurfaceFlags());

  if (NS_FAILED(decoder->Init())) {
    return nullptr;
  }

  return decoder.forget();
}

/* static */ already_AddRefed<Decoder>
DecoderFactory::CreateAnonymousDecoder(DecoderType aType,
                                       NotNull<SourceBuffer*> aSourceBuffer,
                                       const Maybe<IntSize>& aOutputSize,
                                       SurfaceFlags aSurfaceFlags)
{
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

  // Without an image, the decoder can't store anything in the SurfaceCache, so
  // callers will only be able to retrieve the most recent frame via
  // Decoder::GetCurrentFrame(). That means that anonymous decoders should
  // always be first-frame-only decoders, because nobody ever wants the *last*
  // frame.
  decoderFlags |= DecoderFlags::FIRST_FRAME_ONLY;

  decoder->SetDecoderFlags(decoderFlags);
  decoder->SetSurfaceFlags(aSurfaceFlags);

  // Set an output size for downscale-during-decode if requested.
  if (aOutputSize) {
    decoder->SetOutputSize(*aOutputSize);
  }

  if (NS_FAILED(decoder->Init())) {
    return nullptr;
  }

  return decoder.forget();
}

/* static */ already_AddRefed<Decoder>
DecoderFactory::CreateAnonymousMetadataDecoder(DecoderType aType,
                                               NotNull<SourceBuffer*> aSourceBuffer)
{
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

} // namespace image
} // namespace mozilla
