//
// Copyright (c) 2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// DynamicHLSL.h: Interface for link and run-time HLSL generation
//

#ifndef LIBGLESV2_DYNAMIC_HLSL_H_
#define LIBGLESV2_DYNAMIC_HLSL_H_

#include "common/angleutils.h"
#include "libGLESv2/constants.h"

namespace rx
{
class Renderer;
}

namespace sh
{
struct Attribute;
struct ShaderVariable;
}

namespace gl
{

class InfoLog;
class FragmentShader;
class VertexShader;
struct VariableLocation;
struct LinkedVarying;
struct VertexAttribute;
struct VertexFormat;
struct PackedVarying;

typedef const PackedVarying *VaryingPacking[IMPLEMENTATION_MAX_VARYING_VECTORS][4];

struct PixelShaderOuputVariable
{
    GLenum type;
    std::string name;
    std::string source;
    size_t outputIndex;
};

class DynamicHLSL
{
  public:
    explicit DynamicHLSL(rx::Renderer *const renderer);

    int packVaryings(InfoLog &infoLog, VaryingPacking packing, FragmentShader *fragmentShader,
                     VertexShader *vertexShader, const std::vector<std::string>& transformFeedbackVaryings);
    std::string generateVertexShaderForInputLayout(const std::string &sourceShader, const VertexFormat inputLayout[],
                                                   const sh::Attribute shaderAttributes[]) const;
    std::string generatePixelShaderForOutputSignature(const std::string &sourceShader, const std::vector<PixelShaderOuputVariable> &outputVariables,
                                                      bool usesFragDepth, const std::vector<GLenum> &outputLayout) const;
    bool generateShaderLinkHLSL(InfoLog &infoLog, int registers, const VaryingPacking packing,
                                std::string& pixelHLSL, std::string& vertexHLSL,
                                FragmentShader *fragmentShader, VertexShader *vertexShader,
                                const std::vector<std::string>& transformFeedbackVaryings,
                                std::vector<LinkedVarying> *linkedVaryings,
                                std::map<int, VariableLocation> *programOutputVars,
                                std::vector<PixelShaderOuputVariable> *outPixelShaderKey,
                                bool *outUsesFragDepth) const;

    std::string generateGeometryShaderHLSL(int registers, FragmentShader *fragmentShader, VertexShader *vertexShader) const;
    void getInputLayoutSignature(const VertexFormat inputLayout[], GLenum signature[]) const;

  private:
    DISALLOW_COPY_AND_ASSIGN(DynamicHLSL);

    rx::Renderer *const mRenderer;

    struct SemanticInfo;

    std::string getVaryingSemantic(bool pointSize) const;
    SemanticInfo getSemanticInfo(int startRegisters, bool fragCoord, bool pointCoord, bool pointSize,
                                        bool pixelShader) const;
    std::string generateVaryingLinkHLSL(const SemanticInfo &info, const std::string &varyingHLSL) const;
    std::string generateVaryingHLSL(VertexShader *shader) const;
    void storeUserLinkedVaryings(const VertexShader *vertexShader, std::vector<LinkedVarying> *linkedVaryings) const;
    void storeBuiltinLinkedVaryings(const SemanticInfo &info, std::vector<LinkedVarying> *linkedVaryings) const;
    void defineOutputVariables(FragmentShader *fragmentShader, std::map<int, VariableLocation> *programOutputVars) const;
    std::string generatePointSpriteHLSL(int registers, FragmentShader *fragmentShader, VertexShader *vertexShader) const;

    // Prepend an underscore
    static std::string decorateVariable(const std::string &name);

    std::string generateAttributeConversionHLSL(const VertexFormat &vertexFormat, const sh::ShaderVariable &shaderAttrib) const;
};

// Utility method shared between ProgramBinary and DynamicHLSL
std::string ArrayString(unsigned int i);

}

#endif // LIBGLESV2_DYNAMIC_HLSL_H_
