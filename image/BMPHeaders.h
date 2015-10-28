/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_image_BMPHeaders_h
#define mozilla_image_BMPHeaders_h

#include <stddef.h>
#include <stdint.h>

namespace mozilla {
namespace image {
namespace bmp {

// The length of the file header as defined in the BMP spec.
static const size_t FILE_HEADER_LENGTH = 14;

// This lengths of the info header for the different BMP versions.
struct InfoHeaderLength {
  enum {
    WIN_V2 = 12,
    WIN_V3 = 40,
    WIN_V4 = 108,
    WIN_V5 = 124,

    // OS2_V1 is omitted; it's the same as WIN_V2.
    OS2_V2_MIN = 16,    // Minimum allowed value for OS2v2.
    OS2_V2_MAX = 64,    // Maximum allowed value for OS2v2.

    WIN_ICO = WIN_V3,
  };
};

} // namespace bmp
} // namespace image
} // namespace mozilla

#endif // mozilla_image_BMPHeaders_h
