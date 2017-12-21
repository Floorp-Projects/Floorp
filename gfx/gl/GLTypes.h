/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GLTYPES_H_
#define GLTYPES_H_

#include <stddef.h>
#include <stdint.h>

#ifndef GLAPIENTRY
# ifdef WIN32
#  include <windef.h>
#  define GLAPIENTRY APIENTRY
#  define GLAPI
# else
#  define GLAPIENTRY
#  define GLAPI
# endif
#endif

typedef int8_t realGLboolean;

#if !defined(__gltypes_h_) && !defined(__gl_h_)
#define __gltypes_h_
#define __gl_h_

typedef uint32_t GLenum;
typedef uint32_t GLbitfield;
typedef uint32_t GLuint;
typedef int32_t GLint;
typedef int32_t GLsizei;
typedef int8_t GLbyte;
typedef int16_t GLshort;
typedef uint8_t GLubyte;
typedef uint16_t GLushort;
typedef float GLfloat;
typedef float GLclampf;
#ifndef GLdouble_defined
typedef double GLdouble;
#endif
typedef double GLclampd;
typedef void GLvoid;

typedef char GLchar;
#ifndef __gl2_h_
typedef intptr_t GLsizeiptr;
typedef intptr_t GLintptr;
#endif

#endif /* #if !defined(__gltypes_h_) && !defined(__gl_h_) */

#include <stdint.h>

// ARB_sync
typedef struct __GLsync* GLsync;
typedef int64_t GLint64;
typedef uint64_t GLuint64;

// OES_EGL_image (GLES)
typedef void* GLeglImage;

// KHR_debug
typedef void (GLAPIENTRY *GLDEBUGPROC)(GLenum source,
                                       GLenum type,
                                       GLuint id,
                                       GLenum severity,
                                       GLsizei length,
                                       const GLchar* message,
                                       const GLvoid* userParam);

// EGL types
typedef void* EGLImage;
typedef int EGLint;
typedef unsigned int EGLBoolean;
typedef unsigned int EGLenum;
typedef intptr_t EGLAttrib;
typedef void* EGLConfig;
typedef void* EGLContext;
typedef void* EGLDisplay;
typedef void* EGLDeviceEXT;
typedef void* EGLSurface;
typedef void* EGLClientBuffer;
typedef void* EGLCastToRelevantPtr;
typedef void* EGLImage;
typedef void* EGLSync;
typedef void* EGLStreamKHR;
typedef uint64_t EGLTime;

#define EGL_NO_CONTEXT       ((EGLContext)0)
#define EGL_NO_DISPLAY       ((EGLDisplay)0)
#define EGL_NO_SURFACE       ((EGLSurface)0)
#define EGL_NO_CONFIG        ((EGLConfig)nullptr)
#define EGL_NO_SYNC          ((EGLSync)0)
#define EGL_NO_IMAGE         ((EGLImage)0)


#ifdef XP_WIN
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN 1
    #endif

    #include <windef.h>

    typedef HDC EGLNativeDisplayType;
    typedef HBITMAP EGLNativePixmapType;
    typedef HWND EGLNativeWindowType;
#else
    typedef void* EGLNativeDisplayType;
    typedef void* EGLNativePixmapType;
    typedef void* EGLNativeWindowType;
#endif

#endif // GLTYPES_H_
