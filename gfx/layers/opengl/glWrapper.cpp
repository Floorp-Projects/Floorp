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

#include "glWrapper.h"
#include "nsDebug.h"

#ifdef XP_WIN
#define glGetProcAddress wGetProcAddress
#else
// We do not know how to glGetProcAddress on this platform.
PRFuncPtr glGetProcAddress(const char *)
{
  return NULL;
}
#endif

glWrapper sglWrapper;

struct OGLFunction {
  PRFuncPtr *functor;
  const char *name;
};

glWrapper::glWrapper()
  : mIsInitialized(PR_FALSE)
  , mOGLLibrary(NULL)
{
}

#ifdef XP_WIN
HGLRC
glWrapper::wCreateContext(HDC aDC)
{
  if (!wCreateContextInternal) {
    if (!mOGLLibrary) {
#ifdef XP_WIN
      mOGLLibrary = PR_LoadLibrary("Opengl32.dll");
#else
      return NULL;
#endif
      if (!mOGLLibrary) {
        NS_WARNING("Couldn't load OpenGL DLL.");
        return NULL;
      }
    }
    
    wCreateContextInternal = (PFNWGLCREATECONTEXTPROC)
      PR_FindSymbol(mOGLLibrary, "wglCreateContext");
    wDeleteContext = (PFNWGLDELETECONTEXTPROC)
      PR_FindSymbol(mOGLLibrary, "wglDeleteContext");
    wMakeCurrent = (PFNWGLMAKECURRENTPROC)
      PR_FindSymbol(mOGLLibrary, "wglMakeCurrent");
    wGetProcAddress = (PFNWGLGETPROCADDRESSPROC)
      PR_FindSymbol(mOGLLibrary, "wglGetProcAddress");
  }
  HGLRC retval = wCreateContextInternal(aDC);

  if (!retval) {
    return retval;
  }
  wMakeCurrent(aDC, retval);

  if (!EnsureInitialized()) {
    NS_WARNING("Failed to initialize OpenGL wrapper.");
    return NULL;
  }

  return retval;
}
#endif

PRBool
glWrapper::LoadSymbols(OGLFunction *functions)
{
  for (int i = 0; functions[i].functor; i++) {
    *functions[i].functor =
      PR_FindFunctionSymbol(mOGLLibrary, functions[i].name);

    if (*functions[i].functor) {
      continue;
    }

    if (!glGetProcAddress) {
      return PR_FALSE;
    }
    *functions[i].functor = (PRFuncPtr)glGetProcAddress(functions[i].name);
    if (!*functions[i].functor) {
      return PR_FALSE;
    }
  }

  return PR_TRUE;
}

PRBool
glWrapper::EnsureInitialized()
{
  if (mIsInitialized) {
    return PR_TRUE;
  }

  OGLFunction functions[] = {

    { (PRFuncPtr*) &BlendFuncSeparate, "glBlendFuncSeparate" },
    
    { (PRFuncPtr*) &Enable, "glEnable" },
    { (PRFuncPtr*) &EnableClientState, "glEnableClientState" },
    { (PRFuncPtr*) &EnableVertexAttribArray, "glEnableVertexAttribArray" },
    { (PRFuncPtr*) &Disable, "glDisable" },
    { (PRFuncPtr*) &DisableClientState, "glDisableClientState" },
    { (PRFuncPtr*) &DisableVertexAttribArray, "glDisableVertexAttribArray" },

    { (PRFuncPtr*) &DrawArrays, "glDrawArrays" },

    { (PRFuncPtr*) &Finish, "glFinish" },
    { (PRFuncPtr*) &Clear, "glClear" },
    { (PRFuncPtr*) &Scissor, "glScissor" },
    { (PRFuncPtr*) &Viewport, "glViewport" },
    { (PRFuncPtr*) &ClearColor, "glClearColor" },
    { (PRFuncPtr*) &ReadBuffer, "glReadBuffer" },
    { (PRFuncPtr*) &ReadPixels, "glReadPixels" },

    { (PRFuncPtr*) &TexEnvf, "glTexEnvf" },
    { (PRFuncPtr*) &TexParameteri, "glTexParameteri" },
    { (PRFuncPtr*) &ActiveTexture, "glActiveTexture" },
    { (PRFuncPtr*) &PixelStorei, "glPixelStorei" },

    { (PRFuncPtr*) &GenTextures, "glGenTextures" },
    { (PRFuncPtr*) &GenBuffers, "glGenBuffers" },
    { (PRFuncPtr*) &GenFramebuffersEXT, "glGenFramebuffersEXT" },
    { (PRFuncPtr*) &DeleteTextures, "glDeleteTextures" },
    { (PRFuncPtr*) &DeleteFramebuffersEXT, "glDeleteFramebuffersEXT" },
    
    { (PRFuncPtr*) &BindTexture, "glBindTexture" },
    { (PRFuncPtr*) &BindBuffer, "glBindBuffer" },
    { (PRFuncPtr*) &BindFramebufferEXT, "glBindFramebufferEXT" },

    { (PRFuncPtr*) &FramebufferTexture2DEXT, "glFramebufferTexture2DEXT" }, 

    { (PRFuncPtr*) &BufferData, "glBufferData" },

    { (PRFuncPtr*) &VertexPointer, "glVertexPointer" },
    { (PRFuncPtr*) &TexCoordPointer, "glTexCoordPointer" },

    { (PRFuncPtr*) &TexImage2D, "glTexImage2D" },
    { (PRFuncPtr*) &TexSubImage2D, "glTexSubImage2D" },

    { (PRFuncPtr*) &CreateShader, "glCreateShader" },
    { (PRFuncPtr*) &CreateProgram, "glCreateProgram" },
    { (PRFuncPtr*) &DeleteProgram, "glDeleteProgram" },
    { (PRFuncPtr*) &UseProgram, "glUseProgram" },
    { (PRFuncPtr*) &ShaderSource, "glShaderSource" },
    { (PRFuncPtr*) &CompileShader, "glCompileShader" },
    { (PRFuncPtr*) &AttachShader, "glAttachShader" },
    { (PRFuncPtr*) &LinkProgram, "glLinkProgram" },
    { (PRFuncPtr*) &GetProgramiv, "glGetProgramiv" },
    { (PRFuncPtr*) &GetShaderiv, "glGetShaderiv" },

    { (PRFuncPtr*) &BindAttribLocation, "glBindAttribLocation" },
    { (PRFuncPtr*) &VertexAttribPointer, "glVertexAttribPointer" },

    { (PRFuncPtr*) &Uniform1i, "glUniform1i" },
    { (PRFuncPtr*) &Uniform1f, "glUniform1f" },
    { (PRFuncPtr*) &Uniform4f, "glUniform4f" },
    { (PRFuncPtr*) &UniformMatrix4fv, "glUniformMatrix4fv" },
    { (PRFuncPtr*) &GetUniformLocation, "glGetUniformLocation" },

    { (PRFuncPtr*) &GetString, "glGetString" },

    { NULL, NULL }

  };

  if (!LoadSymbols(functions)) {
    return PR_FALSE;
  }

  mIsInitialized = PR_TRUE;

  return PR_TRUE;
}
