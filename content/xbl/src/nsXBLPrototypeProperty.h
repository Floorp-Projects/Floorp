/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Original Author: Scott MacGregor (mscott@netscape.com)
 *
 * Contributor(s): 
 */

#ifndef nsXBLPrototypeProperty_h__
#define nsXBLPrototypeProperty_h__

#include "nsIXBLPrototypeProperty.h"
#include "nsIXBLPrototypeBinding.h"
#include "nsIAtom.h"
#include "nsString.h"
#include "jsapi.h"
#include "nsIContent.h"
#include "nsString.h"
#include "nsWeakReference.h"

class nsIXBLBinding;
class nsIDocument;
class nsIXBLPrototypeBinding;

class nsXBLPrototypeProperty : public nsIXBLPrototypeProperty
{
public:
  nsXBLPrototypeProperty(nsIXBLPrototypeBinding * aPrototypeBinding);
  virtual ~nsXBLPrototypeProperty();
  
  NS_DECL_ISUPPORTS

  NS_IMETHOD GetNextProperty(nsIXBLPrototypeProperty** aProperty);
  NS_IMETHOD SetNextProperty(nsIXBLPrototypeProperty* aProperty);
  NS_IMETHOD ConstructProperty(nsIContent * aInterfaceElement, nsIContent* aPropertyElement);

  NS_IMETHOD InstallProperty(nsIScriptContext * aContext, nsIContent * aBoundElement, void * aScopeObject, void * aTargetClassObject);
  NS_IMETHOD InitTargetObjects(nsIScriptContext * aContext, nsIContent * aBoundElement, void ** aScriptObject, void ** aTargetClassObject);
  static PRUint32 gRefCnt;

protected:
  nsCOMPtr<nsIXBLPrototypeProperty> mNextProperty; // Prototype properties are chained. We own the next handler in the chain.
  
  static nsIAtom* kMethodAtom;
  static nsIAtom* kParameterAtom;
  static nsIAtom* kBodyAtom;
  static nsIAtom* kPropertyAtom;
  static nsIAtom* kOnSetAtom;
  static nsIAtom* kOnGetAtom;
  static nsIAtom* kGetterAtom;
  static nsIAtom* kSetterAtom;
  static nsIAtom* kNameAtom;
  static nsIAtom* kReadOnlyAtom;
  
  JSObject * mJSMethodObject; // precompiled JS for a method
  JSObject * mJSGetterObject; // precompiled JS for a getter property
  JSObject * mJSSetterObject; // precompiled JS for a setter property
  nsString mLiteralPropertyString; // the property is just a literal string

  JSObject* mClassObject;
  
  nsString mName;         // name of the property

  uintN mJSAttributes;
  nsWeakPtr mPrototypeBinding; // weak reference back to the proto type binding which owns us.

  nsCOMPtr<nsIContent> mInterfaceElement;
  nsCOMPtr<nsIContent> mPropertyElement;
  PRBool mPropertyIsCompiled; 
  nsCString mClassStr; 

protected:
  nsresult GetTextData(nsIContent *aParent, nsString& aResult);
  nsresult ParseMethod(nsIScriptContext * aContext, nsIContent * aNode, const char * aClassStr);
  nsresult ParseProperty(nsIScriptContext * aContext, nsIContent * aNode, const char * aClassStr);
  nsresult ParseLiteral(nsIScriptContext * aContext, nsIContent* aPropertyElement);

  nsresult DelayedPropertyConstruction();
};

extern nsresult
NS_NewXBLPrototypeProperty(nsIXBLPrototypeBinding * aProtoTypeBinding, nsIXBLPrototypeProperty** aResult);

#endif // nsXBLPrototypeProperty_h__
