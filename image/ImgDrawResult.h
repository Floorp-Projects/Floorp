/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_image_ImgDrawResult_h
#define mozilla_image_ImgDrawResult_h

#include <cstdint>  // for uint8_t
#include "mozilla/Attributes.h"
#include "mozilla/Likely.h"

namespace mozilla {
namespace image {

/**
 * An enumeration representing the result of a drawing operation.
 *
 * Most users of ImgDrawResult will only be interested in whether the value is
 * SUCCESS or not. The other values are primarily useful for debugging and error
 * handling.
 *
 * SUCCESS: We successfully drew a completely decoded frame of the requested
 * size. Drawing again with FLAG_SYNC_DECODE would not change the result.
 *
 * SUCCESS_NOT_COMPLETE: The image was drawn successfully and completely, but
 * it hasn't notified about the sync-decode yet. This can only happen when
 * layout pokes at the internal image state beforehand via the result of
 * StartDecodingWithResult. This should probably go away eventually, somehow,
 * see bug 1471583.
 *
 * INCOMPLETE: We successfully drew a frame that was partially decoded. (Note
 * that successfully drawing a partially decoded frame may not actually draw any
 * pixels!) Drawing again with FLAG_SYNC_DECODE would improve the result.
 *
 * WRONG_SIZE: We successfully drew a wrongly-sized frame that had to be scaled.
 * This is only returned if drawing again with FLAG_SYNC_DECODE would improve
 * the result; if the size requested was larger than the intrinsic size of the
 * image, for example, we would generally have to scale whether FLAG_SYNC_DECODE
 * was specified or not, and therefore we would not return WRONG_SIZE.
 *
 * NOT_READY: We failed to draw because no decoded version of the image was
 * available. Drawing again with FLAG_SYNC_DECODE would improve the result.
 * (Though FLAG_SYNC_DECODE will not necessarily work until after the image's
 * load event!)
 *
 * TEMPORARY_ERROR: We failed to draw due to a temporary error. Drawing may
 * succeed at a later time.
 *
 * BAD_IMAGE: We failed to draw because the image has an error. This is a
 * permanent condition.
 *
 * BAD_ARGS: We failed to draw because bad arguments were passed to draw().
 *
 * NOT_SUPPORTED: The requested operation is not supported, but the image is
 *                otherwise valid.
 */
enum class [[nodiscard]] ImgDrawResult : uint8_t{
    SUCCESS,      SUCCESS_NOT_COMPLETE, INCOMPLETE, WRONG_SIZE,
    NOT_READY,    TEMPORARY_ERROR,      BAD_IMAGE,  BAD_ARGS,
    NOT_SUPPORTED};

/**
 * You can combine ImgDrawResults with &. By analogy to bitwise-&, the result is
 * ImgDrawResult::SUCCESS only if both operands are ImgDrawResult::SUCCESS.
 * Otherwise, a failing ImgDrawResult is returned; we favor the left operand's
 * failure when deciding which failure to return, with the exception that we
 * always prefer any other kind of failure over ImgDrawResult::BAD_IMAGE, since
 * other failures are recoverable and we want to know if any recoverable
 * failures occurred.
 */
inline ImgDrawResult operator&(const ImgDrawResult aLeft,
                               const ImgDrawResult aRight) {
  if (MOZ_LIKELY(aLeft == ImgDrawResult::SUCCESS)) {
    return aRight;
  }

  if (aLeft == ImgDrawResult::NOT_SUPPORTED ||
      aRight == ImgDrawResult::NOT_SUPPORTED) {
    return ImgDrawResult::NOT_SUPPORTED;
  }

  if ((aLeft == ImgDrawResult::BAD_IMAGE ||
       aLeft == ImgDrawResult::SUCCESS_NOT_COMPLETE) &&
      aRight != ImgDrawResult::SUCCESS &&
      aRight != ImgDrawResult::SUCCESS_NOT_COMPLETE) {
    return aRight;
  }
  return aLeft;
}

inline ImgDrawResult& operator&=(ImgDrawResult& aLeft,
                                 const ImgDrawResult aRight) {
  aLeft = aLeft & aRight;
  return aLeft;
}

/**
 * A struct used during painting to provide input flags to determine how
 * imagelib draw calls should behave and an output ImgDrawResult to return
 * information about the result of any imagelib draw calls that may have
 * occurred.
 */
struct imgDrawingParams {
  explicit imgDrawingParams(uint32_t aImageFlags = 0)
      : imageFlags(aImageFlags), result(ImgDrawResult::SUCCESS) {}

  const uint32_t imageFlags;  // imgIContainer::FLAG_* image flags to pass to
                              // image lib draw calls.
  ImgDrawResult result;       // To return results from image lib painting.
};

}  // namespace image
}  // namespace mozilla

#endif  // mozilla_image_ImgDrawResult_h
