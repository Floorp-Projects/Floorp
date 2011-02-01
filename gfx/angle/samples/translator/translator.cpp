//
// Copyright (c) 2002-2010 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#include "GLSLANG/ShaderLang.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vector>

//
// Return codes from main.
//
enum TFailCode {
    ESuccess = 0,
    EFailUsage,
    EFailCompile,
    EFailCompilerCreate,
};

static void usage();
static ShShaderType FindShaderType(const char* fileName);
static bool CompileFile(char* fileName, ShHandle compiler, int compileOptions);
static void LogMsg(char* msg, const char* name, const int num, const char* logName);
static void PrintActiveVariables(ShHandle compiler, ShShaderInfo varType);

// If NUM_SOURCE_STRINGS is set to a value > 1, the input file data is
// broken into that many chunks.
const unsigned int NUM_SOURCE_STRINGS = 2;
typedef std::vector<char*> ShaderSource;
static bool ReadShaderSource(const char* fileName, ShaderSource& source);
static void FreeShaderSource(ShaderSource& source);

//
// Set up the per compile resources
//
void GenerateResources(ShBuiltInResources* resources)
{
    ShInitBuiltInResources(resources);

    resources->MaxVertexAttribs = 8;
    resources->MaxVertexUniformVectors = 128;
    resources->MaxVaryingVectors = 8;
    resources->MaxVertexTextureImageUnits = 0;
    resources->MaxCombinedTextureImageUnits = 8;
    resources->MaxTextureImageUnits = 8;
    resources->MaxFragmentUniformVectors = 16;
    resources->MaxDrawBuffers = 1;

    resources->OES_standard_derivatives = 0;
}

int main(int argc, char* argv[])
{
    TFailCode failCode = ESuccess;

    int compileOptions = 0;
    int numCompiles = 0;
    ShHandle vertexCompiler = 0;
    ShHandle fragmentCompiler = 0;
    char* buffer = 0;
    int bufferLen = 0;
    int numAttribs = 0, numUniforms = 0;

    ShInitialize();

    ShBuiltInResources resources;
    GenerateResources(&resources);

    argc--;
    argv++;
    for (; (argc >= 1) && (failCode == ESuccess); argc--, argv++) {
        if (argv[0][0] == '-' || argv[0][0] == '/') {
            switch (argv[0][1]) {
            case 'i': compileOptions |= SH_INTERMEDIATE_TREE; break;
            case 'o': compileOptions |= SH_OBJECT_CODE; break;
            case 'u': compileOptions |= SH_ATTRIBUTES_UNIFORMS; break;
            default: failCode = EFailUsage;
            }
        } else {
            ShHandle compiler = 0;
            switch (FindShaderType(argv[0])) {
            case SH_VERTEX_SHADER:
                if (vertexCompiler == 0)
                    vertexCompiler = ShConstructCompiler(SH_VERTEX_SHADER, SH_GLES2_SPEC, &resources);
                compiler = vertexCompiler;
                break;
            case SH_FRAGMENT_SHADER:
                if (fragmentCompiler == 0)
                    fragmentCompiler = ShConstructCompiler(SH_FRAGMENT_SHADER, SH_GLES2_SPEC, &resources);
                compiler = fragmentCompiler;
                break;
            default: break;
            }
            if (compiler) {
              bool compiled = CompileFile(argv[0], compiler, compileOptions);

              LogMsg("BEGIN", "COMPILER", numCompiles, "INFO LOG");
              ShGetInfo(compiler, SH_INFO_LOG_LENGTH, &bufferLen);
              buffer = (char*) realloc(buffer, bufferLen * sizeof(char));
              ShGetInfoLog(compiler, buffer);
              puts(buffer);
              LogMsg("END", "COMPILER", numCompiles, "INFO LOG");
              printf("\n\n");

              if (compiled && (compileOptions & SH_OBJECT_CODE)) {
                  LogMsg("BEGIN", "COMPILER", numCompiles, "OBJ CODE");
                  ShGetInfo(compiler, SH_OBJECT_CODE_LENGTH, &bufferLen);
                  buffer = (char*) realloc(buffer, bufferLen * sizeof(char));
                  ShGetObjectCode(compiler, buffer);
                  puts(buffer);
                  LogMsg("END", "COMPILER", numCompiles, "OBJ CODE");
                  printf("\n\n");
              }
              if (compiled && (compileOptions & SH_ATTRIBUTES_UNIFORMS)) {
                  LogMsg("BEGIN", "COMPILER", numCompiles, "ACTIVE ATTRIBS");
                  PrintActiveVariables(compiler, SH_ACTIVE_ATTRIBUTES);
                  LogMsg("END", "COMPILER", numCompiles, "ACTIVE ATTRIBS");
                  printf("\n\n");

                  LogMsg("BEGIN", "COMPILER", numCompiles, "ACTIVE UNIFORMS");
                  PrintActiveVariables(compiler, SH_ACTIVE_UNIFORMS);
                  LogMsg("END", "COMPILER", numCompiles, "ACTIVE UNIFORMS");
                  printf("\n\n");
              }
              if (!compiled)
                  failCode = EFailCompile;
              ++numCompiles;
            } else {
                failCode = EFailCompilerCreate;
            }
        }
    }

    if ((vertexCompiler == 0) && (fragmentCompiler == 0))
        failCode = EFailUsage;
    if (failCode == EFailUsage)
        usage();

    if (vertexCompiler)
        ShDestruct(vertexCompiler);
    if (fragmentCompiler)
        ShDestruct(fragmentCompiler);
    if (buffer)
        free(buffer);
    ShFinalize();

    return failCode;
}

//
//   print usage to stdout
//
void usage()
{
    printf("Usage: translate [-i -o -u] file1 file2 ...\n"
        "Where: filename = filename ending in .frag or .vert\n"
        "       -i = print intermediate tree\n"
        "       -o = print translated code\n"
        "       -u = print active attribs and uniforms\n");
}

//
//   Deduce the shader type from the filename.  Files must end in one of the
//   following extensions:
//
//   .frag*    = fragment shader
//   .vert*    = vertex shader
//
ShShaderType FindShaderType(const char* fileName)
{
    assert(fileName);

    const char* ext = strrchr(fileName, '.');

    if (ext && strcmp(ext, ".sl") == 0)
        for (; ext > fileName && ext[0] != '.'; ext--);

    if (ext = strrchr(fileName, '.')) {
        if (strncmp(ext, ".frag", 4) == 0) return SH_FRAGMENT_SHADER;
        if (strncmp(ext, ".vert", 4) == 0) return SH_VERTEX_SHADER;
    }

    return SH_FRAGMENT_SHADER;
}

//
//   Read a file's data into a string, and compile it using ShCompile
//
bool CompileFile(char* fileName, ShHandle compiler, int compileOptions)
{
    ShaderSource source;
    if (!ReadShaderSource(fileName, source))
        return false;

    int ret = ShCompile(compiler, &source[0], source.size(), compileOptions);

    FreeShaderSource(source);
    return ret ? true : false;
}

void LogMsg(char* msg, const char* name, const int num, const char* logName)
{
    printf("#### %s %s %d %s ####\n", msg, name, num, logName);
}

void PrintActiveVariables(ShHandle compiler, ShShaderInfo varType)
{
    int nameSize = 0;
    switch (varType) {
        case SH_ACTIVE_ATTRIBUTES:
            ShGetInfo(compiler, SH_ACTIVE_ATTRIBUTE_MAX_LENGTH, &nameSize);
            break;
        case SH_ACTIVE_UNIFORMS:
            ShGetInfo(compiler, SH_ACTIVE_UNIFORM_MAX_LENGTH, &nameSize);
            break;
        default: assert(0);
    }
    if (nameSize <= 1) return;
    char* name = new char[nameSize];

    int activeVars = 0, size = 0;
    ShDataType type = SH_NONE;
    char* typeName = NULL;
    ShGetInfo(compiler, varType, &activeVars);
    for (int i = 0; i < activeVars; ++i) {
        switch (varType) {
            case SH_ACTIVE_ATTRIBUTES:
                ShGetActiveAttrib(compiler, i, NULL, &size, &type, name);
                break;
            case SH_ACTIVE_UNIFORMS:
                ShGetActiveUniform(compiler, i, NULL, &size, &type, name);
                break;
            default: assert(0);
        }
        switch (type) {
            case SH_FLOAT: typeName = "GL_FLOAT"; break;
            case SH_FLOAT_VEC2: typeName = "GL_FLOAT_VEC2"; break;
            case SH_FLOAT_VEC3: typeName = "GL_FLOAT_VEC3"; break;
            case SH_FLOAT_VEC4: typeName = "GL_FLOAT_VEC4"; break;
            case SH_INT: typeName = "GL_INT"; break;
            case SH_INT_VEC2: typeName = "GL_INT_VEC2"; break;
            case SH_INT_VEC3: typeName = "GL_INT_VEC3"; break;
            case SH_INT_VEC4: typeName = "GL_INT_VEC4"; break;
            case SH_BOOL: typeName = "GL_BOOL"; break;
            case SH_BOOL_VEC2: typeName = "GL_BOOL_VEC2"; break;
            case SH_BOOL_VEC3: typeName = "GL_BOOL_VEC3"; break;
            case SH_BOOL_VEC4: typeName = "GL_BOOL_VEC4"; break;
            case SH_FLOAT_MAT2: typeName = "GL_FLOAT_MAT2"; break;
            case SH_FLOAT_MAT3: typeName = "GL_FLOAT_MAT3"; break;
            case SH_FLOAT_MAT4: typeName = "GL_FLOAT_MAT4"; break;
            case SH_SAMPLER_2D: typeName = "GL_SAMPLER_2D"; break;
            case SH_SAMPLER_CUBE: typeName = "GL_SAMPLER_CUBE"; break;
            default: assert(0);
        }
        printf("%d: name:%s type:%s size:%d\n", i, name, typeName, size);
    }
    delete [] name;
}

static bool ReadShaderSource(const char* fileName, ShaderSource& source) {
    FILE* in = fopen(fileName, "rb");
    if (!in) {
        printf("Error: unable to open input file: %s\n", fileName);
        return false;
    }

    // Obtain file size.
    fseek(in, 0, SEEK_END);
    int count = ftell(in);
    rewind(in);

    int len = (int)ceil((float)count / (float)NUM_SOURCE_STRINGS);
    source.reserve(NUM_SOURCE_STRINGS);
    // Notice the usage of do-while instead of a while loop here.
    // It is there to handle empty files in which case a single empty
    // string is added to vector.
    do {
        char* data = new char[len + 1];
        int nread = fread(data, 1, len, in);
        data[nread] = '\0';
        source.push_back(data);

        count -= nread;
    } while (count > 0);

    fclose(in);
    return true;
}

static void FreeShaderSource(ShaderSource& source) {
    for (ShaderSource::size_type i = 0; i < source.size(); ++i) {
        delete [] source[i];
    }
    source.clear();
}

