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
 *   Pierre Phaneuf <pp@ludusdesign.com>
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
#include "nsCOMPtr.h"
#include "nsIDOMHTMLInputElement.h"
#include "nsIDOMNSHTMLInputElement.h"
#include "nsIControllers.h"
#include "nsIEditorController.h"
#include "nsIFocusController.h"
#include "nsPIDOMWindow.h"
#include "nsIScriptGlobalObject.h"
#include "nsContentCID.h"
#include "nsIComponentManager.h"
#include "nsIDOMHTMLFormElement.h"
#include "nsIDOMEventReceiver.h"
#include "nsIHTMLContent.h"
#include "nsGenericHTMLElement.h"
#include "nsHTMLAtoms.h"
#include "nsHTMLIIDs.h"
#include "nsIStyleContext.h"
#include "nsStyleConsts.h"
#include "nsIPresContext.h"
#include "nsIHTMLAttributes.h"
#include "nsIFormControl.h"
#include "nsIForm.h"
#include "nsIGfxTextControlFrame.h"
#include "nsIDocument.h"
#include "nsIPresShell.h"
#include "nsIFormControlFrame.h"
#include "nsIFrame.h"
#include "nsIEventStateManager.h"
#include "nsISizeOfHandler.h"
#include "nsIServiceManager.h"
#include "nsIScriptSecurityManager.h"
#include "nsDOMError.h"
#include "nsIPrivateDOMEvent.h"
#include "nsIEditor.h"
#include "nsGUIEvent.h"

#include "nsIPresState.h"
#include "nsIDOMEvent.h"
#include "nsIDOMNodeList.h"
#include "nsIDOMHTMLCollection.h"
#include "nsICheckboxControlFrame.h"
#include "nsIRadioControlFrame.h"
#include "nsIFormManager.h"

#include "nsIDOMMutationEvent.h"
#include "nsIDOMEventReceiver.h"
#include "nsMutationEvent.h"

#include "nsIRuleNode.h"

// XXX align=left, hspace, vspace, border? other nav4 attrs

static NS_DEFINE_CID(kXULControllersCID,  NS_XULCONTROLLERS_CID);

typedef nsIGfxTextControlFrame2 textControlPlace;

class nsHTMLInputElement : public nsGenericHTMLLeafFormElement,
                           public nsIDOMHTMLInputElement,
                           public nsIDOMNSHTMLInputElement
{
public:
  nsHTMLInputElement();
  virtual ~nsHTMLInputElement();

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  // nsIDOMNode
  NS_FORWARD_NSIDOMNODE_NO_CLONENODE(nsGenericHTMLLeafFormElement::)

  // nsIDOMElement
    // can't use the macro here because input type=text needs to notify up to 
    // frame system on SetAttribute("value");
  NS_IMETHOD GetTagName(nsAWritableString& aTagName) {
    return nsGenericHTMLLeafFormElement::GetTagName(aTagName);
  }
  NS_IMETHOD GetAttribute(const nsAReadableString& aName,
                          nsAWritableString& aReturn) {      
    return nsGenericHTMLLeafFormElement::GetAttribute(aName, aReturn);
  }
  NS_IMETHOD SetAttribute(const nsAReadableString& aName,
                          const nsAReadableString& aValue) { 
    nsAutoString valueAttribute;
    nsHTMLAtoms::value->ToString(valueAttribute);
    if (PR_TRUE==valueAttribute.Equals(aName)) {
      SetValue(aValue);
      // Don't return here, need to set the attribute in the content model too.
    }
    return nsGenericHTMLLeafFormElement::SetAttribute(aName, aValue);
  }                                                                        
  NS_IMETHOD RemoveAttribute(const nsAReadableString& aName) {
    return nsGenericHTMLLeafFormElement::RemoveAttribute(aName);
  }                                                                        
  NS_IMETHOD GetAttributeNode(const nsAReadableString& aName,
                              nsIDOMAttr** aReturn) {                      
    return nsGenericHTMLLeafFormElement::GetAttributeNode(aName, aReturn);
  }                                                                        
  NS_IMETHOD SetAttributeNode(nsIDOMAttr* aNewAttr, nsIDOMAttr** aReturn) {
    return nsGenericHTMLLeafFormElement::SetAttributeNode(aNewAttr, aReturn);
  }                                                                        
  NS_IMETHOD RemoveAttributeNode(nsIDOMAttr* aOldAttr, nsIDOMAttr** aReturn) {
    return nsGenericHTMLLeafFormElement::RemoveAttributeNode(aOldAttr,
                                                             aReturn);
  }                                                                        
  NS_IMETHOD GetElementsByTagName(const nsAReadableString& aTagname,
                                  nsIDOMNodeList** aReturn) {              
    return nsGenericHTMLLeafFormElement::GetElementsByTagName(aTagname,
                                                              aReturn);
  }                                                                        
  NS_IMETHOD GetAttributeNS(const nsAReadableString& aNamespaceURI,
                            const nsAReadableString& aLocalName,
                            nsAWritableString& aReturn) {
    return nsGenericHTMLLeafFormElement::GetAttributeNS(aNamespaceURI,
                                                        aLocalName, aReturn);
  }
  NS_IMETHOD SetAttributeNS(const nsAReadableString& aNamespaceURI,
                            const nsAReadableString& aQualifiedName,
                            const nsAReadableString& aValue) {
    return nsGenericHTMLLeafFormElement::SetAttributeNS(aNamespaceURI,
                                                        aQualifiedName,
                                                        aValue);
  }
  NS_IMETHOD RemoveAttributeNS(const nsAReadableString& aNamespaceURI,
                               const nsAReadableString& aLocalName) {
    return nsGenericHTMLLeafFormElement::RemoveAttributeNS(aNamespaceURI,
                                                           aLocalName);
  }
  NS_IMETHOD GetAttributeNodeNS(const nsAReadableString& aNamespaceURI,
                                const nsAReadableString& aLocalName,
                                nsIDOMAttr** aReturn) {
    return nsGenericHTMLLeafFormElement::GetAttributeNodeNS(aNamespaceURI,
                                                            aLocalName,
                                                            aReturn);
  }
  NS_IMETHOD SetAttributeNodeNS(nsIDOMAttr* aNewAttr, nsIDOMAttr** aReturn) {
    return nsGenericHTMLLeafFormElement::SetAttributeNodeNS(aNewAttr, aReturn);
  }
  NS_IMETHOD GetElementsByTagNameNS(const nsAReadableString& aNamespaceURI,
                                    const nsAReadableString& aLocalName,
                                    nsIDOMNodeList** aReturn) {
    return nsGenericHTMLLeafFormElement::GetElementsByTagNameNS(aNamespaceURI,
                                                                aLocalName,
                                                                aReturn);
  }
  NS_IMETHOD HasAttribute(const nsAReadableString& aName, PRBool* aReturn) {
    return nsGenericHTMLLeafFormElement::HasAttribute(aName, aReturn);
  }
  NS_IMETHOD HasAttributeNS(const nsAReadableString& aNamespaceURI,
                            const nsAReadableString& aLocalName,
                            PRBool* aReturn) {
    return nsGenericHTMLLeafFormElement::HasAttributeNS(aNamespaceURI,
                                                        aLocalName, aReturn);
  }

  // nsIDOMHTMLElement
  NS_FORWARD_NSIDOMHTMLELEMENT(nsGenericHTMLLeafFormElement::)

  // nsIDOMHTMLInputElement
  NS_DECL_NSIDOMHTMLINPUTELEMENT

  // nsIDOMNSHTMLInputElement
  NS_DECL_NSIDOMNSHTMLINPUTELEMENT

  // Overriden nsIFormControl methods
  NS_IMETHOD GetType(PRInt32* aType);

  NS_IMETHOD SetFocus(nsIPresContext* aPresContext);
  NS_IMETHOD RemoveFocus(nsIPresContext* aPresContext);

  NS_IMETHOD StringToAttribute(nsIAtom* aAttribute,
                               const nsAReadableString& aValue,
                               nsHTMLValue& aResult);
  NS_IMETHOD AttributeToString(nsIAtom* aAttribute,
                               const nsHTMLValue& aValue,
                               nsAWritableString& aResult) const;
  NS_IMETHOD GetMappedAttributeImpact(const nsIAtom* aAttribute, PRInt32 aModType,
                                      PRInt32& aHint) const;
  NS_IMETHOD GetAttributeMappingFunction(nsMapRuleToAttributesFunc& aMapRuleFunc) const;
  NS_IMETHOD HandleDOMEvent(nsIPresContext* aPresContext, nsEvent* aEvent,
                            nsIDOMEvent** aDOMEvent, PRUint32 aFlags,
                            nsEventStatus* aEventStatus);
#ifdef DEBUG
  NS_IMETHOD SizeOf(nsISizeOfHandler* aSizer, PRUint32* aResult) const;
#endif

protected:
  // Helper method
  void SetPresStateChecked(nsIHTMLContent * aHTMLContent, 
                           PRBool aValue);

  nsresult GetSelectionRange(PRInt32* aSelectionStart, PRInt32* aSelectionEnd);
  nsresult MouseClickForAltText(nsIPresContext* aPresContext);
  //Helper method
#ifdef ACCESSIBILITY
  nsresult FireEventForAccessibility(nsIPresContext* aPresContext,
			    		                       const nsAReadableString& aEventType);
#endif

  void SelectAll(nsIPresContext* aPresContext);
  PRBool IsImage() const
  {
    nsAutoString tmp;

    nsGenericHTMLLeafFormElement::GetAttr(kNameSpaceID_HTML,
                                          nsHTMLAtoms::type, tmp);

    return tmp.EqualsIgnoreCase("image");
  }

  nsCOMPtr<nsIControllers> mControllers;

  PRInt8                   mType;
  PRPackedBool             mSkipFocusEvent;
  PRPackedBool             mHandlingClick;
};

// construction, destruction

nsresult
NS_NewHTMLInputElement(nsIHTMLContent** aInstancePtrResult,
                       nsINodeInfo *aNodeInfo)
{
  NS_ENSURE_ARG_POINTER(aInstancePtrResult);

  nsHTMLInputElement* it = new nsHTMLInputElement();

  if (!it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  nsresult rv = it->nsGenericElement::Init(aNodeInfo);

  if (NS_FAILED(rv)) {
    delete it;

    return rv;
  }

  *aInstancePtrResult = NS_STATIC_CAST(nsIHTMLContent *, it);
  NS_ADDREF(*aInstancePtrResult);

  return NS_OK;
}


nsHTMLInputElement::nsHTMLInputElement()
{
  mType = NS_FORM_INPUT_TEXT; // default value
  mSkipFocusEvent = PR_FALSE;
  mHandlingClick = PR_FALSE;
}

nsHTMLInputElement::~nsHTMLInputElement()
{
  // Null out form's pointer to us - no ref counting here!
  SetForm(nsnull);
}


// nsISupports

NS_IMPL_ADDREF_INHERITED(nsHTMLInputElement, nsGenericElement) 
NS_IMPL_RELEASE_INHERITED(nsHTMLInputElement, nsGenericElement) 


// QueryInterface implementation for nsHTMLInputElement
NS_HTML_CONTENT_INTERFACE_MAP_BEGIN(nsHTMLInputElement,
                                    nsGenericHTMLLeafFormElement)
  NS_INTERFACE_MAP_ENTRY(nsIDOMHTMLInputElement)
  NS_INTERFACE_MAP_ENTRY(nsIDOMNSHTMLInputElement)
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO(HTMLInputElement)
NS_HTML_CONTENT_INTERFACE_MAP_END


// nsIDOMNode

nsresult
nsHTMLInputElement::CloneNode(PRBool aDeep, nsIDOMNode** aReturn)
{
  NS_ENSURE_ARG_POINTER(aReturn);
  *aReturn = nsnull;

  nsHTMLInputElement* it = new nsHTMLInputElement();

  if (!it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  nsCOMPtr<nsIDOMNode> kungFuDeathGrip(it);

  nsresult rv = it->nsGenericElement::Init(mNodeInfo);

  if (NS_FAILED(rv))
    return rv;

  CopyInnerTo(this, it, aDeep);

  *aReturn = NS_STATIC_CAST(nsIDOMNode *, it);

  NS_ADDREF(*aReturn);

  return NS_OK;
}


NS_IMETHODIMP
nsHTMLInputElement::GetForm(nsIDOMHTMLFormElement** aForm)
{
  return nsGenericHTMLLeafFormElement::GetForm(aForm);
}

NS_IMETHODIMP 
nsHTMLInputElement::GetDefaultValue(nsAWritableString& aDefaultValue)
{
  return nsGenericHTMLLeafFormElement::GetAttr(kNameSpaceID_HTML,
                                               nsHTMLAtoms::value,
                                               aDefaultValue);
}

NS_IMETHODIMP 
nsHTMLInputElement::SetDefaultValue(const nsAReadableString& aDefaultValue)
{
  return nsGenericHTMLLeafFormElement::SetAttr(kNameSpaceID_HTML,
                                               nsHTMLAtoms::value,
                                               aDefaultValue, PR_TRUE); 
}

NS_IMETHODIMP 
nsHTMLInputElement::GetDefaultChecked(PRBool* aDefaultChecked)
{
  nsHTMLValue val;                                                 
  nsresult rv;
  rv = GetHTMLAttribute(nsHTMLAtoms::checked, val);       

  *aDefaultChecked = (NS_CONTENT_ATTR_NOT_THERE != rv);                        

  return NS_OK;
}

NS_IMETHODIMP
nsHTMLInputElement::SetDefaultChecked(PRBool aDefaultChecked)
{
  nsresult rv = NS_OK;
  nsHTMLValue empty(eHTMLUnit_Empty);

  if (aDefaultChecked) {                                                     
    rv = SetHTMLAttribute(nsHTMLAtoms::checked, empty, PR_TRUE); 
  } else {                                                            
    rv = UnsetAttr(kNameSpaceID_HTML, nsHTMLAtoms::checked, PR_TRUE);
  }

  if (NS_SUCCEEDED(rv)) {
    //When setting DefaultChecked, we must also reset Checked (DOM Errata)
    SetChecked(aDefaultChecked);
  }

  return rv;
}
  
//NS_IMPL_STRING_ATTR(nsHTMLInputElement, DefaultValue, defaultvalue)
//NS_IMPL_BOOL_ATTR(nsHTMLInputElement, DefaultChecked, defaultchecked)
NS_IMPL_STRING_ATTR(nsHTMLInputElement, Accept, accept)
NS_IMPL_STRING_ATTR(nsHTMLInputElement, AccessKey, accesskey)
NS_IMPL_STRING_ATTR(nsHTMLInputElement, Align, align)
NS_IMPL_STRING_ATTR(nsHTMLInputElement, Alt, alt)
//NS_IMPL_BOOL_ATTR(nsHTMLInputElement, Checked, checked)
NS_IMPL_BOOL_ATTR(nsHTMLInputElement, Disabled, disabled)
NS_IMPL_INT_ATTR(nsHTMLInputElement, MaxLength, maxlength)
NS_IMPL_STRING_ATTR(nsHTMLInputElement, Name, name)
NS_IMPL_BOOL_ATTR(nsHTMLInputElement, ReadOnly, readonly)
NS_IMPL_STRING_ATTR(nsHTMLInputElement, Size, size)
NS_IMPL_STRING_ATTR(nsHTMLInputElement, Src, src)
NS_IMPL_INT_ATTR(nsHTMLInputElement, TabIndex, tabindex)
NS_IMPL_STRING_ATTR(nsHTMLInputElement, UseMap, usemap)
//NS_IMPL_STRING_ATTR(nsHTMLInputElement, Value, value)

NS_IMETHODIMP
nsHTMLInputElement::GetType(nsAWritableString& aValue)
{
  nsresult rv = nsGenericHTMLLeafFormElement::GetAttr(kNameSpaceID_HTML,
                                                      nsHTMLAtoms::type,
                                                      aValue);

  if (rv == NS_CONTENT_ATTR_NOT_THERE)
    aValue.Assign(NS_LITERAL_STRING("text"));

  return NS_OK;
}

NS_IMETHODIMP
nsHTMLInputElement::SetType(const nsAReadableString& aValue)
{
  return nsGenericHTMLLeafFormElement::SetAttr(kNameSpaceID_HTML,
                                               nsHTMLAtoms::type, aValue,
                                               PR_TRUE);
}

NS_IMETHODIMP 
nsHTMLInputElement::GetValue(nsAWritableString& aValue)
{
  PRInt32 type;
  GetType(&type);
  if (NS_FORM_INPUT_TEXT == type || NS_FORM_INPUT_PASSWORD == type ||
      NS_FORM_INPUT_FILE == type) {
    nsIFormControlFrame* formControlFrame = nsnull;
    if (NS_SUCCEEDED(GetPrimaryFrame(this, formControlFrame))) {
      if (formControlFrame) {
        formControlFrame->GetProperty(nsHTMLAtoms::value, aValue);
      }
    }
    else {
      // Retrieve the presentation state instead.
      nsCOMPtr<nsIPresState> presState;
      GetPrimaryPresState(this, getter_AddRefs(presState));

      // Obtain the value property from the presentation state.
      if (presState) {
        presState->GetStateProperty(NS_LITERAL_STRING("value"), aValue);
      }
    }
      
    return NS_OK;
  }

  // Treat value == defaultValue for other input elements
  nsresult rv = nsGenericHTMLLeafFormElement::GetAttr(kNameSpaceID_HTML,
                                                      nsHTMLAtoms::value,
                                                      aValue);

  if (rv == NS_CONTENT_ATTR_NOT_THERE && type == NS_FORM_INPUT_RADIO) {
    // The defauly value of a radio input is "on".
    aValue.Assign(NS_LITERAL_STRING("on"));

    return NS_OK;
  }

  return rv;
}

NS_IMETHODIMP 
nsHTMLInputElement::SetValue(const nsAReadableString& aValue)
{
  PRInt32 type;
  GetType(&type);
  if (NS_FORM_INPUT_TEXT == type || NS_FORM_INPUT_PASSWORD == type ||
      NS_FORM_INPUT_FILE == type) {
    if (NS_FORM_INPUT_FILE == type) {
      nsresult result;
      nsCOMPtr<nsIScriptSecurityManager> securityManager = 
               do_GetService(NS_SCRIPTSECURITYMANAGER_CONTRACTID, &result);
      if (NS_FAILED(result)) 
        return result;

      PRBool enabled;

      result = securityManager->IsCapabilityEnabled("UniversalFileRead",
                                                    &enabled);

      if (NS_FAILED(result)) {
        return result;
      }

      if (!enabled) {
        // setting the value of a "FILE" input widget requires the
        // UniversalFileRead privilege
        return NS_ERROR_DOM_SECURITY_ERR;
      }
    }

    nsIFormControlFrame* formControlFrame = nsnull;

    if (NS_SUCCEEDED(GetPrimaryFrame(this, formControlFrame))) {
      if (formControlFrame ) { 
        nsIPresContext* presContext;
        GetPresContext(this, &presContext);
        formControlFrame->SetProperty(presContext, nsHTMLAtoms::value, aValue);
        NS_IF_RELEASE(presContext);
      }
    }
    else {
      // Retrieve the presentation state instead.
      nsCOMPtr<nsIPresState> presState;
      GetPrimaryPresState(this, getter_AddRefs(presState));

      // Obtain the value property from the presentation state.
      if (presState) {
        presState->SetStateProperty(NS_LITERAL_STRING("value"), aValue);
      }
    }
    return NS_OK;
  }

  // Treat value == defaultValue for other input elements.
  return nsGenericHTMLLeafFormElement::SetAttr(kNameSpaceID_HTML,
                                               nsHTMLAtoms::value,
                                               aValue, PR_TRUE);
}

NS_IMETHODIMP 
nsHTMLInputElement::GetChecked(PRBool* aValue)
{
  nsAutoString value; value.AssignWithConversion("0");
  nsIFormControlFrame* formControlFrame = nsnull;
  if (NS_SUCCEEDED(GetPrimaryFrame(this, formControlFrame))) {
    if (formControlFrame) {
      formControlFrame->GetProperty(nsHTMLAtoms::checked, value);
    }
  }
  else {
    // Retrieve the presentation state instead.
    nsCOMPtr<nsIPresState> presState;
    GetPrimaryPresState(this, getter_AddRefs(presState));

    // Obtain the value property from the presentation state.
    if (presState) {
      presState->GetStateProperty(NS_LITERAL_STRING("checked"), value);
    }
  }
  
  if (value.EqualsWithConversion("1")) {
    *aValue = PR_TRUE;
  } else {
    *aValue = PR_FALSE;
  }

  return NS_OK;      
}

void
nsHTMLInputElement::SetPresStateChecked(nsIHTMLContent * aHTMLContent, 
                                        PRBool aValue)
{
  nsCOMPtr<nsIPresState> presState;
  GetPrimaryPresState(aHTMLContent, getter_AddRefs(presState));

  // Obtain the value property from the presentation state.
  if (presState) {
    nsAutoString value; value.AssignWithConversion( aValue ? "1" : "0" );
    presState->SetStateProperty(NS_LITERAL_STRING("checked"), value);
  }
}

NS_IMETHODIMP 
nsHTMLInputElement::SetChecked(PRBool aValue)
{
  // First check to see if the new value is the same as our current value.
  // If so, then return.
  PRBool checked = PR_TRUE;
  GetChecked(&checked);
  if (checked == aValue) {
    return NS_OK;
  }

  // Since the value is changing, send out an onchange event (bug 23571)
  nsCOMPtr<nsIPresContext> presContext;
  GetPresContext(this, getter_AddRefs(presContext));

  nsIFormControlFrame* formControlFrame = nsnull;
  if (NS_SUCCEEDED(GetPrimaryFrame(this, formControlFrame))) {
    // the value is being toggled
    nsAutoString val; val.AssignWithConversion(aValue ? "1" : "0");

    formControlFrame->SetProperty(presContext, nsHTMLAtoms::checked, val);
  }
  else {
    SetPresStateChecked(this, aValue);

    PRInt32 type;
    GetType(&type);
    if (type == NS_FORM_INPUT_RADIO) {
      nsAutoString name;
      GetName(name);

      nsCOMPtr<nsIDOMHTMLFormElement> formElement;
      if (NS_SUCCEEDED(GetForm(getter_AddRefs(formElement))) && formElement) {
        nsCOMPtr<nsIDOMHTMLCollection> controls; 

        nsresult rv = formElement->GetElements(getter_AddRefs(controls)); 

        if (controls) {
          PRUint32 numControls;

          controls->GetLength(&numControls);

          for (PRUint32 i = 0; i < numControls; i++) {
            nsCOMPtr<nsIDOMNode> elementNode;

            controls->Item(i, getter_AddRefs(elementNode));

            if (elementNode) {
              nsCOMPtr<nsIDOMHTMLInputElement>
                inputElement(do_QueryInterface(elementNode));

              if (inputElement && inputElement.get() !=
                  NS_STATIC_CAST(nsIDOMHTMLInputElement *, this)) {
                nsAutoString childName;

                rv = inputElement->GetName(childName);

                if (NS_SUCCEEDED(rv)) {
                  if (name == childName) {
                    nsCOMPtr<nsIHTMLContent>
                      htmlContent(do_QueryInterface(inputElement));

                    SetPresStateChecked(htmlContent, PR_FALSE);
                  }
                }
              }
            }
          }
        }
      }
    }
  }

  nsEventStatus status = nsEventStatus_eIgnore;
  nsEvent event;
  event.eventStructType = NS_EVENT;
  event.message = NS_FORM_CHANGE;
  HandleDOMEvent(presContext, &event, nsnull, NS_EVENT_FLAG_INIT, &status);

  return NS_OK;     
}

NS_IMETHODIMP
nsHTMLInputElement::Blur()
{
  return SetElementFocus(PR_FALSE);
}

NS_IMETHODIMP
nsHTMLInputElement::Focus()
{
  return SetElementFocus(PR_TRUE);
}

NS_IMETHODIMP
nsHTMLInputElement::SetFocus(nsIPresContext* aPresContext)
{
  NS_ENSURE_ARG_POINTER(aPresContext);

  // We can't be focus'd if we don't have a document
  if (! mDocument)
    return NS_OK;

  // first see if we are disabled or not. If disabled then do nothing.
  nsAutoString disabled;
  if (NS_CONTENT_ATTR_HAS_VALUE ==
      nsGenericHTMLLeafFormElement::GetAttr(kNameSpaceID_HTML,
                                            nsHTMLAtoms::disabled,
                                            disabled)) {
    return NS_OK;
  }
 
  // If the window is not active, do not allow the focus to bring the
  // window to the front.  We update the focus controller, but do
  // nothing else.
  nsCOMPtr<nsIFocusController> focusController;
  nsCOMPtr<nsIScriptGlobalObject> globalObj;
  mDocument->GetScriptGlobalObject(getter_AddRefs(globalObj));
  nsCOMPtr<nsPIDOMWindow> win(do_QueryInterface(globalObj));
  win->GetRootFocusController(getter_AddRefs(focusController));
  PRBool isActive = PR_FALSE;
  focusController->GetActive(&isActive);
  if (!isActive) {
    focusController->SetFocusedElement(this);
    return NS_OK;
  }

  nsCOMPtr<nsIEventStateManager> esm;

  aPresContext->GetEventStateManager(getter_AddRefs(esm));

  if (esm) {
    esm->SetContentState(this, NS_EVENT_STATE_FOCUS);
  }

  nsIFormControlFrame* formControlFrame = nsnull;

  GetPrimaryFrame(this, formControlFrame);

  if (formControlFrame) {
    formControlFrame->SetFocus(PR_TRUE, PR_TRUE);
    formControlFrame->ScrollIntoView(aPresContext);
    // Could call SelectAll(aPresContext) here to automatically
    // select text when we receive focus - only for text and password!
  }

  return NS_OK;
}

NS_IMETHODIMP
nsHTMLInputElement::RemoveFocus(nsIPresContext* aPresContext)
{
  NS_ENSURE_ARG_POINTER(aPresContext);
  // If we are disabled, we probably shouldn't have focus in the
  // first place, so allow it to be removed.
  nsresult rv = NS_OK;

  nsIFormControlFrame* formControlFrame = nsnull;

  GetPrimaryFrame(this, formControlFrame);

  if (formControlFrame) {
    formControlFrame->SetFocus(PR_FALSE, PR_FALSE);
  }

  nsCOMPtr<nsIEventStateManager> esm;
  aPresContext->GetEventStateManager(getter_AddRefs(esm));

  if (esm) {
    nsCOMPtr<nsIDocument> doc;

    GetDocument(*getter_AddRefs(doc));

    if (!doc)
      return NS_ERROR_NULL_POINTER;

    nsCOMPtr<nsIContent> rootContent;

    doc->GetRootContent(getter_AddRefs(rootContent));

    rv = esm->SetContentState(rootContent, NS_EVENT_STATE_FOCUS);
  }

  return rv;
}

NS_IMETHODIMP
nsHTMLInputElement::Select()
{
  nsresult rv = NS_OK;

  // first see if we are disabled or not. If disabled then do nothing.
  nsAutoString disabled;
  if (NS_CONTENT_ATTR_HAS_VALUE ==
      nsGenericHTMLLeafFormElement::GetAttr(kNameSpaceID_HTML,
                                            nsHTMLAtoms::disabled,
                                            disabled)) {
    return rv;
  }

  // see what type of input we are.  Only select for texts and passwords
  PRInt32 type;
  GetType(&type);

  if (NS_FORM_INPUT_PASSWORD == type ||
      NS_FORM_INPUT_TEXT == type) {
    // XXX Bug?  We have to give the input focus before contents can be
    // selected

    nsCOMPtr<nsIPresContext> presContext;
    GetPresContext(this, getter_AddRefs(presContext)); 

    // If the window is not active, do not allow the select to bring the
    // window to the front.  We update the focus controller, but do
    // nothing else.
    nsCOMPtr<nsIScriptGlobalObject> globalObj;
    mDocument->GetScriptGlobalObject(getter_AddRefs(globalObj));
    nsCOMPtr<nsPIDOMWindow> win(do_QueryInterface(globalObj));
    nsCOMPtr<nsIFocusController> focusController;
    win->GetRootFocusController(getter_AddRefs(focusController));
    PRBool isActive = PR_FALSE;
    focusController->GetActive(&isActive);
    if (!isActive) {
      focusController->SetFocusedElement(this);
      SelectAll(presContext);
      return NS_OK;
    }

    // Just like SetFocus() but without the ScrollIntoView()!
    nsEventStatus status = nsEventStatus_eIgnore;
    nsEvent event;
    event.eventStructType = NS_EVENT;
    event.message = NS_FORM_SELECTED;
    rv = HandleDOMEvent(presContext, &event, nsnull, NS_EVENT_FLAG_INIT,
                        &status);

    // If the DOM event was not canceled (e.g. by a JS event handler
    // returning false)
    if (status == nsEventStatus_eIgnore) {
      nsCOMPtr<nsIEventStateManager> esm;
      if (NS_OK == presContext->GetEventStateManager(getter_AddRefs(esm))) {
        esm->SetContentState(this, NS_EVENT_STATE_FOCUS);
      }

      nsIFormControlFrame* formControlFrame = nsnull;
      rv = GetPrimaryFrame(this, formControlFrame);
      if (NS_SUCCEEDED(rv)) {
        formControlFrame->SetFocus(PR_TRUE, PR_TRUE);

        // Now Select all the text!
        SelectAll(presContext);
      }
    }
  }

  return rv;
}

void
nsHTMLInputElement::SelectAll(nsIPresContext* aPresContext)
{
  nsIFormControlFrame* formControlFrame = nsnull;
  GetPrimaryFrame(this, formControlFrame);

  if (formControlFrame) {
    formControlFrame->SetProperty(aPresContext, nsHTMLAtoms::select,
                                  nsAutoString());
  }
}

NS_IMETHODIMP
nsHTMLInputElement::Click()
{
  nsresult rv = NS_OK;

  if (mHandlingClick)   // Fixes crash as in bug 41599 --heikki@netscape.com
      return rv;

  // first see if we are disabled or not. If disabled then do nothing.
  nsAutoString disabled;
  if (NS_CONTENT_ATTR_HAS_VALUE ==
      nsGenericHTMLLeafFormElement::GetAttr(kNameSpaceID_HTML,
                                            nsHTMLAtoms::disabled,
                                            disabled)) {
    return rv;
  }

  // see what type of input we are.  Only click button, checkbox, radio,
  // reset, submit, & image
  PRInt32 type;
  GetType(&type);
  if (NS_FORM_INPUT_BUTTON == type ||
      NS_FORM_INPUT_CHECKBOX == type ||
      NS_FORM_INPUT_RADIO == type ||
      NS_FORM_INPUT_RESET == type ||
      NS_FORM_INPUT_SUBMIT == type) {

    nsCOMPtr<nsIDocument> doc; // Strong
    rv = GetDocument(*getter_AddRefs(doc));

    if (doc) {
      PRInt32 numShells = doc->GetNumberOfShells();
      nsCOMPtr<nsIPresContext> context;
      for (PRInt32 i=0; i<numShells; i++) {
        nsCOMPtr<nsIPresShell> shell;
        doc->GetShellAt(i, getter_AddRefs(shell));
        if (shell) {
          rv = shell->GetPresContext(getter_AddRefs(context));
          if (context) {
            nsEventStatus status = nsEventStatus_eIgnore;
            nsMouseEvent event;
            event.eventStructType = NS_MOUSE_EVENT;
            event.message = NS_MOUSE_LEFT_CLICK;
            event.isShift = PR_FALSE;
            event.isControl = PR_FALSE;
            event.isAlt = PR_FALSE;
            event.isMeta = PR_FALSE;
            event.clickCount = 0;
            event.widget = nsnull;
            
            mHandlingClick = PR_TRUE;

            rv = HandleDOMEvent(context, &event, nsnull, NS_EVENT_FLAG_INIT,
                                &status);

            mHandlingClick = PR_FALSE;
          }
        }
      }
    }
  }
  
  return NS_OK;
}

nsresult
nsHTMLInputElement::MouseClickForAltText(nsIPresContext* aPresContext)
{
  NS_ENSURE_ARG_POINTER(aPresContext);
  PRBool disabled;

  nsresult rv = GetDisabled(&disabled);
  if (NS_FAILED(rv) || disabled) {
    return rv;
  }

  // Generate a submit event targetted at the form content
  nsCOMPtr<nsIContent> form(do_QueryInterface(mForm));

  if (form) {
    nsCOMPtr<nsIPresShell> shell;
    aPresContext->GetShell(getter_AddRefs(shell));
    if (shell) {
      nsCOMPtr<nsIContent> formControl = this; // kungFuDeathGrip

      nsFormEvent event;
      event.eventStructType = NS_FORM_EVENT;
      event.message         = NS_FORM_SUBMIT;
      event.originator      = formControl;
      nsEventStatus status  = nsEventStatus_eIgnore;
      shell->HandleDOMEventWithTarget(form, &event, &status);
    }
  }

  return rv;
}

NS_IMETHODIMP
nsHTMLInputElement::HandleDOMEvent(nsIPresContext* aPresContext,
                                   nsEvent* aEvent,
                                   nsIDOMEvent** aDOMEvent,
                                   PRUint32 aFlags,
                                   nsEventStatus* aEventStatus)
{
  NS_ENSURE_ARG_POINTER(aEventStatus);
  if ((aEvent->message == NS_FOCUS_CONTENT && mSkipFocusEvent) ||
      (aEvent->message == NS_BLUR_CONTENT && mSkipFocusEvent)) {
    return NS_OK;
  }

  nsCOMPtr<nsIDOMEventTarget> oldTarget;
      
  // Do not process any DOM events if the element is disabled
  PRBool disabled;

  nsresult rv = GetDisabled(&disabled);
  if (NS_FAILED(rv) || disabled) {
    return rv;
  }
  
  // Bugscape 2369: Type might change during JS event handler
  //                This pointer is only valid until
  //                nsGenericHTMLLeafFormElement::HandleDOMEvent
  nsIFormControlFrame* formControlFrame = nsnull;
  rv = GetPrimaryFrame(this, formControlFrame, PR_FALSE);

  nsIFrame* formFrame = nsnull;

  if (formControlFrame &&
      NS_SUCCEEDED(formControlFrame->QueryInterface(NS_GET_IID(nsIFrame),
                                                    (void **)&formFrame)) &&
      formFrame) {
    const nsStyleUserInterface* uiStyle;
    formFrame->GetStyleData(eStyleStruct_UserInterface,
                            (const nsStyleStruct *&)uiStyle);

    if (uiStyle->mUserInput == NS_STYLE_USER_INPUT_NONE ||
        uiStyle->mUserInput == NS_STYLE_USER_INPUT_DISABLED) {
      return NS_OK;
    }
  }

  // If we're a file input we have anonymous content underneath
  // that we need to hide.  We need to set the event target now
  // to ourselves
  //
  // Bugscape 2369: Type might change during JS event handler
  //                This is only valid until
  //                nsGenericHTMLLeafFormElement::HandleDOMEvent
  //
  PRInt32 type;
  GetType(&type);

  if (type == NS_FORM_INPUT_FILE ||
      type == NS_FORM_INPUT_TEXT ) {
    // If the event is starting here that's fine.  If it's not
    // init'ing here it started beneath us and needs modification.
    if (!(NS_EVENT_FLAG_INIT & aFlags)) {
      if (!*aDOMEvent) {
        // We haven't made a DOMEvent yet.  Force making one now.
        nsCOMPtr<nsIEventListenerManager> listenerManager;
        rv = GetListenerManager(getter_AddRefs(listenerManager));

        if (NS_FAILED(rv)) {
          return rv;
        }

        nsAutoString empty;
        rv = listenerManager->CreateEvent(aPresContext, aEvent, empty,
                                          aDOMEvent);

        if (NS_FAILED(rv)) {
          return rv;
        }
      }

      if (!*aDOMEvent) {
        return NS_ERROR_FAILURE;
      }

      nsCOMPtr<nsIPrivateDOMEvent> privateEvent(do_QueryInterface(*aDOMEvent));

      if (!privateEvent) {
        return NS_ERROR_FAILURE;
      }

      (*aDOMEvent)->GetTarget(getter_AddRefs(oldTarget));

      nsCOMPtr<nsIDOMEventTarget> originalTarget;
      (*aDOMEvent)->GetOriginalTarget(getter_AddRefs(originalTarget));
      if (!originalTarget) {
        privateEvent->SetOriginalTarget(oldTarget);
      }

      nsCOMPtr<nsIDOMEventTarget>
        target(do_QueryInterface((nsIDOMHTMLInputElement*)this));

      privateEvent->SetTarget(target);
    }
  }

  // Preset the the value of the checkbox or the radiobutton before
  // calling into script If the event gets "cancelled" then we have to
  // "back out" this change, but the odds of that are slimmer than it
  // being set each time
  //
  // Start by remember the original value and for radio buttons we
  // must get the currently selected radiobtn. So we go get the
  // content instead of the frame
  PRBool orginalCheckedValue = PR_FALSE;
  PRBool checkWasSet         = PR_FALSE;
  nsCOMPtr<nsIContent> selectedRadiobtn;
  if (!(aFlags & NS_EVENT_FLAG_CAPTURE) &&
      aEvent->message == NS_MOUSE_LEFT_CLICK) {
    GetChecked(&orginalCheckedValue);
    checkWasSet = PR_TRUE;
    switch(type) {
      case NS_FORM_INPUT_CHECKBOX:
        {
          PRBool checked;
          GetChecked(&checked);
          SetChecked(!checked);
          // Fire an event to notify accessibility
#ifdef ACCESSIBILITY
          FireEventForAccessibility( aPresContext, NS_LITERAL_STRING("CheckboxStateChange"));
#endif
        }
        break;

      case NS_FORM_INPUT_RADIO:
        {
          // Get the currently selected button from the radio group
          // we get access to that via the nsIRadioControlFrame interface
          // because the current grouping is kept in the frame.
          nsIRadioControlFrame * rb = nsnull;
          if (formControlFrame != nsnull) {
            nsresult resv = formControlFrame->QueryInterface(NS_GET_IID(nsIRadioControlFrame), (void**)&rb);
            if (NS_SUCCEEDED(resv) && rb) {
              rb->GetRadioGroupSelectedContent(getter_AddRefs(selectedRadiobtn));
            }
          }
          SetChecked(PR_TRUE);
          // Fire an event to notify accessibility
#ifdef ACCESSIBILITY
          if ( selectedRadiobtn != this ) {
            FireEventForAccessibility( aPresContext, NS_LITERAL_STRING("RadiobuttonStateChange"));;
          }
#endif
        }
        break;

      default:
        break;
    } //switch
  }

  // Try script event handlers first if its not a focus/blur event
  //we dont want the doc to get these
  nsresult ret = nsGenericHTMLLeafFormElement::HandleDOMEvent(aPresContext,
                                                              aEvent,
                                                              aDOMEvent,
                                                              aFlags,
                                                              aEventStatus);

  // now check to see if the event was "cancelled"
  if (nsEventStatus_eConsumeNoDefault == *aEventStatus && checkWasSet &&
      (type == NS_FORM_INPUT_CHECKBOX || type == NS_FORM_INPUT_RADIO)) {
    // if it was cancelled and a radio button, then set the old
    // selceted btn to TRUE. if it is a checkbox then set it to it's
    // original value
    if (selectedRadiobtn) {
      nsCOMPtr<nsIDOMHTMLInputElement> inputElement(do_QueryInterface(selectedRadiobtn));
      if (inputElement) {
        inputElement->SetChecked(PR_TRUE);
      }
    } else {
      SetChecked(orginalCheckedValue);
    }
  }

  // Bugscape 2369: Frame might have changed during event handler
  formControlFrame = nsnull;
  rv = nsGenericHTMLElement::GetPrimaryFrame(this, formControlFrame, PR_FALSE);

  // Finish the special file control processing...
  if (oldTarget) {
    // If the event is starting here that's fine.  If it's not
    // init'ing here it started beneath us and needs modification.
    if (!(NS_EVENT_FLAG_INIT & aFlags)) {
      if (!*aDOMEvent) {
        return NS_ERROR_FAILURE;
      }
      nsCOMPtr<nsIPrivateDOMEvent> privateEvent =
        do_QueryInterface(*aDOMEvent);

      if (!privateEvent) {
        return NS_ERROR_FAILURE;
      }

      // This will reset the target to its original value
      privateEvent->SetTarget(oldTarget);
    }
  }

  // Bugscape 2369: type might have changed during event handler
  GetType(&type);

  if (type == NS_FORM_INPUT_IMAGE && 
      aEvent->message == NS_MOUSE_LEFT_BUTTON_UP && 
      nsEventStatus_eIgnore == *aEventStatus &&
      aFlags & NS_EVENT_FLAG_BUBBLE) {
    // Tell the frame about the click
    return MouseClickForAltText(aPresContext);
  }

  if ((NS_OK == rv) && (nsEventStatus_eIgnore == *aEventStatus) &&
      !(aFlags & NS_EVENT_FLAG_CAPTURE)) {
    switch (aEvent->message) {

      case NS_FOCUS_CONTENT:
      {
        if (formControlFrame) {
          mSkipFocusEvent = PR_TRUE;
          formControlFrame->SetFocus(PR_TRUE, PR_TRUE);
          mSkipFocusEvent = PR_FALSE;
          return NS_OK;
        }
      }                                                                         
      break; // NS_FOCUS_CONTENT

      case NS_KEY_PRESS:
      case NS_KEY_UP:
      {
        // For backwards compat, trigger checks/radios/buttons with
        // space or enter (bug 25300)
        nsKeyEvent * keyEvent = (nsKeyEvent *)aEvent;

        if ((aEvent->message == NS_KEY_PRESS &&
             keyEvent->keyCode == NS_VK_RETURN) ||
            (aEvent->message == NS_KEY_UP &&
             keyEvent->keyCode == NS_VK_SPACE)) {
          switch(type) {
            case NS_FORM_INPUT_CHECKBOX:
            case NS_FORM_INPUT_RADIO:
            {
              // Checkbox and Radio try to submit on Enter press
              if (keyEvent->keyCode != NS_VK_SPACE) {
                // Generate a submit event targetted at the form content
                nsCOMPtr<nsIContent> form(do_QueryInterface(mForm));

                if (form) {
                  nsCOMPtr<nsIPresShell> shell;
                  aPresContext->GetShell(getter_AddRefs(shell));
                  if (shell) {
                    nsCOMPtr<nsIContent> formControl = this; // kungFuDeathGrip

                    nsFormEvent event;
                    event.eventStructType = NS_FORM_EVENT;
                    event.message         = NS_FORM_SUBMIT;
                    event.originator      = formControl;
                    nsEventStatus status  = nsEventStatus_eIgnore;
                    shell->HandleDOMEventWithTarget(form, &event, &status);
                  }
                }
                break;  // If we are submitting, do not send click event
              }
              // else fall through and treat Space like click...
            }
            case NS_FORM_INPUT_BUTTON:
            case NS_FORM_INPUT_RESET:
            case NS_FORM_INPUT_SUBMIT:
            case NS_FORM_INPUT_IMAGE: // Bug 34418
            {
              nsEventStatus status = nsEventStatus_eIgnore;
              nsMouseEvent event;
              event.eventStructType = NS_MOUSE_EVENT;
              event.message = NS_MOUSE_LEFT_CLICK;
              event.isShift = PR_FALSE;
              event.isControl = PR_FALSE;
              event.isAlt = PR_FALSE;
              event.isMeta = PR_FALSE;
              event.clickCount = 0;
              event.widget = nsnull;
              rv = HandleDOMEvent(aPresContext, &event, nsnull,
                                  NS_EVENT_FLAG_INIT, &status);
            } // case
          } // switch
        }
      } break;// NS_KEY_PRESS || NS_KEY_UP

      // cancel all of these events for buttons
      case NS_MOUSE_MIDDLE_BUTTON_DOWN:
      case NS_MOUSE_MIDDLE_BUTTON_UP:
      case NS_MOUSE_MIDDLE_DOUBLECLICK:
      case NS_MOUSE_RIGHT_DOUBLECLICK:
      case NS_MOUSE_RIGHT_BUTTON_DOWN:
      case NS_MOUSE_RIGHT_BUTTON_UP:
      {
        if (type == NS_FORM_INPUT_BUTTON || 
            type == NS_FORM_INPUT_RESET || 
            type == NS_FORM_INPUT_SUBMIT ) {
          if (aDOMEvent != nsnull && *aDOMEvent != nsnull) {
            (*aDOMEvent)->PreventBubble();
          } else {
            rv = NS_ERROR_FAILURE;
          }
        }

        break;
      }
      case NS_MOUSE_LEFT_CLICK:
      {
        switch(type) {
          case NS_FORM_INPUT_BUTTON:
          case NS_FORM_INPUT_RESET:
          case NS_FORM_INPUT_SUBMIT:
          case NS_FORM_INPUT_IMAGE:
            {
              // Tell the frame about the click
              if (formControlFrame) {
                formControlFrame->MouseClicked(aPresContext);
              }
            }
            break;

          default:
            break;
        } //switch 
      } break;// NS_MOUSE_LEFT_BUTTON_DOWN
    } //switch 
  } // if

  return rv;
}


// nsIHTMLContent

static nsGenericHTMLElement::EnumTable kInputTypeTable[] = {
  { "browse", NS_FORM_BROWSE }, // XXX not valid html, but it is convenient
  { "button", NS_FORM_INPUT_BUTTON },
  { "checkbox", NS_FORM_INPUT_CHECKBOX },
  { "file", NS_FORM_INPUT_FILE },
  { "hidden", NS_FORM_INPUT_HIDDEN },
  { "reset", NS_FORM_INPUT_RESET },
  { "image", NS_FORM_INPUT_IMAGE },
  { "password", NS_FORM_INPUT_PASSWORD },
  { "radio", NS_FORM_INPUT_RADIO },
  { "submit", NS_FORM_INPUT_SUBMIT },
  { "text", NS_FORM_INPUT_TEXT },
  { 0 }
};

NS_IMETHODIMP
nsHTMLInputElement::StringToAttribute(nsIAtom* aAttribute,
                                      const nsAReadableString& aValue,
                                      nsHTMLValue& aResult)
{
  if (aAttribute == nsHTMLAtoms::type) {
    nsGenericHTMLElement::EnumTable *table = kInputTypeTable;
    nsAutoString valueStr(aValue);
    while (nsnull != table->tag) { 
      if (valueStr.EqualsIgnoreCase(table->tag)) {
        // If the type is being changed to file, set the element value
        // to the empty string. This is for security.
        if (table->value == NS_FORM_INPUT_FILE)
          SetValue(NS_LITERAL_STRING(""));
        aResult.SetIntValue(table->value, eHTMLUnit_Enumerated);
        mType = table->value;  // set the type of this input
        return NS_CONTENT_ATTR_HAS_VALUE;
      }
      table++;
    }
  }
  else if (aAttribute == nsHTMLAtoms::checked) {
    aResult.SetEmptyValue();
    return NS_CONTENT_ATTR_HAS_VALUE;
  }
  else if (aAttribute == nsHTMLAtoms::disabled) {
    aResult.SetEmptyValue();
    return NS_CONTENT_ATTR_HAS_VALUE;
  }
  else if (aAttribute == nsHTMLAtoms::readonly) {
    aResult.SetEmptyValue();
    return NS_CONTENT_ATTR_HAS_VALUE;
  }
  else if (aAttribute == nsHTMLAtoms::width) {
    if (ParseValueOrPercent(aValue, aResult, eHTMLUnit_Pixel)) {
      return NS_CONTENT_ATTR_HAS_VALUE;
    }
  }
  else if (aAttribute == nsHTMLAtoms::height) {
    if (ParseValueOrPercent(aValue, aResult, eHTMLUnit_Pixel)) {
      return NS_CONTENT_ATTR_HAS_VALUE;
    }
  }
  else if (aAttribute == nsHTMLAtoms::maxlength) {
    if (ParseValue(aValue, 0, aResult, eHTMLUnit_Integer)) {
      return NS_CONTENT_ATTR_HAS_VALUE;
    }
  }
  else if (aAttribute == nsHTMLAtoms::size) {
    if (ParseValue(aValue, 0, aResult, eHTMLUnit_Integer)) {
      return NS_CONTENT_ATTR_HAS_VALUE;
    }
  }
  else if (aAttribute == nsHTMLAtoms::tabindex) {
    if (ParseValue(aValue, 0, aResult, eHTMLUnit_Integer)) {
      return NS_CONTENT_ATTR_HAS_VALUE;
    }
  }
  else if (aAttribute == nsHTMLAtoms::border) {
    if (ParseValue(aValue, 0, aResult, eHTMLUnit_Pixel)) {
      return NS_CONTENT_ATTR_HAS_VALUE;
    }
  }
  else if (aAttribute == nsHTMLAtoms::align) {
    if (ParseAlignValue(aValue, aResult)) {
      return NS_CONTENT_ATTR_HAS_VALUE;
    }
  }
  else if (IsImage()) {
    if (ParseImageAttribute(aAttribute, aValue, aResult)) {
      return NS_CONTENT_ATTR_HAS_VALUE;
    }
  }

  return NS_CONTENT_ATTR_NOT_THERE;
}

NS_IMETHODIMP
nsHTMLInputElement::AttributeToString(nsIAtom* aAttribute,
                                      const nsHTMLValue& aValue,
                                      nsAWritableString& aResult) const
{
  if (aAttribute == nsHTMLAtoms::type) {
    if (eHTMLUnit_Enumerated == aValue.GetUnit()) {
      // The DOM spec says that input types should be capitalized but
      // AFAIK all existing browsers return the type in lower case,
      // http://bugzilla.mozilla.org/show_bug.cgi?id=32369 is the bug
      // about this problem, we pass PR_FALSE as the last argument
      // here to avoid capitalizing the input type (this is required for
      // backwards compatibility). -- jst@netscape.com

      EnumValueToString(aValue, kInputTypeTable, aResult, PR_FALSE);

      return NS_CONTENT_ATTR_HAS_VALUE;
    }
  }
  else if (aAttribute == nsHTMLAtoms::align) {
    if (eHTMLUnit_Enumerated == aValue.GetUnit()) {
      AlignValueToString(aValue, aResult);
      return NS_CONTENT_ATTR_HAS_VALUE;
    }
  }
  else if (aAttribute == nsHTMLAtoms::checked) {
    aResult.Assign(NS_LITERAL_STRING("checked"));
    return NS_CONTENT_ATTR_HAS_VALUE;
  }
  else if (IsImage() && ImageAttributeToString(aAttribute, aValue,
                                               aResult)) {
    return NS_CONTENT_ATTR_HAS_VALUE;
  }

  return nsGenericHTMLLeafFormElement::AttributeToString(aAttribute, aValue,
                                                         aResult);
}

static void
MapAttributesIntoRule(const nsIHTMLMappedAttributes* aAttributes,
                      nsRuleData* aData)
{
  if (!aData)
    return;

  nsHTMLValue value;
  aAttributes->GetAttribute(nsHTMLAtoms::type, value);
  if (eHTMLUnit_Enumerated == value.GetUnit()) {  
    switch (value.GetIntValue()) {
      case NS_FORM_INPUT_IMAGE: {
        nsGenericHTMLElement::MapImageBorderAttributeInto(aAttributes, aData);
        nsGenericHTMLElement::MapImageMarginAttributeInto(aAttributes, aData);
        nsGenericHTMLElement::MapImagePositionAttributeInto(aAttributes, aData);
        break;
      }
    }
  }

  nsGenericHTMLElement::MapAlignAttributeInto(aAttributes, aData);
  nsGenericHTMLElement::MapCommonAttributesInto(aAttributes, aData);
}

NS_IMETHODIMP
nsHTMLInputElement::GetMappedAttributeImpact(const nsIAtom* aAttribute, PRInt32 aModType,
                                             PRInt32& aHint) const
{
  if (aAttribute == nsHTMLAtoms::value) {
    aHint = NS_STYLE_HINT_REFLOW;
  }
  else if ((aAttribute == nsHTMLAtoms::align) ||
           (aAttribute == nsHTMLAtoms::type)) {
    aHint = NS_STYLE_HINT_FRAMECHANGE;
  }
  else if (!GetCommonMappedAttributesImpact(aAttribute, aHint)) {
    if (!GetImageMappedAttributesImpact(aAttribute, aHint)) {
      if (!GetImageBorderAttributeImpact(aAttribute, aHint)) {
        aHint = NS_STYLE_HINT_CONTENT;
      }
    }
  }

  return NS_OK;
}


NS_IMETHODIMP
nsHTMLInputElement::GetAttributeMappingFunction(nsMapRuleToAttributesFunc& aMapRuleFunc) const
{
  aMapRuleFunc = &MapAttributesIntoRule;
  return NS_OK;
}


// nsIFormControl

NS_IMETHODIMP
nsHTMLInputElement::GetType(PRInt32* aType)
{
  if (aType) {
    *aType = mType;
    return NS_OK;
  } else {
    return NS_FORM_NOTOK;
  }
}

#ifdef DEBUG
NS_IMETHODIMP
nsHTMLInputElement::SizeOf(nsISizeOfHandler* aSizer, PRUint32* aResult) const
{
  *aResult = sizeof(*this) + BaseSizeOf(aSizer);

  return NS_OK;
}
#endif


// Controllers Methods

NS_IMETHODIMP
nsHTMLInputElement::GetControllers(nsIControllers** aResult)
{
  NS_ENSURE_ARG_POINTER(aResult);

  PRInt32 type;
  GetType(&type);

  //XXX: what about type "file"?
  if (NS_FORM_INPUT_TEXT == type || NS_FORM_INPUT_PASSWORD == type)
  {
    if (!mControllers)
    {
      NS_ENSURE_SUCCESS (
        nsComponentManager::CreateInstance(kXULControllersCID,
                                           nsnull,
                                           NS_GET_IID(nsIControllers),
                                           getter_AddRefs(mControllers)),
        NS_ERROR_FAILURE);
      if (!mControllers) { return NS_ERROR_NULL_POINTER; }

      nsresult rv;
      nsCOMPtr<nsIController>
        controller(do_CreateInstance("@mozilla.org/editor/editorcontroller;1",
                                     &rv));
      if (NS_FAILED(rv)) return rv;

      nsCOMPtr<nsIEditorController>
        editorController = do_QueryInterface(controller, &rv);

      if (NS_FAILED(rv))
        return rv;

      rv = editorController->Init(nsnull);
      if (NS_FAILED(rv))
        return rv;

      mControllers->AppendController(controller);
    }
  }

  *aResult = mControllers;
  NS_IF_ADDREF(*aResult);

  return NS_OK;
}

NS_IMETHODIMP
nsHTMLInputElement::GetTextLength(PRInt32* aTextLength)
{
  nsIFormControlFrame* formControlFrame = nsnull;
  GetPrimaryFrame(this, formControlFrame);

  if (formControlFrame) {
    nsCOMPtr<textControlPlace>
      textControlFrame(do_QueryInterface(formControlFrame));

    if (textControlFrame)
      textControlFrame->GetTextLength(aTextLength);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsHTMLInputElement::SetSelectionRange(PRInt32 aSelectionStart,
                                      PRInt32 aSelectionEnd)
{
  nsIFormControlFrame* formControlFrame = nsnull;
  GetPrimaryFrame(this, formControlFrame);

  if (formControlFrame) {
    nsCOMPtr<textControlPlace>
      textControlFrame(do_QueryInterface(formControlFrame));

    if (textControlFrame)
      textControlFrame->SetSelectionRange(aSelectionStart, aSelectionEnd);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsHTMLInputElement::GetSelectionStart(PRInt32* aSelectionStart)
{
  NS_ENSURE_ARG_POINTER(aSelectionStart);
  
  PRInt32 selEnd;
  return GetSelectionRange(aSelectionStart, &selEnd);
}

NS_IMETHODIMP
nsHTMLInputElement::SetSelectionStart(PRInt32 aSelectionStart)
{
  nsIFormControlFrame* formControlFrame = nsnull;
  GetPrimaryFrame(this, formControlFrame);

  if (formControlFrame) {
    nsCOMPtr<textControlPlace>
      textControlFrame(do_QueryInterface(formControlFrame));

    if (textControlFrame)
      textControlFrame->SetSelectionStart(aSelectionStart);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsHTMLInputElement::GetSelectionEnd(PRInt32* aSelectionEnd)
{
  NS_ENSURE_ARG_POINTER(aSelectionEnd);
  
  PRInt32 selStart;
  return GetSelectionRange(&selStart, aSelectionEnd);
}


NS_IMETHODIMP
nsHTMLInputElement::SetSelectionEnd(PRInt32 aSelectionEnd)
{
  nsIFormControlFrame* formControlFrame = nsnull;
  GetPrimaryFrame(this, formControlFrame);

  if (formControlFrame) {
    nsCOMPtr<textControlPlace>
      textControlFrame(do_QueryInterface(formControlFrame));

    if (textControlFrame)
      textControlFrame->SetSelectionEnd(aSelectionEnd);
  }

  return NS_OK;
}

nsresult
nsHTMLInputElement::GetSelectionRange(PRInt32* aSelectionStart,
                                      PRInt32* aSelectionEnd)
{
  nsIFormControlFrame* formControlFrame = nsnull;
  GetPrimaryFrame(this, formControlFrame);

  if (formControlFrame) {
    nsCOMPtr<textControlPlace>
      textControlFrame(do_QueryInterface(formControlFrame));

    if (textControlFrame)
      textControlFrame->GetSelectionRange(aSelectionStart, aSelectionEnd);
  }

  return NS_OK;
}

#ifdef ACCESSIBILITY
nsresult
nsHTMLInputElement::FireEventForAccessibility(nsIPresContext* aPresContext,
                                              const nsAReadableString& aEventType)
{
  nsCOMPtr<nsIEventListenerManager> listenerManager;

  nsresult rv = GetListenerManager(getter_AddRefs(listenerManager));
  if ( !listenerManager )
    return rv;

  // Create the DOM event
  nsCOMPtr<nsIDOMEvent> domEvent;
  rv = listenerManager->CreateEvent(aPresContext,
                                    nsnull, 
                                    NS_LITERAL_STRING("MutationEvent"),
                                    getter_AddRefs(domEvent) );
  if ( !domEvent )
    return NS_ERROR_FAILURE;

  // Initialize the mutation event
  nsCOMPtr<nsIDOMMutationEvent> mutEvent(do_QueryInterface(domEvent));
  if ( !mutEvent )
    return NS_ERROR_FAILURE;
  nsAutoString empty;
  mutEvent->InitMutationEvent( aEventType, PR_TRUE, PR_TRUE, nsnull, empty, empty, empty, nsIDOMMutationEvent::MODIFICATION);

  // Set the target of the event to this nsHTMLInputElement, which should be checkbox content??
  nsCOMPtr<nsIPrivateDOMEvent> privEvent(do_QueryInterface(domEvent));
  if ( ! privEvent )
    return NS_ERROR_FAILURE;
  nsCOMPtr<nsIDOMEventTarget> targ(do_QueryInterface(NS_STATIC_CAST(nsIDOMHTMLInputElement *, this)));
  if ( ! targ )
    return NS_ERROR_FAILURE;
  privEvent->SetTarget(targ);

  // Dispatch the event
  nsCOMPtr<nsIDOMEventReceiver> eventReceiver(do_QueryInterface(listenerManager));
  if ( ! eventReceiver )
    return NS_ERROR_FAILURE;
  PRBool noDefault;
  eventReceiver->DispatchEvent(domEvent, &noDefault);

  return NS_OK;
}
#endif
