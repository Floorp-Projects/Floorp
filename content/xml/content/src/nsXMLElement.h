/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are Copyright (C) 1998
 * Netscape Communications Corporation.  All Rights Reserved.
 */

#ifndef nsXMLElement_h___
#define nsXMLElement_h___

#include "nsIDOMElement.h"
#include "nsIScriptObjectOwner.h"
#include "nsIDOMEventReceiver.h"
#include "nsIXMLContent.h"
#include "nsIJSScriptObject.h"
#include "nsGenericElement.h"

class nsIDocument;
class nsIAtom;
class nsIEventListenerManager;
class nsIHTMLAttributes;

class nsXMLElement : public nsIDOMElement,
		     public nsIScriptObjectOwner,
		     public nsIDOMEventReceiver,
		     public nsIXMLContent,
		     public nsIJSScriptObject
{
public:
  nsXMLElement(nsIAtom *aTag);
  ~nsXMLElement();

  // nsISupports
  NS_DECL_ISUPPORTS
       
  // nsIDOMNode
  NS_IMPL_IDOMNODE_USING_GENERIC(mInner)
  
  // nsIDOMElement
  NS_IMPL_IDOMELEMENT_USING_GENERIC(mInner)

  // nsIScriptObjectOwner
  NS_IMETHOD GetScriptObject(nsIScriptContext* aContext, void** aScriptObject);
  NS_IMETHOD SetScriptObject(void *aScriptObject);

  // nsIContent
  NS_IMPL_ICONTENT_USING_GENERIC(mInner)

  // nsIXMLContent
  NS_IMETHOD SetNameSpace(nsIAtom* aNameSpace);
  NS_IMETHOD GetNameSpace(nsIAtom*& aNameSpace);
  NS_IMETHOD SetNameSpaceIdentifier(PRInt32 aNameSpaceId);
  NS_IMETHOD GetNameSpaceIdentifier(PRInt32& aNameSpeceId);

  // nsIDOMEventReceiver
  NS_IMPL_IDOMEVENTRECEIVER_USING_GENERIC(mInner)

  // nsIJSScriptObject
  virtual PRBool    AddProperty(JSContext *aContext, jsval aID, jsval *aVp) {
    return mInner.AddProperty(aContext, aID, aVp);
  }
  virtual PRBool   DeleteProperty(JSContext *aContext, jsval aID, jsval *aVp) {
    return mInner.DeleteProperty(aContext, aID, aVp);
  }
  virtual PRBool    GetProperty(JSContext *aContext, jsval aID, jsval *aVp) {
    return mInner.GetProperty(aContext, aID, aVp);
  }
  virtual PRBool    SetProperty(JSContext *aContext, jsval aID, jsval *aVp) {
    return mInner.SetProperty(aContext, aID, aVp);
  }
  virtual PRBool    EnumerateProperty(JSContext *aContext) {
    return mInner.EnumerateProperty(aContext);
  }
  virtual PRBool    Resolve(JSContext *aContext, jsval aID) {
    return mInner.Resolve(aContext, aID);
  }
  virtual PRBool    Convert(JSContext *aContext, jsval aID) {
    return mInner.Convert(aContext, aID);
  }
  virtual void      Finalize(JSContext *aContext) {
    mInner.Finalize(aContext);
  }

protected:
  nsGenericContainerElement mInner;
  
  nsIAtom* mNameSpace;
  PRInt32 mNameSpaceId;
  void *mScriptObject;
};

#endif // nsXMLElement_h___
