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
  NS_IMETHOD GetDocument(nsIDocument*& aResult) const {
    return mInner.GetDocument(aResult);
  }
  NS_IMETHOD SetDocument(nsIDocument* aDocument, PRBool aDeep) {
    return mInner.SetDocument(aDocument, aDeep);
  }
  NS_IMETHOD GetParent(nsIContent*& aResult) const {
    return mInner.GetParent(aResult);
  }
  NS_IMETHOD SetParent(nsIContent* aParent) {
    return mInner.SetParent(aParent);
  }
  NS_IMETHOD CanContainChildren(PRBool& aResult) const {
    return mInner.CanContainChildren(aResult);
  }
  NS_IMETHOD ChildCount(PRInt32& aResult) const {
    return mInner.ChildCount(aResult);
  }
  NS_IMETHOD ChildAt(PRInt32 aIndex, nsIContent*& aResult) const {
    return mInner.ChildAt(aIndex, aResult);
  }
  NS_IMETHOD IndexOf(nsIContent* aPossibleChild, PRInt32& aResult) const {
    return mInner.IndexOf(aPossibleChild, aResult);
  }
  NS_IMETHOD InsertChildAt(nsIContent* aKid, PRInt32 aIndex,
                           PRBool aNotify) {
    return mInner.InsertChildAt(aKid, aIndex, aNotify);
  }
  NS_IMETHOD ReplaceChildAt(nsIContent* aKid, PRInt32 aIndex,
                            PRBool aNotify) {
    return mInner.ReplaceChildAt(aKid, aIndex, aNotify);
  }
  NS_IMETHOD AppendChildTo(nsIContent* aKid, PRBool aNotify) {
    return mInner.AppendChildTo(aKid, aNotify);
  }
  NS_IMETHOD RemoveChildAt(PRInt32 aIndex, PRBool aNotify) {
    return mInner.RemoveChildAt(aIndex, aNotify);
  }
  NS_IMETHOD IsSynthetic(PRBool& aResult) {
    return mInner.IsSynthetic(aResult);
  }
  NS_IMETHOD GetNameSpaceID(PRInt32& aResult) const;
  NS_IMETHOD GetTag(nsIAtom*& aResult) const {
    return mInner.GetTag(aResult);
  }
  NS_IMETHOD SetAttribute(const nsString& aName, const nsString& aValue,
                          PRBool aNotify);
  NS_IMETHOD GetAttribute(const nsString& aName,
                          nsString& aResult) const {
    return mInner.GetAttribute(aName, aResult);
  }
  NS_IMETHOD UnsetAttribute(nsIAtom* aAttribute, PRBool aNotify) {
    return mInner.UnsetAttribute(aAttribute, aNotify);
  }
  NS_IMETHOD GetAllAttributeNames(nsISupportsArray* aArray,
                                  PRInt32& aResult) const {
    return mInner.GetAllAttributeNames(aArray, aResult);
  }
  NS_IMETHOD GetAttributeCount(PRInt32& aResult) const {
    return mInner.GetAttributeCount(aResult);
  }
  NS_IMETHOD List(FILE* out, PRInt32 aIndent) const {
    return mInner.List(out, aIndent);
  }
  NS_IMETHOD BeginConvertToXIF(nsXIFConverter& aConverter) const {
    return mInner.BeginConvertToXIF(aConverter);
  }
  NS_IMETHOD ConvertContentToXIF(nsXIFConverter& aConverter) const {
    return mInner.ConvertContentToXIF(aConverter);
  }
  NS_IMETHOD FinishConvertToXIF(nsXIFConverter& aConverter) const {
    return mInner.FinishConvertToXIF(aConverter);
  }
  NS_IMETHOD SizeOf(nsISizeOfHandler* aHandler) const {
    return mInner.SizeOf(aHandler);
  }
  NS_IMETHOD HandleDOMEvent(nsIPresContext& aPresContext,
                            nsEvent* aEvent,
                            nsIDOMEvent** aDOMEvent,
                            PRUint32 aFlags,
                            nsEventStatus& aEventStatus);

  // nsIXMLContent
  NS_IMETHOD SetNameSpacePrefix(nsIAtom* aNameSpace);
  NS_IMETHOD GetNameSpacePrefix(nsIAtom*& aNameSpace) const;
  NS_IMETHOD SetNameSpaceID(PRInt32 aNameSpaceId);

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
  
  nsIAtom* mNameSpacePrefix;
  PRInt32 mNameSpaceID;
  void *mScriptObject;
  PRBool mIsLink;
};

#endif // nsXMLElement_h___
