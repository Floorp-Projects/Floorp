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
#include "nsIDOMHTMLMapElement.h"
#include "nsIScriptObjectOwner.h"
#include "nsIDOMEventReceiver.h"
#include "nsIHTMLContent.h"
#include "nsGenericHTMLElement.h"
#include "nsHTMLAtoms.h"
#include "nsHTMLIIDs.h"
#include "nsIStyleContext.h"
#include "nsIMutableStyleContext.h"
#include "nsStyleConsts.h"
#include "nsIPresContext.h"
#include "GenericElementCollection.h"
#include "nsIDocument.h"
#include "nsIHTMLDocument.h"
#include "nsCOMPtr.h"

static NS_DEFINE_IID(kIDOMHTMLMapElementIID, NS_IDOMHTMLMAPELEMENT_IID);
static NS_DEFINE_IID(kIHTMLDocumentIID, NS_IHTMLDOCUMENT_IID);

class nsHTMLMapElement : public nsIDOMHTMLMapElement,
                         public nsIJSScriptObject,
                         public nsIHTMLContent
{
public:
  nsHTMLMapElement(nsINodeInfo *aNodeInfo);
  virtual ~nsHTMLMapElement();

  // nsISupports
  NS_DECL_ISUPPORTS

  // nsIDOMNode
  NS_IMPL_IDOMNODE_USING_GENERIC(mInner)

  // nsIDOMElement
  NS_IMPL_IDOMELEMENT_USING_GENERIC(mInner)

  // nsIDOMHTMLElement
  NS_IMPL_IDOMHTMLELEMENT_USING_GENERIC(mInner)

  // nsIDOMHTMLMapElement
  NS_DECL_IDOMHTMLMAPELEMENT

  // nsIJSScriptObject
  NS_IMPL_IJSSCRIPTOBJECT_USING_GENERIC(mInner)

  // nsIContent
  NS_IMETHOD GetDocument(nsIDocument*& aResult) const {
    return mInner.GetDocument(aResult);                                        
  }                                                                        
  NS_IMETHOD SetDocument(nsIDocument* aDocument, PRBool aDeep, PRBool aCompileEventHandlers);            
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
  NS_IMETHOD GetNameSpaceID(PRInt32& aResult) const {                      
    return mInner.GetNameSpaceID(aResult);                                     
  }                                                                        
  NS_IMETHOD GetTag(nsIAtom*& aResult) const {                             
    return mInner.GetTag(aResult);                                             
  }                                                                        
  NS_IMETHOD GetNodeInfo(nsINodeInfo*& aResult) const {
    return mInner.GetNodeInfo(aResult);
  }
  NS_IMETHOD NormalizeAttributeString(const nsAReadableString& aStr,
                                      nsINodeInfo*& aNodeInfo) {
    return mInner.NormalizeAttributeString(aStr, aNodeInfo);
  }                                                                        
  NS_IMETHOD SetAttribute(PRInt32 aNameSpaceID, nsIAtom* aName,            
                          const nsAReadableString& aValue, PRBool aNotify) {        
    return mInner.SetAttribute(aNameSpaceID, aName, aValue, aNotify);          
  }                                                                        
  NS_IMETHOD SetAttribute(nsINodeInfo* aNodeInfo,
                          const nsAReadableString& aValue, PRBool aNotify) {
    return mInner.SetAttribute(aNodeInfo, aValue, aNotify);
  }
  NS_IMETHOD GetAttribute(PRInt32 aNameSpaceID, nsIAtom* aName,            
                          nsAWritableString& aResult) const {                       
    return mInner.GetAttribute(aNameSpaceID, aName, aResult);                  
  }                                                                        
  NS_IMETHOD GetAttribute(PRInt32 aNameSpaceID, nsIAtom* aName,            
                          nsIAtom*& aPrefix, nsAWritableString& aResult) const {
    return mInner.GetAttribute(aNameSpaceID, aName, aPrefix, aResult);
  }
  NS_IMETHOD UnsetAttribute(PRInt32 aNameSpaceID, nsIAtom* aAttribute,     
                            PRBool aNotify) {                              
    return mInner.UnsetAttribute(aNameSpaceID, aAttribute, aNotify);           
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
    return mInner.GetContentID(aID);
  }
  NS_IMETHOD SetContentID(PRUint32 aID) {
    return mInner.SetContentID(aID);
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

  NS_IMETHOD SizeOf(nsISizeOfHandler* aSizer, PRUint32* aResult) const;

  // nsIHTMLContent
  NS_IMPL_IHTMLCONTENT_USING_GENERIC(mInner)

protected:
  nsGenericHTMLContainerElement mInner;
  GenericElementCollection* mAreas;
};

nsresult
NS_NewHTMLMapElement(nsIHTMLContent** aInstancePtrResult,
                     nsINodeInfo *aNodeInfo)
{
  NS_ENSURE_ARG_POINTER(aInstancePtrResult);
  NS_ENSURE_ARG_POINTER(aNodeInfo);

  nsIHTMLContent* it = new nsHTMLMapElement(aNodeInfo);
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return it->QueryInterface(kIHTMLContentIID, (void**) aInstancePtrResult);
}


nsHTMLMapElement::nsHTMLMapElement(nsINodeInfo *aNodeInfo)
{
  NS_INIT_REFCNT();
  mInner.Init(this, aNodeInfo);
  mAreas = nsnull;
}

nsHTMLMapElement::~nsHTMLMapElement()
{
  if (nsnull != mAreas) {
    mAreas->ParentDestroyed();
    NS_RELEASE(mAreas);
  }
}

NS_IMPL_ADDREF(nsHTMLMapElement)

NS_IMPL_RELEASE(nsHTMLMapElement)

nsresult
nsHTMLMapElement::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
  NS_IMPL_HTML_CONTENT_QUERY_INTERFACE(aIID, aInstancePtr, this)
  if (aIID.Equals(kIDOMHTMLMapElementIID)) {
    nsIDOMHTMLMapElement* tmp = this;
    *aInstancePtr = (void*) tmp;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  return NS_NOINTERFACE;
}

NS_IMETHODIMP 
nsHTMLMapElement::SetDocument(nsIDocument* aDocument, PRBool aDeep, PRBool aCompileEventHandlers)
{
  nsresult rv;
  
  if (nsnull != mInner.mDocument) {
    nsCOMPtr<nsIHTMLDocument> htmlDoc;
    
    rv = mInner.mDocument->QueryInterface(kIHTMLDocumentIID, 
                                          getter_AddRefs(htmlDoc));
    if (NS_OK == rv) {
      htmlDoc->RemoveImageMap(this);
    }
  }

  rv = mInner.SetDocument(aDocument, aDeep, aCompileEventHandlers);

  if (NS_SUCCEEDED(rv) && (nsnull != aDocument)) {
    nsCOMPtr<nsIHTMLDocument> htmlDoc;
    
    rv = aDocument->QueryInterface(kIHTMLDocumentIID, 
                                   getter_AddRefs(htmlDoc));
    if (NS_OK == rv) {
      htmlDoc->AddImageMap(this);
    }
  }
  
  return rv;
}

NS_IMETHODIMP
nsHTMLMapElement::CloneNode(PRBool aDeep, nsIDOMNode** aReturn)
{
  nsHTMLMapElement* it = new nsHTMLMapElement(mInner.mNodeInfo);
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  nsCOMPtr<nsIDOMNode> kungFuDeathGrip(it);
  mInner.CopyInnerTo(this, &it->mInner, aDeep);
  return it->QueryInterface(kIDOMNodeIID, (void**) aReturn);
}

NS_IMETHODIMP
nsHTMLMapElement::GetAreas(nsIDOMHTMLCollection** aAreas)
{
  if (nsnull == aAreas) {
    return NS_ERROR_NULL_POINTER;
  }
  if (nsnull == mAreas) {
    mAreas = new GenericElementCollection(NS_STATIC_CAST(nsIContent*, this),
                                          nsHTMLAtoms::area);
    if (nsnull == mAreas) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
    NS_ADDREF(mAreas);
  }
  *aAreas = NS_STATIC_CAST(nsIDOMHTMLCollection*, mAreas);
  NS_ADDREF(mAreas);
  return NS_OK;
}


NS_IMPL_STRING_ATTR(nsHTMLMapElement, Name, name)

NS_IMETHODIMP
nsHTMLMapElement::StringToAttribute(nsIAtom* aAttribute,
                                    const nsAReadableString& aValue,
                                    nsHTMLValue& aResult)
{
  return NS_CONTENT_ATTR_NOT_THERE;
}

NS_IMETHODIMP
nsHTMLMapElement::AttributeToString(nsIAtom* aAttribute,
                                    const nsHTMLValue& aValue,
                                    nsAWritableString& aResult) const
{
  return mInner.AttributeToString(aAttribute, aValue, aResult);
}

static void
MapAttributesInto(const nsIHTMLMappedAttributes* aAttributes,
                  nsIMutableStyleContext* aContext,
                  nsIPresContext* aPresContext)
{
  nsGenericHTMLElement::MapCommonAttributesInto(aAttributes, aContext,
                                                aPresContext);
}

NS_IMETHODIMP
nsHTMLMapElement::GetMappedAttributeImpact(const nsIAtom* aAttribute,
                                           PRInt32& aHint) const
{
  if (aAttribute == nsHTMLAtoms::name) {
    aHint = NS_STYLE_HINT_RECONSTRUCT_ALL;
  }
  else if (! nsGenericHTMLElement::GetCommonMappedAttributesImpact(aAttribute, aHint)) {
    aHint = NS_STYLE_HINT_CONTENT;
  }

  return NS_OK;
}



NS_IMETHODIMP
nsHTMLMapElement::GetAttributeMappingFunctions(nsMapAttributesFunc& aFontMapFunc,
                                               nsMapAttributesFunc& aMapFunc) const
{
  aFontMapFunc = nsnull;
  aMapFunc = &MapAttributesInto;
  return NS_OK;
}


NS_IMETHODIMP
nsHTMLMapElement::HandleDOMEvent(nsIPresContext* aPresContext,
                                 nsEvent* aEvent,
                                 nsIDOMEvent** aDOMEvent,
                                 PRUint32 aFlags,
                                 nsEventStatus* aEventStatus)
{
  return mInner.HandleDOMEvent(aPresContext, aEvent, aDOMEvent,
                               aFlags, aEventStatus);
}


NS_IMETHODIMP
nsHTMLMapElement::SizeOf(nsISizeOfHandler* aSizer, PRUint32* aResult) const
{
  if (!aResult) return NS_ERROR_NULL_POINTER;
#ifdef DEBUG
  PRUint32 sum = 0;
  mInner.SizeOf(aSizer, &sum, sizeof(*this));
  if (mAreas) {
    PRUint32 asize;
    mAreas->SizeOf(aSizer, &asize);
    sum += asize;
  }
  *aResult = sum;
#endif
  return NS_OK;
}
