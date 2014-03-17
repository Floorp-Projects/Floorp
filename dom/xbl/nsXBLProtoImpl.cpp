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
#include "nsIScriptContext.h"
#include "nsIXPConnect.h"
#include "nsIServiceManager.h"
#include "nsIDOMNode.h"
#include "nsXBLPrototypeBinding.h"
#include "nsXBLProtoImplProperty.h"
#include "nsIURI.h"
#include "mozilla/dom/XULElementBinding.h"
#include "xpcpublic.h"
#include "js/CharacterEncoding.h"

using namespace mozilla;
using js::GetGlobalForObjectCrossCompartment;
using js::AssertSameCompartment;

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
  JSContext* cx = context->GetNativeContext();
  AutoCxPusher pusher(cx);

  // InitTarget objects gives us back the JS object that represents the bound element and the
  // class object in the bound document that represents the concrete version of this implementation.
  // This function also has the side effect of building up the prototype implementation if it has
  // not been built already.
  JS::Rooted<JSObject*> targetClassObject(cx, nullptr);
  bool targetObjectIsNew = false;
  nsresult rv = InitTargetObjects(aPrototypeBinding,
                                  aBinding->GetBoundElement(),
                                  &targetClassObject,
                                  &targetObjectIsNew);
  NS_ENSURE_SUCCESS(rv, rv); // kick out if we were unable to properly intialize our target objects
  MOZ_ASSERT(targetClassObject);

  // Stash a strong reference to the JSClass in the binding.
  aBinding->SetJSClass(nsXBLJSClass::fromJSClass(JS_GetClass(targetClassObject)));

  // If the prototype already existed, we don't need to install anything. return early.
  if (!targetObjectIsNew)
    return NS_OK;

  // We want to define the canonical set of members in a safe place. If we're
  // using a separate XBL scope, we want to define them there first (so that
  // they'll be available for Xray lookups, among other things), and then copy
  // the properties to the content-side prototype as needed. We don't need to
  // bother about the field accessors here, since we don't use/support those
  // for in-content bindings.

  // First, start by entering the compartment of the XBL scope. This may or may
  // not be the same compartment as globalObject.
  JS::Rooted<JSObject*> globalObject(cx,
    GetGlobalForObjectCrossCompartment(targetClassObject));
  JS::Rooted<JSObject*> scopeObject(cx, xpc::GetXBLScope(cx, globalObject));
  NS_ENSURE_TRUE(scopeObject, NS_ERROR_OUT_OF_MEMORY);
  JSAutoCompartment ac(cx, scopeObject);

  // If they're different, create our safe holder object in the XBL scope.
  JS::Rooted<JSObject*> propertyHolder(cx);
  if (scopeObject != globalObject) {

    // This is just a property holder, so it doesn't need any special JSClass.
    propertyHolder = JS_NewObjectWithGivenProto(cx, nullptr, JS::NullPtr(), scopeObject);
    NS_ENSURE_TRUE(propertyHolder, NS_ERROR_OUT_OF_MEMORY);

    // Define it as a property on the scopeObject, using the same name used on
    // the content side.
    bool ok = JS_DefineProperty(cx, scopeObject,
                                js::GetObjectClass(targetClassObject)->name,
                                JS::ObjectValue(*propertyHolder), JS_PropertyStub,
                                JS_StrictPropertyStub,
                                JSPROP_PERMANENT | JSPROP_READONLY);
    NS_ENSURE_TRUE(ok, NS_ERROR_UNEXPECTED);
  } else {
    propertyHolder = targetClassObject;
  }

  // Walk our member list and install each one in turn on the XBL scope object.
  for (nsXBLProtoImplMember* curr = mMembers;
       curr;
       curr = curr->GetNext())
    curr->InstallMember(cx, propertyHolder);

  // Now, if we're using a separate XBL scope, enter the compartment of the
  // bound node and copy exposable properties to the prototype there. This
  // rewraps them appropriately, which should result in cross-compartment
  // function wrappers.
  if (propertyHolder != targetClassObject) {
    AssertSameCompartment(propertyHolder, scopeObject);
    AssertSameCompartment(targetClassObject, globalObject);
    for (nsXBLProtoImplMember* curr = mMembers; curr; curr = curr->GetNext()) {
      if (curr->ShouldExposeToUntrustedContent()) {
        JS::Rooted<jsid> id(cx);
        JS::TwoByteChars chars(curr->GetName(), NS_strlen(curr->GetName()));
        bool ok = JS_CharsToId(cx, chars, &id);
        NS_ENSURE_TRUE(ok, NS_ERROR_UNEXPECTED);
        JS_CopyPropertyFrom(cx, id, targetClassObject, propertyHolder);
        NS_ENSURE_TRUE(ok, NS_ERROR_UNEXPECTED);
      }
    }
  }

  // From here on out, work in the scope of the bound element.
  JSAutoCompartment ac2(cx, targetClassObject);

  // Install all of our field accessors.
  for (nsXBLProtoImplField* curr = mFields;
       curr;
       curr = curr->GetNext())
    curr->InstallAccessors(cx, targetClassObject);

  return NS_OK;
}

nsresult 
nsXBLProtoImpl::InitTargetObjects(nsXBLPrototypeBinding* aBinding,
                                  nsIContent* aBoundElement, 
                                  JS::MutableHandle<JSObject*> aTargetClassObject,
                                  bool* aTargetIsNew)
{
  nsresult rv = NS_OK;

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
  AutoJSContext cx;
  JS::Rooted<JSObject*> global(cx, sgo->GetGlobalJSObject());
  JS::Rooted<JS::Value> v(cx);

  JSAutoCompartment ac(cx, global);
  // Make sure the interface object is created before the prototype object
  // so that XULElement is hidden from content. See bug 909340.
  bool defineOnGlobal = dom::XULElementBinding::ConstructorEnabled(cx, global);
  dom::XULElementBinding::GetConstructorObject(cx, global, defineOnGlobal);

  rv = nsContentUtils::WrapNative(cx, global, aBoundElement, &v,
                                  /* aAllowWrapping = */ false);
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

  aBoundElement->PreserveWrapper(aBoundElement);

  return rv;
}

nsresult
nsXBLProtoImpl::CompilePrototypeMembers(nsXBLPrototypeBinding* aBinding)
{
  // We want to pre-compile our implementation's members against a "prototype context". Then when we actually 
  // bind the prototype to a real xbl instance, we'll clone the pre-compiled JS into the real instance's 
  // context.
  AutoSafeJSContext cx;
  JS::Rooted<JSObject*> compilationGlobal(cx, aBinding->XBLDocumentInfo()->GetCompilationGlobal());
  NS_ENSURE_TRUE(compilationGlobal, NS_ERROR_UNEXPECTED);
  JSAutoCompartment ac(cx, compilationGlobal);

  JS::Rooted<JSObject*> classObject(cx);
  bool classObjectIsNew = false;
  nsresult rv = aBinding->InitClass(mClassName, cx, compilationGlobal, compilationGlobal,
                                    &classObject, &classObjectIsNew);
  if (NS_FAILED(rv))
    return rv;

  MOZ_ASSERT(classObjectIsNew);
  MOZ_ASSERT(classObject);
  mClassObject = classObject;

  // Now that we have a class object installed, we walk our member list and compile each of our
  // properties and methods in turn.
  for (nsXBLProtoImplMember* curr = mMembers;
       curr;
       curr = curr->GetNext()) {
    nsresult rv = curr->CompileMember(mClassName, classObject);
    if (NS_FAILED(rv)) {
      DestroyMembers();
      return rv;
    }
  }

  return NS_OK;
}

bool
nsXBLProtoImpl::LookupMember(JSContext* aCx, nsString& aName,
                             JS::Handle<jsid> aNameAsId,
                             JS::MutableHandle<JSPropertyDescriptor> aDesc,
                             JS::Handle<JSObject*> aClassObject)
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
  for (nsXBLProtoImplField* f = mFields; f; f = f->GetNext()) {
    // Using OBJ_LOOKUP_PROPERTY is a pain, since what we have is a
    // char16_t* for the property name.  Let's just use the public API and
    // all.
    nsDependentString name(f->GetName());
    JS::Rooted<JS::Value> dummy(cx);
    if (!::JS_LookupUCProperty(cx, obj, name.get(), name.Length(), &dummy)) {
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

    const jschar* s = name.get();
    bool hasProp;
    if (::JS_AlreadyHasOwnUCProperty(cx, obj, s, name.Length(), &hasProp) &&
        hasProp) {
      bool dummy;
      ::JS_DeleteUCProperty2(cx, obj, s, name.Length(), &dummy);
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
nsXBLProtoImpl::Read(nsIObjectInputStream* aStream,
                     nsXBLPrototypeBinding* aBinding)
{
  AssertInCompilationScope();
  AutoJSContext cx;
  // Set up a class object first so that deserialization is possible
  JS::Rooted<JSObject*> global(cx, JS::CurrentGlobalOrNull(cx));

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
        rv = field->Read(aStream);
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
        rv = prop->Read(aStream, type & XBLBinding_Serialize_Mask);
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
        rv = method->Read(aStream);
        if (NS_FAILED(rv)) {
          delete method;
          return rv;
        }

        previousMember = AddMember(method, previousMember);
        break;
      }
      case XBLBinding_Serialize_Constructor:
      {
        nsAutoString name;
        rv = aStream->ReadString(name);
        NS_ENSURE_SUCCESS(rv, rv);

        mConstructor = new nsXBLProtoImplAnonymousMethod(name.get());
        rv = mConstructor->Read(aStream);
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
        nsAutoString name;
        rv = aStream->ReadString(name);
        NS_ENSURE_SUCCESS(rv, rv);

        mDestructor = new nsXBLProtoImplAnonymousMethod(name.get());
        rv = mDestructor->Read(aStream);
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
nsXBLProtoImpl::Write(nsIObjectOutputStream* aStream,
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
    rv = curr->Write(aStream);
    NS_ENSURE_SUCCESS(rv, rv);
  }
  for (nsXBLProtoImplMember* curr = mMembers; curr; curr = curr->GetNext()) {
    if (curr == mConstructor) {
      rv = mConstructor->Write(aStream, XBLBinding_Serialize_Constructor);
    }
    else if (curr == mDestructor) {
      rv = mDestructor->Write(aStream, XBLBinding_Serialize_Destructor);
    }
    else {
      rv = curr->Write(aStream);
    }
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return aStream->Write8(XBLBinding_Serialize_NoMoreItems);
}

nsresult
NS_NewXBLProtoImpl(nsXBLPrototypeBinding* aBinding, 
                   const char16_t* aClassName, 
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

