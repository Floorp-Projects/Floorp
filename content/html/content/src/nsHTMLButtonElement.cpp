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
#include "nsIDOMHTMLButtonElement.h"
#include "nsIDOMNSHTMLButtonElement.h"
#include "nsIDOMHTMLFormElement.h"
#include "nsIDOMEventReceiver.h"
#include "nsIHTMLContent.h"
#include "nsGenericHTMLElement.h"
#include "nsHTMLAtoms.h"
#include "nsHTMLIIDs.h"
#include "nsIStyleContext.h"
#include "nsStyleConsts.h"
#include "nsIPresContext.h"
#include "nsIFormControl.h"
#include "nsIForm.h"
#include "nsIURL.h"

#include "nsIFrame.h"
#include "nsIFormControlFrame.h"
#include "nsIEventStateManager.h"
#include "nsIDOMEvent.h"
#include "nsISizeOfHandler.h"
#include "nsIDocument.h"
#include "nsGUIEvent.h"


class nsHTMLButtonElement : public nsGenericHTMLContainerFormElement,
                            public nsIDOMHTMLButtonElement,
                            public nsIDOMNSHTMLButtonElement
{
public:
  nsHTMLButtonElement();
  virtual ~nsHTMLButtonElement();

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  // nsIDOMNode
  NS_FORWARD_NSIDOMNODE_NO_CLONENODE(nsGenericHTMLContainerFormElement::)

  // nsIDOMElement
  NS_FORWARD_NSIDOMELEMENT(nsGenericHTMLContainerFormElement::)

  // nsIDOMHTMLElement
  NS_FORWARD_NSIDOMHTMLELEMENT(nsGenericHTMLContainerElement::)

  // nsIDOMHTMLButtonElement
  NS_DECL_NSIDOMHTMLBUTTONELEMENT

  // nsIDOMNSHTMLButtonElement
  NS_DECL_NSIDOMNSHTMLBUTTONELEMENT

  // overrided nsIFormControl method
  NS_IMETHOD GetType(PRInt32* aType);
  NS_IMETHOD Reset();
  NS_IMETHOD IsSuccessful(nsIContent* aSubmitElement, PRBool *_retval);
  NS_IMETHOD GetMaxNumValues(PRInt32 *_retval);
  NS_IMETHOD GetNamesValues(PRInt32 aMaxNumValues,
                            PRInt32& aNumValues,
                            nsString* aValues,
                            nsString* aNames);

  // nsIContent overrides...
  NS_IMETHOD GetAttribute(PRInt32 aNameSpaceID, nsIAtom* aName,
                          nsAWritableString& aResult) const;
  NS_IMETHOD SetAttribute(PRInt32 aNameSpaceID, nsIAtom* aName,
                          const nsAReadableString& aValue, PRBool aNotify);
  NS_IMETHOD SetFocus(nsIPresContext* aPresContext);
  NS_IMETHOD RemoveFocus(nsIPresContext* aPresContext);
  NS_IMETHOD StringToAttribute(nsIAtom* aAttribute,
                               const nsAReadableString& aValue,
                               nsHTMLValue& aResult);
  NS_IMETHOD AttributeToString(nsIAtom* aAttribute,
                               const nsHTMLValue& aValue,
                               nsAWritableString& aResult) const;
  NS_IMETHOD HandleDOMEvent(nsIPresContext* aPresContext,
                            nsEvent* aEvent,
                            nsIDOMEvent** aDOMEvent,
                            PRUint32 aFlags,
                            nsEventStatus* aEventStatus);
#ifdef DEBUG
  NS_IMETHOD SizeOf(nsISizeOfHandler* aSizer, PRUint32* aResult) const;
#endif

protected:
  PRInt8 mType;

private:
  // The analogue of defaultValue in the DOM for input and textarea
  nsresult SetDefaultValue(const nsAReadableString& aDefaultValue);
  nsresult GetDefaultValue(nsAWritableString& aDefaultValue);

};


// Construction, destruction

nsresult
NS_NewHTMLButtonElement(nsIHTMLContent** aInstancePtrResult,
                        nsINodeInfo *aNodeInfo)
{
  NS_ENSURE_ARG_POINTER(aInstancePtrResult);

  nsHTMLButtonElement* it = new nsHTMLButtonElement();

  if (!it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  nsresult rv = NS_STATIC_CAST(nsGenericElement *, it)->Init(aNodeInfo);

  if (NS_FAILED(rv)) {
    delete it;

    return rv;
  }

  *aInstancePtrResult = NS_STATIC_CAST(nsIHTMLContent *, it);
  NS_ADDREF(*aInstancePtrResult);

  return NS_OK;
}


nsHTMLButtonElement::nsHTMLButtonElement()
{
  mType = NS_FORM_BUTTON_SUBMIT; // default
}

nsHTMLButtonElement::~nsHTMLButtonElement()
{
  // Null out form's pointer to us - no ref counting here!
  SetForm(nsnull);
}

// nsISupports

NS_IMPL_ADDREF_INHERITED(nsHTMLButtonElement, nsGenericElement);
NS_IMPL_RELEASE_INHERITED(nsHTMLButtonElement, nsGenericElement);


// QueryInterface implementation for nsHTMLButtonElement
NS_HTML_CONTENT_INTERFACE_MAP_BEGIN(nsHTMLButtonElement,
                                    nsGenericHTMLContainerFormElement)
  NS_INTERFACE_MAP_ENTRY(nsIDOMHTMLButtonElement)
  NS_INTERFACE_MAP_ENTRY(nsIDOMNSHTMLButtonElement)
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO(HTMLButtonElement)
NS_HTML_CONTENT_INTERFACE_MAP_END


NS_IMETHODIMP
nsHTMLButtonElement::GetAttribute(PRInt32 aNameSpaceID, nsIAtom* aName,
                                  nsAWritableString& aResult) const
{
  if (aName == nsHTMLAtoms::disabled) {
    nsresult rv = nsGenericHTMLContainerFormElement::GetAttr(kNameSpaceID_None, nsHTMLAtoms::disabled, aResult);
    if (rv == NS_CONTENT_ATTR_NOT_THERE) {
      aResult.Assign(NS_LITERAL_STRING("false"));
    } else {
      aResult.Assign(NS_LITERAL_STRING("true"));
    }

    return rv;
  }

  return nsGenericHTMLContainerFormElement::GetAttr(aNameSpaceID, aName,
                                                    aResult);
}

NS_IMETHODIMP
nsHTMLButtonElement::SetAttribute(PRInt32 aNameSpaceID, nsIAtom* aName,
                                  const nsAReadableString& aValue,
                                  PRBool aNotify)
{
  nsAutoString value(aValue);

  if (aName == nsHTMLAtoms::disabled &&
      value.EqualsWithConversion("false", PR_TRUE)) {
    return UnsetAttr(aNameSpaceID, aName, aNotify);
  }

  return nsGenericHTMLContainerFormElement::SetAttr(aNameSpaceID, aName,
                                                    aValue, aNotify);
}


// nsIDOMHTMLButtonElement

nsresult
nsHTMLButtonElement::CloneNode(PRBool aDeep, nsIDOMNode** aReturn)
{
  NS_ENSURE_ARG_POINTER(aReturn);
  *aReturn = nsnull;

  nsHTMLButtonElement* it = new nsHTMLButtonElement();

  if (!it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  nsCOMPtr<nsIDOMNode> kungFuDeathGrip(it);

  nsresult rv = NS_STATIC_CAST(nsGenericElement *, it)->Init(mNodeInfo);

  if (NS_FAILED(rv))
    return rv;

  CopyInnerTo(this, it, aDeep);

  *aReturn = NS_STATIC_CAST(nsIDOMNode *, it);

  NS_ADDREF(*aReturn);

  return NS_OK;
}


// nsIContent

NS_IMETHODIMP
nsHTMLButtonElement::GetForm(nsIDOMHTMLFormElement** aForm)
{
  return nsGenericHTMLContainerFormElement::GetForm(aForm);
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
  return SetElementFocus(PR_FALSE);
}

NS_IMETHODIMP
nsHTMLButtonElement::Focus()
{
  return SetElementFocus(PR_TRUE);
}

NS_IMETHODIMP
nsHTMLButtonElement::SetFocus(nsIPresContext* aPresContext)
{
  NS_ENSURE_ARG_POINTER(aPresContext);
  // first see if we are disabled or not. If disabled then do nothing.
  nsAutoString disabled;
  if (NS_CONTENT_ATTR_HAS_VALUE == GetAttribute(kNameSpaceID_HTML,
                                                nsHTMLAtoms::disabled,
                                                disabled)) {
    return NS_OK;
  }

  nsCOMPtr<nsIEventStateManager> esm;
  if (NS_OK == aPresContext->GetEventStateManager(getter_AddRefs(esm))) {
    esm->SetContentState(this, NS_EVENT_STATE_FOCUS);
  }

  nsIFormControlFrame* formControlFrame = nsnull;
  GetPrimaryFrame(this, formControlFrame, PR_FALSE, PR_FALSE);

  if (formControlFrame) {
    formControlFrame->SetFocus(PR_TRUE, PR_TRUE);
    formControlFrame->ScrollIntoView(aPresContext);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsHTMLButtonElement::RemoveFocus(nsIPresContext* aPresContext)
{
  NS_ENSURE_ARG_POINTER(aPresContext);
  // If we are disabled, we probably shouldn't have focus in the
  // first place, so allow it to be removed.
  nsresult rv = NS_OK;

  nsIFormControlFrame* formControlFrame = nsnull;
  GetPrimaryFrame(this, formControlFrame, PR_FALSE, PR_FALSE);

  if (formControlFrame) {
    formControlFrame->SetFocus(PR_FALSE, PR_FALSE);
  }

  nsCOMPtr<nsIEventStateManager> esm;
  if (NS_OK == aPresContext->GetEventStateManager(getter_AddRefs(esm))) {

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
    if (ParseValue(aValue, 0, 32767, aResult, eHTMLUnit_Integer)) {
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
      EnumValueToString(aValue, kButtonTypeTable, aResult, PR_FALSE);
      return NS_CONTENT_ATTR_HAS_VALUE;
    }
  }

  return nsGenericHTMLContainerFormElement::AttributeToString(aAttribute,
                                                              aValue, aResult);
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

  nsIFormControlFrame* formControlFrame = nsnull;
  GetPrimaryFrame(this, formControlFrame, PR_FALSE, PR_FALSE);

  if (formControlFrame) {
    nsIFrame* formFrame = nsnull;
    CallQueryInterface(formControlFrame, &formFrame);

    if (formFrame) {
      const nsStyleUserInterface* uiStyle;
      formFrame->GetStyleData(eStyleStruct_UserInterface,
                              (const nsStyleStruct *&)uiStyle);

      if (uiStyle->mUserInput == NS_STYLE_USER_INPUT_NONE ||
          uiStyle->mUserInput == NS_STYLE_USER_INPUT_DISABLED)
        return NS_OK;
    }
  }
  
  // Try script event handlers first
  nsresult ret;
  ret = nsGenericHTMLContainerFormElement::HandleDOMEvent(aPresContext,
                                                          aEvent, aDOMEvent,
                                                          aFlags,
                                                          aEventStatus);

  if ((NS_OK == ret) && (nsEventStatus_eIgnore == *aEventStatus) &&
      !(aFlags & NS_EVENT_FLAG_CAPTURE)) {
    switch (aEvent->message) {

    case NS_KEY_PRESS:
    case NS_KEY_UP:
      {
        // For backwards compat, trigger buttons with space or enter
        // (bug 25300)
        nsKeyEvent * keyEvent = (nsKeyEvent *)aEvent;
        if ((keyEvent->keyCode == NS_VK_RETURN && NS_KEY_PRESS == aEvent->message) ||
            keyEvent->keyCode == NS_VK_SPACE  && NS_KEY_UP == aEvent->message) {
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
        }
      }
      break;// NS_KEY_PRESS

    case NS_MOUSE_LEFT_CLICK:
      {
        // Tell the frame about the click
        nsIFormControlFrame* formControlFrame = nsnull;
        GetPrimaryFrame(this, formControlFrame, PR_FALSE, PR_FALSE);
        if (formControlFrame) {
          formControlFrame->MouseClicked(aPresContext);
        }
      }
      break;// NS_MOUSE_LEFT_CLICK

    case NS_MOUSE_LEFT_BUTTON_DOWN:
      {
        nsIEventStateManager *stateManager;
        if (NS_OK == aPresContext->GetEventStateManager(&stateManager)) {
          stateManager->SetContentState(this, NS_EVENT_STATE_ACTIVE |
                                        NS_EVENT_STATE_FOCUS);
          NS_RELEASE(stateManager);
        }
        *aEventStatus = nsEventStatus_eConsumeNoDefault; 
      }
      break;

    // cancel all of these events for buttons
    case NS_MOUSE_MIDDLE_BUTTON_DOWN:
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

#ifdef DEBUG
NS_IMETHODIMP
nsHTMLButtonElement::SizeOf(nsISizeOfHandler* aSizer, PRUint32* aResult) const
{
  *aResult = sizeof(*this) + BaseSizeOf(aSizer);

  return NS_OK;
}
#endif

nsresult
nsHTMLButtonElement::GetDefaultValue(nsAWritableString& aDefaultValue)
{
  return GetAttr(kNameSpaceID_HTML, nsHTMLAtoms::value, aDefaultValue);
}

nsresult
nsHTMLButtonElement::SetDefaultValue(const nsAReadableString& aDefaultValue)
{
  return SetAttr(kNameSpaceID_HTML, nsHTMLAtoms::value, aDefaultValue, PR_TRUE);
}

nsresult
nsHTMLButtonElement::Reset()
{
  return NS_OK;
}

nsresult
nsHTMLButtonElement::IsSuccessful(nsIContent* aSubmitElement,
                                  PRBool *_retval)
{
  *_retval = PR_FALSE;
  if (aSubmitElement != this) {
    return NS_OK;
  }

  // if it's disabled, it won't submit
  PRBool disabled;
  nsresult rv = GetDisabled(&disabled);
  NS_ENSURE_SUCCESS(rv, rv);

  if (disabled) {
    return NS_OK;
  }

  // If there is no name, it won't submit
  nsAutoString val;
  rv = GetAttr(kNameSpaceID_None, nsHTMLAtoms::name, val);
  *_retval = rv != NS_CONTENT_ATTR_NOT_THERE;
  return NS_OK;
}

nsresult
nsHTMLButtonElement::GetMaxNumValues(PRInt32 *_retval)
{
  *_retval = 1;
  return NS_OK;
}

nsresult
nsHTMLButtonElement::GetNamesValues(PRInt32 aMaxNumValues,
                                    PRInt32& aNumValues,
                                    nsString* aValues,
                                    nsString* aNames)
{
  NS_ENSURE_TRUE(aMaxNumValues >= 1, NS_ERROR_UNEXPECTED);

  // We'll of course use the name of the control for the submit
  nsAutoString name;
  nsresult rv = GetName(name);
  NS_ENSURE_SUCCESS(rv, rv);

  nsAutoString value;
  rv = GetValue(value);
  NS_ENSURE_SUCCESS(rv, rv);

  aNames[0] = name;
  aValues[0] = value;
  aNumValues = 1;
  return NS_OK;
}
