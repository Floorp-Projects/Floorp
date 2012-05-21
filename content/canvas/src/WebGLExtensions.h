/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WEBGLEXTENSIONS_H_
#define WEBGLEXTENSIONS_H_

namespace mozilla {

class WebGLExtensionLoseContext :
    public nsIWebGLExtensionLoseContext,
    public WebGLExtension
{
public:
    WebGLExtensionLoseContext(WebGLContext*);
    virtual ~WebGLExtensionLoseContext();

    NS_DECL_ISUPPORTS_INHERITED
    NS_DECL_NSIWEBGLEXTENSIONLOSECONTEXT
};

class WebGLExtensionStandardDerivatives :
    public nsIWebGLExtensionStandardDerivatives,
    public WebGLExtension
{
public:
    WebGLExtensionStandardDerivatives(WebGLContext* context);
    virtual ~WebGLExtensionStandardDerivatives();

    NS_DECL_ISUPPORTS_INHERITED
    NS_DECL_NSIWEBGLEXTENSION
};

class WebGLExtensionTextureFilterAnisotropic :
    public nsIWebGLExtensionTextureFilterAnisotropic,
    public WebGLExtension
{
public:
    WebGLExtensionTextureFilterAnisotropic(WebGLContext* context);
    virtual ~WebGLExtensionTextureFilterAnisotropic();

    NS_DECL_ISUPPORTS_INHERITED
    NS_DECL_NSIWEBGLEXTENSION
};

class WebGLExtensionCompressedTextureS3TC :
    public nsIWebGLExtensionCompressedTextureS3TC,
    public WebGLExtension
{
public:
    WebGLExtensionCompressedTextureS3TC(WebGLContext* context);
    virtual ~WebGLExtensionCompressedTextureS3TC();

    NS_DECL_ISUPPORTS_INHERITED
    NS_DECL_NSIWEBGLEXTENSION
};

}

#endif // WEBGLEXTENSIONS_H_
