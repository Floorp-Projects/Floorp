/** @file
    @brief Header

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

#ifndef INCLUDED_QuaternionC_h_GUID_1470A5FE_8209_41A6_C19E_46077FDF9C66
#define INCLUDED_QuaternionC_h_GUID_1470A5FE_8209_41A6_C19E_46077FDF9C66

/* Internal Includes */
#include <osvr/Util/APIBaseC.h>

/* Library/third-party includes */
/* none */

/* Standard includes */
/* none */

OSVR_EXTERN_C_BEGIN

/** @addtogroup UtilMath
    @{
*/
/** @brief A structure defining a quaternion, often a unit quaternion
 * representing 3D rotation.
 */
typedef struct OSVR_Quaternion {
  /** @brief Internal data - direct access not recommended */
  double data[4];
} OSVR_Quaternion;

#define OSVR_QUAT_MEMBER(COMPONENT, INDEX)                                  \
  /** @brief Accessor for quaternion component COMPONENT */                 \
  OSVR_INLINE double osvrQuatGet##COMPONENT(OSVR_Quaternion const* q) {     \
    return q->data[INDEX];                                                  \
  }                                                                         \
  /** @brief Setter for quaternion component COMPONENT */                   \
  OSVR_INLINE void osvrQuatSet##COMPONENT(OSVR_Quaternion* q, double val) { \
    q->data[INDEX] = val;                                                   \
  }

OSVR_QUAT_MEMBER(W, 0)
OSVR_QUAT_MEMBER(X, 1)
OSVR_QUAT_MEMBER(Y, 2)
OSVR_QUAT_MEMBER(Z, 3)

#undef OSVR_QUAT_MEMBER

/** @brief Set a quaternion to the identity rotation */
OSVR_INLINE void osvrQuatSetIdentity(OSVR_Quaternion* q) {
  osvrQuatSetW(q, 1);
  osvrQuatSetX(q, 0);
  osvrQuatSetY(q, 0);
  osvrQuatSetZ(q, 0);
}

/** @} */

OSVR_EXTERN_C_END

#ifdef __cplusplus
template <typename StreamType>
inline StreamType& operator<<(StreamType& os, OSVR_Quaternion const& quat) {
  os << "(" << osvrQuatGetW(&quat) << ", (" << osvrQuatGetX(&quat) << ", "
     << osvrQuatGetY(&quat) << ", " << osvrQuatGetZ(&quat) << "))";
  return os;
}
#endif

#endif
