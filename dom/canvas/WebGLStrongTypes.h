
/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WEBGLSTRONGTYPES_H_
#define WEBGLSTRONGTYPES_H_

#include "GLDefs.h"
#include "mozilla/Assertions.h"
#include "mozilla/ArrayUtils.h"

// Usage:
// ===========
//
// To create a new type from a set of GLenums do the following:
//
//   STRONG_GLENUM_BEGIN(TypeName)
//     Enum1,
//     Enum2,
//     ...
//   STRONG_GLENUM_END()
//
// where TypeName is the name you want to give the type. Now simply use TypeName
// instead of GLenum.
//
// ~~~~~~~~~~~~~~~~
// Important Notes:
// ~~~~~~~~~~~~~~~~
//
// Boolean operators (==, !=) are provided in an effort to prevent some mistakes
// when using constants. For example we want to make sure that GL_ENUM_X is
// a valid value for the type in code like:
//
//   if (myNewType == LOCAL_GL_SOME_ENUM)
//       ...
//
// The operators will assert that LOCAL_GL_SOME_ENUM is a value that myNewType
// can have.
//
// ----
//
// A get() method is provided to allow access to the underlying GLenum. This
// method should ideally only be called when passing parameters to the gl->fXXXX
// functions, and be used very minimally everywhere else.
//
// Definitely XXX - DO NOT DO - XXX:
//
//   if (myNewType.get() == LOCAL_GL_SOME_ENUM)
//       ...
//
// As that undermines the debug checks that were implemented in the ==, and !=
// operators. If you see code like this it should be treated with suspicion.
//
// Background:
// ===========
//
// This macro is the first step in an effort to make the WebGL code safer.
// Many of the different functions take GLenum as their parameter which leads
// to bugs because of subtle differences in the enums purpose. For example there
// are two types of 'texture targets'. One is the texture binding locations:
//
//   GL_TEXTURE_2D
//   GL_TEXTURE_CUBE_MAP
//
// Yet, this is not the same as texture image targets:
//
//   GL_TEXTURE_2D
//   GL_TEXTURE_CUBE_MAP_POSITIVE_X
//   GL_TEXTURE_CUBE_MAP_NEGATIVE_X
//   GL_TEXTURE_CUBE_MAP_POSITIVE_Y
//   ...
//
// This subtle distinction has already led to many bugs in the texture code
// because of invalid assumptions about what type goes where. The macro below
// is an attempt at fixing this by providing a small wrapper around GLenum that
// validates its values.
//
#ifdef DEBUG

template<size_t N>
static bool
IsValueInArr(GLenum value, const GLenum (&arr)[N])
{
    for (size_t i = 0; i < N; ++i) {
        if (value == arr[i])
            return true;
    }

    return false;
}

#endif

#define STRONG_GLENUM_BEGIN(NAME)                  \
    class NAME {                                   \
    private:                                       \
        GLenum mValue;                             \
    public:                                        \
        MOZ_CONSTEXPR NAME(const NAME& other)      \
            : mValue(other.mValue) { }             \
                                                   \
        bool operator==(const NAME& other) const { \
            return mValue == other.mValue;         \
        }                                          \
                                                   \
        bool operator!=(const NAME& other) const { \
            return mValue != other.mValue;         \
        }                                          \
                                                   \
        GLenum get() const {                       \
            MOZ_ASSERT(mValue != LOCAL_GL_NONE);   \
            return mValue;                         \
        }                                          \
                                                   \
        NAME(GLenum val)                           \
            : mValue(val)                          \
        {                                          \
            const GLenum validValues[] = {

#define STRONG_GLENUM_END()                        \
            };                                     \
            (void)validValues;                     \
            MOZ_ASSERT(IsValueInArr(mValue, validValues)); \
        }                                          \
    };

/******************************************************************************
 *  Add your types after this comment
 *****************************************************************************/

STRONG_GLENUM_BEGIN(TexImageTarget)
    LOCAL_GL_NONE,
    LOCAL_GL_TEXTURE_2D,
    LOCAL_GL_TEXTURE_3D,
    LOCAL_GL_TEXTURE_CUBE_MAP_POSITIVE_X,
    LOCAL_GL_TEXTURE_CUBE_MAP_NEGATIVE_X,
    LOCAL_GL_TEXTURE_CUBE_MAP_POSITIVE_Y,
    LOCAL_GL_TEXTURE_CUBE_MAP_NEGATIVE_Y,
    LOCAL_GL_TEXTURE_CUBE_MAP_POSITIVE_Z,
    LOCAL_GL_TEXTURE_CUBE_MAP_NEGATIVE_Z,
STRONG_GLENUM_END()

STRONG_GLENUM_BEGIN(TexTarget)
    LOCAL_GL_NONE,
    LOCAL_GL_TEXTURE_2D,
    LOCAL_GL_TEXTURE_3D,
    LOCAL_GL_TEXTURE_CUBE_MAP,
STRONG_GLENUM_END()

#endif
