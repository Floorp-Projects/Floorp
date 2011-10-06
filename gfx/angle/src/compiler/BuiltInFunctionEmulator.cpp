//
// Copyright (c) 2002-2011 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#include "compiler/BuiltInFunctionEmulator.h"

#include "compiler/SymbolTable.h"

namespace {

const char* kFunctionEmulationSource[] = {
    "float webgl_normalize_emu(float a) { return normalize(a) * 1; }",
    "vec2 webgl_normalize_emu(vec2 a) { return normalize(a) * 1; }",
    "vec3 webgl_normalize_emu(vec3 a) { return normalize(a) * 1; }",
    "vec4 webgl_normalize_emu(vec4 a) { return normalize(a) * 1; }",
    "float webgl_abs_emu(float a) { float rt = abs(a); if (rt < 0.0) rt = 0.0;  return rt; }",
    "vec2 webgl_abs_emu(vec2 a) { vec2 rt = abs(a); if (rt[0] < 0.0) rt[0] = 0.0;  return rt; }",
    "vec3 webgl_abs_emu(vec3 a) { vec3 rt = abs(a); if (rt[0] < 0.0) rt[0] = 0.0;  return rt; }",
    "vec4 webgl_abs_emu(vec4 a) { vec4 rt = abs(a); if (rt[0] < 0.0) rt[0] = 0.0;  return rt; }",
    "float webgl_sign_emu(float a) { float rt = sign(a); if (rt > 1.0) rt = 1.0;  return rt; }",
    "vec2 webgl_sign_emu(vec2 a) { float rt = sign(a); if (rt[0] > 1.0) rt[0] = 1.0;  return rt; }",
    "vec3 webgl_sign_emu(vec3 a) { float rt = sign(a); if (rt[0] > 1.0) rt[0] = 1.0;  return rt; }",
    "vec4 webgl_sign_emu(vec4 a) { float rt = sign(a); if (rt[0] > 1.0) rt[0] = 1.0;  return rt; }",
};

class BuiltInFunctionEmulationMarker : public TIntermTraverser {
public:
    BuiltInFunctionEmulationMarker(BuiltInFunctionEmulator& emulator)
        : mEmulator(emulator)
    {
    }

    virtual bool visitUnary(Visit visit, TIntermUnary* node)
    {
        if (visit == PreVisit) {
            bool needToEmulate = mEmulator.SetFunctionCalled(
                node->getOp(), node->getOperand()->getType());
            if (needToEmulate)
                node->setUseEmulatedFunction();
        }
        return true;
    }

private:
    BuiltInFunctionEmulator& mEmulator;
};

}  // anonymous namepsace

BuiltInFunctionEmulator::BuiltInFunctionEmulator()
    : mFunctionGroupMask(TFunctionGroupAll)
{
}

void BuiltInFunctionEmulator::SetFunctionGroupMask(
    unsigned int functionGroupMask)
{
    mFunctionGroupMask = functionGroupMask;
}

bool BuiltInFunctionEmulator::SetFunctionCalled(
    TOperator op, const TType& returnType)
{
    TBuiltInFunction function = IdentifyFunction(op, returnType);
    if (function == TFunctionUnknown)
        return false;
    for (size_t i = 0; i < mFunctions.size(); ++i) {
        if (mFunctions[i] == function)
            return true;
    }
    switch (function) {
        case TFunctionNormalize1:
        case TFunctionNormalize2:
        case TFunctionNormalize3:
        case TFunctionNormalize4:
            if (mFunctionGroupMask & TFunctionGroupNormalize) {
                mFunctions.push_back(function);
                return true;
            }
            break;
        case TFunctionAbs1:
        case TFunctionAbs2:
        case TFunctionAbs3:
        case TFunctionAbs4:
            if (mFunctionGroupMask & TFunctionGroupAbs) {
                mFunctions.push_back(function);
                return true;
            }
            break;
        case TFunctionSign1:
        case TFunctionSign2:
        case TFunctionSign3:
        case TFunctionSign4:
            if (mFunctionGroupMask & TFunctionGroupSign) {
                mFunctions.push_back(function);
                return true;
            }
            break;
        default:
            UNREACHABLE();
            break;
    }
    return false;
}

void BuiltInFunctionEmulator::OutputEmulatedFunctionDefinition(
    TInfoSinkBase& out, bool withPrecision) const
{
    if (mFunctions.size() == 0)
        return;
    out << "// BEGIN: Generated code for built-in function emulation\n\n";
    if (withPrecision) {
        out << "#if defined(GL_FRAGMENT_PRECISION_HIGH) && (GL_FRAGMENT_PRECISION_HIGH == 1)\n"
            << "precision highp float;\n"
            << "#else\n"
            << "precision mediump float;\n"
            << "#endif\n\n";
    }
    for (size_t i = 0; i < mFunctions.size(); ++i) {
        out << kFunctionEmulationSource[mFunctions[i]] << "\n\n";
    }
    out << "// END: Generated code for built-in function emulation\n\n";
}

BuiltInFunctionEmulator::TBuiltInFunction
BuiltInFunctionEmulator::IdentifyFunction(TOperator op, const TType& returnType)
{
    unsigned int function = TFunctionUnknown;
    if (op == EOpNormalize)
        function = TFunctionNormalize1;
    else if (op == EOpAbs)
        function = TFunctionAbs1;
    else if (op == EOpSign)
        function = TFunctionSign1;
    else
        return static_cast<TBuiltInFunction>(function);

    if (returnType.isVector())
        function += returnType.getNominalSize() - 1;
    return static_cast<TBuiltInFunction>(function);
}

void BuiltInFunctionEmulator::MarkBuiltInFunctionsForEmulation(
    TIntermNode* root)
{
    ASSERT(root);

    BuiltInFunctionEmulationMarker marker(*this);
    root->traverse(&marker);
}

//static
TString BuiltInFunctionEmulator::GetEmulatedFunctionName(
    const TString& name)
{
    ASSERT(name[name.length() - 1] == '(');
    return "webgl_" + name.substr(0, name.length() - 1) + "_emu(";
}

