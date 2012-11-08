/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <xmmintrin.h>
#include <emmintrin.h>

/* Before Nehalem _mm_loadu_si128 could be very slow, this trick is a little
 * faster. Once enough people are on architectures where _mm_loadu_si128 is
 * fast we can migrate to it.
 */
MOZ_ALWAYS_INLINE __m128i loadUnaligned128(const __m128i *aSource)
{
  // Yes! We use uninitialized memory here, we'll overwrite it though!
  __m128 res = _mm_loadl_pi(_mm_set1_ps(0), (const __m64*)aSource);
  return _mm_castps_si128(_mm_loadh_pi(res, ((const __m64*)(aSource)) + 1));
}
