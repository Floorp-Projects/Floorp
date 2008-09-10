/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 sw=2 et tw=78: */
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
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Vidur Apparao <vidur@netscape.com>
 *   L. David Baron <dbaron@mozillafoundation.org>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

/**
 * This is not a generated file. It contains common utility functions
 * invoked from the JavaScript code generated from IDL interfaces.
 * The goal of the utility functions is to cut down on the size of
 * the generated code itself.
 */

#include "nsJSUtils.h"
#include "jsapi.h"
#include "jsdbgapi.h"
#include "prprf.h"
#include "nsIScriptContext.h"
#include "nsIScriptObjectOwner.h"
#include "nsIScriptGlobalObject.h"
#include "nsIServiceManager.h"
#include "nsIXPConnect.h"
#include "nsCOMPtr.h"
#include "nsContentUtils.h"
#include "nsIScriptSecurityManager.h"

#include "nsDOMJSUtils.h" // for GetScriptContextFromJSContext

JSBool
nsJSUtils::GetCallingLocation(JSContext* aContext, const char* *aFilename,
                              PRUint32* aLineno, nsIPrincipal* aPrincipal)
{
  // Get the current filename and line number
  JSStackFrame* frame = nsnull;
  JSScript* script = nsnull;
  do {
    frame = ::JS_FrameIterator(aContext, &frame);

    if (frame) {
      script = ::JS_GetFrameScript(aContext, frame);
    }
  } while (frame && !script);

  if (script) {
    // If aPrincipals is non-null then our caller is asking us to ensure
    // that the filename we return does not have elevated privileges.
    if (aPrincipal) {
      uint32 flags = JS_GetScriptFilenameFlags(script);

      // Use the principal for the filename if it shouldn't be receiving
      // implicit XPCNativeWrappers.
      PRBool system;
      if (flags & JSFILENAME_PROTECTED) {
        nsIScriptSecurityManager *ssm = nsContentUtils::GetSecurityManager();

        if (NS_FAILED(ssm->IsSystemPrincipal(aPrincipal, &system)) || !system) {
          JSPrincipals* jsprins;
          aPrincipal->GetJSPrincipals(aContext, &jsprins);

          *aFilename = jsprins->codebase;
          *aLineno = 0;
          JSPRINCIPALS_DROP(aContext, jsprins);
          return JS_TRUE;
        }
      }
    }

    const char* filename = ::JS_GetScriptFilename(aContext, script);

    if (filename) {
      PRUint32 lineno = 0;
      jsbytecode* bytecode = ::JS_GetFramePC(aContext, frame);

      if (bytecode) {
        lineno = ::JS_PCToLineNumber(aContext, script, bytecode);
      }

      *aFilename = filename;
      *aLineno = lineno;
      return JS_TRUE;
    }
  }

  return JS_FALSE;
}

nsIScriptGlobalObject *
nsJSUtils::GetStaticScriptGlobal(JSContext* aContext, JSObject* aObj)
{
  nsISupports* supports;
  JSClass* clazz;
  JSObject* parent;
  JSObject* glob = aObj; // starting point for search

  if (!glob)
    return nsnull;

  while ((parent = ::JS_GetParent(aContext, glob)))
    glob = parent;

  clazz = JS_GET_CLASS(aContext, glob);

  if (!clazz ||
      !(clazz->flags & JSCLASS_HAS_PRIVATE) ||
      !(clazz->flags & JSCLASS_PRIVATE_IS_NSISUPPORTS) ||
      !(supports = (nsISupports*)::JS_GetPrivate(aContext, glob))) {
    return nsnull;
  }

  nsCOMPtr<nsIXPConnectWrappedNative> wrapper(do_QueryInterface(supports));
  NS_ENSURE_TRUE(wrapper, nsnull);

  nsCOMPtr<nsIScriptGlobalObject> sgo(do_QueryWrappedNative(wrapper));

  // We're returning a pointer to something that's about to be
  // released, but that's ok here.
  return sgo;
}

nsIScriptContext *
nsJSUtils::GetStaticScriptContext(JSContext* aContext, JSObject* aObj)
{
  nsIScriptGlobalObject *nativeGlobal = GetStaticScriptGlobal(aContext, aObj);
  if (!nativeGlobal)
    return nsnull;

  return nativeGlobal->GetScriptContext(nsIProgrammingLanguage::JAVASCRIPT);
}

nsIScriptGlobalObject *
nsJSUtils::GetDynamicScriptGlobal(JSContext* aContext)
{
  nsIScriptContext *scriptCX = GetDynamicScriptContext(aContext);
  if (!scriptCX)
    return nsnull;
  return scriptCX->GetGlobalObject();
}

nsIScriptContext *
nsJSUtils::GetDynamicScriptContext(JSContext *aContext)
{
  return GetScriptContextFromJSContext(aContext);
}
