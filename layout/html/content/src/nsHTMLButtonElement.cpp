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
#include "nsIDOMHTMLButtonElement.h"
#include "nsIDOMNSHTMLButtonElement.h"
#include "nsIDOMHTMLFormElement.h"
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
#include "nsIFormControl.h"
#include "nsIForm.h"
#include "nsIURL.h"

#include "nsIFormControlFrame.h"
#include "nsIEventStateManager.h"
#include "nsDOMEvent.h"
#include "nsISizeOfHandler.h"
#include "nsIDocument.h"


class nsHTMLButtonElement : public nsIDOMHTMLButtonElement,
                            public nsIJSScriptObject,
                            public nsIHTMLContent,
                            public nsIFormControl
{
public:
  nsHTMLButtonElement(nsINodeInfo *aNodeInfo);
  virtual ~nsHTMLButtonElement();

  // nsISupports
  NS_DECL_ISUPPORTS

  // nsIDOMNode
  NS_IMPL_IDOMNODE_USING_GENERIC(mInner)

  // nsIDOMElement
//  NS_IMPL_IDOMELEMENT_USING_GENERIC(mInner)
  NS_IMETHOD GetTagName(nsAWritableString& aTagName) {
    return mInner.GetTagName(aTagName);
  }
  NS_IMETHOD GetAttribute(const nsAReadableString& aName, nsAWritableString& aReturn);
  NS_IMETHOD SetAttribute(const nsAReadableString& aName, const nsAReadableString& aValue);
  NS_IMETHOD RemoveAttribute(const nsAReadableString& aName) {
    return mInner.RemoveAttribute(aName);
  }
  NS_IMETHOD GetAttributeNode(const nsAReadableString& aName,
                              nsIDOMAttr** aReturn) {
    return mInner.GetAttributeNode(aName, aReturn);
  }
  NS_IMETHOD SetAttributeNode(nsIDOMAttr* aNewAttr, nsIDOMAttr** aReturn) {
    return mInner.SetAttributeNode(aNewAttr, aReturn);
  }
  NS_IMETHOD RemoveAttributeNode(nsIDOMAttr* aOldAttr, nsIDOMAttr** aReturn) {
    return mInner.RemoveAttributeNode(aOldAttr, aReturn);
  }
  NS_IMETHOD GetElementsByTagName(const nsAReadableString& aTagname,
                                  nsIDOMNodeList** aReturn) {
    return mInner.GetElementsByTagName(aTagname, aReturn);
  }
  NS_IMETHOD GetAttributeNS(const nsAReadableString& aNamespaceURI,
                            const nsAReadableString& aLocalName, nsAWritableString& aReturn) {
    return mInner.GetAttributeNS(aNamespaceURI, aLocalName, aReturn);
  }
  NS_IMETHOD SetAttributeNS(const nsAReadableString& aNamespaceURI,
                            const nsAReadableString& aQualifiedName,
                            const nsAReadableString& aValue) {
    return mInner.SetAttributeNS(aNamespaceURI, aQualifiedName, aValue);
  }
  NS_IMETHOD RemoveAttributeNS(const nsAReadableString& aNamespaceURI,
                               const nsAReadableString& aLocalName) {
    return mInner.RemoveAttributeNS(aNamespaceURI, aLocalName);
  }
  NS_IMETHOD GetAttributeNodeNS(const nsAReadableString& aNamespaceURI,
                                const nsAReadableString& aLocalName,
                                nsIDOMAttr** aReturn) {
    return mInner.GetAttributeNodeNS(aNamespaceURI, aLocalName, aReturn);
  }
  NS_IMETHOD SetAttributeNodeNS(nsIDOMAttr* aNewAttr, nsIDOMAttr** aReturn) {
    return mInner.SetAttributeNodeNS(aNewAttr, aReturn);
  }
  NS_IMETHOD GetElementsByTagNameNS(const nsAReadableString& aNamespaceURI,
                                    const nsAReadableString& aLocalName,
                                    nsIDOMNodeList** aReturn) {
    return mInner.GetElementsByTagNameNS(aNamespaceURI, aLocalName, aReturn);
  }
  NS_IMETHOD HasAttribute(const nsAReadableString& aName, PRBool* aReturn) {
    return mInner.HasAttribute(aName, aReturn);
  }
  NS_IMETHOD HasAttributeNS(const nsAReadableString& aNamespaceURI,
                            const nsAReadableString& aLocalName, PRBool* aReturn) {
    return mInner.HasAttributeNS(aNamespaceURI, aLocalName, aReturn);
  }

  // nsIDOMHTMLElement
  NS_IMPL_IDOMHTMLELEMENT_USING_GENERIC(mInner)

  // nsIDOMHTMLButtonElement
  NS_DECL_IDOMHTMLBUTTONELEMENT

  // nsIDOMHTMLButtonElement
  NS_IMETHOD Blur();
  NS_IMETHOD Focus();

  // nsIJSScriptObject
  NS_IMPL_IJSSCRIPTOBJECT_USING_GENERIC(mInner)

  // nsIContent
  NS_IMPL_ICONTENT_NO_SETPARENT_NO_SETDOCUMENT_NO_FOCUS_USING_GENERIC(mInner)
  
  // nsIHTMLContent
  NS_IMPL_IHTMLCONTENT_USING_GENERIC(mInner)

  // nsIFormControl
  NS_IMETHOD SetForm(nsIDOMHTMLFormElement* aForm);
  NS_IMETHOD GetType(PRInt32* aType);
  NS_IMETHOD Init() { return NS_OK; }

protected:
  nsGenericHTMLContainerFormElement mInner;
  nsIForm*                          mForm;
  PRInt32                           mType;
};


// Construction, destruction

nsresult
NS_NewHTMLButtonElement(nsIHTMLContent** aInstancePtrResult,
                        nsINodeInfo *aNodeInfo)
{
  NS_ENSURE_ARG_POINTER(aInstancePtrResult);
  NS_ENSURE_ARG_POINTER(aNodeInfo);

  nsIHTMLContent* it = new nsHTMLButtonElement(aNodeInfo);
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return it->QueryInterface(NS_GET_IID(nsIHTMLContent), (void**) aInstancePtrResult);
}


nsHTMLButtonElement::nsHTMLButtonElement(nsINodeInfo *aNodeInfo)
{
  NS_INIT_REFCNT();
  mInner.Init(this, aNodeInfo);
  mForm = nsnull;
  mType = NS_FORM_BUTTON_BUTTON; // default
}

nsHTMLButtonElement::~nsHTMLButtonElement()
{
  // Null out form's pointer to us - no ref counting here!
  SetForm(nsnull);
}

// nsISupports

NS_IMPL_ADDREF(nsHTMLButtonElement);
NS_IMPL_RELEASE(nsHTMLButtonElement);

nsresult
nsHTMLButtonElement::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
  NS_IMPL_HTML_CONTENT_QUERY_INTERFACE(aIID, aInstancePtr, this)
  if (aIID.Equals(NS_GET_IID(nsIDOMHTMLButtonElement))) {
    *aInstancePtr = (void*)(nsIDOMHTMLButtonElement*)this;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  else if (aIID.Equals(NS_GET_IID(nsIFormControl))) {
    *aInstancePtr = (void*)(nsIFormControl*) this;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  return NS_NOINTERFACE;
}

NS_IMETHODIMP
nsHTMLButtonElement::GetAttribute(const nsAReadableString& aName, nsAWritableString& aReturn)
{
  nsAutoString name(aName);
  if (name.EqualsWithConversion("disabled", PR_TRUE)) {
    nsresult rv = GetAttribute(kNameSpaceID_None, nsHTMLAtoms::disabled, aReturn);
    if (rv == NS_CONTENT_ATTR_NOT_THERE) {
      aReturn.Assign(NS_LITERAL_STRING("false"));
    } else {
      aReturn.Assign(NS_LITERAL_STRING("true"));
    }

    return NS_OK;
  }

  return mInner.GetAttribute(aName, aReturn);
}

NS_IMETHODIMP
nsHTMLButtonElement::SetAttribute(const nsAReadableString& aName, const nsAReadableString& aValue)
{
  nsAutoString name(aName), value(aValue);
  if (name.EqualsWithConversion("disabled", PR_TRUE) &&
      value.EqualsWithConversion("false", PR_TRUE)) {
    return mInner.RemoveAttribute(aName);
  }

  return mInner.SetAttribute(aName, aValue);
}

// nsIDOMHTMLButtonElement

nsresult
nsHTMLButtonElement::CloneNode(PRBool aDeep, nsIDOMNode** aReturn)
{
  nsHTMLButtonElement* it = new nsHTMLButtonElement(mInner.mNodeInfo);
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  nsCOMPtr<nsIDOMNode> kungFuDeathGrip(it);
  mInner.CopyInnerTo(this, &it->mInner, aDeep);
  return it->QueryInterface(NS_GET_IID(nsIDOMNode), (void**) aReturn);
}

// nsIContent

NS_IMETHODIMP
nsHTMLButtonElement::SetParent(nsIContent* aParent)
{
  return mInner.SetParentForFormControls(aParent, this, mForm);
}

NS_IMETHODIMP
nsHTMLButtonElement::SetDocument(nsIDocument* aDocument, PRBool aDeep, PRBool aCompileEventHandlers)
{
  return mInner.SetDocumentForFormControls(aDocument, aDeep, aCompileEventHandlers, this, mForm);
}

NS_IMETHODIMP
nsHTMLButtonElement::GetForm(nsIDOMHTMLFormElement** aForm)
{
  nsresult result = NS_OK;
  *aForm = nsnull;
  if (nsnull != mForm) {
    nsIDOMHTMLFormElement* formElem = nsnull;
    result = mForm->QueryInterface(NS_GET_IID(nsIDOMHTMLFormElement), (void**)&formElem);
    if (NS_OK == result) {
      *aForm = formElem;
    }
  }
  return result;
}
NS_IMETHODIMP
nsHTMLButtonElement::GetType(nsAWritableString& aType)
{
  return AttributeToString(nsHTMLAtoms::type,
                           nsHTMLValue(mType, eHTMLUnit_Enumerated),
                           aType);
}

NS_IMPL_STRING_ATTR(nsHTMLButtonElement, AccessKey, accesskey)
NS_IMPL_BOOL_ATTR(nsHTMLButtonElement, Disabled, disabled)
NS_IMPL_STRING_ATTR(nsHTMLButtonElement, Name, name)
NS_IMPL_INT_ATTR(nsHTMLButtonElement, TabIndex, tabindex)
NS_IMPL_STRING_ATTR(nsHTMLButtonElement, Value, value)

NS_IMETHODIMP
nsHTMLButtonElement::Blur()
{
  nsCOMPtr<nsIPresContext> presContext;
  nsGenericHTMLElement::GetPresContext(this, getter_AddRefs(presContext));
  return RemoveFocus(presContext);
}

NS_IMETHODIMP
nsHTMLButtonElement::Focus()
{
  nsCOMPtr<nsIPresContext> presContext;
  nsGenericHTMLElement::GetPresContext(this, getter_AddRefs(presContext));
  return SetFocus(presContext);
}

NS_IMETHODIMP
nsHTMLButtonElement::SetFocus(nsIPresContext* aPresContext)
{
  // first see if we are disabled or not. If disabled then do nothing.
  nsAutoString disabled;
  if (NS_CONTENT_ATTR_HAS_VALUE == mInner.GetAttribute(kNameSpaceID_HTML, nsHTMLAtoms::disabled, disabled)) {
    return NS_OK;
  }

  nsCOMPtr<nsIEventStateManager> esm;
  if (NS_OK == aPresContext->GetEventStateManager(getter_AddRefs(esm))) {
    esm->SetContentState(this, NS_EVENT_STATE_FOCUS);
  }

  nsIFormControlFrame* formControlFrame = nsnull;
  nsresult rv = nsGenericHTMLElement::GetPrimaryFrame(this, formControlFrame);
  if (NS_SUCCEEDED(rv)) {
    formControlFrame->SetFocus(PR_TRUE, PR_TRUE);
    formControlFrame->ScrollIntoView(aPresContext);
  }
  return rv;
}

NS_IMETHODIMP
nsHTMLButtonElement::RemoveFocus(nsIPresContext* aPresContext)
{
  // If we are disabled, we probably shouldn't have focus in the
  // first place, so allow it to be removed.
  nsresult rv = NS_OK;

  nsIFormControlFrame* formControlFrame = nsnull;
  rv = nsGenericHTMLElement::GetPrimaryFrame(this, formControlFrame);
  if (NS_SUCCEEDED(rv)) {
    formControlFrame->SetFocus(PR_FALSE, PR_FALSE);
  }

  nsCOMPtr<nsIEventStateManager> esm;
  if (NS_OK == aPresContext->GetEventStateManager(getter_AddRefs(esm))) {

    nsCOMPtr<nsIDocument> doc;
    GetDocument(*getter_AddRefs(doc));
    if (!doc)
      return NS_ERROR_NULL_POINTER;

    nsCOMPtr<nsIContent> rootContent;
    rootContent = getter_AddRefs(doc->GetRootContent());
    rv = esm->SetContentState(rootContent, NS_EVENT_STATE_FOCUS);
  }

  return rv;
}

static nsGenericHTMLElement::EnumTable kButtonTypeTable[] = {
  { "button", NS_FORM_BUTTON_BUTTON },
  { "reset", NS_FORM_BUTTON_RESET },
  { "submit", NS_FORM_BUTTON_SUBMIT },
  { 0 }
};

NS_IMETHODIMP
nsHTMLButtonElement::StringToAttribute(nsIAtom* aAttribute,
                                       const nsAReadableString& aValue,
                                       nsHTMLValue& aResult)
{
  if (aAttribute == nsHTMLAtoms::tabindex) {
    if (nsGenericHTMLElement::ParseValue(aValue, 0, 32767, aResult,
                                         eHTMLUnit_Integer)) {
      return NS_CONTENT_ATTR_HAS_VALUE;
    }
  }
  else if (aAttribute == nsHTMLAtoms::type) {
    nsGenericHTMLElement::EnumTable *table = kButtonTypeTable;
    nsAutoString val(aValue);
    while (nsnull != table->tag) { 
      if (val.EqualsIgnoreCase(table->tag)) {
        aResult.SetIntValue(table->value, eHTMLUnit_Enumerated);
        mType = table->value;  
        return NS_CONTENT_ATTR_HAS_VALUE;
      }
      table++;
    }
  }
  else if (aAttribute == nsHTMLAtoms::disabled) {
    aResult.SetEmptyValue();
    return NS_CONTENT_ATTR_HAS_VALUE;
  }
  return NS_CONTENT_ATTR_NOT_THERE;
}

NS_IMETHODIMP
nsHTMLButtonElement::AttributeToString(nsIAtom* aAttribute,
                                       const nsHTMLValue& aValue,
                                       nsAWritableString& aResult) const
{
  if (aAttribute == nsHTMLAtoms::type) {
    if (eHTMLUnit_Enumerated == aValue.GetUnit()) {
      nsGenericHTMLElement::EnumValueToString(aValue, kButtonTypeTable, aResult, PR_FALSE);
      return NS_CONTENT_ATTR_HAS_VALUE;
    }
  }
  return mInner.AttributeToString(aAttribute, aValue, aResult);
}

static void
MapAttributesInto(const nsIHTMLMappedAttributes* aAttributes,
                  nsIMutableStyleContext* aContext,
                  nsIPresContext* aPresContext)
{
  nsGenericHTMLElement::MapCommonAttributesInto(aAttributes, aContext, aPresContext);
}

NS_IMETHODIMP
nsHTMLButtonElement::GetMappedAttributeImpact(const nsIAtom* aAttribute,
                                              PRInt32& aHint) const
{
  if (! nsGenericHTMLElement::GetCommonMappedAttributesImpact(aAttribute, aHint)) {
    aHint = NS_STYLE_HINT_CONTENT;
  }
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLButtonElement::GetAttributeMappingFunctions(nsMapAttributesFunc& aFontMapFunc,
                                                  nsMapAttributesFunc& aMapFunc) const
{
  aFontMapFunc = nsnull;
  aMapFunc = &MapAttributesInto;
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLButtonElement::HandleDOMEvent(nsIPresContext* aPresContext,
                                    nsEvent* aEvent,
                                    nsIDOMEvent** aDOMEvent,
                                    PRUint32 aFlags,
                                    nsEventStatus* aEventStatus)
{
  NS_ENSURE_ARG(aPresContext);
  NS_ENSURE_ARG_POINTER(aEventStatus);

  // Do not process any DOM events if the element is disabled
  PRBool bDisabled;
  nsresult rv = GetDisabled(&bDisabled);
  if (NS_FAILED(rv) || bDisabled) {
    return rv;
  }

  // Try script event handlers first
  nsresult ret = mInner.HandleDOMEvent(aPresContext, aEvent, aDOMEvent,
                                       aFlags, aEventStatus);

  if ((NS_OK == ret) && (nsEventStatus_eIgnore == *aEventStatus) &&
      !(aFlags & NS_EVENT_FLAG_CAPTURE)) {
    switch (aEvent->message) {

    case NS_KEY_PRESS:
      {
        // For backwards compat, trigger buttons with space or enter (bug 25300)
        nsKeyEvent * keyEvent = (nsKeyEvent *)aEvent;
        if (keyEvent->keyCode == NS_VK_RETURN || keyEvent->charCode == NS_VK_SPACE) {
          nsEventStatus status = nsEventStatus_eIgnore;
          nsMouseEvent event;
          event.eventStructType = NS_GUI_EVENT;
          event.message = NS_MOUSE_LEFT_CLICK;
          event.isShift = PR_FALSE;
          event.isControl = PR_FALSE;
          event.isAlt = PR_FALSE;
          event.isMeta = PR_FALSE;
          event.clickCount = 0;
          event.widget = nsnull;
          rv = HandleDOMEvent(aPresContext, &event, nsnull, NS_EVENT_FLAG_INIT, &status);
        }
      }
      break;// NS_KEY_PRESS

    case NS_MOUSE_LEFT_CLICK:
      {
        // Tell the frame about the click
        nsIFormControlFrame* formControlFrame = nsnull;
        rv = nsGenericHTMLElement::GetPrimaryFrame(this, formControlFrame);
        if (NS_SUCCEEDED(rv)) {
          formControlFrame->MouseClicked(aPresContext);
        }
      }
      break;// NS_MOUSE_LEFT_CLICK

    case NS_MOUSE_LEFT_BUTTON_DOWN:
      {
        nsIEventStateManager *stateManager;
        if (NS_OK == aPresContext->GetEventStateManager(&stateManager)) {
          stateManager->SetContentState(this, NS_EVENT_STATE_ACTIVE | NS_EVENT_STATE_FOCUS);
          NS_RELEASE(stateManager);
        }
        *aEventStatus = nsEventStatus_eConsumeNoDefault; 
      }
      break;

    case NS_MOUSE_LEFT_BUTTON_UP:
      {
        nsIEventStateManager *stateManager;
        nsIContent *activeLink = nsnull;
        if (NS_OK == aPresContext->GetEventStateManager(&stateManager)) {
          //stateManager->GetActiveLink(&activeLink);
          NS_RELEASE(stateManager);
        }

        if (activeLink == this) {
          if (nsEventStatus_eConsumeNoDefault != *aEventStatus) {
            nsAutoString href, target, disabled;
            nsIURI* baseURL = nsnull;
            GetBaseURL(baseURL);
            GetAttribute(kNameSpaceID_HTML, nsHTMLAtoms::href, href);
            GetAttribute(kNameSpaceID_HTML, nsHTMLAtoms::target, target);
            GetAttribute(kNameSpaceID_HTML, nsHTMLAtoms::disabled, disabled); 
            
            if (target.Length() == 0) {
              GetBaseTarget(target);
            }
            ret = mInner.TriggerLink(aPresContext, eLinkVerb_Replace, baseURL, href, target, PR_TRUE);
            NS_IF_RELEASE(baseURL);
            *aEventStatus = nsEventStatus_eConsumeNoDefault; 
          }
        }
      }
      break;

    case NS_MOUSE_MIDDLE_BUTTON_DOWN:     // cancel all of these events for buttons
    case NS_MOUSE_MIDDLE_BUTTON_UP:
    case NS_MOUSE_MIDDLE_DOUBLECLICK:
    case NS_MOUSE_RIGHT_DOUBLECLICK:
    case NS_MOUSE_RIGHT_BUTTON_DOWN:
    case NS_MOUSE_RIGHT_BUTTON_UP:
      if (aDOMEvent != nsnull && *aDOMEvent != nsnull) {
        (*aDOMEvent)->PreventBubble();
      } else {
        ret = NS_ERROR_FAILURE;
      }
      break;

    case NS_MOUSE_ENTER_SYNTH:
      {
        nsIEventStateManager *stateManager;
        if (NS_OK == aPresContext->GetEventStateManager(&stateManager)) {
          stateManager->SetContentState(this, NS_EVENT_STATE_HOVER);
          NS_RELEASE(stateManager);
        }
        *aEventStatus = nsEventStatus_eConsumeNoDefault; 
      }
      break;

      // XXX this doesn't seem to do anything yet
    case NS_MOUSE_EXIT_SYNTH:
      {
        nsIEventStateManager *stateManager;
        if (NS_OK == aPresContext->GetEventStateManager(&stateManager)) {
          stateManager->SetContentState(nsnull, NS_EVENT_STATE_HOVER);
          NS_RELEASE(stateManager);
        }
        *aEventStatus = nsEventStatus_eConsumeNoDefault; 
      }
      break;

    default:
      break;
    }
  }
  return ret;
}

NS_IMETHODIMP
nsHTMLButtonElement::GetType(PRInt32* aType)
{
  if (aType) {
    *aType = mType;
    return NS_OK;
  } else {
    return NS_FORM_NOTOK;
  }
}

NS_IMETHODIMP
nsHTMLButtonElement::SetForm(nsIDOMHTMLFormElement* aForm)
{
  nsCOMPtr<nsIFormControl> formControl;
  nsresult result = QueryInterface(NS_GET_IID(nsIFormControl), getter_AddRefs(formControl));
  if (NS_FAILED(result)) formControl = nsnull;

  nsAutoString nameVal, idVal;
  mInner.GetAttribute(kNameSpaceID_None, nsHTMLAtoms::name, nameVal);
  mInner.GetAttribute(kNameSpaceID_None, nsHTMLAtoms::id, idVal);

  if (mForm && formControl) {
    mForm->RemoveElement(formControl);

    if (nameVal.Length())
      mForm->RemoveElementFromTable(this, nameVal);

    if (idVal.Length())
      mForm->RemoveElementFromTable(this, idVal);
  }

  if (aForm) {
    nsCOMPtr<nsIForm> theForm = do_QueryInterface(aForm, &result);
    mForm = theForm;  // Even if we fail, update mForm (nsnull in failure)
    if ((NS_OK == result) && theForm) {
      if (formControl) {
        theForm->AddElement(formControl);

        if (nameVal.Length())
          theForm->AddElementToTable(this, nameVal);

        if (idVal.Length())
          theForm->AddElementToTable(this, idVal);
      }
    }
  } else {
    mForm = nsnull;
  }

  mInner.SetForm(mForm);

  return result;
}


NS_IMETHODIMP
nsHTMLButtonElement::SizeOf(nsISizeOfHandler* aSizer, PRUint32* aResult) const
{
  if (!aResult) return NS_ERROR_NULL_POINTER;
#ifdef DEBUG
  mInner.SizeOf(aSizer, aResult, sizeof(*this));
  if (mForm) {
    PRBool recorded;
    aSizer->RecordObject(mForm, &recorded);
    if (!recorded) {
      PRUint32 formSize;
      mForm->SizeOf(aSizer, &formSize);
      aSizer->AddSize(nsHTMLAtoms::iform, formSize);
    }
  }
#endif
  return NS_OK;
}
