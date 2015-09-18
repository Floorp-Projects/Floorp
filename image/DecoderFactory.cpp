/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DecoderFactory.h"

#include "nsMimeTypes.h"
#include "mozilla/nsRefPtr.h"
#include "nsString.h"

#include "Decoder.h"
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
  nsRefPtr<Decoder> decoder;

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

/* static */ already_AddRefed<Decoder>
DecoderFactory::CreateDecoder(DecoderType aType,
                              RasterImage* aImage,
                              SourceBuffer* aSourceBuffer,
                              const Maybe<IntSize>& aTargetSize,
                              DecoderFlags aDecoderFlags,
                              SurfaceFlags aSurfaceFlags,
                              int aSampleSize,
                              const IntSize& aResolution)
{
  if (aType == DecoderType::UNKNOWN) {
    return nullptr;
  }

  nsRefPtr<Decoder> decoder =
    GetDecoder(aType, aImage, bool(aDecoderFlags & DecoderFlags::IS_REDECODE));
  MOZ_ASSERT(decoder, "Should have a decoder now");

  // Initialize the decoder.
  decoder->SetMetadataDecode(false);
  decoder->SetIterator(aSourceBuffer->Iterator());
  decoder->SetDecoderFlags(aDecoderFlags | DecoderFlags::FIRST_FRAME_ONLY);
  decoder->SetSurfaceFlags(aSurfaceFlags);
  decoder->SetSampleSize(aSampleSize);
  decoder->SetResolution(aResolution);

  // Set a target size for downscale-during-decode if applicable.
  if (aTargetSize) {
    DebugOnly<nsresult> rv = decoder->SetTargetSize(*aTargetSize);
    MOZ_ASSERT(NS_SUCCEEDED(rv), "Bad downscale-during-decode target size?");
  }

  decoder->Init();
  if (NS_FAILED(decoder->GetDecoderError())) {
    return nullptr;
  }

  return decoder.forget();
}

/* static */ already_AddRefed<Decoder>
DecoderFactory::CreateAnimationDecoder(DecoderType aType,
                                       RasterImage* aImage,
                                       SourceBuffer* aSourceBuffer,
                                       DecoderFlags aDecoderFlags,
                                       SurfaceFlags aSurfaceFlags,
                                       const IntSize& aResolution)
{
  if (aType == DecoderType::UNKNOWN) {
    return nullptr;
  }

  MOZ_ASSERT(aType == DecoderType::GIF || aType == DecoderType::PNG,
             "Calling CreateAnimationDecoder for non-animating DecoderType");

  nsRefPtr<Decoder> decoder =
    GetDecoder(aType, aImage, /* aIsRedecode = */ true);
  MOZ_ASSERT(decoder, "Should have a decoder now");

  // Initialize the decoder.
  decoder->SetMetadataDecode(false);
  decoder->SetIterator(aSourceBuffer->Iterator());
  decoder->SetDecoderFlags(aDecoderFlags | DecoderFlags::IS_REDECODE);
  decoder->SetSurfaceFlags(aSurfaceFlags);
  decoder->SetResolution(aResolution);

  decoder->Init();
  if (NS_FAILED(decoder->GetDecoderError())) {
    return nullptr;
  }

  return decoder.forget();
}

/* static */ already_AddRefed<Decoder>
DecoderFactory::CreateMetadataDecoder(DecoderType aType,
                                      RasterImage* aImage,
                                      SourceBuffer* aSourceBuffer,
                                      int aSampleSize,
                                      const IntSize& aResolution)
{
  if (aType == DecoderType::UNKNOWN) {
    return nullptr;
  }

  nsRefPtr<Decoder> decoder =
    GetDecoder(aType, aImage, /* aIsRedecode = */ false);
  MOZ_ASSERT(decoder, "Should have a decoder now");

  // Initialize the decoder.
  decoder->SetMetadataDecode(true);
  decoder->SetIterator(aSourceBuffer->Iterator());
  decoder->SetSampleSize(aSampleSize);
  decoder->SetResolution(aResolution);

  decoder->Init();
  if (NS_FAILED(decoder->GetDecoderError())) {
    return nullptr;
  }

  return decoder.forget();
}

/* static */ already_AddRefed<Decoder>
DecoderFactory::CreateAnonymousDecoder(DecoderType aType,
                                       SourceBuffer* aSourceBuffer,
                                       SurfaceFlags aSurfaceFlags)
{
  if (aType == DecoderType::UNKNOWN) {
    return nullptr;
  }

  nsRefPtr<Decoder> decoder =
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

  decoder->Init();
  if (NS_FAILED(decoder->GetDecoderError())) {
    return nullptr;
  }

  return decoder.forget();
}

/* static */ already_AddRefed<Decoder>
DecoderFactory::CreateAnonymousMetadataDecoder(DecoderType aType,
                                               SourceBuffer* aSourceBuffer)
{
  if (aType == DecoderType::UNKNOWN) {
    return nullptr;
  }

  nsRefPtr<Decoder> decoder =
    GetDecoder(aType, /* aImage = */ nullptr, /* aIsRedecode = */ false);
  MOZ_ASSERT(decoder, "Should have a decoder now");

  // Initialize the decoder.
  decoder->SetMetadataDecode(true);
  decoder->SetIterator(aSourceBuffer->Iterator());
  decoder->SetDecoderFlags(DecoderFlags::FIRST_FRAME_ONLY);

  decoder->Init();
  if (NS_FAILED(decoder->GetDecoderError())) {
    return nullptr;
  }

  return decoder.forget();
}

} // namespace image
} // namespace mozilla
