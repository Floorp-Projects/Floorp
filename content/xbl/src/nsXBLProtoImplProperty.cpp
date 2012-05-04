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
#include "nsIDocument.h"
#include "nsXBLProtoImplProperty.h"
#include "nsUnicharUtils.h"
#include "nsReadableUtils.h"
#include "nsIScriptContext.h"
#include "nsIScriptGlobalObject.h"
#include "nsContentUtils.h"
#include "nsXBLPrototypeBinding.h"
#include "nsXBLSerialize.h"

nsXBLProtoImplProperty::nsXBLProtoImplProperty(const PRUnichar* aName,
                                               const PRUnichar* aGetter, 
                                               const PRUnichar* aSetter,
                                               const PRUnichar* aReadOnly) :
  nsXBLProtoImplMember(aName), 
  mGetterText(nsnull),
  mSetterText(nsnull),
  mJSAttributes(JSPROP_ENUMERATE)
#ifdef DEBUG
  , mIsCompiled(false)
#endif
{
  MOZ_COUNT_CTOR(nsXBLProtoImplProperty);

  if (aReadOnly) {
    nsAutoString readOnly; readOnly.Assign(*aReadOnly);
    if (readOnly.LowerCaseEqualsLiteral("true"))
      mJSAttributes |= JSPROP_READONLY;
  }

  if (aGetter)
    AppendGetterText(nsDependentString(aGetter));
  if (aSetter)
    AppendSetterText(nsDependentString(aSetter));
}

nsXBLProtoImplProperty::nsXBLProtoImplProperty(const PRUnichar* aName,
                                               const bool aIsReadOnly)
  : nsXBLProtoImplMember(aName),
    mGetterText(nsnull),
    mSetterText(nsnull),
    mJSAttributes(JSPROP_ENUMERATE)
#ifdef DEBUG
  , mIsCompiled(false)
#endif
{
  MOZ_COUNT_CTOR(nsXBLProtoImplProperty);

  if (aIsReadOnly)
    mJSAttributes |= JSPROP_READONLY;
}

nsXBLProtoImplProperty::~nsXBLProtoImplProperty()
{
  MOZ_COUNT_DTOR(nsXBLProtoImplProperty);

  if (!(mJSAttributes & JSPROP_GETTER)) {
    delete mGetterText;
  }

  if (!(mJSAttributes & JSPROP_SETTER)) {
    delete mSetterText;
  }
}

void 
nsXBLProtoImplProperty::AppendGetterText(const nsAString& aText)
{
  NS_PRECONDITION(!mIsCompiled,
                  "Must not be compiled when accessing getter text");
  if (!mGetterText) {
    mGetterText = new nsXBLTextWithLineNumber();
    if (!mGetterText)
      return;
  }

  mGetterText->AppendText(aText);
}

void 
nsXBLProtoImplProperty::AppendSetterText(const nsAString& aText)
{
  NS_PRECONDITION(!mIsCompiled,
                  "Must not be compiled when accessing setter text");
  if (!mSetterText) {
    mSetterText = new nsXBLTextWithLineNumber();
    if (!mSetterText)
      return;
  }

  mSetterText->AppendText(aText);
}

void
nsXBLProtoImplProperty::SetGetterLineNumber(PRUint32 aLineNumber)
{
  NS_PRECONDITION(!mIsCompiled,
                  "Must not be compiled when accessing getter text");
  if (!mGetterText) {
    mGetterText = new nsXBLTextWithLineNumber();
    if (!mGetterText)
      return;
  }

  mGetterText->SetLineNumber(aLineNumber);
}

void
nsXBLProtoImplProperty::SetSetterLineNumber(PRUint32 aLineNumber)
{
  NS_PRECONDITION(!mIsCompiled,
                  "Must not be compiled when accessing setter text");
  if (!mSetterText) {
    mSetterText = new nsXBLTextWithLineNumber();
    if (!mSetterText)
      return;
  }

  mSetterText->SetLineNumber(aLineNumber);
}

const char* gPropertyArgs[] = { "val" };

nsresult
nsXBLProtoImplProperty::InstallMember(nsIScriptContext* aContext,
                                      nsIContent* aBoundElement, 
                                      void* aScriptObject,
                                      void* aTargetClassObject,
                                      const nsCString& aClassStr)
{
  NS_PRECONDITION(mIsCompiled,
                  "Should not be installing an uncompiled property");
  JSContext* cx = aContext->GetNativeContext();

  nsIDocument *ownerDoc = aBoundElement->OwnerDoc();
  nsIScriptGlobalObject *sgo;

  if (!(sgo = ownerDoc->GetScopeObject())) {
    return NS_ERROR_UNEXPECTED;
  }

  JSObject * scriptObject = (JSObject *) aScriptObject;
  NS_ASSERTION(scriptObject, "uh-oh, script Object should NOT be null or bad things will happen");
  if (!scriptObject)
    return NS_ERROR_FAILURE;

  JSObject * targetClassObject = (JSObject *) aTargetClassObject;
  JSObject * globalObject = sgo->GetGlobalJSObject();

  // now we want to reevaluate our property using aContext and the script object for this window...
  if ((mJSGetterObject || mJSSetterObject) && targetClassObject) {
    JSObject * getter = nsnull;
    JSAutoRequest ar(cx);
    JSAutoEnterCompartment ac;

    if (!ac.enter(cx, globalObject))
      return NS_ERROR_UNEXPECTED;

    if (mJSGetterObject)
      if (!(getter = ::JS_CloneFunctionObject(cx, mJSGetterObject, globalObject)))
        return NS_ERROR_OUT_OF_MEMORY;

    JSObject * setter = nsnull;
    if (mJSSetterObject)
      if (!(setter = ::JS_CloneFunctionObject(cx, mJSSetterObject, globalObject)))
        return NS_ERROR_OUT_OF_MEMORY;

    nsDependentString name(mName);
    if (!::JS_DefineUCProperty(cx, targetClassObject,
                               reinterpret_cast<const jschar*>(mName),
                               name.Length(), JSVAL_VOID,
                               JS_DATA_TO_FUNC_PTR(JSPropertyOp, getter),
                               JS_DATA_TO_FUNC_PTR(JSStrictPropertyOp, setter),
                               mJSAttributes))
      return NS_ERROR_OUT_OF_MEMORY;
  }
  return NS_OK;
}

nsresult 
nsXBLProtoImplProperty::CompileMember(nsIScriptContext* aContext, const nsCString& aClassStr,
                                      JSObject* aClassObject)
{
  NS_PRECONDITION(!mIsCompiled,
                  "Trying to compile an already-compiled property");
  NS_PRECONDITION(aClassObject,
                  "Must have class object to compile");

  if (!mName)
    return NS_ERROR_FAILURE; // Without a valid name, we can't install the member.

  // We have a property.
  nsresult rv = NS_OK;

  nsCAutoString functionUri;
  if (mGetterText || mSetterText) {
    functionUri = aClassStr;
    PRInt32 hash = functionUri.RFindChar('#');
    if (hash != kNotFound) {
      functionUri.Truncate(hash);
    }
  }

  bool deletedGetter = false;
  if (mGetterText && mGetterText->GetText()) {
    nsDependentString getter(mGetterText->GetText());
    if (!getter.IsEmpty()) {
      // Compile into a temp object so we don't wipe out mGetterText
      JSObject* getterObject = nsnull;
      rv = aContext->CompileFunction(aClassObject,
                                     NS_LITERAL_CSTRING("get_") +
                                     NS_ConvertUTF16toUTF8(mName),
                                     0,
                                     nsnull,
                                     getter, 
                                     functionUri.get(),
                                     mGetterText->GetLineNumber(),
                                     JSVERSION_LATEST,
                                     true,
                                     &getterObject);

      // Make sure we free mGetterText here before setting mJSGetterObject, since
      // that'll overwrite mGetterText
      delete mGetterText;
      deletedGetter = true;
      mJSGetterObject = getterObject;
    
      if (mJSGetterObject && NS_SUCCEEDED(rv)) {
        mJSAttributes |= JSPROP_GETTER | JSPROP_SHARED;
      }
      if (NS_FAILED(rv)) {
        mJSGetterObject = nsnull;
        mJSAttributes &= ~JSPROP_GETTER;
        /*chaining to return failure*/
      }
    }
  } // if getter is not empty

  if (!deletedGetter) {  // Empty getter
    delete mGetterText;
    mJSGetterObject = nsnull;
  }
  
  if (NS_FAILED(rv)) {
    // We failed to compile our getter.  So either we've set it to null, or
    // it's still set to the text object.  In either case, it's safe to return
    // the error here, since then we'll be cleaned up as uncompiled and that
    // will be ok.  Going on and compiling the setter and _then_ returning an
    // error, on the other hand, will try to clean up a compiled setter as
    // uncompiled and crash.
    return rv;
  }

  bool deletedSetter = false;
  if (mSetterText && mSetterText->GetText()) {
    nsDependentString setter(mSetterText->GetText());
    if (!setter.IsEmpty()) {
      // Compile into a temp object so we don't wipe out mSetterText
      JSObject* setterObject = nsnull;
      rv = aContext->CompileFunction(aClassObject,
                                     NS_LITERAL_CSTRING("set_") +
                                     NS_ConvertUTF16toUTF8(mName),
                                     1,
                                     gPropertyArgs,
                                     setter, 
                                     functionUri.get(),
                                     mSetterText->GetLineNumber(),
                                     JSVERSION_LATEST,
                                     true,
                                     &setterObject);

      // Make sure we free mSetterText here before setting mJSGetterObject, since
      // that'll overwrite mSetterText
      delete mSetterText;
      deletedSetter = true;
      mJSSetterObject = setterObject;

      if (mJSSetterObject && NS_SUCCEEDED(rv)) {
        mJSAttributes |= JSPROP_SETTER | JSPROP_SHARED;
      }
      if (NS_FAILED(rv)) {
        mJSSetterObject = nsnull;
        mJSAttributes &= ~JSPROP_SETTER;
        /*chaining to return failure*/
      }
    }
  } // if setter wasn't empty....

  if (!deletedSetter) {  // Empty setter
    delete mSetterText;
    mJSSetterObject = nsnull;
  }

#ifdef DEBUG
  mIsCompiled = NS_SUCCEEDED(rv);
#endif
  
  return rv;
}

void
nsXBLProtoImplProperty::Trace(TraceCallback aCallback, void *aClosure) const
{
  if (mJSAttributes & JSPROP_GETTER) {
    aCallback(mJSGetterObject, "mJSGetterObject", aClosure);
  }

  if (mJSAttributes & JSPROP_SETTER) {
    aCallback(mJSSetterObject, "mJSSetterObject", aClosure);
  }
}

nsresult
nsXBLProtoImplProperty::Read(nsIScriptContext* aContext,
                             nsIObjectInputStream* aStream,
                             XBLBindingSerializeDetails aType)
{
  if (aType == XBLBinding_Serialize_GetterProperty ||
      aType == XBLBinding_Serialize_GetterSetterProperty) {
    JSObject* getterObject;
    nsresult rv = XBL_DeserializeFunction(aContext, aStream, &getterObject);
    NS_ENSURE_SUCCESS(rv, rv);

    mJSGetterObject = getterObject;
    mJSAttributes |= JSPROP_GETTER | JSPROP_SHARED;
  }

  if (aType == XBLBinding_Serialize_SetterProperty ||
      aType == XBLBinding_Serialize_GetterSetterProperty) {
    JSObject* setterObject;
    nsresult rv = XBL_DeserializeFunction(aContext, aStream, &setterObject);
    NS_ENSURE_SUCCESS(rv, rv);

    mJSSetterObject = setterObject;
    mJSAttributes |= JSPROP_SETTER | JSPROP_SHARED;
  }

#ifdef DEBUG
  mIsCompiled = true;
#endif

  return NS_OK;
}

nsresult
nsXBLProtoImplProperty::Write(nsIScriptContext* aContext,
                              nsIObjectOutputStream* aStream)
{
  XBLBindingSerializeDetails type;

  if (mJSAttributes & JSPROP_GETTER) {
    type = mJSAttributes & JSPROP_SETTER ?
           XBLBinding_Serialize_GetterSetterProperty :
           XBLBinding_Serialize_GetterProperty;
  }
  else {
    type = XBLBinding_Serialize_SetterProperty;
  }

  if (mJSAttributes & JSPROP_READONLY) {
    type |= XBLBinding_Serialize_ReadOnly;
  }

  nsresult rv = aStream->Write8(type);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = aStream->WriteWStringZ(mName);
  NS_ENSURE_SUCCESS(rv, rv);

  if (mJSAttributes & JSPROP_GETTER) {
    rv = XBL_SerializeFunction(aContext, aStream, mJSGetterObject);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  if (mJSAttributes & JSPROP_SETTER) {
    rv = XBL_SerializeFunction(aContext, aStream, mJSSetterObject);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return NS_OK;
}
