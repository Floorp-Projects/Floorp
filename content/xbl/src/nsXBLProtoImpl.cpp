/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/DebugOnly.h"

#include "nsXBLProtoImpl.h"
#include "nsIContent.h"
#include "nsIDocument.h"
#include "nsContentUtils.h"
#include "nsCxPusher.h"
#include "nsIScriptGlobalObject.h"
#include "nsIScriptGlobalObjectOwner.h"
#include "nsIScriptContext.h"
#include "nsIXPConnect.h"
#include "nsIServiceManager.h"
#include "nsIDOMNode.h"
#include "nsXBLPrototypeBinding.h"
#include "nsXBLProtoImplProperty.h"

using namespace mozilla;

// Checks that the version is not modified in a given scope.
class AutoVersionChecker
{
  DebugOnly<JSContext *> const cx;
  DebugOnly<JSVersion> versionBefore;

public:
  explicit AutoVersionChecker(JSContext *aCx) : cx(aCx) {
#ifdef DEBUG
    versionBefore = JS_GetVersion(cx);
#endif
  }

  ~AutoVersionChecker() {
#ifdef DEBUG
    JSVersion versionAfter = JS_GetVersion(cx);
    NS_ABORT_IF_FALSE(versionAfter == versionBefore, "version must not change");
#endif
  }
};

nsresult
nsXBLProtoImpl::InstallImplementation(nsXBLPrototypeBinding* aPrototypeBinding,
                                      nsXBLBinding* aBinding)
{
  // This function is called to install a concrete implementation on a bound element using
  // this prototype implementation as a guide.  The prototype implementation is compiled lazily,
  // so for the first bound element that needs a concrete implementation, we also build the
  // prototype implementation.
  if (!mMembers && !mFields)  // Constructor and destructor also live in mMembers
    return NS_OK; // Nothing to do, so let's not waste time.

  // If the way this gets the script context changes, fix
  // nsXBLProtoImplAnonymousMethod::Execute
  nsIDocument* document = aBinding->GetBoundElement()->OwnerDoc();

  nsCOMPtr<nsIScriptGlobalObject> global =  do_QueryInterface(document->GetScopeObject());
  if (!global) return NS_OK;

  nsCOMPtr<nsIScriptContext> context = global->GetContext();
  if (!context) return NS_OK;

  // InitTarget objects gives us back the JS object that represents the bound element and the
  // class object in the bound document that represents the concrete version of this implementation.
  // This function also has the side effect of building up the prototype implementation if it has
  // not been built already.
  nsCOMPtr<nsIXPConnectJSObjectHolder> holder;
  JS::Rooted<JSObject*> targetClassObject(context->GetNativeContext(), nullptr);
  bool targetObjectIsNew = false;
  nsresult rv = InitTargetObjects(aPrototypeBinding, context,
                                  aBinding->GetBoundElement(),
                                  getter_AddRefs(holder), &targetClassObject,
                                  &targetObjectIsNew);
  NS_ENSURE_SUCCESS(rv, rv); // kick out if we were unable to properly intialize our target objects
  MOZ_ASSERT(targetClassObject);

  // Stash a strong reference to the JSClass in the binding.
  aBinding->SetJSClass(static_cast<nsXBLJSClass*>(JS_GetClass(targetClassObject)));

  // If the prototype already existed, we don't need to install anything. return early.
  if (!targetObjectIsNew)
    return NS_OK;

  JS::Rooted<JSObject*> targetScriptObject(context->GetNativeContext(),
                                           holder->GetJSObject());

  AutoPushJSContext cx(context->GetNativeContext());
  JSAutoCompartment ac(cx, targetClassObject);
  AutoVersionChecker avc(cx);

  // Walk our member list and install each one in turn.
  for (nsXBLProtoImplMember* curr = mMembers;
       curr;
       curr = curr->GetNext())
    curr->InstallMember(cx, targetClassObject);

  // If we're using a separate XBL scope, make a safe copy of the target class
  // object in the XBL scope that we can use for Xray lookups. We don't need
  // the field accessors, so do this before installing them.
  JS::Rooted<JSObject*> globalObject(cx,
    JS_GetGlobalForObject(cx, targetClassObject));
  JS::Rooted<JSObject*> scopeObject(cx, xpc::GetXBLScope(cx, globalObject));
  NS_ENSURE_TRUE(scopeObject, NS_ERROR_OUT_OF_MEMORY);
  if (scopeObject != globalObject) {
    JSAutoCompartment ac2(cx, scopeObject);

    // Create the object. This is just a property holder, so it doesn't need
    // any special JSClass.
    JS::Rooted<JSObject*> shadowProto(cx,
      JS_NewObjectWithGivenProto(cx, nullptr, nullptr, scopeObject));
    NS_ENSURE_TRUE(shadowProto, NS_ERROR_OUT_OF_MEMORY);

    // Define it as a property on the scopeObject, using the same name used on
    // the content side.
    bool ok = JS_DefineProperty(cx, scopeObject,
                                js::GetObjectClass(targetClassObject)->name,
                                JS::ObjectValue(*shadowProto), JS_PropertyStub,
                                JS_StrictPropertyStub,
                                JSPROP_PERMANENT | JSPROP_READONLY);
    NS_ENSURE_TRUE(ok, NS_ERROR_UNEXPECTED);

    // Copy all the properties from the content-visible prototype to the shadow
    // object. This rewraps them appropriately, which should result in vanilla
    // functions, since the properties on the content prototype were cross-
    // compartment wrappers.
    ok = JS_CopyPropertiesFrom(cx, shadowProto, targetClassObject);
    NS_ENSURE_TRUE(ok, NS_ERROR_UNEXPECTED);

    // Content shouldn't have any way to touch this object, but freeze it just
    // to be safe.
    ok = JS_FreezeObject(cx, shadowProto);
    NS_ENSURE_TRUE(ok, NS_ERROR_UNEXPECTED);
  }

  // Install all of our field accessors.
  for (nsXBLProtoImplField* curr = mFields;
       curr;
       curr = curr->GetNext())
    curr->InstallAccessors(cx, targetClassObject);

  return NS_OK;
}

nsresult 
nsXBLProtoImpl::InitTargetObjects(nsXBLPrototypeBinding* aBinding,
                                  nsIScriptContext* aContext, 
                                  nsIContent* aBoundElement, 
                                  nsIXPConnectJSObjectHolder** aScriptObjectHolder, 
                                  JS::MutableHandle<JSObject*> aTargetClassObject,
                                  bool* aTargetIsNew)
{
  nsresult rv = NS_OK;
  *aScriptObjectHolder = nullptr;
  
  if (!mClassObject) {
    rv = CompilePrototypeMembers(aBinding); // This is the first time we've ever installed this binding on an element.
                                 // We need to go ahead and compile all methods and properties on a class
                                 // in our prototype binding.
    if (NS_FAILED(rv))
      return rv;

    MOZ_ASSERT(mClassObject);
  }

  nsIDocument *ownerDoc = aBoundElement->OwnerDoc();
  nsIGlobalObject *sgo;

  if (!(sgo = ownerDoc->GetScopeObject())) {
    return NS_ERROR_UNEXPECTED;
  }

  // Because our prototype implementation has a class, we need to build up a corresponding
  // class for the concrete implementation in the bound document.
  AutoPushJSContext cx(aContext->GetNativeContext());
  JS::Rooted<JSObject*> global(cx, sgo->GetGlobalJSObject());
  nsCOMPtr<nsIXPConnectJSObjectHolder> wrapper;
  JS::Rooted<JS::Value> v(cx);
  rv = nsContentUtils::WrapNative(cx, global, aBoundElement, v.address(),
                                  getter_AddRefs(wrapper));
  NS_ENSURE_SUCCESS(rv, rv);

  JS::Rooted<JSObject*> value(cx, &v.toObject());

  // All of the above code was just obtaining the bound element's script object and its immediate
  // concrete base class.  We need to alter the object so that our concrete class is interposed
  // between the object and its base class.  We become the new base class of the object, and the
  // object's old base class becomes the new class' base class.
  rv = aBinding->InitClass(mClassName, cx, global, value, aTargetClassObject, aTargetIsNew);
  if (NS_FAILED(rv)) {
    return rv;
  }

  nsContentUtils::PreserveWrapper(aBoundElement, aBoundElement);

  wrapper.swap(*aScriptObjectHolder);
  
  return rv;
}

nsresult
nsXBLProtoImpl::CompilePrototypeMembers(nsXBLPrototypeBinding* aBinding)
{
  // We want to pre-compile our implementation's members against a "prototype context". Then when we actually 
  // bind the prototype to a real xbl instance, we'll clone the pre-compiled JS into the real instance's 
  // context.
  nsCOMPtr<nsIScriptGlobalObjectOwner> globalOwner(
      do_QueryObject(aBinding->XBLDocumentInfo()));

  nsIScriptGlobalObject* globalObject = globalOwner->GetScriptGlobalObject();
  NS_ENSURE_TRUE(globalObject, NS_ERROR_UNEXPECTED);

  nsIScriptContext *context = globalObject->GetContext();
  NS_ENSURE_TRUE(context, NS_ERROR_OUT_OF_MEMORY);

  AutoPushJSContext cx(context->GetNativeContext());

  JS::Rooted<JSObject*> global(cx, globalObject->GetGlobalJSObject());
  JS::Rooted<JSObject*> classObject(cx);
  bool classObjectIsNew = false;
  nsresult rv = aBinding->InitClass(mClassName, cx, global, global,
                                    &classObject, &classObjectIsNew);
  if (NS_FAILED(rv))
    return rv;

  MOZ_ASSERT(classObjectIsNew);
  MOZ_ASSERT(classObject);
  mClassObject = classObject;

  AutoVersionChecker avc(cx);

  // Now that we have a class object installed, we walk our member list and compile each of our
  // properties and methods in turn.
  for (nsXBLProtoImplMember* curr = mMembers;
       curr;
       curr = curr->GetNext()) {
    nsresult rv = curr->CompileMember(context, mClassName, classObject);
    if (NS_FAILED(rv)) {
      DestroyMembers();
      return rv;
    }
  }

  return NS_OK;
}

bool
nsXBLProtoImpl::LookupMember(JSContext* aCx, nsString& aName,
                             JS::HandleId aNameAsId,
                             JSPropertyDescriptor* aDesc,
                             JSObject* aClassObject)
{
  for (nsXBLProtoImplMember* m = mMembers; m; m = m->GetNext()) {
    if (aName.Equals(m->GetName())) {
      return JS_GetPropertyDescriptorById(aCx, aClassObject, aNameAsId, 0, aDesc);
    }
  }
  return true;
}

void
nsXBLProtoImpl::Trace(const TraceCallbacks& aCallbacks, void *aClosure)
{
  // If we don't have a class object then we either didn't compile members
  // or we only have fields, in both cases there are no cycles through our
  // members.
  if (!mClassObject) {
    return;
  }

  nsXBLProtoImplMember *member;
  for (member = mMembers; member; member = member->GetNext()) {
    member->Trace(aCallbacks, aClosure);
  }
}

void
nsXBLProtoImpl::UnlinkJSObjects()
{
  if (mClassObject) {
    DestroyMembers();
  }
}

nsXBLProtoImplField*
nsXBLProtoImpl::FindField(const nsString& aFieldName) const
{
  for (nsXBLProtoImplField* f = mFields; f; f = f->GetNext()) {
    if (aFieldName.Equals(f->GetName())) {
      return f;
    }
  }

  return nullptr;
}

bool
nsXBLProtoImpl::ResolveAllFields(JSContext *cx, JS::Handle<JSObject*> obj) const
{
  AutoVersionChecker avc(cx);
  for (nsXBLProtoImplField* f = mFields; f; f = f->GetNext()) {
    // Using OBJ_LOOKUP_PROPERTY is a pain, since what we have is a
    // PRUnichar* for the property name.  Let's just use the public API and
    // all.
    nsDependentString name(f->GetName());
    JS::Rooted<JS::Value> dummy(cx);
    if (!::JS_LookupUCProperty(cx, obj,
                               reinterpret_cast<const jschar*>(name.get()),
                               name.Length(), dummy.address())) {
      return false;
    }
  }

  return true;
}

void
nsXBLProtoImpl::UndefineFields(JSContext *cx, JS::Handle<JSObject*> obj) const
{
  JSAutoRequest ar(cx);
  for (nsXBLProtoImplField* f = mFields; f; f = f->GetNext()) {
    nsDependentString name(f->GetName());

    const jschar* s = reinterpret_cast<const jschar*>(name.get());
    JSBool hasProp;
    if (::JS_AlreadyHasOwnUCProperty(cx, obj, s, name.Length(), &hasProp) &&
        hasProp) {
      JS::Rooted<JS::Value> dummy(cx);
      ::JS_DeleteUCProperty2(cx, obj, s, name.Length(), dummy.address());
    }
  }
}

void
nsXBLProtoImpl::DestroyMembers()
{
  NS_ASSERTION(mClassObject, "This should never be called when there is no class object");

  delete mMembers;
  mMembers = nullptr;
  mConstructor = nullptr;
  mDestructor = nullptr;
}

nsresult
nsXBLProtoImpl::Read(nsIScriptContext* aContext,
                     nsIObjectInputStream* aStream,
                     nsXBLPrototypeBinding* aBinding,
                     nsIScriptGlobalObject* aGlobal)
{
  // Set up a class object first so that deserialization is possible
  AutoPushJSContext cx(aContext->GetNativeContext());
  JS::Rooted<JSObject*> global(cx, aGlobal->GetGlobalJSObject());

  JS::Rooted<JSObject*> classObject(cx);
  bool classObjectIsNew = false;
  nsresult rv = aBinding->InitClass(mClassName, cx, global, global, &classObject,
                                    &classObjectIsNew);
  NS_ENSURE_SUCCESS(rv, rv);
  MOZ_ASSERT(classObject);
  MOZ_ASSERT(classObjectIsNew);

  mClassObject = classObject;

  nsXBLProtoImplField* previousField = nullptr;
  nsXBLProtoImplMember* previousMember = nullptr;

  do {
    XBLBindingSerializeDetails type;
    rv = aStream->Read8(&type);
    NS_ENSURE_SUCCESS(rv, rv);
    if (type == XBLBinding_Serialize_NoMoreItems)
      break;

    switch (type & XBLBinding_Serialize_Mask) {
      case XBLBinding_Serialize_Field:
      {
        nsXBLProtoImplField* field =
          new nsXBLProtoImplField(type & XBLBinding_Serialize_ReadOnly);
        rv = field->Read(aContext, aStream);
        if (NS_FAILED(rv)) {
          delete field;
          return rv;
        }

        if (previousField) {
          previousField->SetNext(field);
        }
        else {
          mFields = field;
        }
        previousField = field;

        break;
      }
      case XBLBinding_Serialize_GetterProperty:
      case XBLBinding_Serialize_SetterProperty:
      case XBLBinding_Serialize_GetterSetterProperty:
      {
        nsAutoString name;
        nsresult rv = aStream->ReadString(name);
        NS_ENSURE_SUCCESS(rv, rv);

        nsXBLProtoImplProperty* prop =
          new nsXBLProtoImplProperty(name.get(), type & XBLBinding_Serialize_ReadOnly);
        rv = prop->Read(aContext, aStream, type & XBLBinding_Serialize_Mask);
        if (NS_FAILED(rv)) {
          delete prop;
          return rv;
        }

        previousMember = AddMember(prop, previousMember);
        break;
      }
      case XBLBinding_Serialize_Method:
      {
        nsAutoString name;
        rv = aStream->ReadString(name);
        NS_ENSURE_SUCCESS(rv, rv);

        nsXBLProtoImplMethod* method = new nsXBLProtoImplMethod(name.get());
        rv = method->Read(aContext, aStream);
        if (NS_FAILED(rv)) {
          delete method;
          return rv;
        }

        previousMember = AddMember(method, previousMember);
        break;
      }
      case XBLBinding_Serialize_Constructor:
      {
        mConstructor = new nsXBLProtoImplAnonymousMethod();
        rv = mConstructor->Read(aContext, aStream);
        if (NS_FAILED(rv)) {
          delete mConstructor;
          mConstructor = nullptr;
          return rv;
        }

        previousMember = AddMember(mConstructor, previousMember);
        break;
      }
      case XBLBinding_Serialize_Destructor:
      {
        mDestructor = new nsXBLProtoImplAnonymousMethod();
        rv = mDestructor->Read(aContext, aStream);
        if (NS_FAILED(rv)) {
          delete mDestructor;
          mDestructor = nullptr;
          return rv;
        }

        previousMember = AddMember(mDestructor, previousMember);
        break;
      }
      default:
        NS_ERROR("Unexpected binding member type");
        break;
    }
  } while (1);

  return NS_OK;
}

nsresult
nsXBLProtoImpl::Write(nsIScriptContext* aContext,
                      nsIObjectOutputStream* aStream,
                      nsXBLPrototypeBinding* aBinding)
{
  nsresult rv;

  if (!mClassObject) {
    rv = CompilePrototypeMembers(aBinding);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  rv = aStream->WriteStringZ(mClassName.get());
  NS_ENSURE_SUCCESS(rv, rv);

  for (nsXBLProtoImplField* curr = mFields; curr; curr = curr->GetNext()) {
    rv = curr->Write(aContext, aStream);
    NS_ENSURE_SUCCESS(rv, rv);
  }
  for (nsXBLProtoImplMember* curr = mMembers; curr; curr = curr->GetNext()) {
    if (curr == mConstructor) {
      rv = mConstructor->Write(aContext, aStream, XBLBinding_Serialize_Constructor);
    }
    else if (curr == mDestructor) {
      rv = mDestructor->Write(aContext, aStream, XBLBinding_Serialize_Destructor);
    }
    else {
      rv = curr->Write(aContext, aStream);
    }
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return aStream->Write8(XBLBinding_Serialize_NoMoreItems);
}

nsresult
NS_NewXBLProtoImpl(nsXBLPrototypeBinding* aBinding, 
                   const PRUnichar* aClassName, 
                   nsXBLProtoImpl** aResult)
{
  nsXBLProtoImpl* impl = new nsXBLProtoImpl();
  if (!impl)
    return NS_ERROR_OUT_OF_MEMORY;
  if (aClassName)
    impl->mClassName.AssignWithConversion(aClassName);
  else
    aBinding->BindingURI()->GetSpec(impl->mClassName);
  aBinding->SetImplementation(impl);
  *aResult = impl;

  return NS_OK;
}

