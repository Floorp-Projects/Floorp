/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   David Hyatt <hyatt@netscape.com> (Original Author)
 *
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsXBLProtoImplMember.h"
#include "jsapi.h"

// statics get inited to 0 by default
/* static */
nsIJSRuntimeService* nsXBLProtoImplMember::gJSRuntimeService;
/* static */
JSRuntime* nsXBLProtoImplMember::gScriptRuntime;
/* static */
PRInt32 nsXBLProtoImplMember::gScriptRuntimeRefcnt;

/* static */
nsresult
nsXBLProtoImplMember::AddJSGCRoot(void* aScriptObjectRef, const char* aName)
{
  if (++gScriptRuntimeRefcnt == 1 || !gScriptRuntime) {
    CallGetService("@mozilla.org/js/xpc/RuntimeService;1",
                   &gJSRuntimeService);
    if (! gJSRuntimeService) {
      NS_NOTREACHED("couldn't add GC root");
      return NS_ERROR_FAILURE;
    }

    gJSRuntimeService->GetRuntime(&gScriptRuntime);
    if (! gScriptRuntime) {
      NS_NOTREACHED("couldn't add GC root");
      return NS_ERROR_FAILURE;
    }
  }

  PRBool ok;
  ok = ::JS_AddNamedRootRT(gScriptRuntime, aScriptObjectRef, aName);
  if (! ok) {
    NS_NOTREACHED("couldn't add GC root");
    return NS_ERROR_OUT_OF_MEMORY;
  }

  return NS_OK;
}

/* static */
nsresult
nsXBLProtoImplMember::RemoveJSGCRoot(void* aScriptObjectRef)
{
  if (!gScriptRuntime) {
    NS_NOTREACHED("couldn't remove GC root");
    return NS_ERROR_FAILURE;
  }

  ::JS_RemoveRootRT(gScriptRuntime, aScriptObjectRef);

  if (--gScriptRuntimeRefcnt == 0) {
    NS_RELEASE(gJSRuntimeService);
    gScriptRuntime = nsnull;
  }

  return NS_OK;
}
