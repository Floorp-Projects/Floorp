//
// Copyright (c) 2017 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// UniformLinker.h: implements link-time checks for default block uniforms, and generates uniform
// locations. Populates data structures related to uniforms so that they can be stored in program
// state.

#ifndef LIBANGLE_UNIFORMLINKER_H_
#define LIBANGLE_UNIFORMLINKER_H_

#include "libANGLE/Program.h"
#include "libANGLE/Uniform.h"

namespace gl
{

class UniformLinker
{
  public:
    UniformLinker(const ProgramState &state);

    bool link(const Context *context,
              InfoLog &infoLog,
              const Program::Bindings &uniformLocationBindings);

    void getResults(std::vector<LinkedUniform> *uniforms,
                    std::vector<VariableLocation> *uniformLocations);

  private:
    struct ShaderUniformCount
    {
        ShaderUniformCount() : vectorCount(0), samplerCount(0), imageCount(0), atomicCounterCount(0)
        {
        }
        ShaderUniformCount(const ShaderUniformCount &other) = default;
        ShaderUniformCount &operator=(const ShaderUniformCount &other) = default;

        ShaderUniformCount &operator+=(const ShaderUniformCount &other)
        {
            vectorCount += other.vectorCount;
            samplerCount += other.samplerCount;
            imageCount += other.imageCount;
            atomicCounterCount += other.atomicCounterCount;
            return *this;
        }

        unsigned int vectorCount;
        unsigned int samplerCount;
        unsigned int imageCount;
        unsigned int atomicCounterCount;
    };

    bool validateVertexAndFragmentUniforms(const Context *context, InfoLog &infoLog) const;

    static bool linkValidateUniforms(InfoLog &infoLog,
                                     const std::string &uniformName,
                                     const sh::Uniform &vertexUniform,
                                     const sh::Uniform &fragmentUniform);

    bool flattenUniformsAndCheckCapsForShader(const Context *context,
                                              Shader *shader,
                                              GLuint maxUniformComponents,
                                              GLuint maxTextureImageUnits,
                                              GLuint maxImageUnits,
                                              GLuint maxAtomicCounters,
                                              const std::string &componentsErrorMessage,
                                              const std::string &samplerErrorMessage,
                                              const std::string &imageErrorMessage,
                                              const std::string &atomicCounterErrorMessage,
                                              std::vector<LinkedUniform> &samplerUniforms,
                                              std::vector<LinkedUniform> &imageUniforms,
                                              std::vector<LinkedUniform> &atomicCounterUniforms,
                                              InfoLog &infoLog);

    bool flattenUniformsAndCheckCaps(const Context *context, InfoLog &infoLog);
    bool checkMaxCombinedAtomicCounters(const Caps &caps, InfoLog &infoLog);

    ShaderUniformCount flattenUniform(const sh::Uniform &uniform,
                                      std::vector<LinkedUniform> *samplerUniforms,
                                      std::vector<LinkedUniform> *imageUniforms,
                                      std::vector<LinkedUniform> *atomicCounterUniforms);

    // markStaticUse is given as a separate parameter because it is tracked here at struct
    // granularity.
    ShaderUniformCount flattenUniformImpl(const sh::ShaderVariable &uniform,
                                          const std::string &fullName,
                                          const std::string &fullMappedName,
                                          std::vector<LinkedUniform> *samplerUniforms,
                                          std::vector<LinkedUniform> *imageUniforms,
                                          std::vector<LinkedUniform> *atomicCounterUniforms,
                                          bool markStaticUse,
                                          int binding,
                                          int offset,
                                          int *location);

    bool indexUniforms(InfoLog &infoLog, const Program::Bindings &uniformLocationBindings);
    bool gatherUniformLocationsAndCheckConflicts(InfoLog &infoLog,
                                                 const Program::Bindings &uniformLocationBindings,
                                                 std::set<GLuint> *reservedLocations,
                                                 std::set<GLuint> *ignoredLocations,
                                                 int *maxUniformLocation);
    void pruneUnusedUniforms();

    const ProgramState &mState;
    std::vector<LinkedUniform> mUniforms;
    std::vector<VariableLocation> mUniformLocations;
};

}  // namespace gl

#endif  // LIBANGLE_UNIFORMLINKER_H_
