//
// Copyright (c) 2002-2012 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
#include "compiler/VariablePacker.h"
#include "gtest/gtest.h"

TEST(VariablePacking, Pack) {
  VariablePacker packer;
  TVariableInfoList vars;
  const int kMaxRows = 16;
  // test no vars.
  EXPECT_TRUE(packer.CheckVariablesWithinPackingLimits(kMaxRows, vars));

  ShDataType types[] = {
    SH_FLOAT_MAT4,            // 0
    SH_FLOAT_MAT2,            // 1
    SH_FLOAT_VEC4,            // 2
    SH_INT_VEC4,              // 3
    SH_BOOL_VEC4,             // 4
    SH_FLOAT_MAT3,            // 5
    SH_FLOAT_VEC3,            // 6
    SH_INT_VEC3,              // 7
    SH_BOOL_VEC3,             // 8
    SH_FLOAT_VEC2,            // 9
    SH_INT_VEC2,              // 10
    SH_BOOL_VEC2,             // 11
    SH_FLOAT,                 // 12
    SH_INT,                   // 13
    SH_BOOL,                  // 14
    SH_SAMPLER_2D,            // 15
    SH_SAMPLER_CUBE,          // 16
    SH_SAMPLER_EXTERNAL_OES,  // 17
    SH_SAMPLER_2D_RECT_ARB,   // 18
  };

  for (size_t tt = 0; tt < sizeof(types) / sizeof(types[0]); ++tt) {
    ShDataType type = types[tt];
    int num_rows = VariablePacker::GetNumRows(type);
    int num_components_per_row = VariablePacker::GetNumComponentsPerRow(type);
    // Check 1 of the type.
    vars.clear();
    vars.push_back(TVariableInfo(type, 1));
    EXPECT_TRUE(packer.CheckVariablesWithinPackingLimits(kMaxRows, vars));

    // Check exactly the right amount of 1 type as an array.
    int num_vars = kMaxRows / num_rows;
    vars.clear();
    vars.push_back(TVariableInfo(type, num_vars));
    EXPECT_TRUE(packer.CheckVariablesWithinPackingLimits(kMaxRows, vars));

    // test too many
    vars.clear();
    vars.push_back(TVariableInfo(type, num_vars + 1));
    EXPECT_FALSE(packer.CheckVariablesWithinPackingLimits(kMaxRows, vars));

    // Check exactly the right amount of 1 type as individual vars.
    num_vars = kMaxRows / num_rows *
        ((num_components_per_row > 2) ? 1 : (4 / num_components_per_row));
    vars.clear();
    for (int ii = 0; ii < num_vars; ++ii) {
      vars.push_back(TVariableInfo(type, 1));
    }
    EXPECT_TRUE(packer.CheckVariablesWithinPackingLimits(kMaxRows, vars));

    // Check 1 too many.
    vars.push_back(TVariableInfo( type, 1));
    EXPECT_FALSE(packer.CheckVariablesWithinPackingLimits(kMaxRows, vars));
  }

  // Test example from GLSL ES 3.0 spec chapter 11.
  vars.clear();
  vars.push_back(TVariableInfo(SH_FLOAT_VEC4, 1));
  vars.push_back(TVariableInfo(SH_FLOAT_MAT3, 1));
  vars.push_back(TVariableInfo(SH_FLOAT_MAT3, 1));
  vars.push_back(TVariableInfo(SH_FLOAT_VEC2, 6));
  vars.push_back(TVariableInfo(SH_FLOAT_VEC2, 4));
  vars.push_back(TVariableInfo(SH_FLOAT_VEC2, 1));
  vars.push_back(TVariableInfo(SH_FLOAT, 3));
  vars.push_back(TVariableInfo(SH_FLOAT, 2));
  vars.push_back(TVariableInfo(SH_FLOAT, 1));
  EXPECT_TRUE(packer.CheckVariablesWithinPackingLimits(kMaxRows, vars));
}

