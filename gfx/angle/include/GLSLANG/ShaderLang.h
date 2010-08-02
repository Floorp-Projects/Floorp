//
// Copyright (c) 2002-2010 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
#ifndef _COMPILER_INTERFACE_INCLUDED_
#define _COMPILER_INTERFACE_INCLUDED_

#include "nscore.h"

#include "ResourceLimits.h"

#ifdef WIN32
# if !defined(MOZ_ENABLE_LIBXUL) && !defined(MOZ_STATIC_BUILD)
#  ifdef ANGLE_BUILD
#   define ANGLE_API NS_EXPORT
#  else
#   define ANGLE_API NS_IMPORT
#  endif
# else
#  define ANGLE_API  /*nothing*/
# endif
#else
# define ANGLE_API NS_EXTERNAL_VIS
#endif

//
// This is the platform independent interface between an OGL driver
// and the shading language compiler.
//

#ifdef __cplusplus
extern "C" {
#endif
//
// Driver must call this first, once, before doing any other
// compiler operations.
//
ANGLE_API int ShInitialize();
//
// Driver should call this at shutdown.
//
ANGLE_API int ShFinalize();
//
// Types of languages the compiler can consume.
//
typedef enum {
    EShLangVertex,
    EShLangFragment,
    EShLangCount
} EShLanguage;

//
// The language specification compiler conforms to.
// It currently supports OpenGL ES and WebGL specifications.
//
typedef enum {
    EShSpecGLES2,
    EShSpecWebGL
} EShSpec;

//
// Optimization level for the compiler.
//
typedef enum {
    EShOptNoGeneration,
    EShOptNone,
    EShOptSimple,       // Optimizations that can be done quickly
    EShOptFull          // Optimizations that will take more time
} EShOptimizationLevel;

enum TDebugOptions {
    EDebugOpNone               = 0x000,
    EDebugOpIntermediate       = 0x001   // Writes intermediate tree into info-log.
};

//
// ShHandle held by but opaque to the driver.  It is allocated,
// managed, and de-allocated by the compiler. It's contents 
// are defined by and used by the compiler.
//
// If handle creation fails, 0 will be returned.
//
typedef void* ShHandle;

//
// Driver calls these to create and destroy compiler objects.
//
ANGLE_API ShHandle ShConstructCompiler(EShLanguage, EShSpec, const TBuiltInResource*);
ANGLE_API void ShDestruct(ShHandle);

//
// The return value of ShCompile is boolean, indicating
// success or failure.
//
// The info-log should be written by ShCompile into 
// ShHandle, so it can answer future queries.
//
ANGLE_API int ShCompile(
    const ShHandle,
    const char* const shaderStrings[],
    const int numStrings,
    const EShOptimizationLevel,
    int debugOptions
    );

//
// All the following return 0 if the information is not
// available in the object passed down, or the object is bad.
//
ANGLE_API const char* ShGetInfoLog(const ShHandle);
ANGLE_API const char* ShGetObjectCode(const ShHandle);

#ifdef __cplusplus
}
#endif

#endif // _COMPILER_INTERFACE_INCLUDED_
