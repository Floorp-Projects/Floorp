//
// Copyright 2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#include "libGLESv2/renderer/d3d/HLSLCompiler.h"
#include "libGLESv2/Program.h"
#include "libGLESv2/main.h"

#include "common/utilities.h"

#include "third_party/trace_event/trace_event.h"

namespace rx
{

CompileConfig::CompileConfig()
    : flags(0),
      name()
{
}

CompileConfig::CompileConfig(UINT flags, const std::string &name)
    : flags(flags),
      name(name)
{
}

HLSLCompiler::HLSLCompiler()
    : mD3DCompilerModule(NULL),
      mD3DCompileFunc(NULL)
{
}

HLSLCompiler::~HLSLCompiler()
{
    release();
}

bool HLSLCompiler::initialize()
{
    TRACE_EVENT0("gpu", "initializeCompiler");
#if defined(ANGLE_PRELOADED_D3DCOMPILER_MODULE_NAMES)
    // Find a D3DCompiler module that had already been loaded based on a predefined list of versions.
    static const char *d3dCompilerNames[] = ANGLE_PRELOADED_D3DCOMPILER_MODULE_NAMES;

    for (size_t i = 0; i < ArraySize(d3dCompilerNames); ++i)
    {
        if (GetModuleHandleExA(0, d3dCompilerNames[i], &mD3DCompilerModule))
        {
            break;
        }
    }
#endif  // ANGLE_PRELOADED_D3DCOMPILER_MODULE_NAMES

    if (!mD3DCompilerModule)
    {
        // Load the version of the D3DCompiler DLL associated with the Direct3D version ANGLE was built with.
        mD3DCompilerModule = LoadLibrary(D3DCOMPILER_DLL);
    }

    if (!mD3DCompilerModule)
    {
        ERR("No D3D compiler module found - aborting!\n");
        return false;
    }

    mD3DCompileFunc = reinterpret_cast<pD3DCompile>(GetProcAddress(mD3DCompilerModule, "D3DCompile"));
    ASSERT(mD3DCompileFunc);

    return mD3DCompileFunc != NULL;
}

void HLSLCompiler::release()
{
    if (mD3DCompilerModule)
    {
        FreeLibrary(mD3DCompilerModule);
        mD3DCompilerModule = NULL;
        mD3DCompileFunc = NULL;
    }
}

gl::Error HLSLCompiler::compileToBinary(gl::InfoLog &infoLog, const std::string &hlsl, const std::string &profile,
                                        const std::vector<CompileConfig> &configs, ID3DBlob **outCompiledBlob) const
{
    ASSERT(mD3DCompilerModule && mD3DCompileFunc);

    if (gl::perfActive())
    {
        std::string sourcePath = getTempPath();
        std::string sourceText = FormatString("#line 2 \"%s\"\n\n%s", sourcePath.c_str(), hlsl.c_str());
        writeFile(sourcePath.c_str(), sourceText.c_str(), sourceText.size());
    }

    for (size_t i = 0; i < configs.size(); ++i)
    {
        ID3DBlob *errorMessage = NULL;
        ID3DBlob *binary = NULL;

        HRESULT result = mD3DCompileFunc(hlsl.c_str(), hlsl.length(), gl::g_fakepath, NULL, NULL, "main", profile.c_str(),
                                         configs[i].flags, 0, &binary, &errorMessage);

        if (errorMessage)
        {
            const char *message = (const char*)errorMessage->GetBufferPointer();

            infoLog.appendSanitized(message);
            TRACE("\n%s", hlsl);
            TRACE("\n%s", message);

            SafeRelease(errorMessage);
        }

        if (SUCCEEDED(result))
        {
            *outCompiledBlob = binary;
            return gl::Error(GL_NO_ERROR);
        }
        else
        {
            if (result == E_OUTOFMEMORY)
            {
                *outCompiledBlob = NULL;
                return gl::Error(GL_OUT_OF_MEMORY, "HLSL compiler had an unexpected failure, result: 0x%X.", result);
            }

            infoLog.append("Warning: D3D shader compilation failed with %s flags.", configs[i].name.c_str());

            if (i + 1 < configs.size())
            {
                infoLog.append(" Retrying with %s.\n", configs[i + 1].name.c_str());
            }
        }
    }

    // None of the configurations succeeded in compiling this shader but the compiler is still intact
    *outCompiledBlob = NULL;
    return gl::Error(GL_NO_ERROR);
}

}
