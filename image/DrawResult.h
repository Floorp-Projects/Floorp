/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_image_DrawResult_h
#define mozilla_image_DrawResult_h

#include <cstdint> // for uint8_t
#include "mozilla/Attributes.h"
#include "mozilla/Likely.h"

namespace mozilla {
namespace image {

/**
 * An enumeration representing the result of a drawing operation.
 *
 * Most users of DrawResult will only be interested in whether the value is
 * SUCCESS or not. The other values are primarily useful for debugging and error
 * handling.
 *
 * SUCCESS: We successfully drew a completely decoded frame of the requested
 * size. Drawing again with FLAG_SYNC_DECODE would not change the result.
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
 */
enum class MOZ_MUST_USE_TYPE DrawResult : uint8_t
{
  SUCCESS,
  INCOMPLETE,
  WRONG_SIZE,
  NOT_READY,
  TEMPORARY_ERROR,
  BAD_IMAGE,
  BAD_ARGS
};

/**
 * You can combine DrawResults with &. By analogy to bitwise-&, the result is
 * DrawResult::SUCCESS only if both operands are DrawResult::SUCCESS. Otherwise,
 * a failing DrawResult is returned; we favor the left operand's failure when
 * deciding which failure to return, with the exception that we always prefer
 * any other kind of failure over DrawResult::BAD_IMAGE, since other failures
 * are recoverable and we want to know if any recoverable failures occurred.
 */
inline DrawResult
operator&(const DrawResult aLeft, const DrawResult aRight)
{
  if (MOZ_LIKELY(aLeft == DrawResult::SUCCESS)) {
    return aRight;
  }
  if (aLeft == DrawResult::BAD_IMAGE && aRight != DrawResult::SUCCESS) {
    return aRight;
  }
  return aLeft;
}

inline DrawResult&
operator&=(DrawResult& aLeft, const DrawResult aRight)
{
  aLeft = aLeft & aRight;
  return aLeft;
}

/**
 * A struct used during painting to provide input flags to determine how
 * imagelib draw calls should behave and an output DrawResult to return
 * information about the result of any imagelib draw calls that may have
 * occurred.
 */
struct imgDrawingParams {
  explicit imgDrawingParams(uint32_t aImageFlags = 0)
    : imageFlags(aImageFlags), result(DrawResult::SUCCESS)
  {}

  const uint32_t imageFlags; // imgIContainer::FLAG_* image flags to pass to
                             // image lib draw calls.
  DrawResult result;         // To return results from image lib painting.
};

} // namespace image
} // namespace mozilla

#endif // mozilla_image_DrawResult_h
