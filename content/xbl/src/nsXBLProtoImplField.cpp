/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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

#include "nsIAtom.h"
#include "nsString.h"
#include "jsapi.h"
#include "nsIContent.h"
#include "nsString.h"
#include "nsUnicharUtils.h"
#include "nsReadableUtils.h"
#include "nsXBLProtoImplField.h"
#include "nsIScriptContext.h"

MOZ_DECL_CTOR_COUNTER(nsXBLProtoImplField)

nsXBLProtoImplField::nsXBLProtoImplField(const PRUnichar* aName, const PRUnichar* aReadOnly)
  : nsXBLProtoImplMember(aName),
    mFieldText(nsnull),
    mFieldTextLength(0),
    mLineNumber(0)
{
  MOZ_COUNT_CTOR(nsXBLProtoImplField);
  mJSAttributes = JSPROP_ENUMERATE;
  if (aReadOnly) {
    nsAutoString readOnly; readOnly.Assign(*aReadOnly);
    if (readOnly.LowerCaseEqualsLiteral("true"))
      mJSAttributes |= JSPROP_READONLY;
  }
}

nsXBLProtoImplField::~nsXBLProtoImplField()
{
  MOZ_COUNT_DTOR(nsXBLProtoImplField);
  if (mFieldText)
    nsMemory::Free(mFieldText);
}

void
nsXBLProtoImplField::Destroy(PRBool aIsCompiled)
{
}

void 
nsXBLProtoImplField::AppendFieldText(const nsAString& aText)
{
  if (mFieldText) {
    nsDependentString fieldTextStr(mFieldText, mFieldTextLength);
    nsAutoString newFieldText = fieldTextStr + aText;
    PRUnichar* temp = mFieldText;
    mFieldText = ToNewUnicode(newFieldText);
    mFieldTextLength = newFieldText.Length();
    nsMemory::Free(temp);
  }
  else {
    mFieldText = ToNewUnicode(aText);
    mFieldTextLength = aText.Length();
  }
}

nsresult
nsXBLProtoImplField::InstallMember(nsIScriptContext* aContext,
                                   nsIContent* aBoundElement, 
                                   void* aScriptObject,
                                   void* aTargetClassObject,
                                   const nsCString& aClassStr)
{
  if (mFieldTextLength == 0)
    return NS_OK; // nothing to do.

  JSContext* cx = (JSContext*) aContext->GetNativeContext();
  JSObject * scriptObject = (JSObject *) aScriptObject;
  NS_ASSERTION(scriptObject, "uh-oh, script Object should NOT be null or bad things will happen");
  if (!scriptObject)
    return NS_ERROR_FAILURE;

  nsCAutoString bindingURI(aClassStr);
  PRInt32 hash = bindingURI.RFindChar('#');
  if (hash != kNotFound)
    bindingURI.Truncate(hash);
  
  // compile the literal string 
  jsval result = nsnull;
  PRBool undefined;
  // XXX Need a URI here!
  nsCOMPtr<nsIScriptContext> context = aContext;
  nsresult rv = context->EvaluateStringWithValue(nsDependentString(mFieldText,
                                                                   mFieldTextLength), 
                                                 scriptObject,
                                                 nsnull, bindingURI.get(),
                                                 mLineNumber, nsnull,
                                                 (void*) &result, &undefined);
  if (NS_FAILED(rv))
    return rv;

  if (!undefined) {
    // Define the evaluated result as a JS property
    nsDependentString name(mName);
    if (!::JS_DefineUCProperty(cx, scriptObject, NS_REINTERPRET_CAST(const jschar*, mName), 
                               name.Length(), result, nsnull, nsnull, mJSAttributes))
      return NS_ERROR_OUT_OF_MEMORY;
  }
  
  return NS_OK;
}

nsresult 
nsXBLProtoImplField::CompileMember(nsIScriptContext* aContext, const nsCString& aClassStr,
                                   void* aClassObject)
{
  return NS_OK;
}
