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
 *   Pierre Phaneuf <pp@ludusdesign.com>
 */
#include "nsCOMPtr.h"
#include "nsIDOMHTMLInputElement.h"
#include "nsIDOMNSHTMLInputElement.h"
#include "nsIControllers.h"
#include "nsIEditorController.h"

#include "nsRDFCID.h"
#include "nsIComponentManager.h"
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

#include "nsIPresState.h"
#include "nsIDOMEvent.h"
#include "nsIDOMNodeList.h"
#include "nsIDOMHTMLCollection.h"
#include "nsICheckboxControlFrame.h"
#include "nsIRadioControlFrame.h"

// XXX align=left, hspace, vspace, border? other nav4 attrs

static NS_DEFINE_IID(kIFrameIID, NS_IFRAME_IID);
static NS_DEFINE_CID(kXULControllersCID,  NS_XULCONTROLLERS_CID);

#ifdef ENDER_LITE
typedef nsIGfxTextControlFrame2 textControlPlace;
#else
typedef nsIGfxTextControlFrame textControlPlace;
#endif

class nsHTMLInputElement : public nsIDOMHTMLInputElement,
                           public nsIDOMNSHTMLInputElement,
                           public nsIJSScriptObject,
                           public nsIHTMLContent,
                           public nsIFormControl
{
public:
  nsHTMLInputElement(nsINodeInfo *aNodeInfo);
  virtual ~nsHTMLInputElement();

  // nsISupports
  NS_DECL_ISUPPORTS

  // nsIDOMNode
  NS_IMPL_IDOMNODE_USING_GENERIC(mInner)

  // nsIDOMElement
    // can't use the macro here because input type=text needs to notify up to 
    // frame system on SetAttribute("value");
  NS_IMETHOD GetTagName(nsAWritableString& aTagName) {
    return mInner.GetTagName(aTagName);
  }
  NS_IMETHOD GetAttribute(const nsAReadableString& aName, nsAWritableString& aReturn) {      
    return mInner.GetAttribute(aName, aReturn);                                
  }                                                                        
  NS_IMETHOD SetAttribute(const nsAReadableString& aName, const nsAReadableString& aValue) { 
    nsAutoString valueAttribute;
    nsHTMLAtoms::value->ToString(valueAttribute);
    if (PR_TRUE==valueAttribute.Equals(aName)) {
      SetValue(aValue);
      // Don't return here, need to set the attribute in the content model too.
    }
    return mInner.SetAttribute(aName, aValue);                                 
  }                                                                        
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

  // nsIDOMHTMLInputElement
  NS_DECL_IDOMHTMLINPUTELEMENT

  // nsIDOMNSHTMLInputElement
  NS_DECL_IDOMNSHTMLINPUTELEMENT

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

  // Helper method
  NS_IMETHOD SetPresStateChecked(nsIHTMLContent * aHTMLContent, 
                                 nsIStatefulFrame::StateType aStateType,
                                 PRBool aValue);

protected:

  nsresult   GetSelectionRange(PRInt32* aSelectionStart, PRInt32* aSelectionEnd);

protected:
  nsGenericHTMLLeafFormElement mInner;
  nsIForm*                 mForm;
  PRInt32                  mType;
  PRBool                   mSkipFocusEvent;
  nsCOMPtr<nsIControllers> mControllers;


  NS_IMETHOD SelectAll(nsIPresContext* aPresContext);
  PRBool IsImage() const {
    nsAutoString tmp;
    mInner.GetAttribute(kNameSpaceID_HTML, nsHTMLAtoms::type, tmp);
    return tmp.EqualsIgnoreCase("image");
  }

  PRBool mHandlingClick;
};

// construction, destruction

nsresult
NS_NewHTMLInputElement(nsIHTMLContent** aInstancePtrResult,
                       nsINodeInfo *aNodeInfo)
{
  NS_ENSURE_ARG_POINTER(aInstancePtrResult);
  NS_ENSURE_ARG_POINTER(aNodeInfo);

  nsIHTMLContent* it = new nsHTMLInputElement(aNodeInfo);
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return it->QueryInterface(NS_GET_IID(nsIHTMLContent), (void**) aInstancePtrResult);
}


nsHTMLInputElement::nsHTMLInputElement(nsINodeInfo *aNodeInfo)
{
  NS_INIT_REFCNT();
  mInner.Init(this, aNodeInfo);
  mType = NS_FORM_INPUT_TEXT; // default value
  mForm = nsnull;
  mSkipFocusEvent = PR_FALSE;
  //nsTraceRefcnt::Create((nsIFormControl*)this, "nsHTMLFormControlElement", __FILE__, __LINE__);
  mHandlingClick = PR_FALSE;
}

nsHTMLInputElement::~nsHTMLInputElement()
{
  // Null out form's pointer to us - no ref counting here!
  SetForm(nsnull);
}

// nsISupports

NS_IMETHODIMP
nsHTMLInputElement::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
  NS_IMPL_HTML_CONTENT_QUERY_INTERFACE(aIID, aInstancePtr, this)
  if (aIID.Equals(NS_GET_IID(nsIDOMHTMLInputElement))) {
    *aInstancePtr = (void*)(nsIDOMHTMLInputElement*) this;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(NS_GET_IID(nsIDOMNSHTMLInputElement))) {
    *aInstancePtr = (void*)(nsIDOMNSHTMLInputElement*) this;
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

NS_IMPL_ADDREF(nsHTMLInputElement);
NS_IMPL_RELEASE(nsHTMLInputElement);

// nsIDOMNode

nsresult
nsHTMLInputElement::CloneNode(PRBool aDeep, nsIDOMNode** aReturn)
{
  nsHTMLInputElement* it = new nsHTMLInputElement(mInner.mNodeInfo);
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  nsCOMPtr<nsIDOMNode> kungFuDeathGrip(it);
  mInner.CopyInnerTo(this, &it->mInner, aDeep);
  return it->QueryInterface(NS_GET_IID(nsIDOMNode), (void**) aReturn);
}

// nsIContent

NS_IMETHODIMP
nsHTMLInputElement::SetParent(nsIContent* aParent)
{
  return mInner.SetParentForFormControls(aParent, this, mForm);
}

NS_IMETHODIMP
nsHTMLInputElement::SetDocument(nsIDocument* aDocument, PRBool aDeep, PRBool aCompileEventHandlers)
{
  return mInner.SetDocumentForFormControls(aDocument, aDeep, aCompileEventHandlers, this, mForm);
}

NS_IMETHODIMP
nsHTMLInputElement::GetForm(nsIDOMHTMLFormElement** aForm)
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
nsHTMLInputElement::GetDefaultValue(nsAWritableString& aDefaultValue)
{
  return mInner.GetAttribute(kNameSpaceID_HTML, nsHTMLAtoms::value, aDefaultValue);
}

NS_IMETHODIMP 
nsHTMLInputElement::SetDefaultValue(const nsAReadableString& aDefaultValue)
{
  return mInner.SetAttribute(kNameSpaceID_HTML, nsHTMLAtoms::value, aDefaultValue, PR_TRUE); 
}

NS_IMETHODIMP 
nsHTMLInputElement::GetDefaultChecked(PRBool* aDefaultChecked)
{
  nsHTMLValue val;                                                 
  nsresult rv = mInner.GetHTMLAttribute(nsHTMLAtoms::checked, val);       
  *aDefaultChecked = (NS_CONTENT_ATTR_NOT_THERE != rv);                        
  return NS_OK;                                                     
}

NS_IMETHODIMP
nsHTMLInputElement::SetDefaultChecked(PRBool aDefaultChecked)
{
  nsresult rv = NS_OK;
  nsHTMLValue empty(eHTMLUnit_Empty);
  if (aDefaultChecked) {                                                     
    rv = mInner.SetHTMLAttribute(nsHTMLAtoms::checked, empty, PR_TRUE); 
  } else {                                                            
    rv = mInner.UnsetAttribute(kNameSpaceID_HTML, nsHTMLAtoms::checked, PR_TRUE);             
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
  nsresult rv = mInner.GetAttribute(kNameSpaceID_HTML, nsHTMLAtoms::type,
                                    aValue);

  if (rv == NS_CONTENT_ATTR_NOT_THERE)
    aValue.Assign(NS_LITERAL_STRING("text"));

  return NS_OK;
}

NS_IMETHODIMP
nsHTMLInputElement::SetType(const nsAReadableString& aValue)
{
  return mInner.SetAttribute(kNameSpaceID_HTML, nsHTMLAtoms::type, aValue,
                             PR_TRUE);
}

NS_IMETHODIMP 
nsHTMLInputElement::GetValue(nsAWritableString& aValue)
{
  PRInt32 type;
  GetType(&type);
  if (NS_FORM_INPUT_TEXT == type || NS_FORM_INPUT_PASSWORD == type || NS_FORM_INPUT_FILE == type) {
    nsIFormControlFrame* formControlFrame = nsnull;
    if (NS_SUCCEEDED(nsGenericHTMLElement::GetPrimaryFrame(this, formControlFrame))) {
      if (nsnull != formControlFrame) {
        formControlFrame->GetProperty(nsHTMLAtoms::value, aValue);
      }
    }
    else {
      // Retrieve the presentation state instead.
      nsCOMPtr<nsIPresState> presState;
      nsGenericHTMLElement::GetPrimaryPresState(this, nsIStatefulFrame::eTextType, getter_AddRefs(presState));

      // Obtain the value property from the presentation state.
      if (presState) {
        presState->GetStateProperty(NS_LITERAL_STRING("value"), aValue);
      }
    }
      
    return NS_OK;
  }
  // Treat value == defaultValue for other input elements
  return mInner.GetAttribute(kNameSpaceID_HTML, nsHTMLAtoms::value, aValue); 
}

NS_IMETHODIMP 
nsHTMLInputElement::SetValue(const nsAReadableString& aValue)
{
  PRInt32 type;
  GetType(&type);
  if (NS_FORM_INPUT_TEXT == type || NS_FORM_INPUT_PASSWORD == type || NS_FORM_INPUT_FILE == type) {
    if (NS_FORM_INPUT_FILE == type) {
      nsresult result;
      NS_WITH_SERVICE(nsIScriptSecurityManager, securityManager,
                      NS_SCRIPTSECURITYMANAGER_PROGID, &result);
      if (NS_FAILED(result)) 
        return result;
      PRBool enabled;
      if (NS_FAILED(result = securityManager->IsCapabilityEnabled("UniversalFileRead", 
                                                                  &enabled)))
      {
        return result;
      }
      if (!enabled) {
        // setting the value of a "FILE" input widget requires the UniversalFileRead privilege
        return NS_ERROR_DOM_SECURITY_ERR;
      }

    }
    nsIFormControlFrame* formControlFrame = nsnull;
    if (NS_SUCCEEDED(nsGenericHTMLElement::GetPrimaryFrame(this, formControlFrame))) {
      if (nsnull != formControlFrame ) { 
        nsIPresContext* presContext;
        nsGenericHTMLElement::GetPresContext(this, &presContext);
        formControlFrame->SetProperty(presContext, nsHTMLAtoms::value, aValue);
        NS_IF_RELEASE(presContext);
      }
    }
    else {
      // Retrieve the presentation state instead.
      nsCOMPtr<nsIPresState> presState;
      nsGenericHTMLElement::GetPrimaryPresState(this, nsIStatefulFrame::eTextType, getter_AddRefs(presState));

      // Obtain the value property from the presentation state.
      if (presState) {
        presState->SetStateProperty(NS_LITERAL_STRING("value"), aValue);
      }
    }
    return NS_OK;
  }

  // Treat value == defaultValue for other input elements.
  return mInner.SetAttribute(kNameSpaceID_HTML, nsHTMLAtoms::value, aValue, PR_TRUE);
}

NS_IMETHODIMP 
nsHTMLInputElement::GetChecked(PRBool* aValue)
{
  nsAutoString value; value.AssignWithConversion("0");
  nsIFormControlFrame* formControlFrame = nsnull;
  if (NS_SUCCEEDED(nsGenericHTMLElement::GetPrimaryFrame(this, formControlFrame))) {
    if (nsnull != formControlFrame) {
      formControlFrame->GetProperty(nsHTMLAtoms::checked, value);
    }
  }
  else {
    // Retrieve the presentation state instead.
    nsCOMPtr<nsIPresState> presState;
    PRInt32 type;
    GetType(&type);
    nsIStatefulFrame::StateType stateType = (type == NS_FORM_INPUT_CHECKBOX?nsIStatefulFrame::eCheckboxType:
                                                                            nsIStatefulFrame::eRadioType);
    nsGenericHTMLElement::GetPrimaryPresState(this, stateType, getter_AddRefs(presState));

    // Obtain the value property from the presentation state.
    if (presState) {
      presState->GetStateProperty(NS_ConvertASCIItoUCS2("checked"), value);
    }
  }
  
  if (value.EqualsWithConversion("1"))
    *aValue = PR_TRUE;
  else
    *aValue = PR_FALSE;

  return NS_OK;      
}

NS_IMETHODIMP 
nsHTMLInputElement::SetPresStateChecked(nsIHTMLContent * aHTMLContent, 
                                        nsIStatefulFrame::StateType aStateType,
                                        PRBool aValue)
{
  nsCOMPtr<nsIPresState> presState;
  nsGenericHTMLElement::GetPrimaryPresState(aHTMLContent, aStateType, getter_AddRefs(presState));

  // Obtain the value property from the presentation state.
  if (presState) {
    nsAutoString value; value.AssignWithConversion( aValue ? "1" : "0" );
    presState->SetStateProperty(NS_ConvertASCIItoUCS2("checked"), value);
    return NS_OK;
  }
  return NS_ERROR_FAILURE;
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
  nsGenericHTMLElement::GetPresContext(this, getter_AddRefs(presContext));

  nsIFormControlFrame* formControlFrame = nsnull;
  if (NS_OK == nsGenericHTMLElement::GetPrimaryFrame(this, formControlFrame)) {

    // the value is being toggled
    formControlFrame->SetProperty(presContext, nsHTMLAtoms::checked, NS_ConvertASCIItoUCS2(PR_TRUE == aValue?"1":"0"));
  }
  else {
    // Retrieve the presentation state instead.
    nsCOMPtr<nsIPresState> presState;
    PRInt32 type;
    GetType(&type);
    nsIStatefulFrame::StateType stateType = (type == NS_FORM_INPUT_CHECKBOX?nsIStatefulFrame::eCheckboxType:
                                                                            nsIStatefulFrame::eRadioType);
    if (NS_FAILED(SetPresStateChecked(this, stateType, aValue))) {
      return NS_ERROR_FAILURE;
    }

    if (stateType == nsIStatefulFrame::eRadioType) {
      nsIDOMHTMLInputElement * radioElement = (nsIDOMHTMLInputElement*)this;
      nsAutoString name;
      GetName(name);
    
      nsCOMPtr<nsIDOMHTMLFormElement> formElement;
      if (NS_SUCCEEDED(GetForm(getter_AddRefs(formElement)))) {
        nsCOMPtr<nsIDOMHTMLCollection> controls; 
        nsresult rv = formElement->GetElements(getter_AddRefs(controls)); 
        if (controls) {
          if (NS_SUCCEEDED(rv) && nsnull != controls) {
            PRUint32 numControls;
            controls->GetLength(&numControls);
            for (PRUint32 i = 0; i < numControls; i++) {
              nsCOMPtr<nsIDOMNode> elementNode;
              controls->Item(i, getter_AddRefs(elementNode));
              if (elementNode) {
                nsCOMPtr<nsIDOMHTMLInputElement> inputElement(do_QueryInterface(elementNode));
                if (NS_SUCCEEDED(rv) && inputElement && (radioElement != inputElement.get())) {
                  nsAutoString childName;
                  rv = inputElement->GetName(childName);
                  if (NS_SUCCEEDED(rv)) {
                    if (name == childName) {
                      nsCOMPtr<nsIHTMLContent> htmlContent = do_QueryInterface(inputElement);
                      SetPresStateChecked(htmlContent, nsIStatefulFrame::eRadioType, PR_FALSE);
                    }
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
  nsGUIEvent event;
  event.eventStructType = NS_GUI_EVENT;
  event.message = NS_FORM_CHANGE;
  event.flags = NS_EVENT_FLAG_NONE;
  event.widget = nsnull;
  HandleDOMEvent(presContext, &event, nsnull, NS_EVENT_FLAG_INIT, &status);

  return NS_OK;     
}

NS_IMETHODIMP
nsHTMLInputElement::Blur()
{
  nsCOMPtr<nsIPresContext> presContext;
  nsGenericHTMLElement::GetPresContext(this, getter_AddRefs(presContext));
  return RemoveFocus(presContext);
}

NS_IMETHODIMP
nsHTMLInputElement::Focus()
{
  nsCOMPtr<nsIPresContext> presContext;
  nsGenericHTMLElement::GetPresContext(this, getter_AddRefs(presContext));
  return SetFocus(presContext);
}

NS_IMETHODIMP
nsHTMLInputElement::SetFocus(nsIPresContext* aPresContext)
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
    // Could call SelectAll(aPresContext) here to automatically
    // select text when we receive focus - only for text and password!
  }

  return rv;
}

NS_IMETHODIMP
nsHTMLInputElement::RemoveFocus(nsIPresContext* aPresContext)
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

NS_IMETHODIMP
nsHTMLInputElement::Select()
{
  nsresult rv = NS_OK;

  // first see if we are disabled or not. If disabled then do nothing.
  nsAutoString disabled;
  if (NS_CONTENT_ATTR_HAS_VALUE == mInner.GetAttribute(kNameSpaceID_HTML, nsHTMLAtoms::disabled, disabled)) {
    return rv;
  }

  // see what type of input we are.  Only select for texts and passwords
  PRInt32 type;
  GetType(&type);
  if (NS_FORM_INPUT_PASSWORD == type ||
      NS_FORM_INPUT_TEXT == type) {
 
    // XXX Bug?  We have to give the input focus before contents can be selected

    // Just like SetFocus() but without the ScrollIntoView()!
    nsCOMPtr<nsIPresContext> presContext;
    nsGenericHTMLElement::GetPresContext(this, getter_AddRefs(presContext)); 
    nsEventStatus status = nsEventStatus_eIgnore;
    nsGUIEvent event;
    event.eventStructType = NS_GUI_EVENT;
    event.message = NS_FORM_SELECTED;
    event.flags = NS_EVENT_FLAG_NONE;
    event.widget = nsnull;
    rv = HandleDOMEvent(presContext, &event, nsnull, NS_EVENT_FLAG_INIT, &status);

    // If the DOM event was not canceled (e.g. by a JS event handler returning false)
    if (status == nsEventStatus_eIgnore) {
      nsCOMPtr<nsIEventStateManager> esm;
      if (NS_OK == presContext->GetEventStateManager(getter_AddRefs(esm))) {
        esm->SetContentState(this, NS_EVENT_STATE_FOCUS);
      }

      nsIFormControlFrame* formControlFrame = nsnull;
      rv = nsGenericHTMLElement::GetPrimaryFrame(this, formControlFrame);
      if (NS_SUCCEEDED(rv)) {
        formControlFrame->SetFocus(PR_TRUE, PR_TRUE);

        // Now Select all the text!
        SelectAll(presContext);
      }
    }
  }
  return rv;
}

NS_IMETHODIMP
nsHTMLInputElement::SelectAll(nsIPresContext* aPresContext)
{
  nsIFormControlFrame* formControlFrame = nsnull;
  nsresult rv = nsGenericHTMLElement::GetPrimaryFrame(this, formControlFrame);
  if (NS_SUCCEEDED(rv)) {
    if (formControlFrame )
    {
      formControlFrame->SetProperty(aPresContext, nsHTMLAtoms::select, nsAutoString());
      return NS_OK;
    }
  }
  return rv;
}

NS_IMETHODIMP
nsHTMLInputElement::Click()
{
  nsresult rv = NS_OK;

  if (mHandlingClick)   // Fixes crash as in bug 41599 --heikki@netscape.com
      return rv;

  // first see if we are disabled or not. If disabled then do nothing.
  nsAutoString disabled;
  if (NS_CONTENT_ATTR_HAS_VALUE == mInner.GetAttribute(kNameSpaceID_HTML, nsHTMLAtoms::disabled, disabled)) {
    return rv;
  }

  // see what type of input we are.  Only click button, checkbox, radio, reset, submit, & image
  PRInt32 type;
  GetType(&type);
  if (NS_FORM_INPUT_BUTTON == type ||
      NS_FORM_INPUT_CHECKBOX == type ||
      NS_FORM_INPUT_RADIO == type ||
      NS_FORM_INPUT_RESET == type ||
      NS_FORM_INPUT_SUBMIT == type) {

    nsCOMPtr<nsIDocument> doc; // Strong
    rv = GetDocument(*getter_AddRefs(doc));
    if (NS_SUCCEEDED(rv) && doc) {
      PRInt32 numShells = doc->GetNumberOfShells();
      nsCOMPtr<nsIPresContext> context;
      for (PRInt32 i=0; i<numShells; i++) {
        nsCOMPtr<nsIPresShell> shell = getter_AddRefs(doc->GetShellAt(i));
        if (shell) {
          rv = shell->GetPresContext(getter_AddRefs(context));
          if (NS_SUCCEEDED(rv) && context) {
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
            
            mHandlingClick = PR_TRUE;

            rv = HandleDOMEvent(context, &event, nsnull, NS_EVENT_FLAG_INIT, &status);

            mHandlingClick = PR_FALSE;
          }
        }
      }
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
  
  nsIFormControlFrame* formControlFrame = nsnull;
  rv = nsGenericHTMLElement::GetPrimaryFrame(this, formControlFrame, PR_FALSE);
  nsIFrame* formFrame = nsnull;

  if (formControlFrame && NS_SUCCEEDED(formControlFrame->QueryInterface(kIFrameIID, (void **)&formFrame)) && formFrame)
  {
    const nsStyleUserInterface* uiStyle;
    formFrame->GetStyleData(eStyleStruct_UserInterface, (const nsStyleStruct *&)uiStyle);
    if (uiStyle->mUserInput == NS_STYLE_USER_INPUT_NONE ||
        uiStyle->mUserInput == NS_STYLE_USER_INPUT_DISABLED)
    {
      return NS_OK;
    }
  }

  // If we're a file input we have anonymous content underneath
  // that we need to hide.  We need to set the event target now
  // to ourselves
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
        if (NS_FAILED(rv = mInner.GetListenerManager(getter_AddRefs(listenerManager)))) {
          return rv;
        }
        nsAutoString empty;
        if (NS_FAILED(rv = listenerManager->CreateEvent(aPresContext, aEvent, empty, aDOMEvent))) {
          return rv;
        }
      }
      if (!*aDOMEvent) {
        return NS_ERROR_FAILURE;
      }
      nsCOMPtr<nsIPrivateDOMEvent> privateEvent = do_QueryInterface(*aDOMEvent);
      if (!privateEvent) {
        return NS_ERROR_FAILURE;
      }

      (*aDOMEvent)->GetTarget(getter_AddRefs(oldTarget));

      nsCOMPtr<nsIDOMEventTarget> originalTarget;
      (*aDOMEvent)->GetOriginalTarget(getter_AddRefs(originalTarget));
      if (!originalTarget) {
        privateEvent->SetOriginalTarget(oldTarget);
      }

      nsCOMPtr<nsIDOMEventTarget> target = do_QueryInterface((nsIDOMHTMLInputElement*)this);
      privateEvent->SetTarget(target);
    }
  }

  // When a user clicks on a checkbox the value needs to be set after the onmouseup
  // and before the onclick event is processed via script. The EVM always lets script
  // get first crack at the processing, and script can cancel further processing of 
  // the event by return null.
  //
  // This means the checkbox needs to have it's new value set before it goes to script 
  // to process the onclick and then if script cancels the event it needs to be set back.
  // In Nav and IE there is a flash of it being set and then unset
  // 
  // We have added this extra method to the checkbox to tell it to temporarily return the
  // opposite value while processing the click event. This way script gets the correct "future"
  // value of the checkbox, but there is no visual updating until after script is done processing.
  // That way if the event is cancelled then the checkbox will not flash.
  //
  // So get the Frame for the checkbox and tell it we are processing an onclick event
  nsCOMPtr<nsICheckboxControlFrame> chkBx;
  nsCOMPtr<nsIRadioControlFrame> radio;
  if (NS_SUCCEEDED(rv)) {
    chkBx = do_QueryInterface(formControlFrame);
    if (!chkBx) {
      radio = do_QueryInterface(formControlFrame);
    }
  }
  if (aEvent->message == NS_MOUSE_LEFT_CLICK) {
    if (chkBx) chkBx->SetIsInClickEvent(PR_TRUE);
    if (radio) radio->SetIsInClickEvent(PR_TRUE);
  }

  // Try script event handlers first if its not a focus/blur event
  //we dont want the doc to get these
  nsresult ret = mInner.HandleDOMEvent(aPresContext, aEvent, aDOMEvent,
                               aFlags, aEventStatus);

  // Script is done processing, now tell the checkbox we are no longer doing an onclick
  // and if it was cancelled the checkbox will get the propriate value via the DOM listener

  if (aEvent->message == NS_MOUSE_LEFT_CLICK) {
    if (chkBx) chkBx->SetIsInClickEvent(PR_FALSE);
    if (radio) radio->SetIsInClickEvent(PR_FALSE);
  }

  // Finish the special file control processing...
  if (type == NS_FORM_INPUT_FILE ||
      type == NS_FORM_INPUT_TEXT ) {
    // If the event is starting here that's fine.  If it's not
    // init'ing here it started beneath us and needs modification.
    if (!(NS_EVENT_FLAG_INIT & aFlags)) {
      if (!*aDOMEvent) {
        return NS_ERROR_FAILURE;
      }
      nsCOMPtr<nsIPrivateDOMEvent> privateEvent = do_QueryInterface(*aDOMEvent);
      if (!privateEvent) {
        return NS_ERROR_FAILURE;
      }

      // This will reset the target to its original value
      privateEvent->SetTarget(oldTarget);
    }
  }


  if ((NS_OK == ret) && (nsEventStatus_eIgnore == *aEventStatus) &&
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
      {
        // For backwards compat, trigger checks/radios/buttons with space or enter (bug 25300)
        nsKeyEvent * keyEvent = (nsKeyEvent *)aEvent;
        if (keyEvent->keyCode == NS_VK_RETURN || keyEvent->charCode == NS_VK_SPACE) {
          switch(type) {
            case NS_FORM_INPUT_CHECKBOX:
            case NS_FORM_INPUT_RADIO:
            case NS_FORM_INPUT_BUTTON:
            case NS_FORM_INPUT_RESET:
            case NS_FORM_INPUT_SUBMIT:
            case NS_FORM_INPUT_IMAGE: // Bug 34418
            {
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
            } // case
          } // switch
        }
      } break;// NS_KEY_PRESS

      case NS_MOUSE_MIDDLE_BUTTON_DOWN:     // cancel all of these events for buttons
      case NS_MOUSE_MIDDLE_BUTTON_UP:
      case NS_MOUSE_MIDDLE_DOUBLECLICK:
      case NS_MOUSE_RIGHT_DOUBLECLICK:
      case NS_MOUSE_RIGHT_BUTTON_DOWN:
      case NS_MOUSE_RIGHT_BUTTON_UP: {
          if (type == NS_FORM_INPUT_BUTTON || 
              type == NS_FORM_INPUT_RESET || 
              type == NS_FORM_INPUT_SUBMIT ) {
            if (aDOMEvent != nsnull && *aDOMEvent != nsnull) {
              (*aDOMEvent)->PreventBubble();
            } else {
              ret = NS_ERROR_FAILURE;
            }
          }
        } break;

      case NS_MOUSE_LEFT_CLICK:
      {
        switch(type) {
          case NS_FORM_INPUT_CHECKBOX:
            {
              PRBool checked;
              GetChecked(&checked);
              SetChecked(!checked);
            }
            break;
          case NS_FORM_INPUT_RADIO:
            SetChecked(PR_TRUE);
            break;

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


        } //switch 
      } break;// NS_MOUSE_LEFT_BUTTON_DOWN
    } //switch 

  } // if

  return ret;
}


// nsIHTMLContent

static nsGenericHTMLElement::EnumTable kInputTypeTable[] = {
  { "browse", NS_FORM_BROWSE }, // XXX not valid html, but it is convient
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
    if (nsGenericHTMLElement::ParseValueOrPercent(aValue, aResult,
                                                  eHTMLUnit_Pixel)) {
      return NS_CONTENT_ATTR_HAS_VALUE;
    }
  } 
  else if (aAttribute == nsHTMLAtoms::height) {
    if (nsGenericHTMLElement::ParseValueOrPercent(aValue, aResult,
                                                  eHTMLUnit_Pixel)) {
      return NS_CONTENT_ATTR_HAS_VALUE;
    }
  } 
  else if (aAttribute == nsHTMLAtoms::maxlength) {
    if (nsGenericHTMLElement::ParseValue(aValue, 0, aResult, eHTMLUnit_Integer)) {
      return NS_CONTENT_ATTR_HAS_VALUE;
    }
  }
  else if (aAttribute == nsHTMLAtoms::size) {
    if (nsGenericHTMLElement::ParseValue(aValue, 0, aResult, eHTMLUnit_Integer)) {
      return NS_CONTENT_ATTR_HAS_VALUE;
    }
  }
  else if (aAttribute == nsHTMLAtoms::tabindex) {
    if (nsGenericHTMLElement::ParseValue(aValue, 0, aResult, eHTMLUnit_Integer)) {
      return NS_CONTENT_ATTR_HAS_VALUE;
    }
  }
  else if (aAttribute == nsHTMLAtoms::border) {
    if (nsGenericHTMLElement::ParseValue(aValue, 0, aResult, eHTMLUnit_Pixel)) {
      return NS_CONTENT_ATTR_HAS_VALUE;
    }
  } 
  else if (aAttribute == nsHTMLAtoms::align) {
    if (nsGenericHTMLElement::ParseAlignValue(aValue, aResult)) {
      return NS_CONTENT_ATTR_HAS_VALUE;
    }
  }
  else if (IsImage()) {
    if (nsGenericHTMLElement::ParseImageAttribute(aAttribute,
                                                  aValue, aResult)) {
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

      nsGenericHTMLElement::EnumValueToString(aValue, kInputTypeTable,
                                              aResult, PR_FALSE);
      return NS_CONTENT_ATTR_HAS_VALUE;
    }
  }
  else if (aAttribute == nsHTMLAtoms::align) {
    if (eHTMLUnit_Enumerated == aValue.GetUnit()) {
      nsGenericHTMLElement::AlignValueToString(aValue, aResult);
      return NS_CONTENT_ATTR_HAS_VALUE;
    }
  }
  else if (IsImage() &&
           nsGenericHTMLElement::ImageAttributeToString(aAttribute,
                                                        aValue, aResult)) {
    return NS_CONTENT_ATTR_HAS_VALUE;
  } 
  return mInner.AttributeToString(aAttribute, aValue, aResult);
}

static void
MapAttributesInto(const nsIHTMLMappedAttributes* aAttributes,
                  nsIMutableStyleContext* aContext,
                  nsIPresContext* aPresContext)
{
  nsHTMLValue value;

  aAttributes->GetAttribute(nsHTMLAtoms::align, value);
  if (eHTMLUnit_Enumerated == value.GetUnit()) {
    nsStyleDisplay* display = (nsStyleDisplay*)
      aContext->GetMutableStyleData(eStyleStruct_Display);
    nsStyleText* text = (nsStyleText*)
      aContext->GetMutableStyleData(eStyleStruct_Text);
    switch (value.GetIntValue()) {
    case NS_STYLE_TEXT_ALIGN_LEFT:
      display->mFloats = NS_STYLE_FLOAT_LEFT;
      break;
    case NS_STYLE_TEXT_ALIGN_RIGHT:
      display->mFloats = NS_STYLE_FLOAT_RIGHT;
      break;
    default:
      text->mVerticalAlign.SetIntValue(value.GetIntValue(), eStyleUnit_Enumerated);
      break;
    }
  }

  aAttributes->GetAttribute(nsHTMLAtoms::type, value);
  if (eHTMLUnit_Enumerated == value.GetUnit()) {  
    switch (value.GetIntValue()) {
//XXX when there exists both a Standard and Quirks ua.css, remove this code 
//XXX it may be needed again if we don't have 2 ua.css files
//XXX this is now handled by attribute selectors in ua.css
#if 0
    case NS_FORM_INPUT_CHECKBOX:
      case NS_FORM_INPUT_RADIO:
      {
        float p2t;
        aPresContext->GetScaledPixelsToTwips(&p2t);
        nscoord pad = NSIntPixelsToTwips(3, p2t);

        // add left and right padding around the radio button via css
        nsStyleSpacing* spacing = (nsStyleSpacing*) aContext->GetMutableStyleData(eStyleStruct_Spacing);
        if (eStyleUnit_Null == spacing->mMargin.GetLeftUnit()) {
          nsStyleCoord left(pad);
          spacing->mMargin.SetLeft(left);
        }
        if (eStyleUnit_Null == spacing->mMargin.GetRightUnit()) {
          nsStyleCoord right(NSIntPixelsToTwips(5, p2t));
          spacing->mMargin.SetRight(right);
        }
        // add bottom padding if backward mode
        // XXX why isn't this working?
        nsCompatibility mode;
        aPresContext->GetCompatibilityMode(&mode);
        if (eCompatibility_NavQuirks == mode) {
          if (eStyleUnit_Null == spacing->mMargin.GetBottomUnit()) {
            nsStyleCoord bottom(pad);
            spacing->mMargin.SetBottom(bottom);
          }
        }
        break;
      }
#endif
      case NS_FORM_INPUT_IMAGE:
      {
        nsGenericHTMLElement::MapImageBorderAttributeInto(aAttributes, aContext, aPresContext, nsnull);
        nsGenericHTMLElement::MapImageAttributesInto(aAttributes, aContext, aPresContext);
        break;
      }
    }
  }
  nsGenericHTMLElement::MapCommonAttributesInto(aAttributes, aContext, aPresContext);
}

NS_IMETHODIMP
nsHTMLInputElement::GetMappedAttributeImpact(const nsIAtom* aAttribute,
                                             PRInt32& aHint) const
{
  if ((aAttribute == nsHTMLAtoms::align) || (aAttribute == nsHTMLAtoms::value)) {
    aHint = NS_STYLE_HINT_REFLOW;
  }
  else if ((aAttribute == nsHTMLAtoms::type)) {
    aHint = NS_STYLE_HINT_FRAMECHANGE;
  }
  else if (! nsGenericHTMLElement::GetCommonMappedAttributesImpact(aAttribute, aHint)) {
    if (! nsGenericHTMLElement::GetImageMappedAttributesImpact(aAttribute, aHint)) {
      if (! nsGenericHTMLElement::GetImageBorderAttributeImpact(aAttribute, aHint)) {
        aHint = NS_STYLE_HINT_CONTENT;
      }
    }
  }

  return NS_OK;
}


NS_IMETHODIMP
nsHTMLInputElement::GetAttributeMappingFunctions(nsMapAttributesFunc& aFontMapFunc,
                                                 nsMapAttributesFunc& aMapFunc) const
{
  aFontMapFunc = nsnull;
  aMapFunc = &MapAttributesInto;
  return NS_OK;
}

// nsIFormControl
NS_IMETHODIMP
nsHTMLInputElement::SetForm(nsIDOMHTMLFormElement* aForm)
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
nsHTMLInputElement::GetType(PRInt32* aType)
{
  if (aType) {
    *aType = mType;
    return NS_OK;
  } else {
    return NS_FORM_NOTOK;
  }
}



NS_IMETHODIMP
nsHTMLInputElement::SizeOf(nsISizeOfHandler* aSizer, PRUint32* aResult) const
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
      nsCOMPtr<nsIController> controller = do_CreateInstance("component://netscape/editor/editorcontroller", &rv);
      if (NS_FAILED(rv)) return rv;
      nsCOMPtr<nsIEditorController> editorController = do_QueryInterface(controller, &rv);
      if (NS_FAILED(rv)) return rv;
      rv = editorController->Init(nsnull);
      if (NS_FAILED(rv)) return rv;
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
  nsresult rv = nsGenericHTMLElement::GetPrimaryFrame(this, formControlFrame);
  if (NS_SUCCEEDED(rv) && formControlFrame)
  {
    nsCOMPtr<textControlPlace> textControlFrame(do_QueryInterface(formControlFrame));
    if (textControlFrame)
      textControlFrame->GetTextLength(aTextLength);
      
    return NS_OK;
  }
  return rv;
}

NS_IMETHODIMP
nsHTMLInputElement::SetSelectionRange(PRInt32 aSelectionStart, PRInt32 aSelectionEnd)
{
  nsIFormControlFrame* formControlFrame = nsnull;
  nsresult rv = nsGenericHTMLElement::GetPrimaryFrame(this, formControlFrame);
  if (NS_SUCCEEDED(rv) && formControlFrame)
  {
    nsCOMPtr<textControlPlace> textControlFrame(do_QueryInterface(formControlFrame));
    if (textControlFrame)
      textControlFrame->SetSelectionRange(aSelectionStart, aSelectionEnd);
      
    return NS_OK;
  }
  return rv;
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
  nsresult rv = nsGenericHTMLElement::GetPrimaryFrame(this, formControlFrame);
  if (NS_SUCCEEDED(rv) && formControlFrame)
  {
    nsCOMPtr<textControlPlace> textControlFrame(do_QueryInterface(formControlFrame));
    if (textControlFrame)
      textControlFrame->SetSelectionStart(aSelectionStart);
      
    return NS_OK;
  }
  return rv;
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
  nsresult rv = nsGenericHTMLElement::GetPrimaryFrame(this, formControlFrame);
  if (NS_SUCCEEDED(rv) && formControlFrame)
  {
    nsCOMPtr<textControlPlace> textControlFrame(do_QueryInterface(formControlFrame));
    if (textControlFrame)
      textControlFrame->SetSelectionEnd(aSelectionEnd);
      
    return NS_OK;
  }
  return rv;
}

nsresult
nsHTMLInputElement::GetSelectionRange(PRInt32* aSelectionStart, PRInt32* aSelectionEnd)
{
  nsIFormControlFrame* formControlFrame = nsnull;
  nsresult rv = nsGenericHTMLElement::GetPrimaryFrame(this, formControlFrame);
  if (NS_SUCCEEDED(rv) && formControlFrame)
  {
    nsCOMPtr<textControlPlace> textControlFrame(do_QueryInterface(formControlFrame));
    if (textControlFrame)
      textControlFrame->GetSelectionRange(aSelectionStart, aSelectionEnd);
      
    return NS_OK;
  }
  return rv;
}

