/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsIAtom.h"
#include "nsString.h"
#include "jsapi.h"
#include "nsIContent.h"
#include "nsUnicharUtils.h"
#include "nsReadableUtils.h"
#include "mozilla/FunctionTimer.h"
#include "nsXBLProtoImplField.h"
#include "nsIScriptContext.h"
#include "nsContentUtils.h"
#include "nsIURI.h"
#include "nsXBLSerialize.h"
#include "nsXBLPrototypeBinding.h"

nsXBLProtoImplField::nsXBLProtoImplField(const PRUnichar* aName, const PRUnichar* aReadOnly)
  : mNext(nsnull),
    mFieldText(nsnull),
    mFieldTextLength(0),
    mLineNumber(0)
{
  MOZ_COUNT_CTOR(nsXBLProtoImplField);
  mName = NS_strdup(aName);  // XXXbz make more sense to use a stringbuffer?
  
  mJSAttributes = JSPROP_ENUMERATE;
  if (aReadOnly) {
    nsAutoString readOnly; readOnly.Assign(aReadOnly);
    if (readOnly.LowerCaseEqualsLiteral("true"))
      mJSAttributes |= JSPROP_READONLY;
  }
}


nsXBLProtoImplField::nsXBLProtoImplField(const bool aIsReadOnly)
  : mNext(nsnull),
    mFieldText(nsnull),
    mFieldTextLength(0),
    mLineNumber(0)
{
  MOZ_COUNT_CTOR(nsXBLProtoImplField);

  mJSAttributes = JSPROP_ENUMERATE;
  if (aIsReadOnly)
    mJSAttributes |= JSPROP_READONLY;
}

nsXBLProtoImplField::~nsXBLProtoImplField()
{
  MOZ_COUNT_DTOR(nsXBLProtoImplField);
  if (mFieldText)
    nsMemory::Free(mFieldText);
  NS_Free(mName);
  NS_CONTENT_DELETE_LIST_MEMBER(nsXBLProtoImplField, this, mNext);
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
nsXBLProtoImplField::InstallField(nsIScriptContext* aContext,
                                  JSObject* aBoundNode,
                                  nsIPrincipal* aPrincipal,
                                  nsIURI* aBindingDocURI,
                                  bool* aDidInstall) const
{
  NS_TIME_FUNCTION_MIN(5);
  NS_PRECONDITION(aBoundNode,
                  "uh-oh, bound node should NOT be null or bad things will "
                  "happen");

  *aDidInstall = false;

  if (mFieldTextLength == 0) {
    return NS_OK;
  }

  nsAutoMicroTask mt;

  // EvaluateStringWithValue and JS_DefineUCProperty can both trigger GC, so
  // protect |result| here.
  nsresult rv;

  nsCAutoString uriSpec;
  aBindingDocURI->GetSpec(uriSpec);
  
  JSContext* cx = aContext->GetNativeContext();
  NS_ASSERTION(!::JS_IsExceptionPending(cx),
               "Shouldn't get here when an exception is pending!");
  
  // compile the literal string
  bool undefined;
  nsCOMPtr<nsIScriptContext> context = aContext;

  JSAutoRequest ar(cx);
  jsval result = JSVAL_NULL;
  rv = context->EvaluateStringWithValue(nsDependentString(mFieldText,
                                                          mFieldTextLength), 
                                        aBoundNode,
                                        aPrincipal, uriSpec.get(),
                                        mLineNumber, JSVERSION_LATEST,
                                        &result, &undefined);
  if (NS_FAILED(rv)) {
    return rv;
  }

  if (undefined) {
    result = JSVAL_VOID;
  }

  // Define the evaluated result as a JS property
  nsDependentString name(mName);
  if (!::JS_DefineUCProperty(cx, aBoundNode,
                             reinterpret_cast<const jschar*>(mName), 
                             name.Length(), result, nsnull, nsnull,
                             mJSAttributes)) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  *aDidInstall = true;
  return NS_OK;
}

nsresult
nsXBLProtoImplField::Read(nsIScriptContext* aContext,
                          nsIObjectInputStream* aStream)
{
  nsAutoString name;
  nsresult rv = aStream->ReadString(name);
  NS_ENSURE_SUCCESS(rv, rv);
  mName = ToNewUnicode(name);

  rv = aStream->Read32(&mLineNumber);
  NS_ENSURE_SUCCESS(rv, rv);

  nsAutoString fieldText;
  rv = aStream->ReadString(fieldText);
  NS_ENSURE_SUCCESS(rv, rv);
  mFieldTextLength = fieldText.Length();
  if (mFieldTextLength)
    mFieldText = ToNewUnicode(fieldText);

  return NS_OK;
}

nsresult
nsXBLProtoImplField::Write(nsIScriptContext* aContext,
                           nsIObjectOutputStream* aStream)
{
  XBLBindingSerializeDetails type = XBLBinding_Serialize_Field;

  if (mJSAttributes & JSPROP_READONLY) {
    type |= XBLBinding_Serialize_ReadOnly;
  }

  nsresult rv = aStream->Write8(type);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = aStream->WriteWStringZ(mName);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = aStream->Write32(mLineNumber);
  NS_ENSURE_SUCCESS(rv, rv);

  return aStream->WriteWStringZ(mFieldText ? mFieldText : EmptyString().get());
}
