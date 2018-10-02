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

#ifndef INCLUDED_Vec2C_h_GUID_F9715DE4_2649_4182_0F4C_D62121235D5F
#define INCLUDED_Vec2C_h_GUID_F9715DE4_2649_4182_0F4C_D62121235D5F

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
/** @brief A structure defining a 2D vector, which represents position
*/
typedef struct OSVR_Vec2 {
    /** @brief Internal array data. */
    double data[2];
} OSVR_Vec2;

#define OSVR_VEC_MEMBER(COMPONENT, INDEX)                                      \
    /** @brief Accessor for Vec2 component COMPONENT */                        \
    OSVR_INLINE double osvrVec2Get##COMPONENT(OSVR_Vec2 const *v) {            \
        return v->data[INDEX];                                                 \
    }                                                                          \
    /** @brief Setter for Vec2 component COMPONENT */                          \
    OSVR_INLINE void osvrVec2Set##COMPONENT(OSVR_Vec2 *v, double val) {        \
        v->data[INDEX] = val;                                                  \
    }

OSVR_VEC_MEMBER(X, 0)
OSVR_VEC_MEMBER(Y, 1)

#undef OSVR_VEC_MEMBER

/** @brief Set a Vec2 to the zero vector */
OSVR_INLINE void osvrVec2Zero(OSVR_Vec2 *v) {
    osvrVec2SetX(v, 0);
    osvrVec2SetY(v, 0);
}

/** @} */

OSVR_EXTERN_C_END

#ifdef __cplusplus
template <typename StreamType>
inline StreamType &operator<<(StreamType &os, OSVR_Vec2 const &vec) {
    os << "(" << vec.data[0] << ", " << vec.data[1] << ")";
    return os;
}
#endif

#endif // INCLUDED_Vec2C_h_GUID_F9715DE4_2649_4182_0F4C_D62121235D5F
