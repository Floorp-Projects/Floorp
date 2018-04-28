/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsAtom.h"
#include "nsIContent.h"
#include "nsString.h"
#include "nsJSUtils.h"
#include "jsapi.h"
#include "js/CharacterEncoding.h"
#include "nsUnicharUtils.h"
#include "nsReadableUtils.h"
#include "nsXBLProtoImplField.h"
#include "nsIScriptContext.h"
#include "nsIURI.h"
#include "nsXBLSerialize.h"
#include "nsXBLPrototypeBinding.h"
#include "mozilla/dom/BindingUtils.h"
#include "mozilla/dom/ElementBinding.h"
#include "mozilla/dom/ScriptSettings.h"
#include "nsGlobalWindow.h"
#include "xpcpublic.h"
#include "WrapperFactory.h"

using namespace mozilla;
using namespace mozilla::dom;

nsXBLProtoImplField::nsXBLProtoImplField(const char16_t* aName, const char16_t* aReadOnly)
  : mNext(nullptr),
    mFieldText(nullptr),
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
  : mNext(nullptr),
    mFieldText(nullptr),
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
    free(mFieldText);
  free(mName);
  NS_CONTENT_DELETE_LIST_MEMBER(nsXBLProtoImplField, this, mNext);
}

void
nsXBLProtoImplField::AppendFieldText(const nsAString& aText)
{
  if (mFieldText) {
    nsDependentString fieldTextStr(mFieldText, mFieldTextLength);
    nsAutoString newFieldText = fieldTextStr + aText;
    char16_t* temp = mFieldText;
    mFieldText = ToNewUnicode(newFieldText);
    mFieldTextLength = newFieldText.Length();
    free(temp);
  }
  else {
    mFieldText = ToNewUnicode(aText);
    mFieldTextLength = aText.Length();
  }
}

// XBL fields are represented on elements inheriting that field a bit trickily.
// When setting up the XBL prototype object, we install accessors for the fields
// on the prototype object. Those accessors, when used, will then (via
// InstallXBLField below) reify a property for the field onto the actual XBL-backed
// element.
//
// The accessor property is a plain old property backed by a getter function and
// a setter function.  These properties are backed by the FieldGetter and
// FieldSetter natives; they're created by InstallAccessors.  The precise field to be
// reified is identified using two extra slots on the getter/setter functions.
// XBLPROTO_SLOT stores the XBL prototype object that provides the field.
// FIELD_SLOT stores the name of the field, i.e. its JavaScript property name.
//
// This two-step field installation process -- creating an accessor on the
// prototype, then have that reify an own property on the actual element -- is
// admittedly convoluted.  Better would be for XBL-backed elements to be proxies
// that could resolve fields onto themselves.  But given that XBL bindings are
// associated with elements mutably -- you can add/remove/change -moz-binding
// whenever you want, alas -- doing so would require all elements to be proxies,
// which isn't performant now.  So we do this two-step instead.
static const uint32_t XBLPROTO_SLOT = 0;
static const uint32_t FIELD_SLOT = 1;

bool
ValueHasISupportsPrivate(JS::Handle<JS::Value> v)
{
  if (!v.isObject()) {
    return false;
  }

  const DOMJSClass* domClass = GetDOMClass(&v.toObject());
  if (domClass) {
    return domClass->mDOMObjectIsISupports;
  }

  const JSClass* clasp = ::JS_GetClass(&v.toObject());
  const uint32_t HAS_PRIVATE_NSISUPPORTS =
    JSCLASS_HAS_PRIVATE | JSCLASS_PRIVATE_IS_NSISUPPORTS;
  return (clasp->flags & HAS_PRIVATE_NSISUPPORTS) == HAS_PRIVATE_NSISUPPORTS;
}

#ifdef DEBUG
static bool
ValueHasISupportsPrivate(JSContext* cx, const JS::Value& aVal)
{
    JS::Rooted<JS::Value> v(cx, aVal);
    return ValueHasISupportsPrivate(v);
}
#endif

// Define a shadowing property on |this| for the XBL field defined by the
// contents of the callee's reserved slots.  If the property was defined,
// *installed will be true, and idp will be set to the property name that was
// defined.
static bool
InstallXBLField(JSContext* cx,
                JS::Handle<JSObject*> callee, JS::Handle<JSObject*> thisObj,
                JS::MutableHandle<jsid> idp, bool* installed)
{
  *installed = false;

  // First ensure |this| is a reasonable XBL bound node.
  //
  // FieldAccessorGuard already determined whether |thisObj| was acceptable as
  // |this| in terms of not throwing a TypeError.  Assert this for good measure.
  MOZ_ASSERT(ValueHasISupportsPrivate(cx, JS::ObjectValue(*thisObj)));

  // But there are some cases where we must accept |thisObj| but not install a
  // property on it, or otherwise touch it.  Hence this split of |this|-vetting
  // duties.
  nsCOMPtr<nsISupports> native = xpc::UnwrapReflectorToISupports(thisObj);
  if (!native) {
    // Looks like whatever |thisObj| is it's not our nsIContent.  It might well
    // be the proto our binding installed, however, where the private is the
    // nsXBLDocumentInfo, so just baul out quietly.  Do NOT throw an exception
    // here.
    //
    // We could make this stricter by checking the class maybe, but whatever.
    return true;
  }

  nsCOMPtr<nsIContent> xblNode = do_QueryInterface(native);
  if (!xblNode) {
    xpc::Throw(cx, NS_ERROR_UNEXPECTED);
    return false;
  }

  // Now that |this| is okay, actually install the field.

  // Because of the possibility (due to XBL binding inheritance, because each
  // XBL binding lives in its own global object) that |this| might be in a
  // different compartment from the callee (not to mention that this method can
  // be called with an arbitrary |this| regardless of how insane XBL is), and
  // because in this method we've entered |this|'s compartment (see in
  // Field[GS]etter where we attempt a cross-compartment call), we must enter
  // the callee's compartment to access its reserved slots.
  nsXBLPrototypeBinding* protoBinding;
  nsAutoJSString fieldName;
  {
    JSAutoCompartment ac(cx, callee);

    JS::Rooted<JSObject*> xblProto(cx);
    xblProto = &js::GetFunctionNativeReserved(callee, XBLPROTO_SLOT).toObject();

    JS::Rooted<JS::Value> name(cx, js::GetFunctionNativeReserved(callee, FIELD_SLOT));
    if (!fieldName.init(cx, name.toString())) {
      return false;
    }

    MOZ_ALWAYS_TRUE(JS_ValueToId(cx, name, idp));

    // If a separate XBL scope is being used, the callee is not same-compartment
    // with the xbl prototype, and the object is a cross-compartment wrapper.
    xblProto = js::UncheckedUnwrap(xblProto);
    JSAutoCompartment ac2(cx, xblProto);
    JS::Value slotVal = ::JS_GetReservedSlot(xblProto, 0);
    protoBinding = static_cast<nsXBLPrototypeBinding*>(slotVal.toPrivate());
    MOZ_ASSERT(protoBinding);
  }

  nsXBLProtoImplField* field = protoBinding->FindField(fieldName);
  MOZ_ASSERT(field);

  nsresult rv = field->InstallField(thisObj, *protoBinding, installed);
  if (NS_SUCCEEDED(rv)) {
    return true;
  }

  if (!::JS_IsExceptionPending(cx)) {
    xpc::Throw(cx, rv);
  }
  return false;
}

bool
FieldGetterImpl(JSContext *cx, const JS::CallArgs& args)
{
  JS::Handle<JS::Value> thisv = args.thisv();
  MOZ_ASSERT(ValueHasISupportsPrivate(thisv));

  JS::Rooted<JSObject*> thisObj(cx, &thisv.toObject());

  // We should be in the compartment of |this|. If we got here via nativeCall,
  // |this| is not same-compartment with |callee|, and it's possible via
  // asymmetric security semantics that |args.calleev()| is actually a security
  // wrapper. In this case, we know we want to do an unsafe unwrap, and
  // InstallXBLField knows how to handle cross-compartment pointers.
  bool installed = false;
  JS::Rooted<JSObject*> callee(cx, js::UncheckedUnwrap(&args.calleev().toObject()));
  JS::Rooted<jsid> id(cx);
  if (!InstallXBLField(cx, callee, thisObj, &id, &installed)) {
    return false;
  }

  if (!installed) {
    args.rval().setUndefined();
    return true;
  }

  return JS_GetPropertyById(cx, thisObj, id, args.rval());
}

static bool
FieldGetter(JSContext *cx, unsigned argc, JS::Value *vp)
{
  JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
  return JS::CallNonGenericMethod<ValueHasISupportsPrivate, FieldGetterImpl>
                                 (cx, args);
}

bool
FieldSetterImpl(JSContext *cx, const JS::CallArgs& args)
{
  JS::Handle<JS::Value> thisv = args.thisv();
  MOZ_ASSERT(ValueHasISupportsPrivate(thisv));

  JS::Rooted<JSObject*> thisObj(cx, &thisv.toObject());

  // We should be in the compartment of |this|. If we got here via nativeCall,
  // |this| is not same-compartment with |callee|, and it's possible via
  // asymmetric security semantics that |args.calleev()| is actually a security
  // wrapper. In this case, we know we want to do an unsafe unwrap, and
  // InstallXBLField knows how to handle cross-compartment pointers.
  bool installed = false;
  JS::Rooted<JSObject*> callee(cx, js::UncheckedUnwrap(&args.calleev().toObject()));
  JS::Rooted<jsid> id(cx);
  if (!InstallXBLField(cx, callee, thisObj, &id, &installed)) {
    return false;
  }

  if (installed) {
    if (!::JS_SetPropertyById(cx, thisObj, id, args.get(0))) {
      return false;
    }
  }
  args.rval().setUndefined();
  return true;
}

static bool
FieldSetter(JSContext *cx, unsigned argc, JS::Value *vp)
{
  JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
  return JS::CallNonGenericMethod<ValueHasISupportsPrivate, FieldSetterImpl>
                                 (cx, args);
}

nsresult
nsXBLProtoImplField::InstallAccessors(JSContext* aCx,
                                      JS::Handle<JSObject*> aTargetClassObject)
{
  MOZ_ASSERT(js::IsObjectInContextCompartment(aTargetClassObject, aCx));
  JS::Rooted<JSObject*> globalObject(aCx, JS_GetGlobalForObject(aCx, aTargetClassObject));
  JS::Rooted<JSObject*> scopeObject(aCx, xpc::GetXBLScopeOrGlobal(aCx, globalObject));
  NS_ENSURE_TRUE(scopeObject, NS_ERROR_OUT_OF_MEMORY);

  // Don't install it if the field is empty; see also InstallField which also must
  // implement the not-empty requirement.
  if (IsEmpty()) {
    return NS_OK;
  }

  // Install a getter/setter pair which will resolve the field onto the actual
  // object, when invoked.

  // Get the field name as an id.
  JS::Rooted<jsid> id(aCx);
  JS::TwoByteChars chars(mName, NS_strlen(mName));
  if (!JS_CharsToId(aCx, chars, &id))
    return NS_ERROR_OUT_OF_MEMORY;

  // Properties/Methods have historically taken precendence over fields. We
  // install members first, so just bounce here if the property is already
  // defined.
  bool found = false;
  if (!JS_AlreadyHasOwnPropertyById(aCx, aTargetClassObject, id, &found))
    return NS_ERROR_FAILURE;
  if (found)
    return NS_OK;

  // FieldGetter and FieldSetter need to run in the XBL scope so that they can
  // see through any SOWs on their targets.

  // First, enter the XBL scope, and compile the functions there.
  JSAutoCompartment ac(aCx, scopeObject);
  JS::Rooted<JS::Value> wrappedClassObj(aCx, JS::ObjectValue(*aTargetClassObject));
  if (!JS_WrapValue(aCx, &wrappedClassObj))
    return NS_ERROR_OUT_OF_MEMORY;

  JS::Rooted<JSObject*> get(aCx,
    JS_GetFunctionObject(js::NewFunctionByIdWithReserved(aCx, FieldGetter,
                                                         0, 0, id)));
  if (!get) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  js::SetFunctionNativeReserved(get, XBLPROTO_SLOT, wrappedClassObj);
  js::SetFunctionNativeReserved(get, FIELD_SLOT,
                                JS::StringValue(JSID_TO_STRING(id)));

  JS::Rooted<JSObject*> set(aCx,
    JS_GetFunctionObject(js::NewFunctionByIdWithReserved(aCx, FieldSetter,
                                                          1, 0, id)));
  if (!set) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  js::SetFunctionNativeReserved(set, XBLPROTO_SLOT, wrappedClassObj);
  js::SetFunctionNativeReserved(set, FIELD_SLOT,
                                JS::StringValue(JSID_TO_STRING(id)));

  // Now, re-enter the class object's scope, wrap the getters/setters, and define
  // them there.
  JSAutoCompartment ac2(aCx, aTargetClassObject);
  if (!JS_WrapObject(aCx, &get) || !JS_WrapObject(aCx, &set)) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  if (!::JS_DefinePropertyById(aCx, aTargetClassObject, id,
                               JS_DATA_TO_FUNC_PTR(JSNative, get.get()),
                               JS_DATA_TO_FUNC_PTR(JSNative, set.get()),
                               AccessorAttributes())) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  return NS_OK;
}

nsresult
nsXBLProtoImplField::InstallField(JS::Handle<JSObject*> aBoundNode,
                                  const nsXBLPrototypeBinding& aProtoBinding,
                                  bool* aDidInstall) const
{
  MOZ_ASSERT(aBoundNode,
             "uh-oh, bound node should NOT be null or bad things will happen");

  *aDidInstall = false;

  // Empty fields are treated as not actually present.
  if (IsEmpty()) {
    return NS_OK;
  }

  nsAutoMicroTask mt;

  nsAutoCString uriSpec;
  nsresult rv = aProtoBinding.DocURI()->GetSpec(uriSpec);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsIGlobalObject* globalObject = xpc::WindowGlobalOrNull(aBoundNode);
  if (!globalObject) {
    return NS_OK;
  }

  // We are going to run script via EvaluateString, so we need a script entry
  // point, but as this is XBL related it does not appear in the HTML spec.
  // We need an actual JSContext to do GetXBLScopeOrGlobal, and it needs to
  // be in the compartment of globalObject.  But we want our XBL execution scope
  // to be our entry global.
  AutoJSAPI jsapi;
  if (!jsapi.Init(globalObject)) {
    return NS_ERROR_UNEXPECTED;
  }
  MOZ_ASSERT(!::JS_IsExceptionPending(jsapi.cx()),
             "Shouldn't get here when an exception is pending!");

  // Note: the UNWRAP_OBJECT may mutate boundNode; don't use it after that call.
  JS::Rooted<JSObject*> boundNode(jsapi.cx(), aBoundNode);
  Element* boundElement = nullptr;
  rv = UNWRAP_OBJECT(Element, &boundNode, boundElement);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // First, enter the xbl scope, build the element's scope chain, and use
  // that as the scope chain for the evaluation.
  JS::Rooted<JSObject*> scopeObject(jsapi.cx(),
    xpc::GetXBLScopeOrGlobal(jsapi.cx(), aBoundNode));
  NS_ENSURE_TRUE(scopeObject, NS_ERROR_OUT_OF_MEMORY);

  AutoEntryScript aes(scopeObject, "XBL <field> initialization", true);
  JSContext* cx = aes.cx();

  JS::Rooted<JS::Value> result(cx);
  JS::CompileOptions options(cx);
  options.setFileAndLine(uriSpec.get(), mLineNumber);
  JS::AutoObjectVector scopeChain(cx);
  if (!nsJSUtils::GetScopeChainForXBL(cx, boundElement, aProtoBinding, scopeChain)) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  rv = NS_OK;
  {
    nsJSUtils::ExecutionContext exec(cx, scopeObject);
    exec.SetScopeChain(scopeChain);
    exec.CompileAndExec(options, nsDependentString(mFieldText,
                                                   mFieldTextLength));
    rv = exec.ExtractReturnValue(&result);
  }

  if (NS_FAILED(rv)) {
    return rv;
  }

  if (rv == NS_SUCCESS_DOM_SCRIPT_EVALUATION_THREW) {
    // Report the exception now, before we try using the JSContext for
    // the JS_DefineUCProperty call.  Note that this reports in our current
    // compartment, which is the XBL scope.
    aes.ReportException();
  }

  // Now, enter the node's compartment, wrap the eval result, and define it on
  // the bound node.
  JSAutoCompartment ac2(cx, aBoundNode);
  nsDependentString name(mName);
  if (!JS_WrapValue(cx, &result) ||
      !::JS_DefineUCProperty(cx, aBoundNode,
                             reinterpret_cast<const char16_t*>(mName),
                             name.Length(), result, mJSAttributes)) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  *aDidInstall = true;
  return NS_OK;
}

nsresult
nsXBLProtoImplField::Read(nsIObjectInputStream* aStream)
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
nsXBLProtoImplField::Write(nsIObjectOutputStream* aStream)
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

  return aStream->WriteWStringZ(mFieldText ? mFieldText : u"");
}
