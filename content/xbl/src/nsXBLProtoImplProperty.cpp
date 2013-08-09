/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsIAtom.h"
#include "nsString.h"
#include "jsapi.h"
#include "nsIContent.h"
#include "nsXBLProtoImplProperty.h"
#include "nsUnicharUtils.h"
#include "nsContentUtils.h"
#include "nsCxPusher.h"
#include "nsReadableUtils.h"
#include "nsJSUtils.h"
#include "nsXBLPrototypeBinding.h"
#include "nsXBLSerialize.h"
#include "xpcpublic.h"

using namespace mozilla;

nsXBLProtoImplProperty::nsXBLProtoImplProperty(const PRUnichar* aName,
                                               const PRUnichar* aGetter, 
                                               const PRUnichar* aSetter,
                                               const PRUnichar* aReadOnly,
                                               uint32_t aLineNumber) :
  nsXBLProtoImplMember(aName), 
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

  if (aGetter) {
    AppendGetterText(nsDependentString(aGetter));
    SetGetterLineNumber(aLineNumber);
  }
  if (aSetter) {
    AppendSetterText(nsDependentString(aSetter));
    SetSetterLineNumber(aLineNumber);
  }
}

nsXBLProtoImplProperty::nsXBLProtoImplProperty(const PRUnichar* aName,
                                               const bool aIsReadOnly)
  : nsXBLProtoImplMember(aName),
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

  if (!mGetter.IsCompiled()) {
    delete mGetter.GetUncompiled();
  }

  if (!mSetter.IsCompiled()) {
    delete mSetter.GetUncompiled();
  }
}

void nsXBLProtoImplProperty::EnsureUncompiledText(PropertyOp& aPropertyOp)
{
  if (!aPropertyOp.GetUncompiled()) {
    nsXBLTextWithLineNumber* text = new nsXBLTextWithLineNumber();
    aPropertyOp.SetUncompiled(text);
  }
}

void 
nsXBLProtoImplProperty::AppendGetterText(const nsAString& aText)
{
  NS_PRECONDITION(!mIsCompiled,
                  "Must not be compiled when accessing getter text");
  EnsureUncompiledText(mGetter);
  mGetter.GetUncompiled()->AppendText(aText);
}

void 
nsXBLProtoImplProperty::AppendSetterText(const nsAString& aText)
{
  NS_PRECONDITION(!mIsCompiled,
                  "Must not be compiled when accessing setter text");
  EnsureUncompiledText(mSetter);
  mSetter.GetUncompiled()->AppendText(aText);
}

void
nsXBLProtoImplProperty::SetGetterLineNumber(uint32_t aLineNumber)
{
  NS_PRECONDITION(!mIsCompiled,
                  "Must not be compiled when accessing getter text");
  EnsureUncompiledText(mGetter);
  mGetter.GetUncompiled()->SetLineNumber(aLineNumber);
}

void
nsXBLProtoImplProperty::SetSetterLineNumber(uint32_t aLineNumber)
{
  NS_PRECONDITION(!mIsCompiled,
                  "Must not be compiled when accessing setter text");
  EnsureUncompiledText(mSetter);
  mSetter.GetUncompiled()->SetLineNumber(aLineNumber);
}

const char* gPropertyArgs[] = { "val" };

nsresult
nsXBLProtoImplProperty::InstallMember(JSContext *aCx,
                                      JS::Handle<JSObject*> aTargetClassObject)
{
  NS_PRECONDITION(mIsCompiled,
                  "Should not be installing an uncompiled property");
  MOZ_ASSERT(mGetter.IsCompiled() && mSetter.IsCompiled());
  MOZ_ASSERT(js::IsObjectInContextCompartment(aTargetClassObject, aCx));
  JS::Rooted<JSObject*> globalObject(aCx, JS_GetGlobalForObject(aCx, aTargetClassObject));
  JS::Rooted<JSObject*> scopeObject(aCx, xpc::GetXBLScope(aCx, globalObject));
  NS_ENSURE_TRUE(scopeObject, NS_ERROR_OUT_OF_MEMORY);

  if (mGetter.GetJSFunction() || mSetter.GetJSFunction()) {
    // First, enter the compartment of the scope object and clone the functions.
    JSAutoCompartment ac(aCx, scopeObject);

    JS::Rooted<JSObject*> getter(aCx, nullptr);
    if (mGetter.GetJSFunction()) {
      if (!(getter = ::JS_CloneFunctionObject(aCx, mGetter.GetJSFunction(), scopeObject)))
        return NS_ERROR_OUT_OF_MEMORY;
    }

    JS::Rooted<JSObject*> setter(aCx, nullptr);
    if (mSetter.GetJSFunction()) {
      if (!(setter = ::JS_CloneFunctionObject(aCx, mSetter.GetJSFunction(), scopeObject)))
        return NS_ERROR_OUT_OF_MEMORY;
    }

    // Now, enter the content compartment, wrap the getter/setter, and define
    // them on the class object.
    JSAutoCompartment ac2(aCx, aTargetClassObject);
    nsDependentString name(mName);
    if (!JS_WrapObject(aCx, getter.address()) ||
        !JS_WrapObject(aCx, setter.address()) ||
        !::JS_DefineUCProperty(aCx, aTargetClassObject,
                               static_cast<const jschar*>(mName),
                               name.Length(), JSVAL_VOID,
                               JS_DATA_TO_FUNC_PTR(JSPropertyOp, getter.get()),
                               JS_DATA_TO_FUNC_PTR(JSStrictPropertyOp, setter.get()),
                               mJSAttributes))
      return NS_ERROR_OUT_OF_MEMORY;
  }
  return NS_OK;
}

nsresult
nsXBLProtoImplProperty::CompileMember(const nsCString& aClassStr,
                                      JS::Handle<JSObject*> aClassObject)
{
  AssertInCompilationScope();
  NS_PRECONDITION(!mIsCompiled,
                  "Trying to compile an already-compiled property");
  NS_PRECONDITION(aClassObject,
                  "Must have class object to compile");
  MOZ_ASSERT(!mGetter.IsCompiled() && !mSetter.IsCompiled());

  if (!mName)
    return NS_ERROR_FAILURE; // Without a valid name, we can't install the member.

  // We have a property.
  nsresult rv = NS_OK;

  nsAutoCString functionUri;
  if (mGetter.GetUncompiled() || mSetter.GetUncompiled()) {
    functionUri = aClassStr;
    int32_t hash = functionUri.RFindChar('#');
    if (hash != kNotFound) {
      functionUri.Truncate(hash);
    }
  }

  bool deletedGetter = false;
  nsXBLTextWithLineNumber *getterText = mGetter.GetUncompiled();
  if (getterText && getterText->GetText()) {
    nsDependentString getter(getterText->GetText());
    if (!getter.IsEmpty()) {
      AutoJSContext cx;
      JSAutoCompartment ac(cx, aClassObject);
      JS::CompileOptions options(cx);
      options.setFileAndLine(functionUri.get(), getterText->GetLineNumber())
             .setVersion(JSVERSION_LATEST);
      nsCString name = NS_LITERAL_CSTRING("get_") + NS_ConvertUTF16toUTF8(mName);
      JS::RootedObject rootedNull(cx, nullptr); // See bug 781070.
      JS::RootedObject getterObject(cx);
      rv = nsJSUtils::CompileFunction(cx, rootedNull, options, name, 0, nullptr,
                                      getter, getterObject.address());

      delete getterText;
      deletedGetter = true;

      mGetter.SetJSFunction(getterObject);
    
      if (mGetter.GetJSFunction() && NS_SUCCEEDED(rv)) {
        mJSAttributes |= JSPROP_GETTER | JSPROP_SHARED;
      }
      if (NS_FAILED(rv)) {
        mGetter.SetJSFunction(nullptr);
        mJSAttributes &= ~JSPROP_GETTER;
        /*chaining to return failure*/
      }
    }
  } // if getter is not empty

  if (!deletedGetter) {  // Empty getter
    delete getterText;
    mGetter.SetJSFunction(nullptr);
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
  nsXBLTextWithLineNumber *setterText = mSetter.GetUncompiled();
  if (setterText && setterText->GetText()) {
    nsDependentString setter(setterText->GetText());
    if (!setter.IsEmpty()) {
      AutoJSContext cx;
      JSAutoCompartment ac(cx, aClassObject);
      JS::CompileOptions options(cx);
      options.setFileAndLine(functionUri.get(), setterText->GetLineNumber())
             .setVersion(JSVERSION_LATEST);
      nsCString name = NS_LITERAL_CSTRING("set_") + NS_ConvertUTF16toUTF8(mName);
      JS::RootedObject rootedNull(cx, nullptr); // See bug 781070.
      JS::RootedObject setterObject(cx);
      rv = nsJSUtils::CompileFunction(cx, rootedNull, options, name, 1,
                                      gPropertyArgs, setter, setterObject.address());

      delete setterText;
      deletedSetter = true;
      mSetter.SetJSFunction(setterObject);

      if (mSetter.GetJSFunction() && NS_SUCCEEDED(rv)) {
        mJSAttributes |= JSPROP_SETTER | JSPROP_SHARED;
      }
      if (NS_FAILED(rv)) {
        mSetter.SetJSFunction(nullptr);
        mJSAttributes &= ~JSPROP_SETTER;
        /*chaining to return failure*/
      }
    }
  } // if setter wasn't empty....

  if (!deletedSetter) {  // Empty setter
    delete setterText;
    mSetter.SetJSFunction(nullptr);
  }

#ifdef DEBUG
  mIsCompiled = NS_SUCCEEDED(rv);
#endif

  return rv;
}

void
nsXBLProtoImplProperty::Trace(const TraceCallbacks& aCallbacks, void *aClosure)
{
  if (mJSAttributes & JSPROP_GETTER) {
    aCallbacks.Trace(&mGetter.AsHeapObject(), "mGetter", aClosure);
  }

  if (mJSAttributes & JSPROP_SETTER) {
    aCallbacks.Trace(&mSetter.AsHeapObject(), "mSetter", aClosure);
  }
}

nsresult
nsXBLProtoImplProperty::Read(nsIObjectInputStream* aStream,
                             XBLBindingSerializeDetails aType)
{
  AssertInCompilationScope();
  MOZ_ASSERT(!mIsCompiled);
  MOZ_ASSERT(!mGetter.GetUncompiled() && !mSetter.GetUncompiled());

  AutoJSContext cx;
  JS::Rooted<JSObject*> getterObject(cx);
  if (aType == XBLBinding_Serialize_GetterProperty ||
      aType == XBLBinding_Serialize_GetterSetterProperty) {
    nsresult rv = XBL_DeserializeFunction(aStream, &getterObject);
    NS_ENSURE_SUCCESS(rv, rv);

    mJSAttributes |= JSPROP_GETTER | JSPROP_SHARED;
  }
  mGetter.SetJSFunction(getterObject);
  
  JS::Rooted<JSObject*> setterObject(cx);
  if (aType == XBLBinding_Serialize_SetterProperty ||
      aType == XBLBinding_Serialize_GetterSetterProperty) {
    nsresult rv = XBL_DeserializeFunction(aStream, &setterObject);
    NS_ENSURE_SUCCESS(rv, rv);

    mJSAttributes |= JSPROP_SETTER | JSPROP_SHARED;
  }
  mSetter.SetJSFunction(setterObject);

#ifdef DEBUG
  mIsCompiled = true;
#endif

  return NS_OK;
}

nsresult
nsXBLProtoImplProperty::Write(nsIObjectOutputStream* aStream)
{
  AssertInCompilationScope();
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

  // The calls to fromMarkedLocation() below are safe because mSetter and
  // mGetter are traced by the Trace() method above, and because their values
  // are never changed after they have been set to a compiled function.
  MOZ_ASSERT_IF(mJSAttributes & (JSPROP_GETTER | JSPROP_SETTER), mIsCompiled);

  if (mJSAttributes & JSPROP_GETTER) {
    JS::Handle<JSObject*> function =
      JS::Handle<JSObject*>::fromMarkedLocation(mGetter.AsHeapObject().address());
    rv = XBL_SerializeFunction(aStream, function);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  if (mJSAttributes & JSPROP_SETTER) {
     JS::Handle<JSObject*> function =
      JS::Handle<JSObject*>::fromMarkedLocation(mSetter.AsHeapObject().address());
    rv = XBL_SerializeFunction(aStream, function);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return NS_OK;
}
