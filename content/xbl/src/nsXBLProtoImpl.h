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

class nsIXPConnectJSObjectHolder;
class nsXBLPrototypeBinding;
class nsXBLProtoImplAnonymousMethod;

class nsXBLProtoImpl
{
public:
  nsXBLProtoImpl() 
    : mClassObject(nsnull),
      mMembers(nsnull),
      mFields(nsnull),
      mConstructor(nsnull),
      mDestructor(nsnull)
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
  
  nsresult InstallImplementation(nsXBLPrototypeBinding* aBinding, nsIContent* aBoundElement);
  nsresult InitTargetObjects(nsXBLPrototypeBinding* aBinding, nsIScriptContext* aContext, 
                             nsIContent* aBoundElement, 
                             nsIXPConnectJSObjectHolder** aScriptObjectHolder,
                             JSObject** aTargetClassObject);
  nsresult CompilePrototypeMembers(nsXBLPrototypeBinding* aBinding);

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

  void Trace(TraceCallback aCallback, void *aClosure) const;
  void UnlinkJSObjects();

  nsXBLProtoImplField* FindField(const nsString& aFieldName) const;

  // Resolve all the fields for this implementation on the object |obj| False
  // return means a JS exception was set.
  bool ResolveAllFields(JSContext *cx, JSObject *obj) const;

  // Undefine all our fields from object |obj| (which should be a
  // JSObject for a bound element).
  void UndefineFields(JSContext* cx, JSObject* obj) const;

  bool CompiledMembers() const {
    return mClassObject != nsnull;
  }

  nsresult Read(nsIScriptContext* aContext,
                nsIObjectInputStream* aStream,
                nsXBLPrototypeBinding* aBinding,
                nsIScriptGlobalObject* aGlobal);
  nsresult Write(nsIScriptContext* aContext,
                 nsIObjectOutputStream* aStream,
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
                   const PRUnichar* aClassName, 
                   nsXBLProtoImpl** aResult);

#endif // nsXBLProtoImpl_h__
