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
 * Contributor(s): 
 */

#ifndef nsXMLElement_h___
#define nsXMLElement_h___

#include "nsCOMPtr.h"
#include "nsIDOMElement.h"
#include "nsIScriptObjectOwner.h"
#include "nsIDOMEventReceiver.h"
#include "nsIXMLContent.h"
#include "nsIJSScriptObject.h"
#include "nsGenericXMLElement.h"

class nsIDocument;
class nsIAtom;
class nsINodeInfo;
class nsIEventListenerManager;
class nsIHTMLAttributes;
class nsIURI;
class nsIWebShell;

class nsXMLElement : public nsIDOMElement,
		     public nsIXMLContent,
		     public nsIJSScriptObject
{
public:
  nsXMLElement(nsINodeInfo *aNodeInfo);
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
  NS_IMETHOD SetDocument(nsIDocument* aDocument, PRBool aDeep, PRBool aCompileEventHandlers) {
    return mInner.SetDocument(aDocument, aDeep, aCompileEventHandlers);
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
  NS_IMETHOD GetNodeInfo(nsINodeInfo*& aResult) const {
    return mInner.GetNodeInfo(aResult);
  }
  NS_IMETHOD ParseAttributeString(const nsAReadableString& aStr,
                                  nsIAtom*& aName,
                                  PRInt32& aNameSpaceID) {
    return mInner.ParseAttributeString(aStr, aName, aNameSpaceID);
  }
  NS_IMETHOD GetNameSpacePrefixFromId(PRInt32 aNameSpaceID,
                                      nsIAtom*& aPrefix) {
    return mInner.GetNameSpacePrefixFromId(aNameSpaceID, aPrefix);
  }
  NS_IMETHOD SetAttribute(PRInt32 aNameSpaceID, nsIAtom* aName, const nsAReadableString& aValue,
                          PRBool aNotify);
  NS_IMETHOD SetAttribute(nsINodeInfo *aNodeInfo, const nsAReadableString& aValue,
                          PRBool aNotify);
  NS_IMETHOD GetAttribute(PRInt32 aNameSpaceID, nsIAtom* aName,
                          nsAWritableString& aResult) const {
    return mInner.GetAttribute(aNameSpaceID, aName, aResult);
  }
  NS_IMETHOD GetAttribute(PRInt32 aNameSpaceID, nsIAtom* aName,
                          nsIAtom*& aPrefix, nsAWritableString& aResult) const {
    return mInner.GetAttribute(aNameSpaceID, aName, aPrefix, aResult);
  }
  NS_IMETHOD UnsetAttribute(PRInt32 aNameSpaceID, nsIAtom* aName, PRBool aNotify) {
    return mInner.UnsetAttribute(aNameSpaceID, aName, aNotify);
  }
  NS_IMETHOD GetAttributeNameAt(PRInt32 aIndex,
                                PRInt32& aNameSpaceID, 
                                nsIAtom*& aName,
                                nsIAtom*& aPrefix) const {
    return mInner.GetAttributeNameAt(aIndex, aNameSpaceID, aName, aPrefix);
  }
  NS_IMETHOD GetAttributeCount(PRInt32& aResult) const {
    return mInner.GetAttributeCount(aResult);
  }
  NS_IMETHOD List(FILE* out, PRInt32 aIndent) const {
    return mInner.List(out, aIndent);
  }
  NS_IMETHOD DumpContent(FILE* out, PRInt32 aIndent,PRBool aDumpAll) const {
    return mInner.DumpContent(out, aIndent,aDumpAll);
  } 
  NS_IMETHOD BeginConvertToXIF(nsIXIFConverter* aConverter) const {
    return mInner.BeginConvertToXIF(aConverter);
  }
  NS_IMETHOD ConvertContentToXIF(nsIXIFConverter* aConverter) const {
    return mInner.ConvertContentToXIF(aConverter);
  }
  NS_IMETHOD FinishConvertToXIF(nsIXIFConverter* aConverter) const {
    return mInner.FinishConvertToXIF(aConverter);
  }
  NS_IMETHOD HandleDOMEvent(nsIPresContext* aPresContext,
                            nsEvent* aEvent,
                            nsIDOMEvent** aDOMEvent,
                            PRUint32 aFlags,
                            nsEventStatus* aEventStatus);

  NS_IMETHOD GetContentID(PRUint32* aID) {
    *aID = mContentID;
    return NS_OK;
  }
  NS_IMETHOD SetContentID(PRUint32 aID) {
    mContentID = aID;
    return NS_OK;
  }

  NS_IMETHOD RangeAdd(nsIDOMRange& aRange) {  
    return mInner.RangeAdd(aRange); 
  } 
  NS_IMETHOD RangeRemove(nsIDOMRange& aRange) {
    return mInner.RangeRemove(aRange); 
  }                                                                        
  NS_IMETHOD GetRangeList(nsVoidArray*& aResult) const {
    return mInner.GetRangeList(aResult); 
  }          
  
  NS_IMETHOD SetFocus(nsIPresContext* aPresContext) {
    return mInner.SetFocus(aPresContext);
  }
  NS_IMETHOD RemoveFocus(nsIPresContext* aPresContext) {
    return mInner.RemoveFocus(aPresContext);
  }

  NS_IMETHOD GetBindingParent(nsIContent** aContent) {
    return mInner.GetBindingParent(aContent);
  }

  NS_IMETHOD SetBindingParent(nsIContent* aParent) {
    return mInner.SetBindingParent(aParent);
  }  

  NS_IMETHOD SizeOf(nsISizeOfHandler* aSizer, PRUint32* aResult) const {
    if (!aResult) {
      return NS_ERROR_NULL_POINTER;
    }
#ifdef DEBUG
    *aResult = sizeof(*this);
#else
    *aResult = 0;
#endif
    return NS_OK;
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
  NS_IMETHOD MaybeTriggerAutoLink(nsIWebShell *aShell);

  // nsIJSScriptObject
  virtual PRBool    AddProperty(JSContext *aContext, JSObject *aObj, jsval aID, jsval *aVp) {
    return mInner.AddProperty(aContext, aObj, aID, aVp);
  }
  virtual PRBool   DeleteProperty(JSContext *aContext, JSObject *aObj, jsval aID, jsval *aVp) {
    return mInner.DeleteProperty(aContext, aObj, aID, aVp);
  }
  virtual PRBool    GetProperty(JSContext *aContext, JSObject *aObj, jsval aID, jsval *aVp) {
    return mInner.GetProperty(aContext, aObj, aID, aVp);
  }
  virtual PRBool    SetProperty(JSContext *aContext, JSObject *aObj, jsval aID, jsval *aVp) {
    return mInner.SetProperty(aContext, aObj, aID, aVp);
  }
  virtual PRBool    EnumerateProperty(JSContext *aContext, JSObject *aObj) {
    return mInner.EnumerateProperty(aContext, aObj);
  }
  virtual PRBool    Resolve(JSContext *aContext, JSObject *aObj, jsval aID) {
    return mInner.Resolve(aContext, aObj, aID);
  }
  virtual PRBool    Convert(JSContext *aContext, JSObject *aObj, jsval aID) {
    return mInner.Convert(aContext, aObj, aID);
  }
  virtual void      Finalize(JSContext *aContext, JSObject *aObj) {
    mInner.Finalize(aContext, aObj);
  }

protected:
  nsresult GetXMLBaseURI(nsIURI **aURI);  // XXX This should perhaps be moved to nsIXMLContent
  nsGenericXMLElement mInner;
  PRBool mIsLink;
  PRUint32 mContentID;
};

#endif // nsXMLElement_h___
