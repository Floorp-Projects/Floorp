/** @file
    @brief Header defining a dependency-free, cross-platform substitute for
   struct timeval

    Must be c-safe!

    @date 2014

    @author
    Sensics, Inc.
    <http://sensics.com/osvr>
*/

/*
// Copyright 2014 Sensics, Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
*/

#ifndef INCLUDED_TimeValueC_h_GUID_A02C6917_124D_4CB3_E63E_07F2DA7144E9
#define INCLUDED_TimeValueC_h_GUID_A02C6917_124D_4CB3_E63E_07F2DA7144E9

/* Internal Includes */
#include <osvr/Util/Export.h>
#include <osvr/Util/APIBaseC.h>
#include <osvr/Util/AnnotationMacrosC.h>
#include <osvr/Util/PlatformConfig.h>
#include <osvr/Util/StdInt.h>
#include <osvr/Util/BoolC.h>

/* Library/third-party includes */
/* none */

/* Standard includes */
/* none */

OSVR_EXTERN_C_BEGIN

/** @defgroup UtilTime Timestamp interaction
    @ingroup Util

    This provides a level of interoperability with struct timeval on systems
    with that facility. It provides a neutral representation with sufficiently
    large types.

    For C++ code, use of std::chrono or boost::chrono instead is recommended.

    Note that these time values may not necessarily correlate between processes
   so should not be used to estimate or measure latency, etc.

    @{
*/

/** @brief The signed integer type storing the seconds in a struct
    OSVR_TimeValue */
typedef int64_t OSVR_TimeValue_Seconds;
/** @brief The signed integer type storing the microseconds in a struct
    OSVR_TimeValue */
typedef int32_t OSVR_TimeValue_Microseconds;

/** @brief Standardized, portable parallel to struct timeval for representing
   both absolute times and time intervals.

   Where interpreted as an absolute time, its meaning is to be considered the
   same as that of the POSIX struct timeval:
   time since 00:00 Coordinated Universal Time (UTC), January 1, 1970.

   For best results, please keep normalized. Output of all functions here
   is normalized.
   */
typedef struct OSVR_TimeValue {
  /** @brief Seconds portion of the time value. */
  OSVR_TimeValue_Seconds seconds;
  /** @brief Microseconds portion of the time value. */
  OSVR_TimeValue_Microseconds microseconds;
} OSVR_TimeValue;

#ifdef OSVR_HAVE_STRUCT_TIMEVAL
/** @brief Gets the current time in the TimeValue. Parallel to gettimeofday. */
OSVR_UTIL_EXPORT void osvrTimeValueGetNow(OSVR_OUT OSVR_TimeValue* dest)
    OSVR_FUNC_NONNULL((1));

struct timeval; /* forward declaration */

/** @brief Converts from a TimeValue struct to your system's struct timeval.

    @param dest Pointer to an empty struct timeval for your platform.
    @param src A pointer to an OSVR_TimeValue you'd like to convert from.

    If either parameter is NULL, the function will return without doing
   anything.
*/
OSVR_UTIL_EXPORT void osvrTimeValueToStructTimeval(
    OSVR_OUT struct timeval* dest, OSVR_IN_PTR const OSVR_TimeValue* src)
    OSVR_FUNC_NONNULL((1, 2));

/** @brief Converts from a TimeValue struct to your system's struct timeval.
    @param dest An OSVR_TimeValue destination pointer.
    @param src Pointer to a struct timeval you'd like to convert from.

    The result is normalized.

    If either parameter is NULL, the function will return without doing
   anything.
*/
OSVR_UTIL_EXPORT void osvrStructTimevalToTimeValue(
    OSVR_OUT OSVR_TimeValue* dest, OSVR_IN_PTR const struct timeval* src)
    OSVR_FUNC_NONNULL((1, 2));
#endif

/** @brief "Normalizes" a time value so that the absolute number of microseconds
    is less than 1,000,000, and that the sign of both components is the same.

    @param tv Address of a struct TimeValue to normalize in place.

    If the given pointer is NULL, this function returns without doing anything.
*/
OSVR_UTIL_EXPORT void osvrTimeValueNormalize(OSVR_INOUT_PTR OSVR_TimeValue* tv)
    OSVR_FUNC_NONNULL((1));

/** @brief Sums two time values, replacing the first with the result.

    @param tvA Destination and first source.
    @param tvB second source

    If a given pointer is NULL, this function returns without doing anything.

    Both parameters are expected to be in normalized form.
*/
OSVR_UTIL_EXPORT void osvrTimeValueSum(OSVR_INOUT_PTR OSVR_TimeValue* tvA,
                                       OSVR_IN_PTR const OSVR_TimeValue* tvB)
    OSVR_FUNC_NONNULL((1, 2));

/** @brief Computes the difference between two time values, replacing the first
    with the result.

    Effectively, `*tvA = *tvA - *tvB`

    @param tvA Destination and first source.
    @param tvB second source

    If a given pointer is NULL, this function returns without doing anything.

    Both parameters are expected to be in normalized form.
*/
OSVR_UTIL_EXPORT void osvrTimeValueDifference(
    OSVR_INOUT_PTR OSVR_TimeValue* tvA, OSVR_IN_PTR const OSVR_TimeValue* tvB)
    OSVR_FUNC_NONNULL((1, 2));

/** @brief  Compares two time values (assumed to be normalized), returning
    the same values as strcmp

    @return <0 if A is earlier than B, 0 if they are the same, and >0 if A
    is later than B.
*/
OSVR_UTIL_EXPORT int osvrTimeValueCmp(OSVR_IN_PTR const OSVR_TimeValue* tvA,
                                      OSVR_IN_PTR const OSVR_TimeValue* tvB)
    OSVR_FUNC_NONNULL((1, 2));

OSVR_EXTERN_C_END

/** @brief Compute the difference between the two time values, returning the
    duration as a double-precision floating-point number of seconds.

    Effectively, `ret = *tvA - *tvB`

    @param tvA first source.
    @param tvB second source
    @return Duration of timespan in seconds (floating-point)
*/
OSVR_INLINE double osvrTimeValueDurationSeconds(
    OSVR_IN_PTR const OSVR_TimeValue* tvA,
    OSVR_IN_PTR const OSVR_TimeValue* tvB) {
  OSVR_TimeValue A = *tvA;
  osvrTimeValueDifference(&A, tvB);
  double dt = A.seconds + A.microseconds / 1000000.0;
  return dt;
}

/** @brief True if A is later than B */
OSVR_INLINE OSVR_CBool
osvrTimeValueGreater(OSVR_IN_PTR const OSVR_TimeValue* tvA,
                     OSVR_IN_PTR const OSVR_TimeValue* tvB) {
  if (!tvA || !tvB) {
    return OSVR_FALSE;
  }
  return ((tvA->seconds > tvB->seconds) ||
          (tvA->seconds == tvB->seconds &&
           tvA->microseconds > tvB->microseconds))
             ? OSVR_TRUE
             : OSVR_FALSE;
}

#ifdef __cplusplus

#  include <cmath>
#  include <cassert>

/// Returns true if the time value is normalized. Typically used in assertions.
inline bool osvrTimeValueIsNormalized(const OSVR_TimeValue& tv) {
#  ifdef __APPLE__
  // apparently standard library used on mac only has floating-point abs?
  return std::abs(double(tv.microseconds)) < 1000000 &&
#  else
  return std::abs(tv.microseconds) < 1000000 &&
#  endif
         ((tv.seconds > 0) == (tv.microseconds > 0));
}

/// True if A is later than B
inline bool osvrTimeValueGreater(const OSVR_TimeValue& tvA,
                                 const OSVR_TimeValue& tvB) {
  assert(osvrTimeValueIsNormalized(tvA) &&
         "First timevalue argument to comparison was not normalized!");
  assert(osvrTimeValueIsNormalized(tvB) &&
         "Second timevalue argument to comparison was not normalized!");
  return (tvA.seconds > tvB.seconds) ||
         (tvA.seconds == tvB.seconds && tvA.microseconds > tvB.microseconds);
}

/// Operator > overload for time values
inline bool operator>(const OSVR_TimeValue& tvA, const OSVR_TimeValue& tvB) {
  return osvrTimeValueGreater(tvA, tvB);
}

/// Operator < overload for time values
inline bool operator<(const OSVR_TimeValue& tvA, const OSVR_TimeValue& tvB) {
  // Change the order of arguments before forwarding.
  return osvrTimeValueGreater(tvB, tvA);
}

/// Operator == overload for time values
inline bool operator==(const OSVR_TimeValue& tvA, const OSVR_TimeValue& tvB) {
  assert(osvrTimeValueIsNormalized(tvA) &&
         "First timevalue argument to equality comparison was not normalized!");
  assert(
      osvrTimeValueIsNormalized(tvB) &&
      "Second timevalue argument to equality comparison was not normalized!");
  return (tvA.seconds == tvB.seconds) && (tvA.microseconds == tvB.microseconds);
}
/// Operator == overload for time values
inline bool operator!=(const OSVR_TimeValue& tvA, const OSVR_TimeValue& tvB) {
  assert(osvrTimeValueIsNormalized(tvA) &&
         "First timevalue argument to "
         "inequality comparison was not "
         "normalized!");
  assert(osvrTimeValueIsNormalized(tvB) &&
         "Second timevalue argument to "
         "inequality comparison was not "
         "normalized!");
  return (tvA.seconds != tvB.seconds) || (tvA.microseconds != tvB.microseconds);
}
#endif

/** @} */

#endif
