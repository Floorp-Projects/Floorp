/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsXBLProtoImpl_h__
#define nsXBLProtoImpl_h__

#include "nsMemory.h"
#include "nsXBLPrototypeHandler.h"
#include "nsXBLProtoImplMember.h"
#include "nsXBLProtoImplField.h"
#include "nsXBLBinding.h"

class nsXBLPrototypeBinding;
class nsXBLProtoImplAnonymousMethod;

class nsXBLProtoImpl MOZ_FINAL
{
public:
  nsXBLProtoImpl()
    : mClassObject(nullptr),
      mMembers(nullptr),
      mFields(nullptr),
      mConstructor(nullptr),
      mDestructor(nullptr)
  {
    MOZ_COUNT_CTOR(nsXBLProtoImpl);
  }
  ~nsXBLProtoImpl()
  {
    MOZ_COUNT_DTOR(nsXBLProtoImpl);
    // Note: the constructor and destructor are in mMembers, so we'll
    // clean them up automatically.
    delete mMembers;
    delete mFields;
  }


  nsresult InstallImplementation(nsXBLPrototypeBinding* aPrototypeBinding, nsXBLBinding* aBinding);

private:
  nsresult InitTargetObjects(nsXBLPrototypeBinding* aBinding,
                             nsIContent* aBoundElement,
                             JS::MutableHandle<JSObject*> aTargetClassObject,
                             bool* aTargetIsNew);

public:
  nsresult CompilePrototypeMembers(nsXBLPrototypeBinding* aBinding);

  bool LookupMember(JSContext* aCx, nsString& aName, JS::Handle<jsid> aNameAsId,
                    JS::MutableHandle<JSPropertyDescriptor> aDesc,
                    JSObject* aClassObject);

  void SetMemberList(nsXBLProtoImplMember* aMemberList)
  {
    delete mMembers;
    mMembers = aMemberList;
  }

  void SetFieldList(nsXBLProtoImplField* aFieldList)
  {
    delete mFields;
    mFields = aFieldList;
  }

  void Trace(const TraceCallbacks& aCallbacks, void *aClosure);
  void UnlinkJSObjects();

  nsXBLProtoImplField* FindField(const nsString& aFieldName) const;

  // Resolve all the fields for this implementation on the object |obj| False
  // return means a JS exception was set.
  bool ResolveAllFields(JSContext *cx, JS::Handle<JSObject*> obj) const;

  // Undefine all our fields from object |obj| (which should be a
  // JSObject for a bound element).
  void UndefineFields(JSContext* cx, JS::Handle<JSObject*> obj) const;

  bool CompiledMembers() const {
    return mClassObject != nullptr;
  }

  nsresult Read(nsIObjectInputStream* aStream,
                nsXBLPrototypeBinding* aBinding);
  nsresult Write(nsIObjectOutputStream* aStream,
                 nsXBLPrototypeBinding* aBinding);

protected:
  // used by Read to add each member
  nsXBLProtoImplMember* AddMember(nsXBLProtoImplMember* aMember,
                                  nsXBLProtoImplMember* aPreviousMember)
  {
    if (aPreviousMember)
      aPreviousMember->SetNext(aMember);
    else
      mMembers = aMember;
    return aMember;
  }

  void DestroyMembers();

public:
  nsCString mClassName; // The name of the class.

protected:
  JSObject* mClassObject; // The class object for the binding. We'll use this to pre-compile properties
                          // and methods for the binding.

  nsXBLProtoImplMember* mMembers; // The members of an implementation are chained in this singly-linked list.

  nsXBLProtoImplField* mFields; // Our fields

public:
  nsXBLProtoImplAnonymousMethod* mConstructor; // Our class constructor.
  nsXBLProtoImplAnonymousMethod* mDestructor;  // Our class destructor.
};

nsresult
NS_NewXBLProtoImpl(nsXBLPrototypeBinding* aBinding,
                   const char16_t* aClassName,
                   nsXBLProtoImpl** aResult);

#endif // nsXBLProtoImpl_h__
