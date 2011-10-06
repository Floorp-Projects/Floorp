/* -*-  Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2; -*- */
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
 * The Mozilla Foundation <http://www.mozilla.org/>.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Dave Camp <dcamp@mozilla.com>
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

#include "JSDebugger.h"
#include "nsIXPConnect.h"
#include "nsThreadUtils.h"
#include "jsapi.h"
#include "jsobj.h"
#include "jsgc.h"
#include "jsfriendapi.h"
#include "jsdbgapi.h"
#include "mozilla/ModuleUtils.h"
#include "nsServiceManagerUtils.h"
#include "nsMemory.h"

#define JSDEBUGGER_CONTRACTID \
  "@mozilla.org/jsdebugger;1"

#define JSDEBUGGER_CID \
{ 0x0365cbd5, 0xd46e, 0x4e94, { 0xa3, 0x9f, 0x83, 0xb6, 0x3c, 0xd1, 0xa9, 0x63 } }

namespace mozilla {
namespace jsdebugger {

NS_GENERIC_FACTORY_CONSTRUCTOR(JSDebugger)

NS_IMPL_ISUPPORTS1(JSDebugger, IJSDebugger)

JSDebugger::JSDebugger()
{
}

JSDebugger::~JSDebugger()
{
}

NS_IMETHODIMP
JSDebugger::AddClass(JSContext *cx)
{
  nsresult rv;
  nsCOMPtr<nsIXPConnect> xpc = do_GetService(nsIXPConnect::GetCID(), &rv);

  JSObject* global = JS_GetGlobalForScopeChain(cx);
  if (!global) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  if (!JS_DefineDebuggerObject(cx, global)) {
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

}
}

NS_DEFINE_NAMED_CID(JSDEBUGGER_CID);

static const mozilla::Module::CIDEntry kJSDebuggerCIDs[] = {
  { &kJSDEBUGGER_CID, false, NULL, mozilla::jsdebugger::JSDebuggerConstructor },
  { NULL }
};

static const mozilla::Module::ContractIDEntry kJSDebuggerContracts[] = {
  { JSDEBUGGER_CONTRACTID, &kJSDEBUGGER_CID },
  { NULL }
};

static const mozilla::Module kJSDebuggerModule = {
  mozilla::Module::kVersion,
  kJSDebuggerCIDs,
  kJSDebuggerContracts
};

NSMODULE_DEFN(jsdebugger) = &kJSDebuggerModule;
