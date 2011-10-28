/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 *   Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2007
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Vladimir Vukicevic <vladimir@pobox.com> (original author)
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

#ifndef WEBGLEXTENSIONS_H_
#define WEBGLEXTENSIONS_H_

namespace mozilla {

class WebGLExtensionLoseContext;
class WebGLExtensionStandardDerivatives;

#define WEBGLEXTENSIONLOSECONTEXT_PRIVATE_IID \
    {0xb0afc2eb, 0x0895, 0x4509, {0x98, 0xde, 0x5c, 0x38, 0x3d, 0x16, 0x06, 0x94}}
class WebGLExtensionLoseContext :
    public nsIWebGLExtensionLoseContext,
    public WebGLExtension
{
public:
    WebGLExtensionLoseContext(WebGLContext*);
    virtual ~WebGLExtensionLoseContext();

    NS_DECL_ISUPPORTS
    NS_DECL_NSIWEBGLEXTENSIONLOSECONTEXT

    NS_DECLARE_STATIC_IID_ACCESSOR(WEBGLEXTENSIONLOSECONTEXT_PRIVATE_IID)
};

NS_DEFINE_STATIC_IID_ACCESSOR(WebGLExtensionLoseContext, WEBGLACTIVEINFO_PRIVATE_IID)

#define WEBGLEXTENSIONSTANDARDDERIVATIVES_PRIVATE_IID \
    {0x3de3dfd9, 0x864a, 0x4e4c, {0x98, 0x9b, 0x29, 0x77, 0xea, 0xa8, 0x0b, 0x7b}}
class WebGLExtensionStandardDerivatives :
    public nsIWebGLExtensionStandardDerivatives,
    public WebGLExtension
{
public:
    WebGLExtensionStandardDerivatives(WebGLContext* context);
    virtual ~WebGLExtensionStandardDerivatives();

    NS_DECL_ISUPPORTS
    NS_DECL_NSIWEBGLEXTENSION

    NS_DECLARE_STATIC_IID_ACCESSOR(WEBGLEXTENSIONSTANDARDDERIVATIVES_PRIVATE_IID)
};

NS_DEFINE_STATIC_IID_ACCESSOR(WebGLExtensionStandardDerivatives, WEBGLACTIVEINFO_PRIVATE_IID)

}

#endif // WEBGLEXTENSIONS_H_
