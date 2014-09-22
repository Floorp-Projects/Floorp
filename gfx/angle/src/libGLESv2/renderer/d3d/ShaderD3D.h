//
// Copyright (c) 2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// ShaderD3D.h: Defines the rx::ShaderD3D class which implements rx::ShaderImpl.

#ifndef LIBGLESV2_RENDERER_SHADERD3D_H_
#define LIBGLESV2_RENDERER_SHADERD3D_H_

#include "libGLESv2/renderer/ShaderImpl.h"
#include "libGLESv2/Shader.h"

#include <map>

namespace rx
{
class Renderer;

class ShaderD3D : public ShaderImpl
{
    friend class DynamicHLSL;

  public:
    ShaderD3D(rx::Renderer *renderer);
    virtual ~ShaderD3D();

    static ShaderD3D *makeShaderD3D(ShaderImpl *impl);
    static const ShaderD3D *makeShaderD3D(const ShaderImpl *impl);

    // ShaderImpl implementation
    const std::string &getInfoLog() const { return mInfoLog; }
    const std::string &getTranslatedSource() const { return mHlsl; }

    // D3D-specific methods
    virtual void uncompile();
    void resetVaryingsRegisterAssignment();
    unsigned int getUniformRegister(const std::string &uniformName) const;
    unsigned int getInterfaceBlockRegister(const std::string &blockName) const;

    rx::D3DWorkaroundType getD3DWorkarounds() const;
    int getShaderVersion() const { return mShaderVersion; }
    bool usesDepthRange() const { return mUsesDepthRange; }
    bool usesPointSize() const { return mUsesPointSize; }
    std::vector<gl::PackedVarying> &getVaryings() { return mVaryings; }
    const std::vector<sh::Uniform> &getUniforms() const { return mActiveUniforms; }
    const std::vector<sh::InterfaceBlock> &getInterfaceBlocks() const  { return mActiveInterfaceBlocks; }

    static void releaseCompiler();
    static ShShaderOutput getCompilerOutputType(GLenum shader);

  protected:
    void compileToHLSL(void *compiler, const std::string &source);
    void parseVaryings(void *compiler);

    static bool compareVarying(const gl::PackedVarying &x, const gl::PackedVarying &y);

    static void *mFragmentCompiler;
    static void *mVertexCompiler;

    rx::Renderer *mRenderer;

    std::vector<gl::PackedVarying> mVaryings;

    int mShaderVersion;

    bool mUsesMultipleRenderTargets;
    bool mUsesFragColor;
    bool mUsesFragData;
    bool mUsesFragCoord;
    bool mUsesFrontFacing;
    bool mUsesPointSize;
    bool mUsesPointCoord;
    bool mUsesDepthRange;
    bool mUsesFragDepth;
    bool mUsesDiscardRewriting;
    bool mUsesNestedBreak;

  private:
    DISALLOW_COPY_AND_ASSIGN(ShaderD3D);

    void initializeCompiler();

    std::string mHlsl;
    std::string mInfoLog;
    std::vector<sh::Uniform> mActiveUniforms;
    std::vector<sh::InterfaceBlock> mActiveInterfaceBlocks;
    std::map<std::string, unsigned int> mUniformRegisterMap;
    std::map<std::string, unsigned int> mInterfaceBlockRegisterMap;
};

class VertexShaderD3D : public ShaderD3D
{
    friend class DynamicHLSL;

  public:
    VertexShaderD3D(rx::Renderer *renderer);
    virtual ~VertexShaderD3D();

    static VertexShaderD3D *makeVertexShaderD3D(ShaderImpl *impl);
    static const VertexShaderD3D *makeVertexShaderD3D(const ShaderImpl *impl);

    virtual bool compile(const std::string &source);
    virtual void uncompile();

    int getSemanticIndex(const std::string &attributeName);
    virtual const std::vector<sh::Attribute> &getActiveAttributes() const { return mActiveAttributes; }

  private:
    DISALLOW_COPY_AND_ASSIGN(VertexShaderD3D);

    void parseAttributes();

    std::vector<sh::Attribute> mActiveAttributes;
};

class FragmentShaderD3D : public ShaderD3D
{
    friend class DynamicHLSL;

  public:
    FragmentShaderD3D(rx::Renderer *renderer);
    virtual ~FragmentShaderD3D();

    static FragmentShaderD3D *makeFragmentShaderD3D(ShaderImpl *impl);
    static const FragmentShaderD3D *makeFragmentShaderD3D(const ShaderImpl *impl);

    virtual bool compile(const std::string &source);
    virtual void uncompile();

    virtual const std::vector<sh::Attribute> &getOutputVariables() const { return mActiveOutputVariables; }

  private:
    DISALLOW_COPY_AND_ASSIGN(FragmentShaderD3D);

    std::vector<sh::Attribute> mActiveOutputVariables;
};

}

#endif // LIBGLESV2_RENDERER_SHADERD3D_H_
