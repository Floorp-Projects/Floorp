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
 * Original Author: Scott MacGregor (mscott@netscape.com)
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
  static nsIAtom* kFieldAtom;
  static nsIAtom* kOnSetAtom;
  static nsIAtom* kOnGetAtom;
  static nsIAtom* kGetterAtom;
  static nsIAtom* kSetterAtom;
  static nsIAtom* kNameAtom;
  static nsIAtom* kReadOnlyAtom;
  
  JSObject * mJSMethodObject; // precompiled JS for a method
  JSObject * mJSGetterObject; // precompiled JS for a getter property
  JSObject * mJSSetterObject; // precompiled JS for a setter property
  nsString mFieldString; // a field's raw value.

  JSObject* mClassObject;
  
  nsString mName;         // name of the property or field

  uintN mJSAttributes;
  nsWeakPtr mPrototypeBinding; // weak reference back to the proto type binding which owns us.

  nsCOMPtr<nsIContent> mInterfaceElement;
  nsCOMPtr<nsIContent> mPropertyElement;
  PRBool mPropertyIsCompiled; 
  nsCString mClassStr; 

protected:
  nsresult GetTextData(nsIContent *aParent, nsString& aResult);
  nsresult ParseMethod(nsIScriptContext * aContext);
  nsresult ParseProperty(nsIScriptContext * aContext);
  nsresult ParseField(nsIScriptContext * aContext);

  nsresult DelayedPropertyConstruction();
};

extern nsresult
NS_NewXBLPrototypeProperty(nsIXBLPrototypeBinding * aProtoTypeBinding, nsIXBLPrototypeProperty** aResult);

#endif // nsXBLPrototypeProperty_h__
