
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
//     STRONG_GLENUM_VALUE(ENUM1),
//     STRONG_GLENUM_VALUE(ENUM2),
//     ...
//   STRONG_GLENUM_END()
//
// where TypeName is the name you want to give the type. Now simply use TypeName
// instead of GLenum. The enum values must be given without GL_ prefix.
//
// ~~~~~~~~~~~~~~~~
// Important Notes:
// ~~~~~~~~~~~~~~~~
//
// Boolean operators (==, !=) are provided in an effort to prevent some mistakes
// when using constants. For example we want to make sure that GL_ENUM_X is
// a valid value for the type in code like:
//
//   if (myNewType == STRONG_GLENUM_VALUE(SOME_ENUM))
//       ...
//
// The operators will assert that STRONG_GLENUM_VALUE(SOME_ENUM) is a value that myNewType
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
//   if (myNewType.get() == STRONG_GLENUM_VALUE(SOME_ENUM))
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
// Comparison between STRONG_GLENUM's vs. enum classes
// ===================================================
//
// The present STRONG_GLENUM's differ from ordinary enum classes
// in that they assert at runtime that their values are legal, and in that they
// allow implicit conversion from integers to STRONG_GLENUM's but disallow
// implicit conversion from STRONG_GLENUM's to integers (enum classes are the opposite).
//
// When to use GLenum's vs. STRONG_GLENUM's vs. enum classes
// =========================================================
//
// Rule of thumb:
// * For unchecked GLenum constants, such as WebGL method parameters that haven't been
//   validated yet, use GLenum.
// * For already-validated GLenum constants, use STRONG_GLENUM's.
// * For custom constants that aren't GL enum values, use enum classes.

template<typename Details>
class StrongGLenum MOZ_FINAL
{
private:
    static const GLenum NonexistantGLenum = 0xdeaddead;

    GLenum mValue;

    static void AssertOnceThatEnumValuesAreSorted()
    {
#ifdef DEBUG
        static bool alreadyChecked = false;
        if (alreadyChecked) {
            return;
        }
        for (size_t i = 1; i < Details::valuesCount(); i++) {
            MOZ_ASSERT(Details::values()[i] > Details::values()[i - 1],
                       "GLenum values should be sorted in ascending order");
        }
        alreadyChecked = true;
#endif
    }

public:
    StrongGLenum(const StrongGLenum& other)
        : mValue(other.mValue)
    {
        AssertOnceThatEnumValuesAreSorted();
    }

    StrongGLenum()
#ifdef DEBUG
        : mValue(NonexistantGLenum)
#endif
    {
        AssertOnceThatEnumValuesAreSorted();
    }

    StrongGLenum(GLenum val)
        : mValue(val)
    {
        AssertOnceThatEnumValuesAreSorted();
        MOZ_ASSERT(IsValueLegal(mValue));
    }

    GLenum get() const {
        MOZ_ASSERT(mValue != NonexistantGLenum);
        return mValue;
    }

    bool operator==(const StrongGLenum& other) const {
        return get() == other.get();
    }

    bool operator!=(const StrongGLenum& other) const {
        return get() != other.get();
    }

    static bool IsValueLegal(GLenum value) {
        if (value > UINT16_MAX) {
            return false;
        }
        return std::binary_search(Details::values(),
                                  Details::values() + Details::valuesCount(),
                                  uint16_t(value));
    }
};

#define STRONG_GLENUM_BEGIN(NAME)                  \
    const uint16_t NAME##Values[] = {

#define STRONG_GLENUM_VALUE(VALUE) LOCAL_GL_##VALUE

#define STRONG_GLENUM_END(NAME)                        \
    };                                     \
    struct NAME##Details { \
        static size_t valuesCount() { return MOZ_ARRAY_LENGTH(NAME##Values); } \
        static const uint16_t* values() { return NAME##Values; } \
    }; \
    typedef StrongGLenum<NAME##Details> NAME;

/******************************************************************************
 *  Add your types after this comment
 *****************************************************************************/

STRONG_GLENUM_BEGIN(TexImageTarget)
    STRONG_GLENUM_VALUE(NONE),
    STRONG_GLENUM_VALUE(TEXTURE_2D),
    STRONG_GLENUM_VALUE(TEXTURE_3D),
    STRONG_GLENUM_VALUE(TEXTURE_CUBE_MAP_POSITIVE_X),
    STRONG_GLENUM_VALUE(TEXTURE_CUBE_MAP_NEGATIVE_X),
    STRONG_GLENUM_VALUE(TEXTURE_CUBE_MAP_POSITIVE_Y),
    STRONG_GLENUM_VALUE(TEXTURE_CUBE_MAP_NEGATIVE_Y),
    STRONG_GLENUM_VALUE(TEXTURE_CUBE_MAP_POSITIVE_Z),
    STRONG_GLENUM_VALUE(TEXTURE_CUBE_MAP_NEGATIVE_Z),
STRONG_GLENUM_END(TexImageTarget)

STRONG_GLENUM_BEGIN(TexTarget)
    STRONG_GLENUM_VALUE(NONE),
    STRONG_GLENUM_VALUE(TEXTURE_2D),
    STRONG_GLENUM_VALUE(TEXTURE_3D),
    STRONG_GLENUM_VALUE(TEXTURE_CUBE_MAP),
STRONG_GLENUM_END(TexTarget)

#endif
