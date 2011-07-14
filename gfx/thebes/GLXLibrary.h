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
 * The Initial Developer of the Original Code is Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Bas Schouten <bschouten@mozilla.com>
 *   Vladimir Vukicevic <vladimir@pobox.com>
 *   Matt Woodrow <mwoodrow@mozilla.com>
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

#ifndef GFX_GLXLIBRARY_H
#define GFX_GLXLIBRARY_H

#include "GLContext.h"
typedef realGLboolean GLboolean;
#include <GL/glx.h>

namespace mozilla {
namespace gl {

class GLXLibrary
{
public:
    GLXLibrary() : mInitialized(PR_FALSE), mTriedInitializing(PR_FALSE),
                   mHasTextureFromPixmap(PR_FALSE), mOGLLibrary(nsnull) {}

    typedef void (GLAPIENTRY * PFNGLXDESTROYCONTEXTPROC) (Display*,
                                                          GLXContext);
    PFNGLXDESTROYCONTEXTPROC xDestroyContext;
    typedef Bool (GLAPIENTRY * PFNGLXMAKECURRENTPROC) (Display*,
                                                       GLXDrawable,
                                                       GLXContext);
    PFNGLXMAKECURRENTPROC xMakeCurrent;
    typedef GLXContext (GLAPIENTRY * PFNGLXGETCURRENTCONTEXT) ();
    PFNGLXGETCURRENTCONTEXT xGetCurrentContext;
    typedef void* (GLAPIENTRY * PFNGLXGETPROCADDRESSPROC) (const char *);
    PFNGLXGETPROCADDRESSPROC xGetProcAddress;
    typedef GLXFBConfig* (GLAPIENTRY * PFNGLXCHOOSEFBCONFIG) (Display *,
                                                              int,
                                                              const int *,
                                                              int *);
    PFNGLXCHOOSEFBCONFIG xChooseFBConfig;
    typedef GLXFBConfig* (GLAPIENTRY * PFNGLXGETFBCONFIGS) (Display *,
                                                            int,
                                                            int *);
    PFNGLXGETFBCONFIGS xGetFBConfigs;
    typedef GLXContext (GLAPIENTRY * PFNGLXCREATENEWCONTEXT) (Display *,
                                                              GLXFBConfig,
                                                              int,
                                                              GLXContext,
                                                              Bool);
    PFNGLXCREATENEWCONTEXT xCreateNewContext;
    typedef XVisualInfo* (GLAPIENTRY * PFNGLXGETVISUALFROMFBCONFIG) (Display *,
                                                                     GLXFBConfig);
    PFNGLXGETVISUALFROMFBCONFIG xGetVisualFromFBConfig;
    typedef int (GLAPIENTRY * PFNGLXGETFBCONFIGATTRIB) (Display *, 
                                                        GLXFBConfig,
                                                        int,
                                                        int *);
    PFNGLXGETFBCONFIGATTRIB xGetFBConfigAttrib;

    typedef void (GLAPIENTRY * PFNGLXSWAPBUFFERS) (Display *,
                                                   GLXDrawable);
    PFNGLXSWAPBUFFERS xSwapBuffers;
    typedef const char * (GLAPIENTRY * PFNGLXQUERYEXTENSIONSSTRING) (Display *,
                                                                     int);
    PFNGLXQUERYEXTENSIONSSTRING xQueryExtensionsString;
    typedef const char * (GLAPIENTRY * PFNGLXGETCLIENTSTRING) (Display *,
                                                               int);
    PFNGLXGETCLIENTSTRING xGetClientString;
    typedef const char * (GLAPIENTRY * PFNGLXQUERYSERVERSTRING) (Display *,
                                                                 int,
                                                                 int);
    PFNGLXQUERYSERVERSTRING xQueryServerString;

    typedef GLXPixmap (GLAPIENTRY * PFNGLXCREATEPIXMAP) (Display *,
                                                         GLXFBConfig,
                                                         Pixmap,
                                                         const int *);
    PFNGLXCREATEPIXMAP xCreatePixmap;
    typedef GLXPixmap (GLAPIENTRY * PFNGLXCREATEGLXPIXMAPWITHCONFIG)
                                                        (Display *,
                                                         GLXFBConfig,
                                                         Pixmap);
    PFNGLXCREATEGLXPIXMAPWITHCONFIG xCreateGLXPixmapWithConfig;
    typedef void (GLAPIENTRY * PFNGLXDESTROYPIXMAP) (Display *,
                                                     GLXPixmap);
    PFNGLXDESTROYPIXMAP xDestroyPixmap;
    typedef GLXContext (GLAPIENTRY * PFNGLXCREATECONTEXT) (Display *,
                                                           XVisualInfo *,
                                                           GLXContext,
                                                           Bool);
    PFNGLXCREATECONTEXT xCreateContext;
    typedef Bool (GLAPIENTRY * PFNGLXQUERYVERSION) (Display *,
                                                    int *,
                                                    int *);
    PFNGLXQUERYVERSION xQueryVersion;

    typedef void (GLAPIENTRY * PFNGLXBINDTEXIMAGE) (Display *,
                                                    GLXDrawable,
                                                    int,
                                                    const int *);
    PFNGLXBINDTEXIMAGE xBindTexImage;

    typedef void (GLAPIENTRY * PFNGLXRELEASETEXIMAGE) (Display *,
                                                       GLXDrawable,
                                                       int);
    PFNGLXRELEASETEXIMAGE xReleaseTexImage;

    typedef void (GLAPIENTRY * PFNGLXWAITGL) ();
    PFNGLXWAITGL xWaitGL;

    PRBool EnsureInitialized();

    GLXPixmap CreatePixmap(gfxASurface* aSurface);
    void DestroyPixmap(GLXPixmap aPixmap);
    void BindTexImage(GLXPixmap aPixmap);
    void ReleaseTexImage(GLXPixmap aPixmap);

    PRBool HasTextureFromPixmap() { return mHasTextureFromPixmap; }
    PRBool SupportsTextureFromPixmap(gfxASurface* aSurface);

private:
    PRBool mInitialized;
    PRBool mTriedInitializing;
    PRBool mHasTextureFromPixmap;
    PRLibrary *mOGLLibrary;
};

// a global GLXLibrary instance
extern GLXLibrary sGLXLibrary;

} /* namespace gl */
} /* namespace mozilla */
#endif /* GFX_GLXLIBRARY_H */

