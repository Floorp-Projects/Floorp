/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef nsXBLProtoImpl_h__
#define nsXBLProtoImpl_h__

#include "nsMemory.h"
#include "nsIXBLPrototypeBinding.h"
#include "nsIXBLPrototypeHandler.h"
#include "nsXBLProtoImplMember.h"

MOZ_DECL_CTOR_COUNTER(nsXBLProtoImpl);

class nsXBLProtoImpl
{
public:
  nsXBLProtoImpl() 
    :mClassObject(nsnull) 
  { 
    MOZ_COUNT_CTOR(nsXBLProtoImpl); 
    mMembers = nsnull; 
  };
  ~nsXBLProtoImpl() 
  { 
    MOZ_COUNT_DTOR(nsXBLProtoImpl); 
    for (nsXBLProtoImplMember* curr = mMembers; curr; curr=curr->GetNext())
      curr->Destroy(mClassObject != nsnull);
    delete mMembers; 
  };
  
  nsresult InstallImplementation(nsIXBLPrototypeBinding* aBinding, nsIContent* aBoundElement);
  nsresult InitTargetObjects(nsIXBLPrototypeBinding* aBinding, nsIScriptContext* aContext, 
                             nsIContent* aBoundElement, 
                             void** aScriptObject, void** aTargetClassObject);
  nsresult CompilePrototypeMembers(nsIXBLPrototypeBinding* aBinding);

  void SetMemberList(nsXBLProtoImplMember* aMemberList) { delete mMembers; mMembers = aMemberList; };

public:
  nsCString mClassName; // The name of the class. 
  void* mClassObject;   // The class object for the binding. We'll use this to pre-compile properties 
                        // and methods for the binding.

  nsXBLProtoImplMember* mMembers; // The members of an implementation are chained in this singly-linked list.
  
  nsCOMPtr<nsIXBLPrototypeHandler> mConstructor; // Our class constructor.
  nsCOMPtr<nsIXBLPrototypeHandler> mDestructor;  // Our class destructor.
};

static nsresult
NS_NewXBLProtoImpl(nsIXBLPrototypeBinding* aBinding, 
                   const nsAReadableString* aClassName, 
                   nsXBLProtoImpl** aResult) {
  nsXBLProtoImpl* impl = new nsXBLProtoImpl();
  if (!impl)
    return NS_ERROR_OUT_OF_MEMORY;
  if (aClassName)
    impl->mClassName.AssignWithConversion(*aClassName);
  else
    aBinding->GetBindingURI(impl->mClassName);
  aBinding->SetImplementation(impl);
  *aResult = impl;
  return NS_OK;
}

#endif // nsXBLProtoImpl_h__
