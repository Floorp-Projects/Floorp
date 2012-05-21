/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_IMAGELIB_ENDIAN_H_
#define MOZILLA_IMAGELIB_ENDIAN_H_

#include "prtypes.h"

#if defined WORDS_BIGENDIAN || defined IS_BIG_ENDIAN
// We must ensure that the entity is unsigned
// otherwise, if it is signed/negative, the MSB will be
// propagated when we shift
#define LITTLE_TO_NATIVE16(x) (((((PRUint16) x) & 0xFF) << 8) | \
                               (((PRUint16) x) >> 8))
#define LITTLE_TO_NATIVE32(x) (((((PRUint32) x) & 0xFF) << 24) | \
                               (((((PRUint32) x) >> 8) & 0xFF) << 16) | \
                               (((((PRUint32) x) >> 16) & 0xFF) << 8) | \
                               (((PRUint32) x) >> 24))
#define NATIVE32_TO_LITTLE LITTLE_TO_NATIVE32
#define NATIVE16_TO_LITTLE LITTLE_TO_NATIVE16
#else
#define LITTLE_TO_NATIVE16(x) x
#define LITTLE_TO_NATIVE32(x) x
#define NATIVE32_TO_LITTLE(x) x
#define NATIVE16_TO_LITTLE(x) x
#endif

#endif
