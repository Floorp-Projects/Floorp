/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
// vim:expandtab:ts=4 sw=4:
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
 * The Original Code is Mozilla.
 *
 * The Initial Developer of the Original Code is
 *   Mozilla Corporation
 * Portions created by the Initial Developer are Copyright (C) 2006
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

#include "nsServiceManagerUtils.h"
#include "nsIComponentManager.h"
#include "nsIGenericFactory.h"

#ifndef MOZILLA_1_8_BRANCH
#include "nsIClassInfoImpl.h"
#endif

#include "nsICanvasRenderingContextGLES11.h"
#include "nsICanvasRenderingContextGLWeb20.h"

// {21788585-e91c-448c-8bb9-6c51aba9735f}
#define NS_CANVASRENDERINGCONTEXTGLES11_CID \
{ 0x21788585, 0xe91c, 0x448c, { 0x8b, 0xb9, 0x6c, 0x51, 0xab, 0xa9, 0x73, 0x5f } }

// {8CB888F5-D754-4e5b-8C72-A5CC798CBB8F}
#define NS_CANVASRENDERINGCONTEXTGLWEB20_CID \
{ 0x8cb888f5, 0xd754, 0x4e5b, { 0x8c, 0x72, 0xa5, 0xcc, 0x79, 0x8c, 0xbb, 0x8f } }

nsresult NS_NewCanvasRenderingContextGLES11(nsICanvasRenderingContextGLES11** aResult);

nsresult NS_NewCanvasRenderingContextGLWeb20(nsICanvasRenderingContextGLWeb20** aResult);

#define MAKE_CTOR(ctor_, iface_, func_)                   \
static NS_IMETHODIMP                                      \
ctor_(nsISupports* aOuter, REFNSIID aIID, void** aResult) \
{                                                         \
  *aResult = nsnull;                                      \
  if (aOuter)                                             \
    return NS_ERROR_NO_AGGREGATION;                       \
  iface_* inst;                                           \
  nsresult rv = func_(&inst);                             \
  if (NS_SUCCEEDED(rv)) {                                 \
    rv = inst->QueryInterface(aIID, aResult);             \
    NS_RELEASE(inst);                                     \
  }                                                       \
  return rv;                                              \
}

NS_DECL_CLASSINFO(nsCanvasRenderingContextGLES11)
MAKE_CTOR(CreateCanvasRenderingContextGLES11, nsICanvasRenderingContextGLES11, NS_NewCanvasRenderingContextGLES11)

NS_DECL_CLASSINFO(nsCanvasRenderingContextGLWeb20)
MAKE_CTOR(CreateCanvasRenderingContextGLWeb20, nsICanvasRenderingContextGLWeb20, NS_NewCanvasRenderingContextGLWeb20)

static const nsModuleComponentInfo components[] = {
    { "GLES 1.1 Canvas Context",
      NS_CANVASRENDERINGCONTEXTGLES11_CID,
      "@mozilla.org/content/canvas-rendering-context;1?id=moz-gles11",
      CreateCanvasRenderingContextGLES11,
      nsnull, nsnull, nsnull,
      NS_CI_INTERFACE_GETTER_NAME(nsCanvasRenderingContextGLES11),
      nsnull, &NS_CLASSINFO_NAME(nsCanvasRenderingContextGLES11),
      nsIClassInfo::DOM_OBJECT
    },
    { "GLES 2.0 Canvas Context",
      NS_CANVASRENDERINGCONTEXTGLWEB20_CID,
      "@mozilla.org/content/canvas-rendering-context;1?id=moz-glweb20",
      CreateCanvasRenderingContextGLWeb20,
      nsnull, nsnull, nsnull,
      NS_CI_INTERFACE_GETTER_NAME(nsCanvasRenderingContextGLWeb20),
      nsnull, &NS_CLASSINFO_NAME(nsCanvasRenderingContextGLWeb20),
      nsIClassInfo::DOM_OBJECT
    },
};

NS_IMPL_NSGETMODULE(nsCanvas3DModule, components)

