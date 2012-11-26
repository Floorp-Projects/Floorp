/*
 * GLContextStuff.h
 *
 *  Created on: Nov 13, 2012
 *      Author: jgilbert
 */

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
