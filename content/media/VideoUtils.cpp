/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla code.
 *
 * The Initial Developer of the Original Code is
 * the Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Chris Pearce <chris@pearce.org.nz>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "VideoUtils.h"
#include "nsMathUtils.h"
#include "prtypes.h"

// Adds two 32bit unsigned numbers, retuns PR_TRUE if addition succeeded,
// or PR_FALSE the if addition would result in an overflow.
bool AddOverflow32(PRUint32 a, PRUint32 b, PRUint32& aResult) {
  PRUint64 rl = static_cast<PRUint64>(a) + static_cast<PRUint64>(b);
  if (rl > PR_UINT32_MAX) {
    return PR_FALSE;
  }
  aResult = static_cast<PRUint32>(rl);
  return true;
}

bool MulOverflow32(PRUint32 a, PRUint32 b, PRUint32& aResult)
{
  // 32 bit integer multiplication with overflow checking. Returns PR_TRUE
  // if the multiplication was successful, or PR_FALSE if the operation resulted
  // in an integer overflow.
  PRUint64 a64 = a;
  PRUint64 b64 = b;
  PRUint64 r64 = a64 * b64;
  if (r64 > PR_UINT32_MAX)
     return PR_FALSE;
  aResult = static_cast<PRUint32>(r64);
  return PR_TRUE;
}

// Adds two 64bit numbers, retuns PR_TRUE if addition succeeded, or PR_FALSE
// if addition would result in an overflow.
bool AddOverflow(PRInt64 a, PRInt64 b, PRInt64& aResult) {
  if (b < 1) {
    if (PR_INT64_MIN - b <= a) {
      aResult = a + b;
      return PR_TRUE;
    }
  } else if (PR_INT64_MAX - b >= a) {
    aResult = a + b;
    return PR_TRUE;
  }
  return PR_FALSE;
}

// 64 bit integer multiplication with overflow checking. Returns PR_TRUE
// if the multiplication was successful, or PR_FALSE if the operation resulted
// in an integer overflow.
bool MulOverflow(PRInt64 a, PRInt64 b, PRInt64& aResult) {
  // We break a multiplication a * b into of sign_a * sign_b * abs(a) * abs(b)
  //
  // This is equivalent to:
  //
  // (sign_a * sign_b) * ((a_hi * 2^32) + a_lo) * ((b_hi * 2^32) + b_lo)
  //
  // Which is equivalent to:
  //
  // (sign_a * sign_b) *
  // ((a_hi * b_hi << 64) +
  //  (a_hi * b_lo << 32) + (a_lo * b_hi << 32) +
  //   a_lo * b_lo)
  //
  // So to check if a*b overflows, we must check each sub part of the above
  // sum.
  //
  // Note: -1 * PR_INT64_MIN == PR_INT64_MIN ; we can't negate PR_INT64_MIN!
  // Note: Shift of negative numbers is undefined.
  //
  // Figure out the sign after multiplication. Then we can just work with
  // unsigned numbers.
  PRInt64 sign = (!(a < 0) == !(b < 0)) ? 1 : -1;

  PRInt64 abs_a = (a < 0) ? -a : a;
  PRInt64 abs_b = (b < 0) ? -b : b;

  if (abs_a < 0) {
    NS_ASSERTION(a == PR_INT64_MIN, "How else can this happen?");
    if (b == 0 || b == 1) {
      aResult = a * b;
      return PR_TRUE;
    } else {
      return PR_FALSE;
    }
  }

  if (abs_b < 0) {
    NS_ASSERTION(b == PR_INT64_MIN, "How else can this happen?");
    if (a == 0 || a == 1) {
      aResult = a * b;
      return PR_TRUE;
    } else {
      return PR_FALSE;
    }
  }

  NS_ASSERTION(abs_a >= 0 && abs_b >= 0, "abs values must be non-negative");

  PRInt64 a_hi = abs_a >> 32;
  PRInt64 a_lo = abs_a & 0xFFFFFFFF;
  PRInt64 b_hi = abs_b >> 32;
  PRInt64 b_lo = abs_b & 0xFFFFFFFF;

  NS_ASSERTION((a_hi<<32) + a_lo == abs_a, "Partition must be correct");
  NS_ASSERTION((b_hi<<32) + b_lo == abs_b, "Partition must be correct");

  // In the sub-equation (a_hi * b_hi << 64), if a_hi or b_hi
  // are non-zero, this will overflow as it's shifted by 64.
  // Abort if this overflows.
  if (a_hi != 0 && b_hi != 0) {
    return PR_FALSE;
  }

  // We can now assume that either a_hi or b_hi is 0.
  NS_ASSERTION(a_hi == 0 || b_hi == 0, "One of these must be 0");

  // Next we calculate:
  // (a_hi * b_lo << 32) + (a_lo * b_hi << 32)
  // We can factor this as:
  // (a_hi * b_lo + a_lo * b_hi) << 32
  PRInt64 q = a_hi * b_lo + a_lo * b_hi;
  if (q > PR_INT32_MAX) {
    // q will overflow when we shift by 32; abort.
    return PR_FALSE;
  }
  q <<= 32;

  // Both a_lo and b_lo are less than INT32_MAX, so can't overflow.
  PRUint64 lo = a_lo * b_lo;
  if (lo > PR_INT64_MAX) {
    return PR_FALSE;
  }

  // Add the final result. We must check for overflow during addition.
  if (!AddOverflow(q, static_cast<PRInt64>(lo), aResult)) {
    return PR_FALSE;
  }

  aResult *= sign;
  NS_ASSERTION(a * b == aResult, "We didn't overflow, but result is wrong!");
  return PR_TRUE;
}

// Converts from number of audio frames to microseconds, given the specified
// audio rate.
bool FramesToUsecs(PRInt64 aFrames, PRUint32 aRate, PRInt64& aOutUsecs)
{
  PRInt64 x;
  if (!MulOverflow(aFrames, USECS_PER_S, x))
    return PR_FALSE;
  aOutUsecs = x / aRate;
  return PR_TRUE;
}

// Converts from microseconds to number of audio frames, given the specified
// audio rate.
bool UsecsToFrames(PRInt64 aUsecs, PRUint32 aRate, PRInt64& aOutFrames)
{
  PRInt64 x;
  if (!MulOverflow(aUsecs, aRate, x))
    return PR_FALSE;
  aOutFrames = x / USECS_PER_S;
  return PR_TRUE;
}

static PRInt32 ConditionDimension(float aValue)
{
  // This will exclude NaNs and too-big values.
  if (aValue > 1.0 && aValue <= PR_INT32_MAX)
    return PRInt32(NS_round(aValue));
  return 0;
}

void ScaleDisplayByAspectRatio(nsIntSize& aDisplay, float aAspectRatio)
{
  if (aAspectRatio > 1.0) {
    // Increase the intrinsic width
    aDisplay.width = ConditionDimension(aAspectRatio * aDisplay.width);
  } else {
    // Increase the intrinsic height
    aDisplay.height = ConditionDimension(aDisplay.height / aAspectRatio);
  }
}
