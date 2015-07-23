/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_image_DecoderFactory_h
#define mozilla_image_DecoderFactory_h

#include "mozilla/Maybe.h"
#include "mozilla/gfx/2D.h"
#include "nsCOMPtr.h"

class nsACString;

namespace mozilla {
namespace image {

class Decoder;
class RasterImage;
class SourceBuffer;

enum class DecoderType
{
  PNG,
  GIF,
  JPEG,
  BMP,
  ICO,
  ICON,
  UNKNOWN
};

class DecoderFactory
{
public:
  /// @return the type of decoder which is appropriate for @aMimeType.
  static DecoderType GetDecoderType(const char* aMimeType);

  /**
   * Creates and initializes a decoder of type @aType. The decoder will send
   * notifications to @aImage.
   *
   * XXX(seth): @aIsRedecode, @aImageIsTransient, and @aImageIsLocked should
   * really be part of @aFlags. This requires changes to the way that decoder
   * flags work, though. See bug 1185800.
   *
   * @param aType Which type of decoder to create - JPEG, PNG, etc.
   * @param aImage The image will own the decoder and which should receive
   *               notifications as decoding progresses.
   * @param aSourceBuffer The SourceBuffer which the decoder will read its data
   *                      from.
   * @param aTargetSize If not Nothing(), the target size which the image should
   *                    be scaled to during decoding. It's an error to specify
   *                    a target size for a decoder type which doesn't support
   *                    downscale-during-decode.
   * @param aFlags Flags specifying what type of output the decoder should
   *               produce; see GetDecodeFlags() in RasterImage.h.
   * @param aIsRedecode Specify 'true' if this image has been decoded before.
   * @param aImageIsTransient Specify 'true' if this image is transient.
   * @param aImageIsLocked Specify 'true' if this image is locked for the
   *                       lifetime of this decoder, and should be unlocked
   *                       when the decoder finishes.
   */
  static already_AddRefed<Decoder>
  CreateDecoder(DecoderType aType,
                RasterImage* aImage,
                SourceBuffer* aSourceBuffer,
                const Maybe<gfx::IntSize>& aTargetSize,
                uint32_t aFlags,
                bool aIsRedecode,
                bool aImageIsTransient,
                bool aImageIsLocked);

  /**
   * Creates and initializes a metadata decoder of type @aType. This decoder
   * will only decode the image's header, extracting metadata like the size of
   * the image. No actual image data will be decoded and no surfaces will be
   * allocated. The decoder will send notifications to @aImage.
   *
   * @param aType Which type of decoder to create - JPEG, PNG, etc.
   * @param aImage The image will own the decoder and which should receive
   *               notifications as decoding progresses.
   * @param aSourceBuffer The SourceBuffer which the decoder will read its data
   *                      from.
   */
  static already_AddRefed<Decoder>
  CreateMetadataDecoder(DecoderType aType,
                        RasterImage* aImage,
                        SourceBuffer* aSourceBuffer);

private:
  virtual ~DecoderFactory() = 0;

  /**
   * An internal method which allocates a new decoder of the requested @aType.
   */
  static already_AddRefed<Decoder> GetDecoder(DecoderType aType,
                                              RasterImage* aImage,
                                              bool aIsRedecode);
};

} // namespace image
} // namespace mozilla

#endif // mozilla_image_DecoderFactory_h
