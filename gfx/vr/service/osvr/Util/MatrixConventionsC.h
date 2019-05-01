/** @file
    @brief Header

    Must be c-safe!

    @date 2015

    @author
    Sensics, Inc.
    <http://sensics.com/osvr>
*/

/*
// Copyright 2015 Sensics, Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
*/

#ifndef INCLUDED_MatrixConventionsC_h_GUID_6FC7A4C6_E6C5_4A96_1C28_C3D21B909681
#define INCLUDED_MatrixConventionsC_h_GUID_6FC7A4C6_E6C5_4A96_1C28_C3D21B909681

/* Internal Includes */
#include <osvr/Util/Export.h>
#include <osvr/Util/APIBaseC.h>
#include <osvr/Util/StdInt.h>
#include <osvr/Util/Pose3C.h>
#include <osvr/Util/ReturnCodesC.h>

/* Library/third-party includes */
/* none */

/* Standard includes */
/* none */

OSVR_EXTERN_C_BEGIN

/** @defgroup MatrixConvention Matrix conventions and bit flags
    @ingroup UtilMath
*/

/** @brief Type for passing matrix convention flags.
    @ingroup MatrixConvention
*/
typedef uint16_t OSVR_MatrixConventions;

#ifndef OSVR_DOXYGEN_EXTERNAL
/** @brief Bitmasks for testing matrix conventions.
    @ingroup MatrixConvention
*/
typedef enum OSVR_MatrixMasks {
  OSVR_MATRIX_MASK_ROWMAJOR = 0x1,
  OSVR_MATRIX_MASK_ROWVECTORS = 0x2,
  OSVR_MATRIX_MASK_LHINPUT = 0x4,
  OSVR_MATRIX_MASK_UNSIGNEDZ = 0x8
} OSVR_MatrixMasks;
#endif

/** @defgroup MatrixFlags Matrix flags
    @ingroup MatrixConvention

    Bit flags for specifying matrix options. Only one option may be specified
   per enum, with all the specified options combined with bitwise-or `|`.

    Most methods that take matrix flags only obey ::OSVR_MatrixOrderingFlags and
   ::OSVR_MatrixVectorFlags - the flags that affect memory order. The remaining
   flags are for use with projection matrix generation API methods.

    @{
*/
/** @brief Flag bit controlling output memory order */
typedef enum OSVR_MatrixOrderingFlags {
  /** @brief Column-major memory order (default) */
  OSVR_MATRIX_COLMAJOR = 0x0,
  /** @brief Row-major memory order */
  OSVR_MATRIX_ROWMAJOR = OSVR_MATRIX_MASK_ROWMAJOR
} OSVR_MatrixOrderingFlags;

/** @brief Flag bit controlling expected input to matrices.
    (Related to ::OSVR_MatrixOrderingFlags - setting one to non-default results
    in an output change, but setting both to non-default results in effectively
    no change in the output. If this blows your mind, just ignore this aside and
    carry on.)
*/
typedef enum OSVR_MatrixVectorFlags {
  /** @brief Matrix transforms column vectors (default) */
  OSVR_MATRIX_COLVECTORS = 0x0,
  /** @brief Matrix transforms row vectors */
  OSVR_MATRIX_ROWVECTORS = OSVR_MATRIX_MASK_ROWVECTORS
} OSVR_MatrixVectorFlags;

/** @brief Flag bit to indicate coordinate system input to projection matrix */
typedef enum OSVR_ProjectionMatrixInputFlags {
  /** @brief Matrix takes vectors from a right-handed coordinate system
     (default) */
  OSVR_MATRIX_RHINPUT = 0x0,
  /** @brief Matrix takes vectors from a left-handed coordinate system */
  OSVR_MATRIX_LHINPUT = OSVR_MATRIX_MASK_LHINPUT

} OSVR_ProjectionMatrixInputFlags;

/** @brief Flag bit to indicate the desired post-projection Z value convention
 */
typedef enum OSVR_ProjectionMatrixZFlags {
  /** @brief Matrix maps the near and far planes to signed Z values (in the
      range [-1, 1])  (default)*/
  OSVR_MATRIX_SIGNEDZ = 0x0,
  /** @brief Matrix maps the near and far planes to unsigned Z values (in the
      range [0, 1]) */
  OSVR_MATRIX_UNSIGNEDZ = OSVR_MATRIX_MASK_UNSIGNEDZ
} OSVR_ProjectionMatrixZFlags;
/** @} */ /* end of matrix flags group */

enum {
  /** @brief Constant for the number of elements in the matrices we use - 4x4.
      @ingroup MatrixConvention
  */
  OSVR_MATRIX_SIZE = 16
};

/** @addtogroup UtilMath
    @{
*/
/** @brief Set a matrix of doubles based on a Pose3.
    @param pose The Pose3 to convert
    @param flags Memory ordering flag - see @ref MatrixFlags
    @param[out] mat an array of 16 doubles
*/
OSVR_UTIL_EXPORT OSVR_ReturnCode osvrPose3ToMatrixd(
    OSVR_Pose3 const* pose, OSVR_MatrixConventions flags, double* mat);

/** @brief Set a matrix of floats based on a Pose3.
    @param pose The Pose3 to convert
    @param flags Memory ordering flag - see @ref MatrixFlags
    @param[out] mat an array of 16 floats
*/
OSVR_UTIL_EXPORT OSVR_ReturnCode osvrPose3ToMatrixf(
    OSVR_Pose3 const* pose, OSVR_MatrixConventions flags, float* mat);
/** @} */

OSVR_EXTERN_C_END

#ifdef __cplusplus
/** @brief Set a matrix based on a Pose3. (C++-only overload - detecting scalar
 * type) */
inline OSVR_ReturnCode osvrPose3ToMatrix(OSVR_Pose3 const* pose,
                                         OSVR_MatrixConventions flags,
                                         double* mat) {
  return osvrPose3ToMatrixd(pose, flags, mat);
}

/** @brief Set a matrix based on a Pose3. (C++-only overload - detecting scalar
 * type) */
inline OSVR_ReturnCode osvrPose3ToMatrix(OSVR_Pose3 const* pose,
                                         OSVR_MatrixConventions flags,
                                         float* mat) {
  return osvrPose3ToMatrixf(pose, flags, mat);
}

/** @brief Set a matrix based on a Pose3. (C++-only overload - detects scalar
 * and takes array rather than pointer) */
template <typename Scalar>
inline OSVR_ReturnCode osvrPose3ToMatrix(OSVR_Pose3 const* pose,
                                         OSVR_MatrixConventions flags,
                                         Scalar mat[OSVR_MATRIX_SIZE]) {
  return osvrPose3ToMatrix(pose, flags, &(mat[0]));
}
/** @brief Set a matrix based on a Pose3. (C++-only overload - detects scalar,
 * takes array, takes pose by reference) */
template <typename Scalar>
inline OSVR_ReturnCode osvrPose3ToMatrix(OSVR_Pose3 const& pose,
                                         OSVR_MatrixConventions flags,
                                         Scalar mat[OSVR_MATRIX_SIZE]) {
  return osvrPose3ToMatrix(&pose, flags, &(mat[0]));
}

#endif

/** @} */

#endif
