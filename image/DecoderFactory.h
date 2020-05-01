/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_image_DecoderFactory_h
#define mozilla_image_DecoderFactory_h

#include "DecoderFlags.h"
#include "mozilla/Attributes.h"
#include "mozilla/Maybe.h"
#include "mozilla/NotNull.h"
#include "mozilla/gfx/2D.h"
#include "nsCOMPtr.h"
#include "SurfaceFlags.h"

namespace mozilla {
namespace image {

class Decoder;
class IDecodingTask;
class nsICODecoder;
class RasterImage;
class SourceBuffer;
class SourceBufferIterator;

/**
 * The type of decoder; this is usually determined from a MIME type using
 * DecoderFactory::GetDecoderType().
 */
enum class DecoderType {
  PNG,
  GIF,
  JPEG,
  BMP,
  BMP_CLIPBOARD,
  ICO,
  ICON,
  WEBP,
  AVIF,
  UNKNOWN
};

class DecoderFactory {
 public:
  /// @return the type of decoder which is appropriate for @aMimeType.
  static DecoderType GetDecoderType(const char* aMimeType);

  /**
   * Creates and initializes a decoder for non-animated images of type @aType.
   * (If the image *is* animated, only the first frame will be decoded.) The
   * decoder will send notifications to @aImage.
   *
   * @param aType Which type of decoder to create - JPEG, PNG, etc.
   * @param aImage The image will own the decoder and which should receive
   *               notifications as decoding progresses.
   * @param aSourceBuffer The SourceBuffer which the decoder will read its data
   *                      from.
   * @param aIntrinsicSize The intrinsic size of the image, normally obtained
   *                       during the metadata decode.
   * @param aOutputSize The output size for the decoder. If this is smaller than
   *                    the intrinsic size, the decoder will downscale the
   *                    image.
   * @param aDecoderFlags Flags specifying the behavior of this decoder.
   * @param aSurfaceFlags Flags specifying the type of output this decoder
   *                      should produce.
   * @param aOutTask Task representing the decoder.
   * @return NS_OK if the decoder has been created/initialized successfully;
   *         NS_ERROR_ALREADY_INITIALIZED if there is already an active decoder
   *           for this image;
   *         Else some other unrecoverable error occurred.
   */
  static nsresult CreateDecoder(DecoderType aType, NotNull<RasterImage*> aImage,
                                NotNull<SourceBuffer*> aSourceBuffer,
                                const gfx::IntSize& aIntrinsicSize,
                                const gfx::IntSize& aOutputSize,
                                DecoderFlags aDecoderFlags,
                                SurfaceFlags aSurfaceFlags,
                                IDecodingTask** aOutTask);

  /**
   * Creates and initializes a decoder for animated images of type @aType.
   * The decoder will send notifications to @aImage.
   *
   * @param aType Which type of decoder to create - JPEG, PNG, etc.
   * @param aImage The image will own the decoder and which should receive
   *               notifications as decoding progresses.
   * @param aSourceBuffer The SourceBuffer which the decoder will read its data
   *                      from.
   * @param aIntrinsicSize The intrinsic size of the image, normally obtained
   *                       during the metadata decode.
   * @param aDecoderFlags Flags specifying the behavior of this decoder.
   * @param aSurfaceFlags Flags specifying the type of output this decoder
   *                      should produce.
   * @param aCurrentFrame The current frame the decoder should auto advance to.
   * @param aOutTask Task representing the decoder.
   * @return NS_OK if the decoder has been created/initialized successfully;
   *         NS_ERROR_ALREADY_INITIALIZED if there is already an active decoder
   *           for this image;
   *         Else some other unrecoverable error occurred.
   */
  static nsresult CreateAnimationDecoder(
      DecoderType aType, NotNull<RasterImage*> aImage,
      NotNull<SourceBuffer*> aSourceBuffer, const gfx::IntSize& aIntrinsicSize,
      DecoderFlags aDecoderFlags, SurfaceFlags aSurfaceFlags,
      size_t aCurrentFrame, IDecodingTask** aOutTask);

  /**
   * Creates and initializes a decoder for animated images, cloned from the
   * given decoder.
   *
   * @param aDecoder Decoder to clone.
   */
  static already_AddRefed<Decoder> CloneAnimationDecoder(Decoder* aDecoder);

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
  static already_AddRefed<IDecodingTask> CreateMetadataDecoder(
      DecoderType aType, NotNull<RasterImage*> aImage,
      NotNull<SourceBuffer*> aSourceBuffer);

  /**
   * Creates and initializes a decoder for an ICO resource, which may be either
   * a BMP or PNG image.
   *
   * @param aType Which type of decoder to create. This must be either BMP or
   *              PNG.
   * @param aIterator The SourceBufferIterator which the decoder will read its
   *                  data from.
   * @param aICODecoder The ICO decoder which is controlling this resource
   *                    decoder. @aICODecoder's settings will be copied to the
   *                    resource decoder, so the two decoders will have the
   *                    same decoder flags, surface flags, target size, and
   *                    other parameters.
   * @param aIsMetadataDecode Indicates whether or not this decoder is for
   *                          metadata or not. Independent of the state of the
   *                          parent decoder.
   * @param aExpectedSize The expected size of the resource from the ICO header.
   * @param aDataOffset If @aType is BMP, specifies the offset at which data
   *                    begins in the BMP resource. Must be Some() if and only
   *                    if @aType is BMP.
   */
  static already_AddRefed<Decoder> CreateDecoderForICOResource(
      DecoderType aType, SourceBufferIterator&& aIterator,
      NotNull<nsICODecoder*> aICODecoder, bool aIsMetadataDecode,
      const Maybe<gfx::IntSize>& aExpectedSize,
      const Maybe<uint32_t>& aDataOffset = Nothing());

  /**
   * Creates and initializes an anonymous decoder (one which isn't associated
   * with an Image object). Only the first frame of the image will be decoded.
   *
   * @param aType Which type of decoder to create - JPEG, PNG, etc.
   * @param aSourceBuffer The SourceBuffer which the decoder will read its data
   *                      from.
   * @param aOutputSize If Some(), the output size for the decoder. If this is
   *                    smaller than the intrinsic size, the decoder will
   *                    downscale the image. If Nothing(), the output size will
   *                    be the intrinsic size.
   * @param aDecoderFlags Flags specifying the behavior of this decoder.
   * @param aSurfaceFlags Flags specifying the type of output this decoder
   *                      should produce.
   */
  static already_AddRefed<Decoder> CreateAnonymousDecoder(
      DecoderType aType, NotNull<SourceBuffer*> aSourceBuffer,
      const Maybe<gfx::IntSize>& aOutputSize, DecoderFlags aDecoderFlags,
      SurfaceFlags aSurfaceFlags);

  /**
   * Creates and initializes an anonymous metadata decoder (one which isn't
   * associated with an Image object). This decoder will only decode the image's
   * header, extracting metadata like the size of the image. No actual image
   * data will be decoded and no surfaces will be allocated.
   *
   * @param aType Which type of decoder to create - JPEG, PNG, etc.
   * @param aSourceBuffer The SourceBuffer which the decoder will read its data
   *                      from.
   */
  static already_AddRefed<Decoder> CreateAnonymousMetadataDecoder(
      DecoderType aType, NotNull<SourceBuffer*> aSourceBuffer);

 private:
  virtual ~DecoderFactory() = 0;

  /**
   * An internal method which allocates a new decoder of the requested @aType.
   */
  static already_AddRefed<Decoder> GetDecoder(DecoderType aType,
                                              RasterImage* aImage,
                                              bool aIsRedecode);
};

}  // namespace image
}  // namespace mozilla

#endif  // mozilla_image_DecoderFactory_h
