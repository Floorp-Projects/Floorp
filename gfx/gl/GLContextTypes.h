/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GLCONTEXTSTUFF_H_
#define GLCONTEXTSTUFF_H_

/**
 * We don't include GLDefs.h here since we don't want to drag in all defines
 * in for all our users.
 */
typedef unsigned int GLenum;
typedef unsigned int GLbitfield;
typedef unsigned int GLuint;
typedef int GLint;
typedef int GLsizei;

namespace mozilla {
namespace gl {

enum ShaderProgramType {
    RGBALayerProgramType,
    RGBALayerExternalProgramType,
    BGRALayerProgramType,
    RGBXLayerProgramType,
    BGRXLayerProgramType,
    RGBARectLayerProgramType,
    RGBAExternalLayerProgramType,
    ColorLayerProgramType,
    YCbCrLayerProgramType,
    ComponentAlphaPass1ProgramType,
    ComponentAlphaPass2ProgramType,
    Copy2DProgramType,
    Copy2DRectProgramType,
    NumProgramTypes
};

} // namespace gl
} // namespace mozilla

#endif /* GLCONTEXTSTUFF_H_ */
