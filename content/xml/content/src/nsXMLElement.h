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
#include "nsGenericXMLElement.h"

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
  virtual ~nsXMLElement();

  // nsISupports
  NS_DECL_ISUPPORTS
       
  // nsIDOMNode
  NS_IMPL_IDOMNODE_USING_GENERIC(mInner)
  
  // nsIDOMElement
  NS_IMPL_IDOMELEMENT_USING_GENERIC(mInner)

  // nsIScriptObjectOwner
  NS_IMPL_ISCRIPTOBJECTOWNER_USING_GENERIC(mInner)

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
  NS_IMETHOD GetNameSpaceID(PRInt32& aNameSpaceId) const {
    return mInner.GetNameSpaceID(aNameSpaceId);
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
  NS_IMETHOD GetTag(nsIAtom*& aResult) const {
    return mInner.GetTag(aResult);
  }
  NS_IMETHOD ParseAttributeString(const nsString& aStr,
                                  nsIAtom*& aName,
                                  PRInt32& aNameSpaceID) {
    return mInner.ParseAttributeString(aStr, aName, aNameSpaceID);
  }
  NS_IMETHOD GetNameSpacePrefixFromId(PRInt32 aNameSpaceID,
                                      nsIAtom*& aPrefix) {
    return mInner.GetNameSpacePrefixFromId(aNameSpaceID, aPrefix);
  }
  NS_IMETHOD SetAttribute(PRInt32 aNameSpaceID, nsIAtom* aName, const nsString& aValue,
                          PRBool aNotify);
  NS_IMETHOD GetAttribute(PRInt32 aNameSpaceID, nsIAtom* aName,
                          nsString& aResult) const {
    return mInner.GetAttribute(aNameSpaceID, aName, aResult);
  }
  NS_IMETHOD UnsetAttribute(PRInt32 aNameSpaceID, nsIAtom* aName, PRBool aNotify) {
    return mInner.UnsetAttribute(aNameSpaceID, aName, aNotify);
  }
  NS_IMETHOD GetAttributeNameAt(PRInt32 aIndex,
                                PRInt32& aNameSpaceID, 
                                nsIAtom*& aName) const {
    return mInner.GetAttributeNameAt(aIndex, aNameSpaceID, aName);
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
  NS_IMETHOD HandleDOMEvent(nsIPresContext& aPresContext,
                            nsEvent* aEvent,
                            nsIDOMEvent** aDOMEvent,
                            PRUint32 aFlags,
                            nsEventStatus& aEventStatus);
  NS_IMETHOD RangeAdd(nsIDOMRange& aRange) {  
    return mInner.RangeAdd(aRange); 
  } 
  NS_IMETHOD RangeRemove(nsIDOMRange& aRange) {
    return mInner.RangeRemove(aRange); 
  }                                                                        
  NS_IMETHOD GetRangeList(nsVoidArray*& aResult) const {
    return mInner.GetRangeList(aResult); 
  }                                                                        

  // nsIXMLContent
  NS_IMETHOD SetContainingNameSpace(nsINameSpace* aNameSpace)  {
    return mInner.SetContainingNameSpace(aNameSpace);
  }
  NS_IMETHOD GetContainingNameSpace(nsINameSpace*& aNameSpace) const  {
    return mInner.GetContainingNameSpace(aNameSpace);
  }
  NS_IMETHOD SetNameSpacePrefix(nsIAtom* aNameSpace) {
    return mInner.SetNameSpacePrefix(aNameSpace);
  }
  NS_IMETHOD GetNameSpacePrefix(nsIAtom*& aNameSpace) const {
    return mInner.GetNameSpacePrefix(aNameSpace);
  }
  NS_IMETHOD SetNameSpaceID(PRInt32 aNameSpaceId) {
    return mInner.SetNameSpaceID(aNameSpaceId);
  }

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
  nsGenericXMLElement mInner;
  PRBool mIsLink;
};

#endif // nsXMLElement_h___
