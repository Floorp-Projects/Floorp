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
#include "nsIDOMHTMLTextAreaElement.h"
#include "nsIDOMNSHTMLTextAreaElement.h"
#include "nsIControllers.h"
#include "nsIEditorController.h"
#include "nsRDFCID.h"
#include "nsCOMPtr.h"
#include "nsIComponentManager.h"
#include "nsIDOMHTMLFormElement.h"
#include "nsIFormControl.h"
#include "nsIForm.h"
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
#include "nsIFormControlFrame.h"
#include "nsIEventStateManager.h"
#include "nsISizeOfHandler.h"
#include "nsLinebreakConverter.h"
#include "nsIDocument.h"
#include "nsIPresShell.h"
#include "nsIFrame.h"
#include "nsIFormControlFrame.h"
#include "nsIPrivateDOMEvent.h"

static NS_DEFINE_CID(kXULControllersCID,  NS_XULCONTROLLERS_CID);
static NS_DEFINE_IID(kIFrameIID, NS_IFRAME_IID);


class nsHTMLTextAreaElement : public nsGenericHTMLContainerFormElement,
                              public nsIDOMHTMLTextAreaElement,
                              public nsIDOMNSHTMLTextAreaElement
{
public:
  nsHTMLTextAreaElement();
  virtual ~nsHTMLTextAreaElement();

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  // nsIDOMNode
  NS_FORWARD_IDOMNODE_NO_CLONENODE(nsGenericHTMLContainerElement::)

  // nsIDOMElement
  NS_FORWARD_IDOMELEMENT(nsGenericHTMLContainerElement::)

  // nsIDOMHTMLElement
  NS_FORWARD_IDOMHTMLELEMENT(nsGenericHTMLContainerElement::)

  // nsIDOMHTMLTextAreaElement
  NS_DECL_IDOMHTMLTEXTAREAELEMENT

  // nsIDOMNSHTMLTextAreaElement
  NS_DECL_IDOMNSHTMLTEXTAREAELEMENT

  // nsIFormControl
  NS_IMETHOD GetType(PRInt32* aType);

  NS_IMETHOD StringToAttribute(nsIAtom* aAttribute,
                               const nsAReadableString& aValue,
                               nsHTMLValue& aResult);
  NS_IMETHOD GetAttributeMappingFunctions(nsMapAttributesFunc& aFontMapFunc, 
                                          nsMapAttributesFunc& aMapFunc) const;
  NS_IMETHOD GetMappedAttributeImpact(const nsIAtom* aAttribute,
                                      PRInt32& aHint) const;
  NS_IMETHOD HandleDOMEvent(nsIPresContext* aPresContext, nsEvent* aEvent,
                            nsIDOMEvent** aDOMEvent, PRUint32 aFlags,
                            nsEventStatus* aEventStatus);
  NS_IMETHOD SetFocus(nsIPresContext* aPresContext);
  NS_IMETHOD RemoveFocus(nsIPresContext* aPresContext);
  NS_IMETHOD SizeOf(nsISizeOfHandler* aSizer, PRUint32* aResult) const;

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

NS_IMPL_HTMLCONTENT_QI2(nsHTMLTextAreaElement,
                        nsGenericHTMLContainerFormElement,
                        nsIDOMHTMLTextAreaElement,
                        nsIDOMNSHTMLTextAreaElement);


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
  nsCOMPtr<nsIPresContext> presContext;

  GetPresContext(this, getter_AddRefs(presContext));

  return RemoveFocus(presContext);
}

NS_IMETHODIMP
nsHTMLTextAreaElement::Focus() 
{
  nsCOMPtr<nsIPresContext> presContext;

  GetPresContext(this, getter_AddRefs(presContext));

  return SetFocus(presContext);
}

NS_IMETHODIMP
nsHTMLTextAreaElement::SetFocus(nsIPresContext* aPresContext)
{
  // first see if we are disabled or not. If disabled then do nothing.
  nsAutoString disabled;

  if (NS_CONTENT_ATTR_HAS_VALUE ==
      nsGenericHTMLContainerFormElement::GetAttribute(kNameSpaceID_HTML,
                                                      nsHTMLAtoms::disabled,
                                                      disabled)) {
    return NS_OK;
  }

  nsCOMPtr<nsIEventStateManager> esm;
  if (NS_OK == aPresContext->GetEventStateManager(getter_AddRefs(esm))) {
    esm->SetContentState(this, NS_EVENT_STATE_FOCUS);
  }

  nsIFormControlFrame* formControlFrame = nsnull;
  nsresult rv = GetPrimaryFrame(this, formControlFrame);

  if (formControlFrame) {
    formControlFrame->SetFocus(PR_TRUE, PR_TRUE);
    formControlFrame->ScrollIntoView(aPresContext);
    // Could call SelectAll(aPresContext) here to automatically
    // select text when we receive focus.
  }

  return rv;
}

NS_IMETHODIMP
nsHTMLTextAreaElement::RemoveFocus(nsIPresContext* aPresContext)
{
  // If we are disabled, we probably shouldn't have focus in the
  // first place, so allow it to be removed.
  nsresult rv = NS_OK;

  nsIFormControlFrame* formControlFrame = nsnull;
  rv = GetPrimaryFrame(this, formControlFrame);

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
    rootContent = getter_AddRefs(doc->GetRootContent());

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
      nsGenericHTMLContainerFormElement::GetAttribute(kNameSpaceID_HTML,
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
    rv = GetPrimaryFrame(this, formControlFrame);

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
  nsresult rv = GetPrimaryFrame(this, formControlFrame);

  if (formControlFrame) {
    formControlFrame->SetProperty(aPresContext, nsHTMLAtoms::select,
                                  nsAutoString());
    return NS_OK;
  }

  return rv;
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
nsHTMLTextAreaElement::GetValue(nsAWritableString& aValue)
{
  nsIFormControlFrame* formControlFrame = nsnull;

  GetPrimaryFrame(this, formControlFrame);

  if (formControlFrame) {
    formControlFrame->GetProperty(nsHTMLAtoms::value, aValue);

    return NS_OK;
  }
   //XXX: Should this ASSERT instead of getting the default value here?
  return nsGenericHTMLContainerFormElement::GetAttribute(kNameSpaceID_HTML,
                                                         nsHTMLAtoms::value,
                                                         aValue); 
}


NS_IMETHODIMP 
nsHTMLTextAreaElement::SetValue(const nsAReadableString& aValue)
{
  nsIFormControlFrame* formControlFrame = nsnull;

  GetPrimaryFrame(this, formControlFrame);

  if (formControlFrame) {
    nsCOMPtr<nsIPresContext> presContext;
    GetPresContext(this, getter_AddRefs(presContext));

    formControlFrame->SetProperty(presContext, nsHTMLAtoms::value, aValue);
  }

  // Set the attribute in the DOM too, we call SetAttribute with aNotify
  // false so that we don't generate unnecessary reflows.
  nsGenericHTMLContainerFormElement::SetAttribute(kNameSpaceID_HTML,
                                                  nsHTMLAtoms::value, aValue,
                                                  PR_FALSE);

  return NS_OK;
}

NS_IMETHODIMP
nsHTMLTextAreaElement::GetDefaultValue(nsAWritableString& aDefaultValue)
{
  nsGenericHTMLContainerFormElement::GetAttribute(kNameSpaceID_HTML,
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

  nsGenericHTMLContainerFormElement::SetAttribute(kNameSpaceID_HTML,
                                                  nsHTMLAtoms::defaultvalue,
                                                  defaultValue, PR_TRUE);
  nsGenericHTMLContainerFormElement::SetAttribute(kNameSpaceID_HTML,
                                                  nsHTMLAtoms::value,
                                                  defaultValue, PR_TRUE);
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
      text->mVerticalAlign.SetIntValue(value.GetIntValue(),
                                       eStyleUnit_Enumerated);
      break;
    }
  }

  nsGenericHTMLElement::MapCommonAttributesInto(aAttributes, aContext,
                                                aPresContext);
}

NS_IMETHODIMP
nsHTMLTextAreaElement::GetMappedAttributeImpact(const nsIAtom* aAttribute,
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
nsHTMLTextAreaElement::GetAttributeMappingFunctions(nsMapAttributesFunc& aFontMapFunc,
                                                    nsMapAttributesFunc& aMapFunc) const
{
  aFontMapFunc = nsnull;
  aMapFunc = &MapAttributesInto;
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
  rv = GetPrimaryFrame(this, formControlFrame);
  nsIFrame* formFrame = nsnull;

  if (formControlFrame &&
      NS_SUCCEEDED(formControlFrame->QueryInterface(kIFrameIID,
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

NS_IMETHODIMP
nsHTMLTextAreaElement::SizeOf(nsISizeOfHandler* aSizer,
                              PRUint32* aResult) const
{
  *aResult = sizeof(*this) + BaseSizeOf(aSizer);

  return NS_OK;
}


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
