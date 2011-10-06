//
// Copyright (c) 2011 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#ifndef COMPILIER_BUILT_IN_FUNCTION_EMULATOR_H_
#define COMPILIER_BUILT_IN_FUNCTION_EMULATOR_H_

#include "compiler/InfoSink.h"
#include "compiler/intermediate.h"

//
// Built-in function groups.  We only list the ones that might need to be
// emulated in certain os/drivers, assuming they are no more than 32.
//
enum TBuiltInFunctionGroup {
    TFunctionGroupNormalize      = 1 << 0,
    TFunctionGroupAbs            = 1 << 1,
    TFunctionGroupSign           = 1 << 2,
    TFunctionGroupAll            =
        TFunctionGroupNormalize | TFunctionGroupAbs | TFunctionGroupSign
};

//
// This class decides which built-in functions need to be replaced with the
// emulated ones.
// It's only a workaround for OpenGL driver bugs, and isn't needed in general.
//
class BuiltInFunctionEmulator {
public:
    BuiltInFunctionEmulator();

    // functionGroupMask is a bitmap of TBuiltInFunctionGroup.
    // We only emulate functions that are marked by this mask and are actually
    // called in a given shader.
    // By default the value is TFunctionGroupAll.
    void SetFunctionGroupMask(unsigned int functionGroupMask);

    // Records that a function is called by the shader and might needs to be
    // emulated.  If the function's group is not in mFunctionGroupFilter, this
    // becomes an no-op.
    // Returns true if the function call needs to be replaced with an emulated
    // one.
    // TODO(zmo): for now, an operator and a return type is enough to identify
    // the function we want to emulate.  Should make this more flexible to
    // handle any functions.
    bool SetFunctionCalled(TOperator op, const TType& returnType);

    // Output function emulation definition.  This should be before any other
    // shader source.
    void OutputEmulatedFunctionDefinition(TInfoSinkBase& out, bool withPrecision) const;

    void MarkBuiltInFunctionsForEmulation(TIntermNode* root);

    // "name(" becomes "webgl_name_emu(".
    static TString GetEmulatedFunctionName(const TString& name);

private:
    //
    // Built-in functions.
    //
    enum TBuiltInFunction {
        TFunctionNormalize1 = 0,  // float normalize(float);
        TFunctionNormalize2,  // vec2 normalize(vec2);
        TFunctionNormalize3,  // vec3 normalize(vec3);
        TFunctionNormalize4,  // fec4 normalize(vec4);
        TFunctionAbs1,  // float abs(float);
        TFunctionAbs2,  // vec2 abs(vec2);
        TFunctionAbs3,  // vec3 abs(vec3);
        TFunctionAbs4,  // vec4 abs(vec4);
        TFunctionSign1,  // float sign(float);
        TFunctionSign2,  // vec2 sign(vec2);
        TFunctionSign3,  // vec3 sign(vec3);
        TFunctionSign4,  // vec4 sign(vec4);
        TFunctionUnknown
    };

    // Same TODO as SetFunctionCalled.
    TBuiltInFunction IdentifyFunction(TOperator op, const TType& returnType);

    TVector<TBuiltInFunction> mFunctions;
    unsigned int mFunctionGroupMask;  // a bitmap of TBuiltInFunctionGroup.
};

#endif  // COMPILIER_BUILT_IN_FUNCTION_EMULATOR_H_
