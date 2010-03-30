/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Corporation code.
 *
 * The Initial Developer of the Original Code is Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Bas Schouten <bschouten@mozilla.org>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef GFX_GLWRAPPER_H
#define GFX_GLWRAPPER_H

#ifdef XP_WIN
#include <windows.h>
#endif
#include "prlink.h"

#include "glDefs.h"

struct OGLFunction;

class glWrapper
{
public:
  glWrapper();

  typedef void (GLAPIENTRY * PFNGLBLENDFUNCSEPARATEPROC) (GLenum,
                                                        GLenum,
                                                        GLenum,
                                                        GLenum);
  
  typedef void (GLAPIENTRY * PFNGLENABLEPROC) (GLenum);
  typedef void (GLAPIENTRY * PFNGLENABLECLIENTSTATEPROC) (GLenum);
  typedef void (GLAPIENTRY * PFNGLENABLEVERTEXATTRIBARRAYPROC) (GLuint);
  typedef void (GLAPIENTRY * PFNGLDISABLEPROC) (GLenum);
  typedef void (GLAPIENTRY * PFNGLDISABLECLIENTSTATEPROC) (GLenum);
  typedef void (GLAPIENTRY * PFNGLDISABLEVERTEXATTRIBARRAYPROC) (GLuint);

  typedef void (GLAPIENTRY * PFNGLDRAWARRAYSPROC) (GLenum, GLint, GLsizei);

  typedef void (GLAPIENTRY * PFNGLFINISHPROC) (void);
  typedef void (GLAPIENTRY * PFNGLCLEARPROC) (GLbitfield);
  typedef void (GLAPIENTRY * PFNGLSCISSORPROC) (GLint, GLint, GLsizei, GLsizei);
  typedef void (GLAPIENTRY * PFNGLVIEWPORTPROC) (GLint, GLint, GLsizei, GLsizei);
  typedef void (GLAPIENTRY * PFNGLCLEARCOLORPROC) (GLclampf,
                                                 GLclampf,
                                                 GLclampf,
                                                 GLclampf);
  typedef void (GLAPIENTRY * PFNGLREADBUFFERPROC) (GLenum);
  typedef void (GLAPIENTRY * PFNGLREADPIXELSPROC) (GLint,
                                                 GLint,
                                                 GLsizei,
                                                 GLsizei,
                                                 GLenum,
                                                 GLenum,
                                                 GLvoid *);

  typedef void (GLAPIENTRY * PFNGLTEXENVFPROC) (GLenum, GLenum, GLfloat);
  typedef void (GLAPIENTRY * PFNGLTEXPARAMETERIPROC) (GLenum, GLenum, GLint);
  typedef void (GLAPIENTRY * PFNGLACTIVETEXTUREPROC) (GLenum);
  typedef void (GLAPIENTRY * PFNGLPIXELSTOREIPROC) (GLenum, GLint);

  typedef void (GLAPIENTRY * PFNGLGENTEXTURESPROC) (GLsizei, GLuint *);
  typedef void (GLAPIENTRY * PFNGLGENBUFFERSPROC) (GLsizei, GLuint *);
  typedef void (GLAPIENTRY * PFNGLGENFRAMEBUFFERSEXTPROC) (GLsizei, GLuint *);
  typedef void (GLAPIENTRY * PFNGLDELETETEXTURESPROC) (GLsizei, const GLuint *);
  typedef void (GLAPIENTRY * PFNGLDELETEFRAMEBUFFERSEXTPROC) (GLsizei,
                                                            const GLuint *);
  
  typedef void (GLAPIENTRY * PFNGLBINDTEXTUREPROC) (GLenum, GLuint);
  typedef void (GLAPIENTRY * PFNGLBINDBUFFERPROC) (GLenum, GLuint);
  typedef void (GLAPIENTRY * PFNGLBINDFRAMEBUFFEREXTPROC) (GLenum, GLuint);

  typedef void (GLAPIENTRY * PFNGLFRAMEBUFFERTEXTURE2DEXTPROC) (GLenum,
                                                              GLenum,
                                                              GLenum,
                                                              GLuint,
                                                              GLint); 

  typedef void (GLAPIENTRY * PFNGLBUFFERDATAPROC) (GLenum,
                                                 GLsizeiptr,
                                                 const GLvoid *,
                                                 GLenum);

  typedef void (GLAPIENTRY * PFNGLVERTEXPOINTERPROC) (GLint,
                                                    GLenum,
                                                    GLsizei,
                                                    const GLvoid *);
  typedef void (GLAPIENTRY * PFNGLTEXCOORDPOINTERPROC) (GLint,
                                                      GLenum,
                                                      GLsizei,
                                                      const GLvoid *);

  typedef void (GLAPIENTRY * PFNGLTEXIMAGE2DPROC) (GLenum,
                                                 GLint,
                                                 GLint,
                                                 GLsizei,
                                                 GLsizei,
                                                 GLint,
                                                 GLenum,
                                                 GLenum,
                                                 const GLvoid *);
  typedef void (GLAPIENTRY * PFNGLTEXSUBIMAGE2DPROC) (GLenum,
                                                    GLint,
                                                    GLint,
                                                    GLint,
                                                    GLsizei,
                                                    GLsizei,
                                                    GLenum,
                                                    GLenum,
                                                    const GLvoid *);

  typedef GLuint (GLAPIENTRY * PFNGLCREATESHADERPROC) (GLenum);
  typedef GLuint (GLAPIENTRY * PFNGLCREATEPROGRAMPROC) (void);
  typedef void (GLAPIENTRY * PFNGLDELETEPROGRAMPROC) (GLuint);
  typedef void (GLAPIENTRY * PFNGLUSEPROGRAMPROC) (GLuint);
  typedef void (GLAPIENTRY * PFNGLSHADERSOURCEPROC) (GLuint,
                                                   GLsizei,
                                                   const GLchar **,
                                                   const GLint *);
  typedef void (GLAPIENTRY * PFNGLCOMPILESHADERPROC) (GLuint);
  typedef void (GLAPIENTRY * PFNGLATTACHSHADERPROC) (GLuint, GLuint);
  typedef void (GLAPIENTRY * PFNGLLINKPROGRAMPROC) (GLuint);
  typedef void (GLAPIENTRY * PFNGLGETPROGRAMIVPROC) (GLuint, GLenum, GLint *);
  typedef void (GLAPIENTRY * PFNGLGETSHADERIVPROC) (GLuint, GLenum, GLint *);

  typedef void (GLAPIENTRY * PFNGLBINDATTRIBLOCATIONPROC) (GLuint,
                                                         GLuint,
                                                         const GLchar *);
  typedef void (GLAPIENTRY * PFNGLVERTEXATTRIBPOINTERPROC) (GLuint,
                                                          GLint,
                                                          GLenum,
                                                          GLboolean,
                                                          GLsizei,
                                                          const GLvoid *);

  typedef void (GLAPIENTRY * PFNGLUNIFORM1IPROC) (GLint, GLint);
  typedef void (GLAPIENTRY * PFNGLUNIFORM1FPROC) (GLint, GLfloat);
  typedef void (GLAPIENTRY * PFNGLUNIFORM4FPROC) (GLint,
                                                GLfloat,
                                                GLfloat,
                                                GLfloat,
                                                GLfloat);
  typedef void (GLAPIENTRY * PFNGLUNIFORMMATRIX4FVPROC) (GLint,
                                                       GLsizei,
                                                       GLboolean,
                                                       const GLfloat *);
  typedef GLint (GLAPIENTRY * PFNGLGETUNIFORMLOCATIONPROC) (GLuint,
                                                          const GLchar *);

  typedef GLubyte* (GLAPIENTRY * PFNGLGETSTRINGPROC) (GLenum);

#ifdef XP_WIN
  typedef HGLRC (GLAPIENTRY * PFNWGLCREATECONTEXTPROC) (HDC);
  typedef BOOL (GLAPIENTRY * PFNWGLDELETECONTEXTPROC) (HGLRC);
  typedef BOOL (GLAPIENTRY * PFNWGLMAKECURRENTPROC) (HDC, HGLRC);
  typedef PROC (GLAPIENTRY * PFNWGLGETPROCADDRESSPROC) (LPCSTR);
#endif

  PFNGLBLENDFUNCSEPARATEPROC BlendFuncSeparate;
  
  PFNGLENABLEPROC Enable;
  PFNGLENABLECLIENTSTATEPROC EnableClientState;
  PFNGLENABLEVERTEXATTRIBARRAYPROC EnableVertexAttribArray;
  PFNGLDISABLEPROC Disable;
  PFNGLDISABLECLIENTSTATEPROC DisableClientState;
  PFNGLDISABLEVERTEXATTRIBARRAYPROC DisableVertexAttribArray;

  PFNGLDRAWARRAYSPROC DrawArrays;

  PFNGLFINISHPROC Finish;
  PFNGLCLEARPROC Clear;
  PFNGLSCISSORPROC Scissor;
  PFNGLVIEWPORTPROC Viewport;
  PFNGLCLEARCOLORPROC ClearColor;
  PFNGLREADBUFFERPROC ReadBuffer;
  PFNGLREADPIXELSPROC ReadPixels;

  PFNGLTEXENVFPROC TexEnvf;
  PFNGLTEXPARAMETERIPROC TexParameteri;
  PFNGLACTIVETEXTUREPROC ActiveTexture;
  PFNGLPIXELSTOREIPROC PixelStorei;

  PFNGLGENTEXTURESPROC GenTextures;
  PFNGLGENBUFFERSPROC GenBuffers;
  PFNGLGENFRAMEBUFFERSEXTPROC GenFramebuffersEXT;
  PFNGLDELETETEXTURESPROC DeleteTextures;
  PFNGLDELETEFRAMEBUFFERSEXTPROC DeleteFramebuffersEXT;
  
  PFNGLBINDTEXTUREPROC BindTexture;
  PFNGLBINDBUFFERPROC BindBuffer;
  PFNGLBINDFRAMEBUFFEREXTPROC BindFramebufferEXT;

  PFNGLFRAMEBUFFERTEXTURE2DEXTPROC FramebufferTexture2DEXT; 

  PFNGLBUFFERDATAPROC BufferData;

  PFNGLVERTEXPOINTERPROC VertexPointer;
  PFNGLTEXCOORDPOINTERPROC TexCoordPointer;

  PFNGLTEXIMAGE2DPROC TexImage2D;
  PFNGLTEXSUBIMAGE2DPROC TexSubImage2D;

  PFNGLCREATESHADERPROC CreateShader;
  PFNGLCREATEPROGRAMPROC CreateProgram;
  PFNGLDELETEPROGRAMPROC DeleteProgram;
  PFNGLUSEPROGRAMPROC UseProgram;
  PFNGLSHADERSOURCEPROC ShaderSource;
  PFNGLCOMPILESHADERPROC CompileShader;
  PFNGLATTACHSHADERPROC AttachShader;
  PFNGLLINKPROGRAMPROC LinkProgram;
  PFNGLGETPROGRAMIVPROC GetProgramiv;
  PFNGLGETSHADERIVPROC GetShaderiv;

  PFNGLBINDATTRIBLOCATIONPROC BindAttribLocation;
  PFNGLVERTEXATTRIBPOINTERPROC VertexAttribPointer;

  PFNGLUNIFORM1IPROC Uniform1i;
  PFNGLUNIFORM1FPROC Uniform1f;
  PFNGLUNIFORM4FPROC Uniform4f;
  PFNGLUNIFORMMATRIX4FVPROC UniformMatrix4fv;
  PFNGLGETUNIFORMLOCATIONPROC GetUniformLocation;

  PFNGLGETSTRINGPROC GetString;

#ifdef XP_WIN
  /**
   * We intercept this call and do some stuff (like load the wglCreateContext
   * and ensure our other function pointers are initialized.
   */
  HGLRC wCreateContext(HDC aDC);
  PFNWGLCREATECONTEXTPROC wCreateContextInternal;
  PFNWGLDELETECONTEXTPROC wDeleteContext;
  PFNWGLMAKECURRENTPROC wMakeCurrent;
  PFNWGLGETPROCADDRESSPROC wGetProcAddress;
#endif

private:
  PRBool EnsureInitialized();

  PRBool LoadSymbols(OGLFunction *functions);

  PRBool mIsInitialized;
  PRLibrary *mOGLLibrary;
};

extern glWrapper sglWrapper;

#endif
