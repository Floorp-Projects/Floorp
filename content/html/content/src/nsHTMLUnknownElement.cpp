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
#include "nsLayoutAtoms.h"
#include "nsHTMLAtoms.h"
#include "nsIEventListenerManager.h"
#include "nsIDOMEventReceiver.h"
#include "nsDOMEventsIIDs.h"
#include "nsIDocument.h"
#include "nsIHTMLStyleSheet.h"
#include "nsIHTMLContentContainer.h"
#include "nsIHTMLAttributes.h"


class nsHTMLUnknownElement : public nsIDOMHTMLElement,
                             public nsIJSScriptObject,
                             public nsIHTMLContent
{
public:
  nsHTMLUnknownElement(nsINodeInfo *aNodeInfo);
  virtual ~nsHTMLUnknownElement();

  // nsISupports
  NS_DECL_ISUPPORTS

  // nsIDOMNode
  NS_IMPL_IDOMNODE_USING_GENERIC(mInner)

  // nsIDOMElement
  NS_IMPL_IDOMELEMENT_USING_GENERIC(mInner)

  // nsIDOMHTMLElement
  NS_IMPL_IDOMHTMLELEMENT_USING_GENERIC(mInner)

  // nsIJSScriptObject
  NS_IMPL_IJSSCRIPTOBJECT_USING_GENERIC(mInner)

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
  NS_IMETHOD ParseAttributeString(const nsString& aStr,
                                  nsIAtom*& aName,
                                  PRInt32& aNameSpaceID) {
    return mInner.ParseAttributeString(aStr, aName, aNameSpaceID);
  }
  NS_IMETHOD GetNameSpacePrefixFromId(PRInt32 aNameSpaceID,
                                nsIAtom*& aPrefix) {
    return mInner.GetNameSpacePrefixFromId(aNameSpaceID, aPrefix);
  }
  NS_IMETHOD SetAttribute(PRInt32 aNameSpaceID, nsIAtom* aName,
                          const nsString& aValue, PRBool aNotify);
  NS_IMETHOD SetAttribute(nsINodeInfo* aNodeInfo,
                          const nsString& aValue, PRBool aNotify);
  NS_IMETHOD GetAttribute(PRInt32 aNameSpaceID, nsIAtom* aName,
                          nsString& aResult) const {
    return mInner.GetAttribute(aNameSpaceID, aName, aResult);
  }
  NS_IMETHOD GetAttribute(PRInt32 aNameSpaceID, nsIAtom* aName,
                          nsIAtom*& aPrefix, nsString& aResult) const {
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
  NS_IMETHOD SetFocus(class nsIPresContext *aPresContext) {
    return mInner.SetFocus(aPresContext);
  }
  NS_IMETHOD RemoveFocus(class nsIPresContext *aPresContext) {
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
};

nsresult
NS_NewHTMLUnknownElement(nsIHTMLContent** aInstancePtrResult,
                         nsINodeInfo *aNodeInfo)
{
  NS_ENSURE_ARG_POINTER(aInstancePtrResult);
  NS_ENSURE_ARG_POINTER(aNodeInfo);

  nsIHTMLContent* it = new nsHTMLUnknownElement(aNodeInfo);
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return it->QueryInterface(kIHTMLContentIID, (void**) aInstancePtrResult);
}


nsHTMLUnknownElement::nsHTMLUnknownElement(nsINodeInfo *aNodeInfo)
{
  NS_INIT_REFCNT();
  mInner.Init(this, aNodeInfo);
}

nsHTMLUnknownElement::~nsHTMLUnknownElement()
{
}

NS_IMPL_ADDREF(nsHTMLUnknownElement)

NS_IMPL_RELEASE(nsHTMLUnknownElement)

nsresult
nsHTMLUnknownElement::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
  NS_IMPL_HTML_CONTENT_QUERY_INTERFACE(aIID, aInstancePtr, this)

  return NS_NOINTERFACE;
}

nsresult
nsHTMLUnknownElement::CloneNode(PRBool aDeep, nsIDOMNode** aReturn)
{
  nsHTMLUnknownElement* it = new nsHTMLUnknownElement(mInner.mNodeInfo);
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  nsCOMPtr<nsIDOMNode> kungFuDeathGrip(it);
  mInner.CopyInnerTo(this, &it->mInner, aDeep);
  return it->QueryInterface(kIDOMNodeIID, (void**) aReturn);
}

NS_IMETHODIMP
nsHTMLUnknownElement::StringToAttribute(nsIAtom* aAttribute,
                             const nsString& aValue,
                             nsHTMLValue& aResult)
{
  // XXX write me
  return NS_CONTENT_ATTR_NOT_THERE;
}

NS_IMETHODIMP
nsHTMLUnknownElement::AttributeToString(nsIAtom* aAttribute,
                             const nsHTMLValue& aValue,
                             nsString& aResult) const
{
  // XXX write me
  return mInner.AttributeToString(aAttribute, aValue, aResult);
}

static void
MapAttributesInto(const nsIHTMLMappedAttributes* aAttributes,
                  nsIMutableStyleContext* aContext,
                  nsIPresContext* aPresContext)
{
  // XXX write me
  nsGenericHTMLElement::MapCommonAttributesInto(aAttributes, aContext, aPresContext);
}

NS_IMETHODIMP
nsHTMLUnknownElement::GetMappedAttributeImpact(const nsIAtom* aAttribute,
                                             PRInt32& aHint) const
{
  if (! nsGenericHTMLElement::GetCommonMappedAttributesImpact(aAttribute, aHint)) {
    aHint = NS_STYLE_HINT_CONTENT;
  }
  return NS_OK;
}


NS_IMETHODIMP
nsHTMLUnknownElement::GetAttributeMappingFunctions(nsMapAttributesFunc& aFontMapFunc,
                                               nsMapAttributesFunc& aMapFunc) const
{
  aFontMapFunc = nsnull;
  aMapFunc = &MapAttributesInto;
  return NS_OK;
}


static nsIHTMLStyleSheet* GetAttrStyleSheet(nsIDocument* aDocument)
{
  nsIHTMLStyleSheet*  sheet = nsnull;
  nsIHTMLContentContainer*  htmlContainer;
  
  if (nsnull != aDocument) {
    if (NS_OK == aDocument->QueryInterface(NS_GET_IID(nsIHTMLContentContainer),
                                           (void**)&htmlContainer)) {
      htmlContainer->GetAttributeStyleSheet(&sheet);
      NS_RELEASE(htmlContainer);
    }
  }
  NS_ASSERTION(nsnull != sheet, "can't get attribute style sheet");
  return sheet;
}

static nsresult EnsureWritableAttributes(nsIHTMLContent* aContent,
                                         nsIHTMLAttributes*& aAttributes, PRBool
 aCreate)
{
  nsresult  result = NS_OK;

  if (nsnull == aAttributes) {
    if (PR_TRUE == aCreate) {
      result = NS_NewHTMLAttributes(&aAttributes);
    }
  }
  return result;
}

static void ReleaseAttributes(nsIHTMLAttributes*& aAttributes)
{
//  aAttributes->ReleaseContentRef();
  NS_RELEASE(aAttributes);
}


NS_IMETHODIMP
nsHTMLUnknownElement::SetAttribute(PRInt32 aNameSpaceID,
                                   nsIAtom* aAttribute,
                                   const nsString& aValue,
                                   PRBool aNotify)
{
  nsresult  result = NS_OK;
  NS_ASSERTION((kNameSpaceID_HTML == aNameSpaceID) || 
               (kNameSpaceID_None == aNameSpaceID) || 
               (kNameSpaceID_Unknown == aNameSpaceID), 
               "html content only holds HTML attributes");

  if ((kNameSpaceID_HTML != aNameSpaceID) && 
      (kNameSpaceID_None != aNameSpaceID) &&
      (kNameSpaceID_Unknown != aNameSpaceID)) {
    return NS_ERROR_ILLEGAL_VALUE;
  }

  // Check for event handlers
  if ((nsLayoutAtoms::onclick == aAttribute) || 
           (nsLayoutAtoms::ondblclick == aAttribute) ||
           (nsLayoutAtoms::onmousedown == aAttribute) ||
           (nsLayoutAtoms::onmouseup == aAttribute) ||
           (nsLayoutAtoms::onmouseover == aAttribute) ||
           (nsLayoutAtoms::onmouseout == aAttribute))
    mInner.AddScriptEventListener(aAttribute, aValue, kIDOMMouseListenerIID); 
  else if ((nsLayoutAtoms::onkeydown == aAttribute) ||
           (nsLayoutAtoms::onkeyup == aAttribute) ||
           (nsLayoutAtoms::onkeypress == aAttribute))
    mInner.AddScriptEventListener(aAttribute, aValue, kIDOMKeyListenerIID); 
  else if (nsLayoutAtoms::onmousemove == aAttribute)
    mInner.AddScriptEventListener(aAttribute, aValue, kIDOMMouseMotionListenerIID); 
  else if (nsLayoutAtoms::onload == aAttribute)
    mInner.AddScriptEventListener(nsLayoutAtoms::onload, aValue, kIDOMLoadListenerIID);
  else if ((nsLayoutAtoms::onunload == aAttribute) ||
           (nsLayoutAtoms::onabort == aAttribute) ||
           (nsLayoutAtoms::onerror == aAttribute))
    mInner.AddScriptEventListener(aAttribute, aValue, kIDOMLoadListenerIID); 
  else if ((nsLayoutAtoms::onfocus == aAttribute) ||
           (nsLayoutAtoms::onblur == aAttribute))
    mInner.AddScriptEventListener(aAttribute, aValue, kIDOMFocusListenerIID); 
  else if ((nsLayoutAtoms::onsubmit == aAttribute) ||
           (nsLayoutAtoms::onreset == aAttribute) ||
           (nsLayoutAtoms::onchange == aAttribute) ||
           (nsLayoutAtoms::onselect == aAttribute))
    mInner.AddScriptEventListener(aAttribute, aValue, kIDOMFormListenerIID); 
  else if (nsLayoutAtoms::onpaint == aAttribute)
    mInner.AddScriptEventListener(aAttribute, aValue, kIDOMPaintListenerIID); 
  else if (nsLayoutAtoms::oninput == aAttribute)
    mInner.AddScriptEventListener(aAttribute, aValue, kIDOMFormListenerIID);
 
  nsHTMLValue val;
  
  if (NS_CONTENT_ATTR_NOT_THERE !=
      StringToAttribute(aAttribute, aValue, val)) {
    // string value was mapped to nsHTMLValue, set it that way
    result = SetHTMLAttribute(aAttribute, val, aNotify);
    return result;
  }
  else {
    if (mInner.ParseCommonAttribute(aAttribute, aValue, val)) {
      // string value was mapped to nsHTMLValue, set it that way
      result = SetHTMLAttribute(aAttribute, val, aNotify);
      return result;
    }
    if (0 == aValue.Length()) { // if empty string
      val.SetEmptyValue();
      result = SetHTMLAttribute(aAttribute, val, aNotify);
      return result;
    }

    if (aNotify && (nsnull != mInner.mDocument)) {
      mInner.mDocument->BeginUpdate();
    }
    // set as string value to avoid another string copy
    PRBool  impact = NS_STYLE_HINT_NONE;
    GetMappedAttributeImpact(aAttribute, impact);
    if (nsnull != mInner.mDocument) {  // set attr via style sheet
      nsIHTMLStyleSheet*  sheet = GetAttrStyleSheet(mInner.mDocument);
      if (nsnull != sheet) {
        result = sheet->SetAttributeFor(aAttribute, aValue, 
                                        (NS_STYLE_HINT_CONTENT < impact), 
                                        this, mInner.mAttributes);
        NS_RELEASE(sheet);
      }
    }
    else {  // manage this ourselves and re-sync when we connect to doc
      result = EnsureWritableAttributes(this, mInner.mAttributes, PR_TRUE);
      if (nsnull != mInner.mAttributes) {
        PRInt32   count;
        result = mInner.mAttributes->SetAttributeFor(aAttribute, aValue, 
                                              (NS_STYLE_HINT_CONTENT < impact),
                                              this, nsnull, count);
        if (0 == count) {
          ReleaseAttributes(mInner.mAttributes);
        }
      }
    }
  }

  if (aNotify && (nsnull != mInner.mDocument)) {
    result = mInner.mDocument->AttributeChanged(mInner.mContent, aNameSpaceID, aAttribute, NS_STYLE_HINT_UNKNOWN);
    mInner.mDocument->EndUpdate();
  }

  return result;
}

NS_IMETHODIMP
nsHTMLUnknownElement::SetAttribute(nsINodeInfo *aNodeInfo,
                                   const nsString& aValue,
                                   PRBool aNotify)
{
  NS_ENSURE_ARG_POINTER(aNodeInfo);  

  nsCOMPtr<nsIAtom> atom;  
  PRInt32 nsid;  

  aNodeInfo->GetNameAtom(*getter_AddRefs(atom));  
  aNodeInfo->GetNamespaceID(nsid);  

  // We still rely on the old way of setting the attribute.  
  
  return SetAttribute(nsid, atom, aValue, aNotify);  
}

NS_IMETHODIMP
nsHTMLUnknownElement::HandleDOMEvent(nsIPresContext* aPresContext,
                                     nsEvent* aEvent,
                                     nsIDOMEvent** aDOMEvent,
                                     PRUint32 aFlags,
                                     nsEventStatus* aEventStatus)
{
  return mInner.HandleDOMEvent(aPresContext, aEvent, aDOMEvent,
                               aFlags, aEventStatus);
}


NS_IMETHODIMP
nsHTMLUnknownElement::SizeOf(nsISizeOfHandler* aSizer, PRUint32* aResult) const
{
  return mInner.SizeOf(aSizer, aResult, sizeof(*this));
}
