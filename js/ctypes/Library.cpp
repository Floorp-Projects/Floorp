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
 * The Original Code is js-ctypes.
 *
 * The Initial Developer of the Original Code is
 * The Mozilla Foundation <http://www.mozilla.org/>.
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Mark Finkle <mark.finkle@gmail.com>, <mfinkle@mozilla.com>
 *  Fredrik Larsson <nossralf@gmail.com>
 *  Dan Witte <dwitte@mozilla.com>
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

#include "Library.h"
#include "Function.h"
#include "nsServiceManagerUtils.h"
#include "nsAutoPtr.h"
#include "nsILocalFile.h"
#include "prlink.h"
#include "jsapi.h"

namespace mozilla {
namespace ctypes {

static inline nsresult
jsvalToUint16(JSContext* aContext, jsval aVal, PRUint16& aResult)
{
  if (JSVAL_IS_INT(aVal)) {
    PRUint32 i = JSVAL_TO_INT(aVal);
    if (i <= PR_UINT16_MAX) {
      aResult = i;
      return NS_OK;
    }
  }

  JS_ReportError(aContext, "Parameter must be an integer");
  return NS_ERROR_INVALID_ARG;
}

static inline nsresult
jsvalToCString(JSContext* aContext, jsval aVal, const char*& aResult)
{
  if (JSVAL_IS_STRING(aVal)) {
    aResult = JS_GetStringBytes(JSVAL_TO_STRING(aVal));
    return NS_OK;
  }

  JS_ReportError(aContext, "Parameter must be a string");
  return NS_ERROR_INVALID_ARG;
}

NS_IMPL_ISUPPORTS1(Library, nsIForeignLibrary)

Library::Library()
  : mLibrary(nsnull)
{
}

Library::~Library()
{
  Close();
}

NS_IMETHODIMP
Library::Open(nsILocalFile* aFile)
{
  NS_ENSURE_ARG(aFile);
  NS_ENSURE_TRUE(!mLibrary, NS_ERROR_ALREADY_INITIALIZED);

  return aFile->Load(&mLibrary);
}

NS_IMETHODIMP
Library::Close()
{
  if (mLibrary) {
    PR_UnloadLibrary(mLibrary);
    mLibrary = nsnull;
  }

  return NS_OK;
}

NS_IMETHODIMP
Library::Declare(nsISupports** aResult)
{
  NS_ENSURE_ARG_POINTER(aResult);
  NS_ENSURE_TRUE(mLibrary, NS_ERROR_NOT_INITIALIZED);

  nsresult rv;

  nsCOMPtr<nsIXPConnect> xpc = do_GetService(nsIXPConnect::GetCID());

  nsAXPCNativeCallContext* ncc;
  rv = xpc->GetCurrentNativeCallContext(&ncc);
  NS_ENSURE_SUCCESS(rv, rv);

  JSContext *ctx;
  rv = ncc->GetJSContext(&ctx);
  NS_ENSURE_SUCCESS(rv, rv);

  JSAutoRequest ar(ctx);

  PRUint32 argc;
  jsval *argv;
  ncc->GetArgc(&argc);
  ncc->GetArgvPtr(&argv);

  // we always need at least a method name, a call type and a return type
  if (argc < 3) {
    JS_ReportError(ctx, "Insufficient number of arguments");
    return NS_ERROR_INVALID_ARG;
  }

  const char* name;
  rv = jsvalToCString(ctx, argv[0], name);
  NS_ENSURE_SUCCESS(rv, rv);

  PRUint16 callType;
  rv = jsvalToUint16(ctx, argv[1], callType);
  NS_ENSURE_SUCCESS(rv, rv);

  nsAutoTArray<jsval, 16> argTypes;
  for (PRUint32 i = 3; i < argc; ++i) {
    argTypes.AppendElement(argv[i]);
  }

  PRFuncPtr func = PR_FindFunctionSymbol(mLibrary, name);
  if (!func) {
    JS_ReportError(ctx, "Couldn't find function symbol in library");
    return NS_ERROR_FAILURE;
  }

  nsRefPtr<Function> call = new Function;
  rv = call->Init(ctx, this, func, callType, argv[2], argTypes);
  NS_ENSURE_SUCCESS(rv, rv);

  call.forget(aResult);
  return rv;
}

}
}

