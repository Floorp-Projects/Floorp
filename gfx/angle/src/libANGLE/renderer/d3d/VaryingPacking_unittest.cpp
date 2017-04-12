//
// Copyright 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// VaryingPacking_unittest.cpp:
//   Tests for ANGLE's internal varying packing algorithm.
//

#include "libANGLE/renderer/d3d/hlsl/VaryingPacking.h"

#include <gtest/gtest.h>

#include "libANGLE/Program.h"

namespace
{

class VaryingPackingTest : public ::testing::TestWithParam<GLuint>
{
  protected:
    VaryingPackingTest() {}

    bool testVaryingPacking(const std::vector<sh::Varying> &shVaryings,
                            rx::VaryingPacking *varyingPacking)
    {
        std::vector<rx::PackedVarying> packedVaryings;
        for (const auto &shVarying : shVaryings)
        {
            packedVaryings.push_back(rx::PackedVarying(shVarying, shVarying.interpolation));
        }

        gl::InfoLog infoLog;
        std::vector<std::string> transformFeedbackVaryings;

        if (!varyingPacking->packUserVaryings(infoLog, packedVaryings, transformFeedbackVaryings))
            return false;

        return varyingPacking->validateBuiltins();
    }

    bool packVaryings(GLuint maxVaryings, const std::vector<sh::Varying> &shVaryings)
    {
        rx::VaryingPacking varyingPacking(maxVaryings);
        return testVaryingPacking(shVaryings, &varyingPacking);
    }

    const int kMaxVaryings = GetParam();
};

std::vector<sh::Varying> MakeVaryings(GLenum type, size_t count, size_t arraySize)
{
    std::vector<sh::Varying> varyings;

    for (size_t index = 0; index < count; ++index)
    {
        std::stringstream strstr;
        strstr << type << index;

        sh::Varying varying;
        varying.type          = type;
        varying.precision     = GL_MEDIUM_FLOAT;
        varying.name          = strstr.str();
        varying.mappedName    = strstr.str();
        varying.arraySize     = static_cast<unsigned int>(arraySize);
        varying.staticUse     = true;
        varying.interpolation = sh::INTERPOLATION_FLAT;
        varying.isInvariant   = false;

        varyings.push_back(varying);
    }

    return varyings;
}

void AddVaryings(std::vector<sh::Varying> *varyings, GLenum type, size_t count, size_t arraySize)
{
    const auto &newVaryings = MakeVaryings(type, count, arraySize);
    varyings->insert(varyings->end(), newVaryings.begin(), newVaryings.end());
}

// Test that a single varying can't overflow the packing.
TEST_P(VaryingPackingTest, OneVaryingLargerThanMax)
{
    ASSERT_FALSE(packVaryings(1, MakeVaryings(GL_FLOAT_MAT4, 1, 0)));
}

// Tests that using FragCoord as a user varying will eat up a register.
TEST_P(VaryingPackingTest, MaxVaryingVec4PlusFragCoord)
{
    const std::string &userSemantic = rx::GetVaryingSemantic(4, false);

    rx::VaryingPacking varyingPacking(kMaxVaryings);
    unsigned int reservedSemanticIndex = varyingPacking.getMaxSemanticIndex();

    varyingPacking.builtins(rx::SHADER_PIXEL)
        .glFragCoord.enable(userSemantic, reservedSemanticIndex);

    const auto &varyings = MakeVaryings(GL_FLOAT_VEC4, kMaxVaryings, 0);

    ASSERT_FALSE(testVaryingPacking(varyings, &varyingPacking));
}

// Tests that using PointCoord as a user varying will eat up a register.
TEST_P(VaryingPackingTest, MaxVaryingVec4PlusPointCoord)
{
    const std::string &userSemantic = rx::GetVaryingSemantic(4, false);

    rx::VaryingPacking varyingPacking(kMaxVaryings);
    unsigned int reservedSemanticIndex = varyingPacking.getMaxSemanticIndex();

    varyingPacking.builtins(rx::SHADER_PIXEL)
        .glPointCoord.enable(userSemantic, reservedSemanticIndex);

    const auto &varyings = MakeVaryings(GL_FLOAT_VEC4, kMaxVaryings, 0);

    ASSERT_FALSE(testVaryingPacking(varyings, &varyingPacking));
}

// This will overflow the available varying space.
TEST_P(VaryingPackingTest, MaxPlusOneVaryingVec3)
{
    ASSERT_FALSE(packVaryings(kMaxVaryings, MakeVaryings(GL_FLOAT_VEC3, kMaxVaryings + 1, 0)));
}

// This will overflow the available varying space.
TEST_P(VaryingPackingTest, MaxPlusOneVaryingVec3Array)
{
    ASSERT_FALSE(packVaryings(kMaxVaryings, MakeVaryings(GL_FLOAT_VEC3, kMaxVaryings / 2 + 1, 2)));
}

// This will overflow the available varying space.
TEST_P(VaryingPackingTest, MaxVaryingVec3AndOneVec2)
{
    std::vector<sh::Varying> varyings = MakeVaryings(GL_FLOAT_VEC3, kMaxVaryings, 0);
    AddVaryings(&varyings, GL_FLOAT_VEC2, 1, 0);
    ASSERT_FALSE(packVaryings(kMaxVaryings, varyings));
}

// This should work since two vec2s are packed in a single register.
TEST_P(VaryingPackingTest, MaxPlusOneVaryingVec2)
{
    ASSERT_TRUE(packVaryings(kMaxVaryings, MakeVaryings(GL_FLOAT_VEC2, kMaxVaryings + 1, 0)));
}

// Same for this one as above.
TEST_P(VaryingPackingTest, TwiceMaxVaryingVec2)
{
    ASSERT_TRUE(packVaryings(kMaxVaryings, MakeVaryings(GL_FLOAT_VEC2, kMaxVaryings * 2, 0)));
}

// This should not work since it overflows available varying space.
TEST_P(VaryingPackingTest, TooManyVaryingVec2)
{
    ASSERT_FALSE(packVaryings(kMaxVaryings, MakeVaryings(GL_FLOAT_VEC2, kMaxVaryings * 2 + 1, 0)));
}

// This should work according to the example GL packing rules - the float varyings are slotted
// into the end of the vec3 varying arrays.
TEST_P(VaryingPackingTest, MaxVaryingVec3ArrayAndFloatArrays)
{
    std::vector<sh::Varying> varyings = MakeVaryings(GL_FLOAT_VEC3, kMaxVaryings / 2, 2);
    AddVaryings(&varyings, GL_FLOAT, kMaxVaryings / 2, 2);
    ASSERT_TRUE(packVaryings(kMaxVaryings, varyings));
}

// This should not work - it has one too many float arrays.
TEST_P(VaryingPackingTest, MaxVaryingVec3ArrayAndMaxPlusOneFloatArray)
{
    std::vector<sh::Varying> varyings = MakeVaryings(GL_FLOAT_VEC3, kMaxVaryings / 2, 2);
    AddVaryings(&varyings, GL_FLOAT, kMaxVaryings / 2 + 1, 2);
    ASSERT_FALSE(packVaryings(kMaxVaryings, varyings));
}

// Makes separate tests for different values of kMaxVaryings.
INSTANTIATE_TEST_CASE_P(, VaryingPackingTest, ::testing::Values(1, 4, 8));

}  // anonymous namespace
