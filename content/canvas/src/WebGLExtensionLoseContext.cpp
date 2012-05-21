/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebGLContext.h"
#include "WebGLExtensions.h"

using namespace mozilla;

WebGLExtensionLoseContext::WebGLExtensionLoseContext(WebGLContext* context) :
    WebGLExtension(context)
{

}

WebGLExtensionLoseContext::~WebGLExtensionLoseContext()
{

}

NS_IMETHODIMP 
WebGLExtensionLoseContext::LoseContext()
{
    if (!mContext->LoseContext())
        mContext->mWebGLError = LOCAL_GL_INVALID_OPERATION;

    return NS_OK;
}

NS_IMETHODIMP 
WebGLExtensionLoseContext::RestoreContext()
{
    if (!mContext->RestoreContext())
        mContext->mWebGLError = LOCAL_GL_INVALID_OPERATION;

    return NS_OK;
}

NS_IMPL_ADDREF_INHERITED(WebGLExtensionLoseContext, WebGLExtension)
NS_IMPL_RELEASE_INHERITED(WebGLExtensionLoseContext, WebGLExtension)

DOMCI_DATA(WebGLExtensionLoseContext, WebGLExtensionLoseContext)

NS_INTERFACE_MAP_BEGIN(WebGLExtensionLoseContext)
  NS_INTERFACE_MAP_ENTRY(nsIWebGLExtensionLoseContext)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, WebGLExtension)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(WebGLExtensionLoseContext)
NS_INTERFACE_MAP_END_INHERITING(WebGLExtension)
