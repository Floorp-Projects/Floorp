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
#include "nsIDOMHTMLTextAreaElement.h"
#include "nsIDOMNSHTMLTextAreaElement.h"
#include "nsITextControlElement.h"
#include "nsIControllers.h"
#include "nsIEditorController.h"
#include "nsContentCID.h"
#include "nsCOMPtr.h"
#include "nsIComponentManager.h"
#include "nsIDOMHTMLFormElement.h"
#include "nsIFormControl.h"
#include "nsIForm.h"
#include "nsIDOMEventReceiver.h"
#include "nsIHTMLContent.h"
#include "nsGenericHTMLElement.h"
#include "nsHTMLAtoms.h"
#include "nsHTMLIIDs.h"
#include "nsIStyleContext.h"
#include "nsStyleConsts.h"
#include "nsIPresContext.h"
#include "nsIHTMLAttributes.h"
#include "nsIFormControlFrame.h"
#include "nsIEventStateManager.h"
#include "nsISizeOfHandler.h"
#include "nsLinebreakConverter.h"
#include "nsIDocument.h"
#include "nsIPresShell.h"
#include "nsIFrame.h"
#include "nsIFormControlFrame.h"
#include "nsIPrivateDOMEvent.h"
#include "nsGUIEvent.h"
#include "nsLinebreakConverter.h"
#include "nsIPresState.h"

static NS_DEFINE_CID(kXULControllersCID,  NS_XULCONTROLLERS_CID);


class nsHTMLTextAreaElement : public nsGenericHTMLContainerFormElement,
                              public nsIDOMHTMLTextAreaElement,
                              public nsIDOMNSHTMLTextAreaElement,
                              public nsITextControlElement
{
public:
  nsHTMLTextAreaElement();
  virtual ~nsHTMLTextAreaElement();

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  // nsIDOMNode
  NS_FORWARD_NSIDOMNODE_NO_CLONENODE(nsGenericHTMLContainerElement::)

  // nsIDOMElement
  NS_FORWARD_NSIDOMELEMENT(nsGenericHTMLContainerElement::)

  // nsIDOMHTMLElement
  NS_FORWARD_NSIDOMHTMLELEMENT(nsGenericHTMLContainerElement::)

  // nsIDOMHTMLTextAreaElement
  NS_DECL_NSIDOMHTMLTEXTAREAELEMENT

  // nsIDOMNSHTMLTextAreaElement
  NS_DECL_NSIDOMNSHTMLTEXTAREAELEMENT

  // nsIFormControl
  NS_IMETHOD GetType(PRInt32* aType);
  NS_IMETHOD Reset();
  NS_IMETHOD IsSuccessful(nsIContent* aSubmitElement, PRBool *_retval);
  NS_IMETHOD GetMaxNumValues(PRInt32 *_retval);
  NS_IMETHOD GetNamesValues(PRInt32 aMaxNumValues,
                            PRInt32& aNumValues,
                            nsString* aValues,
                            nsString* aNames);
  NS_IMETHOD SaveState(nsIPresContext* aPresContext, nsIPresState** aState);
  NS_IMETHOD RestoreState(nsIPresContext* aPresContext, nsIPresState* aState);

  // nsITextControlElement
  NS_IMETHOD GetValueInternal(nsAWritableString& str);
  NS_IMETHOD SetValueInternal(nsAReadableString& str);
  NS_IMETHOD SetValueChanged(PRBool aValueChanged);

  // nsIContent
  NS_IMETHOD StringToAttribute(nsIAtom* aAttribute,
                               const nsAReadableString& aValue,
                               nsHTMLValue& aResult);
  NS_IMETHOD GetAttributeMappingFunction(nsMapRuleToAttributesFunc& aMapRuleFunc) const;
  NS_IMETHOD GetMappedAttributeImpact(const nsIAtom* aAttribute, PRInt32 aModType,
                                      PRInt32& aHint) const;
  NS_IMETHOD HandleDOMEvent(nsIPresContext* aPresContext, nsEvent* aEvent,
                            nsIDOMEvent** aDOMEvent, PRUint32 aFlags,
                            nsEventStatus* aEventStatus);
  NS_IMETHOD SetFocus(nsIPresContext* aPresContext);
  NS_IMETHOD RemoveFocus(nsIPresContext* aPresContext);
#ifdef DEBUG
  NS_IMETHOD SizeOf(nsISizeOfHandler* aSizer, PRUint32* aResult) const;
#endif

protected:
  nsCOMPtr<nsIControllers> mControllers;

  NS_IMETHOD SelectAll(nsIPresContext* aPresContext);
};

nsresult
NS_NewHTMLTextAreaElement(nsIHTMLContent** aInstancePtrResult,
                          nsINodeInfo *aNodeInfo)
{
  NS_ENSURE_ARG_POINTER(aInstancePtrResult);

  nsHTMLTextAreaElement* it = new nsHTMLTextAreaElement();

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


nsHTMLTextAreaElement::nsHTMLTextAreaElement()
{
}

nsHTMLTextAreaElement::~nsHTMLTextAreaElement()
{
  // Null out form's pointer to us - no ref counting here!
  SetForm(nsnull);
}


NS_IMPL_ADDREF_INHERITED(nsHTMLTextAreaElement, nsGenericElement) 
NS_IMPL_RELEASE_INHERITED(nsHTMLTextAreaElement, nsGenericElement) 


// QueryInterface implementation for nsHTMLTextAreaElement
NS_HTML_CONTENT_INTERFACE_MAP_BEGIN(nsHTMLTextAreaElement,
                                    nsGenericHTMLContainerFormElement)
  NS_INTERFACE_MAP_ENTRY(nsIDOMHTMLTextAreaElement)
  NS_INTERFACE_MAP_ENTRY(nsIDOMNSHTMLTextAreaElement)
  NS_INTERFACE_MAP_ENTRY(nsITextControlElement)
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO(HTMLTextAreaElement)
NS_HTML_CONTENT_INTERFACE_MAP_END


// nsIDOMHTMLTextAreaElement

nsresult
nsHTMLTextAreaElement::CloneNode(PRBool aDeep, nsIDOMNode** aReturn)
{
  NS_ENSURE_ARG_POINTER(aReturn);
  *aReturn = nsnull;

  nsHTMLTextAreaElement* it = new nsHTMLTextAreaElement();

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

NS_IMETHODIMP
nsHTMLTextAreaElement::GetForm(nsIDOMHTMLFormElement** aForm)
{
  return nsGenericHTMLContainerFormElement::GetForm(aForm);
}


// nsIContent

NS_IMETHODIMP
nsHTMLTextAreaElement::Blur()
{
  return SetElementFocus(PR_FALSE);
}

NS_IMETHODIMP
nsHTMLTextAreaElement::Focus() 
{
  return SetElementFocus(PR_TRUE);
}

NS_IMETHODIMP
nsHTMLTextAreaElement::SetFocus(nsIPresContext* aPresContext)
{
  NS_ENSURE_ARG_POINTER(aPresContext);
  // first see if we are disabled or not. If disabled then do nothing.
  nsAutoString disabled;

  if (NS_CONTENT_ATTR_HAS_VALUE ==
      nsGenericHTMLContainerFormElement::GetAttr(kNameSpaceID_HTML,
                                                 nsHTMLAtoms::disabled,
                                                 disabled)) {
    return NS_OK;
  }

  nsCOMPtr<nsIEventStateManager> esm;
  if (NS_OK == aPresContext->GetEventStateManager(getter_AddRefs(esm))) {
    esm->SetContentState(this, NS_EVENT_STATE_FOCUS);
  }

  nsIFormControlFrame* formControlFrame = nsnull;
  GetPrimaryFrame(this, formControlFrame, PR_TRUE, PR_FALSE);

  if (formControlFrame) {
    formControlFrame->SetFocus(PR_TRUE, PR_TRUE);
    formControlFrame->ScrollIntoView(aPresContext);
    // Could call SelectAll(aPresContext) here to automatically
    // select text when we receive focus.
  }

  return NS_OK;
}

NS_IMETHODIMP
nsHTMLTextAreaElement::RemoveFocus(nsIPresContext* aPresContext)
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
nsHTMLTextAreaElement::Select()
{
  nsresult rv = NS_OK;

  // first see if we are disabled or not. If disabled then do nothing.
  nsAutoString disabled;
  if (NS_CONTENT_ATTR_HAS_VALUE ==
      nsGenericHTMLContainerFormElement::GetAttr(kNameSpaceID_HTML,
                                                 nsHTMLAtoms::disabled,
                                                 disabled)) {
    return rv;
  }

  // XXX Bug?  We have to give the input focus before contents can be
  // selected

  // Just like SetFocus() but without the ScrollIntoView()!
  nsCOMPtr<nsIPresContext> presContext;
  GetPresContext(this, getter_AddRefs(presContext)); 

  nsEventStatus status = nsEventStatus_eIgnore;
  nsGUIEvent event;
  event.eventStructType = NS_GUI_EVENT;
  event.message = NS_FORM_SELECTED;
  event.flags = NS_EVENT_FLAG_NONE;
  event.widget = nsnull;
  rv = HandleDOMEvent(presContext, &event, nsnull, NS_EVENT_FLAG_INIT, &status);

  // If the DOM event was not canceled (e.g. by a JS event handler
  // returning false)
  if (status == nsEventStatus_eIgnore) {
    nsCOMPtr<nsIEventStateManager> esm;

    presContext->GetEventStateManager(getter_AddRefs(esm));

    if (esm) {
      esm->SetContentState(this, NS_EVENT_STATE_FOCUS);
    }

    nsIFormControlFrame* formControlFrame = nsnull;
    GetPrimaryFrame(this, formControlFrame, PR_TRUE, PR_FALSE);

    if (formControlFrame) {
      formControlFrame->SetFocus(PR_TRUE, PR_TRUE);

      // Now Select all the text!
      SelectAll(presContext);
    }
  }

  return rv;
}

NS_IMETHODIMP
nsHTMLTextAreaElement::SelectAll(nsIPresContext* aPresContext)
{
  nsIFormControlFrame* formControlFrame = nsnull;
  GetPrimaryFrame(this, formControlFrame, PR_TRUE, PR_FALSE);

  if (formControlFrame) {
    formControlFrame->SetProperty(aPresContext, nsHTMLAtoms::select,
                                  nsString());
  }

  return NS_OK;
}

NS_IMPL_STRING_ATTR(nsHTMLTextAreaElement, AccessKey, accesskey)
NS_IMPL_INT_ATTR(nsHTMLTextAreaElement, Cols, cols)
NS_IMPL_BOOL_ATTR(nsHTMLTextAreaElement, Disabled, disabled)
NS_IMPL_STRING_ATTR(nsHTMLTextAreaElement, Name, name)
NS_IMPL_BOOL_ATTR(nsHTMLTextAreaElement, ReadOnly, readonly)
NS_IMPL_INT_ATTR(nsHTMLTextAreaElement, Rows, rows)
NS_IMPL_INT_ATTR(nsHTMLTextAreaElement, TabIndex, tabindex)
  

NS_IMETHODIMP 
nsHTMLTextAreaElement::GetType(nsAWritableString& aType)
{
  aType.Assign(NS_LITERAL_STRING("textarea"));

  return NS_OK;
}

NS_IMETHODIMP
nsHTMLTextAreaElement::GetValueInternal(nsAWritableString& aValue)
{
  return nsGenericHTMLContainerFormElement::GetAttr(kNameSpaceID_HTML,
                                                    nsHTMLAtoms::value,
                                                    aValue);
}

NS_IMETHODIMP 
nsHTMLTextAreaElement::GetValue(nsAWritableString& aValue)
{
  nsIFormControlFrame* formControlFrame = nsnull;

  // No need to flush here, if there is no frame yet for this textarea
  // there won't be a value in it we don't already have even if we
  // force the frame to be created.
  GetPrimaryFrame(this, formControlFrame, PR_FALSE, PR_FALSE);

  if (formControlFrame) {
    formControlFrame->GetProperty(nsHTMLAtoms::value, aValue);
    return NS_OK;
  } else {
    return GetValueInternal(aValue);
  }
}


NS_IMETHODIMP
nsHTMLTextAreaElement::SetValueInternal(nsAReadableString& aValue)
{
  // Set the attribute in the DOM too, we call SetAttribute with aNotify
  // false so that we don't generate unnecessary reflows.
  nsGenericHTMLContainerFormElement::SetAttr(kNameSpaceID_HTML,
                                             nsHTMLAtoms::value, aValue,
                                             PR_FALSE);

  return NS_OK;
}

NS_IMETHODIMP 
nsHTMLTextAreaElement::SetValue(const nsAReadableString& aValue)
{
  nsIFormControlFrame* formControlFrame = nsnull;

  // No need to flush here, if there is no frame for this yet forcing
  // creation of one will not do us any good
  GetPrimaryFrame(this, formControlFrame, PR_FALSE, PR_FALSE);

  if (formControlFrame) {
    nsCOMPtr<nsIPresContext> presContext;
    GetPresContext(this, getter_AddRefs(presContext));

    formControlFrame->SetProperty(presContext, nsHTMLAtoms::value, aValue);
  }

  // Always set the value internally, since it affects layout
  SetValueInternal(aValue);

  return NS_OK;
}


NS_IMETHODIMP
nsHTMLTextAreaElement::SetValueChanged(PRBool aValueChanged)
{
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLTextAreaElement::GetDefaultValue(nsAWritableString& aDefaultValue)
{
  nsGenericHTMLContainerFormElement::GetAttr(kNameSpaceID_HTML,
                                             nsHTMLAtoms::defaultvalue,
                                             aDefaultValue);

  return NS_OK;                                                    
}  

NS_IMETHODIMP
nsHTMLTextAreaElement::SetDefaultValue(const nsAReadableString& aDefaultValue)
{
  nsAutoString defaultValue(aDefaultValue);

  // normalize line breaks. Need this e.g. when the value is
  // coming from a URL, which used platform line breaks.
  nsLinebreakConverter::ConvertStringLineBreaks(defaultValue,
       nsLinebreakConverter::eLinebreakAny, nsLinebreakConverter::eLinebreakContent);

  // Strip only one leading LF if there is one (bug 40394)
  if (0 == defaultValue.Find("\n", PR_FALSE, 0, 1)) {
    defaultValue.Cut(0,1);
  }

  nsGenericHTMLContainerFormElement::SetAttr(kNameSpaceID_HTML,
                                             nsHTMLAtoms::defaultvalue,
                                             defaultValue, PR_TRUE);
  SetValue(defaultValue);
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLTextAreaElement::StringToAttribute(nsIAtom* aAttribute,
                                         const nsAReadableString& aValue,
                                         nsHTMLValue& aResult)
{
  if (aAttribute == nsHTMLAtoms::disabled) {
    aResult.SetEmptyValue();
    return NS_CONTENT_ATTR_HAS_VALUE;
  }
  else if (aAttribute == nsHTMLAtoms::cols) {
    if (ParseValue(aValue, 0, aResult, eHTMLUnit_Integer)) {
      return NS_CONTENT_ATTR_HAS_VALUE;
    }
  }
  else if (aAttribute == nsHTMLAtoms::readonly) {
    aResult.SetEmptyValue();
    return NS_CONTENT_ATTR_HAS_VALUE;
  }
  else if (aAttribute == nsHTMLAtoms::rows) {
    if (ParseValue(aValue, 0, aResult, eHTMLUnit_Integer)) {
      return NS_CONTENT_ATTR_HAS_VALUE;
    }
  }
  else if (aAttribute == nsHTMLAtoms::tabindex) {
    if (ParseValue(aValue, 0, aResult, eHTMLUnit_Integer)) {
      return NS_CONTENT_ATTR_HAS_VALUE;
    }
  }

  return NS_CONTENT_ATTR_NOT_THERE;
}

static void
MapAttributesIntoRule(const nsIHTMLMappedAttributes* aAttributes,
                      nsRuleData* aData)
{
  if (!aAttributes || !aData)
    return;

  nsGenericHTMLElement::MapAlignAttributeInto(aAttributes, aData);
  nsGenericHTMLElement::MapCommonAttributesInto(aAttributes, aData);
}

NS_IMETHODIMP
nsHTMLTextAreaElement::GetMappedAttributeImpact(const nsIAtom* aAttribute, PRInt32 aModType,
                                                PRInt32& aHint) const
{
  // XXX Bug 50280 - It is unclear why we need to do this here for 
  // rows and cols and why the AttributeChanged method in
  // nsGfxTextControlFrame2 does take care of the entire problem, but
  // it doesn't and this makes things better
  if (aAttribute == nsHTMLAtoms::align ||
      aAttribute == nsHTMLAtoms::rows ||
      aAttribute == nsHTMLAtoms::cols) {
    aHint = NS_STYLE_HINT_REFLOW;
  }
  else if (!GetCommonMappedAttributesImpact(aAttribute, aHint)) {
    aHint = NS_STYLE_HINT_CONTENT;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsHTMLTextAreaElement::GetAttributeMappingFunction(nsMapRuleToAttributesFunc& aMapRuleFunc) const
{
  aMapRuleFunc = &MapAttributesIntoRule;
  return NS_OK;
}


NS_IMETHODIMP
nsHTMLTextAreaElement::HandleDOMEvent(nsIPresContext* aPresContext,
                                      nsEvent* aEvent,
                                      nsIDOMEvent** aDOMEvent,
                                      PRUint32 aFlags,
                                      nsEventStatus* aEventStatus)
{
  // Do not process any DOM events if the element is disabled
  PRBool disabled;
  nsresult rv = GetDisabled(&disabled);
  if (NS_FAILED(rv) || disabled) {
    return rv;
  }

  nsIFormControlFrame* formControlFrame = nsnull;
  GetPrimaryFrame(this, formControlFrame, PR_FALSE, PR_FALSE);
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

  // We have anonymous content underneath
  // that we need to hide.  We need to set the event target now
  // to ourselves

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

      if (!*aDOMEvent) {
        return NS_ERROR_FAILURE;
      }

      nsCOMPtr<nsIPrivateDOMEvent> privateEvent(do_QueryInterface(*aDOMEvent));

      if (!privateEvent) {
        return NS_ERROR_FAILURE;
      }

      nsCOMPtr<nsIDOMEventTarget> target(do_QueryInterface(NS_STATIC_CAST(nsIDOMHTMLTextAreaElement *, this)));

      privateEvent->SetTarget(target);
    }
  }

  rv = nsGenericHTMLContainerFormElement::HandleDOMEvent(aPresContext, aEvent,
                                                         aDOMEvent,
                                                         aFlags, aEventStatus);

  // Finish the special anonymous content processing...
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
    privateEvent->SetTarget(nsnull);
  }

  return rv;
}

// nsIFormControl

NS_IMETHODIMP
nsHTMLTextAreaElement::GetType(PRInt32* aType)
{
  if (aType) {
    *aType = NS_FORM_TEXTAREA;
    return NS_OK;
  } else {
    return NS_FORM_NOTOK;
  }
}

#ifdef DEBUG
NS_IMETHODIMP
nsHTMLTextAreaElement::SizeOf(nsISizeOfHandler* aSizer,
                              PRUint32* aResult) const
{
  *aResult = sizeof(*this) + BaseSizeOf(aSizer);

  return NS_OK;
}
#endif


// Controllers Methods

NS_IMETHODIMP
nsHTMLTextAreaElement::GetControllers(nsIControllers** aResult)
{
  NS_ENSURE_ARG_POINTER(aResult);

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
    nsCOMPtr<nsIController> controller = do_CreateInstance("@mozilla.org/editor/editorcontroller;1", &rv);
    if (NS_FAILED(rv))
      return rv;

    nsCOMPtr<nsIEditorController> editorController = do_QueryInterface(controller, &rv);
    if (NS_FAILED(rv))
      return rv;

    rv = editorController->Init(nsnull);
    if (NS_FAILED(rv))
      return rv;

    mControllers->AppendController(controller);
  }

  *aResult = mControllers;
  NS_IF_ADDREF(*aResult);

  return NS_OK;
}


nsresult
nsHTMLTextAreaElement::Reset()
{
  nsAutoString resetVal;
  GetDefaultValue(resetVal);
  nsresult rv = SetValue(resetVal);
  NS_ENSURE_SUCCESS(rv, rv);
  return NS_OK;
}

nsresult
nsHTMLTextAreaElement::IsSuccessful(nsIContent* aSubmitElement,
                                    PRBool *_retval)
{
  // if it's disabled, it won't submit
  PRBool disabled;
  nsresult rv = GetDisabled(&disabled);
  if (disabled) {
    *_retval = PR_FALSE;
    return NS_OK;
  }

  // if it dosn't have a name it we don't submit
  nsAutoString val;
  rv = GetAttr(kNameSpaceID_None, nsHTMLAtoms::name, val);
  *_retval = rv != NS_CONTENT_ATTR_NOT_THERE;
  return NS_OK;
}

nsresult
nsHTMLTextAreaElement::GetMaxNumValues(PRInt32 *_retval)
{
  *_retval = 1;
  return NS_OK;
}

nsresult
nsHTMLTextAreaElement::GetNamesValues(PRInt32 aMaxNumValues,
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

NS_IMETHODIMP
nsHTMLTextAreaElement::SaveState(nsIPresContext* aPresContext,
                                 nsIPresState** aState)
{
  nsresult rv = GetPrimaryPresState(this, aState);
  if (*aState) {
    nsString value;
    GetValue(value);
    // XXX Should use nsAutoString above but ConvertStringLineBreaks requires
    // mOwnsBuffer!
    rv = nsLinebreakConverter::ConvertStringLineBreaks(
             value,
             nsLinebreakConverter::eLinebreakPlatform,
             nsLinebreakConverter::eLinebreakContent);
    NS_ASSERTION(NS_SUCCEEDED(rv), "Converting linebreaks failed!");
    rv = (*aState)->SetStateProperty(NS_LITERAL_STRING("value"), value);
    NS_ASSERTION(NS_SUCCEEDED(rv), "value save failed!");
  }

  return rv;
}

NS_IMETHODIMP
nsHTMLTextAreaElement::RestoreState(nsIPresContext* aPresContext,
                                    nsIPresState* aState)
{
  nsresult rv = NS_OK;

  nsAutoString value;
  rv = aState->GetStateProperty(NS_LITERAL_STRING("value"), value);
  NS_ASSERTION(NS_SUCCEEDED(rv), "value restore failed!");
  SetValue(value);

  return rv;
}
