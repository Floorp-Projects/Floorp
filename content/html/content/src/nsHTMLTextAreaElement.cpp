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
#include "nsContentCID.h"
#include "nsCOMPtr.h"
#include "nsIComponentManager.h"
#include "nsIDOMHTMLFormElement.h"
#include "nsIFormControl.h"
#include "nsIForm.h"
#include "nsIFormSubmission.h"
#include "nsIDOMEventReceiver.h"
#include "nsIHTMLContent.h"
#include "nsGenericHTMLElement.h"
#include "nsHTMLAtoms.h"
#include "nsIStyleContext.h"
#include "nsStyleConsts.h"
#include "nsIPresContext.h"
#include "nsHTMLAttributes.h"
#include "nsIFormControlFrame.h"
#include "nsITextControlFrame.h"
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
#include "nsIDOMText.h"
#include "nsReadableUtils.h"
#include "nsITextContent.h"
#include "nsITextAreaElement.h"

static NS_DEFINE_CID(kXULControllersCID,  NS_XULCONTROLLERS_CID);


class nsHTMLTextAreaElement : public nsGenericHTMLContainerFormElement,
                              public nsIDOMHTMLTextAreaElement,
                              public nsIDOMNSHTMLTextAreaElement,
                              public nsITextControlElement,
                              public nsITextAreaElement
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

  // nsITextAreaElement
  NS_DECL_NSITEXTAREAELEMENT

  // nsIFormControl
  NS_IMETHOD GetType(PRInt32* aType);
  NS_IMETHOD Reset();
  NS_IMETHOD SubmitNamesValues(nsIFormSubmission* aFormSubmission,
                               nsIContent* aSubmitElement);
  NS_IMETHOD SaveState();
  NS_IMETHOD RestoreState(nsIPresState* aState);

  // nsITextControlElement
  NS_IMETHOD SetValueGuaranteed(const nsAString& aValue, nsITextControlFrame* aFrame);
  NS_IMETHOD SetValueChanged(PRBool aValueChanged);

  // nsIContent
  NS_IMETHOD InsertChildAt(nsIContent* aKid, PRInt32 aIndex, PRBool aNotify,
                           PRBool aDeepSetDocument);
  NS_IMETHOD ReplaceChildAt(nsIContent* aKid, PRInt32 aIndex, PRBool aNotify,
                            PRBool aDeepSetDocument);
  NS_IMETHOD AppendChildTo(nsIContent* aKid, PRBool aNotify,
                           PRBool aDeepSetDocument);
  NS_IMETHOD RemoveChildAt(PRInt32 aIndex, PRBool aNotify);
  NS_IMETHOD StringToAttribute(nsIAtom* aAttribute,
                               const nsAString& aValue,
                               nsHTMLValue& aResult);
  NS_IMETHOD GetAttributeMappingFunction(nsMapRuleToAttributesFunc& aMapRuleFunc) const;
  NS_IMETHOD GetMappedAttributeImpact(const nsIAtom* aAttribute, PRInt32 aModType,
                                      nsChangeHint& aHint) const;
  NS_IMETHOD HandleDOMEvent(nsIPresContext* aPresContext, nsEvent* aEvent,
                            nsIDOMEvent** aDOMEvent, PRUint32 aFlags,
                            nsEventStatus* aEventStatus);
  NS_IMETHOD SetFocus(nsIPresContext* aPresContext);
  NS_IMETHOD RemoveFocus(nsIPresContext* aPresContext);

  nsresult GetInnerHTML(nsAString& aInnerHTML);
  nsresult SetInnerHTML(const nsAString& aInnerHTML);

#ifdef DEBUG
  NS_IMETHOD SizeOf(nsISizeOfHandler* aSizer, PRUint32* aResult) const;
#endif

protected:
  nsCOMPtr<nsIControllers> mControllers;
  /** The current value.  This is null if the frame owns the value. */
  char*                    mValue;
  /** Whether or not the value has changed since its default value was given. */
  PRPackedBool             mValueChanged;

  NS_IMETHOD SelectAll(nsIPresContext* aPresContext);
  /**
   * Get the value, whether it is from the content or the frame.
   * @param aValue the value [out]
   * @param aIgnoreWrap whether to ignore the wrap attribute when getting the
   *        value.  If this is true, linebreaks will not be inserted even if
   *        wrap=hard.
   */
  void GetValueInternal(nsAString& aValue, PRBool aIgnoreWrap);
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
  mValue = nsnull;
  mValueChanged = PR_FALSE;
}

nsHTMLTextAreaElement::~nsHTMLTextAreaElement()
{
}


NS_IMPL_ADDREF_INHERITED(nsHTMLTextAreaElement, nsGenericElement) 
NS_IMPL_RELEASE_INHERITED(nsHTMLTextAreaElement, nsGenericElement) 


// QueryInterface implementation for nsHTMLTextAreaElement
NS_HTML_CONTENT_INTERFACE_MAP_BEGIN(nsHTMLTextAreaElement,
                                    nsGenericHTMLContainerFormElement)
  NS_INTERFACE_MAP_ENTRY(nsIDOMHTMLTextAreaElement)
  NS_INTERFACE_MAP_ENTRY(nsIDOMNSHTMLTextAreaElement)
  NS_INTERFACE_MAP_ENTRY(nsITextControlElement)
  NS_INTERFACE_MAP_ENTRY(nsITextAreaElement)
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
      nsGenericHTMLContainerFormElement::GetAttr(kNameSpaceID_None,
                                                 nsHTMLAtoms::disabled,
                                                 disabled)) {
    return NS_OK;
  }

  nsCOMPtr<nsIEventStateManager> esm;
  if (NS_OK == aPresContext->GetEventStateManager(getter_AddRefs(esm))) {
    esm->SetContentState(this, NS_EVENT_STATE_FOCUS);
  }

  nsIFormControlFrame* formControlFrame = GetFormControlFrame(PR_TRUE);

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

  nsIFormControlFrame* formControlFrame = GetFormControlFrame(PR_FALSE);

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
      nsGenericHTMLContainerFormElement::GetAttr(kNameSpaceID_None,
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

    nsIFormControlFrame* formControlFrame = GetFormControlFrame(PR_TRUE);

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
  nsIFormControlFrame* formControlFrame = GetFormControlFrame(PR_TRUE);

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
nsHTMLTextAreaElement::GetType(nsAString& aType)
{
  aType.Assign(NS_LITERAL_STRING("textarea"));

  return NS_OK;
}

NS_IMETHODIMP 
nsHTMLTextAreaElement::GetValue(nsAString& aValue)
{
  GetValueInternal(aValue, PR_TRUE);
  return NS_OK;
}

void
nsHTMLTextAreaElement::GetValueInternal(nsAString& aValue, PRBool aIgnoreWrap)
{
  // Get the frame.
  // No need to flush here, if there is no frame yet for this textarea
  // there won't be a value in it we don't already have even if we
  // force the frame to be created.
  nsIFrame* primaryFrame = GetPrimaryFrame(PR_FALSE);
  nsITextControlFrame* textControlFrame = nsnull;
  if (primaryFrame) {
    CallQueryInterface(primaryFrame, &textControlFrame);
  }

  // If the frame exists and owns the value, get it from the frame.  Otherwise
  // get it from content.
  PRBool frameOwnsValue = PR_FALSE;
  if (textControlFrame) {
    textControlFrame->OwnsValue(&frameOwnsValue);
  }
  if (frameOwnsValue) {
    textControlFrame->GetValue(aValue, aIgnoreWrap);
  } else {
    if (!mValueChanged || !mValue) {
      GetDefaultValue(aValue);
    } else {
      aValue = NS_ConvertUTF8toUCS2(mValue);
    }
  }
}


NS_IMETHODIMP
nsHTMLTextAreaElement::SetValueGuaranteed(const nsAString& aValue,
                                          nsITextControlFrame* aFrame)
{
  nsITextControlFrame* textControlFrame = aFrame;
  nsIFormControlFrame* formControlFrame = textControlFrame;
  if (!textControlFrame) {
    // No need to flush here, if there is no frame for this yet forcing
    // creation of one will not do us any good
    formControlFrame = GetFormControlFrame(PR_FALSE);

    if (formControlFrame) {
      CallQueryInterface(formControlFrame, &textControlFrame);
    }
  }

  PRBool frameOwnsValue = PR_FALSE;
  if (textControlFrame) {
    textControlFrame->OwnsValue(&frameOwnsValue);
  }
  if (frameOwnsValue) {
    nsCOMPtr<nsIPresContext> presContext;
    GetPresContext(this, getter_AddRefs(presContext));

    formControlFrame->SetProperty(presContext, nsHTMLAtoms::value, aValue);
  }
  else {
    if (mValue) {
      nsMemory::Free(mValue);
    }
    mValue = ToNewUTF8String(aValue);
    NS_ENSURE_TRUE(mValue, NS_ERROR_OUT_OF_MEMORY);

    SetValueChanged(PR_TRUE);
  }

  return NS_OK;
}

NS_IMETHODIMP 
nsHTMLTextAreaElement::SetValue(const nsAString& aValue)
{
  return SetValueGuaranteed(aValue, nsnull);
}


NS_IMETHODIMP
nsHTMLTextAreaElement::SetValueChanged(PRBool aValueChanged)
{
  mValueChanged = aValueChanged;
  if (!aValueChanged && mValue) {
    nsMemory::Free(mValue);
    mValue = nsnull;
  }
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLTextAreaElement::GetDefaultValue(nsAString& aDefaultValue)
{
  return GetContentsAsText(aDefaultValue);
}  

NS_IMETHODIMP
nsHTMLTextAreaElement::SetDefaultValue(const nsAString& aDefaultValue)
{
  return ReplaceContentsWithText(aDefaultValue, PR_TRUE);
}

NS_IMETHODIMP
nsHTMLTextAreaElement::InsertChildAt(nsIContent* aKid, PRInt32 aIndex,
                                     PRBool aNotify, PRBool aDeepSetDocument)
{
  nsresult rv;
  rv = nsGenericHTMLContainerFormElement::InsertChildAt(aKid, aIndex, aNotify,
                                                        aDeepSetDocument);
  if (!mValueChanged) {
    Reset();
  }
  return rv;
}

NS_IMETHODIMP
nsHTMLTextAreaElement::ReplaceChildAt(nsIContent* aKid, PRInt32 aIndex,
                                      PRBool aNotify, PRBool aDeepSetDocument)
{
  nsresult rv;
  rv = nsGenericHTMLContainerFormElement::ReplaceChildAt(aKid, aIndex, aNotify,
                                                         aDeepSetDocument);
  if (!mValueChanged) {
    Reset();
  }
  return rv;
}

NS_IMETHODIMP
nsHTMLTextAreaElement::AppendChildTo(nsIContent* aKid, PRBool aNotify,
                                     PRBool aDeepSetDocument)
{
  nsresult rv;
  rv = nsGenericHTMLContainerFormElement::AppendChildTo(aKid, aNotify,
                                                        aDeepSetDocument);
  if (!mValueChanged) {
    Reset();
  }
  return rv;
}

NS_IMETHODIMP
nsHTMLTextAreaElement::RemoveChildAt(PRInt32 aIndex, PRBool aNotify)
{
  nsresult rv;
  rv = nsGenericHTMLContainerFormElement::RemoveChildAt(aIndex, aNotify);
  if (!mValueChanged) {
    Reset();
  }
  return rv;
}

NS_IMETHODIMP
nsHTMLTextAreaElement::StringToAttribute(nsIAtom* aAttribute,
                                         const nsAString& aValue,
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

  nsGenericHTMLElement::MapDivAlignAttributeInto(aAttributes, aData);
  nsGenericHTMLElement::MapCommonAttributesInto(aAttributes, aData);
}

NS_IMETHODIMP
nsHTMLTextAreaElement::GetMappedAttributeImpact(const nsIAtom* aAttribute, PRInt32 aModType,
                                                nsChangeHint& aHint) const
{
  // XXX Bug 50280 - It is unclear why we need to do this here for 
  // rows and cols and why the AttributeChanged method in
  // nsTextControlFrame does take care of the entire problem, but
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

  nsIFormControlFrame* formControlFrame = GetFormControlFrame(PR_FALSE);
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

  // If NS_EVENT_FLAG_NO_CONTENT_DISPATCH is set we will not allow content to handle
  // this event.  But to allow middle mouse button paste to work we must allow 
  // middle clicks to go to text fields anyway.
  PRBool noContentDispatch = aEvent->flags & NS_EVENT_FLAG_NO_CONTENT_DISPATCH;
  if (aEvent->message == NS_MOUSE_MIDDLE_CLICK) {
    aEvent->flags &= ~NS_EVENT_FLAG_NO_CONTENT_DISPATCH;
  }

  rv = nsGenericHTMLContainerFormElement::HandleDOMEvent(aPresContext, aEvent,
                                                         aDOMEvent,
                                                         aFlags, aEventStatus);

  // Reset the flag for other content besides this text field
  aEvent->flags |= noContentDispatch ? NS_EVENT_FLAG_NO_CONTENT_DISPATCH : NS_EVENT_FLAG_NONE;

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

// nsITextAreaElement
NS_IMETHODIMP
nsHTMLTextAreaElement::DoneAddingChildren()
{
  RestoreFormControlState(this, this);
  return NS_OK;
}

// nsIFormControl

NS_IMETHODIMP
nsHTMLTextAreaElement::GetType(PRInt32* aType)
{
  NS_ASSERTION(aType, "Null pointer bad!");
  *aType = NS_FORM_TEXTAREA;
  return NS_OK;
}

nsresult
nsHTMLTextAreaElement::GetInnerHTML(nsAString& aInnerHTML)
{
  return GetContentsAsText(aInnerHTML);
}

nsresult
nsHTMLTextAreaElement::SetInnerHTML(const nsAString& aInnerHTML)
{
  return ReplaceContentsWithText(aInnerHTML, PR_TRUE);
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

    mControllers->AppendController(controller);
  }

  *aResult = mControllers;
  NS_IF_ADDREF(*aResult);

  return NS_OK;
}

NS_IMETHODIMP
nsHTMLTextAreaElement::GetTextLength(PRInt32 *aTextLength)
{
  NS_ENSURE_ARG_POINTER(aTextLength);
  nsAutoString val;
  nsresult rv = GetValue(val);
  *aTextLength = val.Length();

  return rv;
}

NS_IMETHODIMP
nsHTMLTextAreaElement::GetSelectionStart(PRInt32 *aSelectionStart)
{
  NS_ENSURE_ARG_POINTER(aSelectionStart);
  nsCOMPtr<nsIFormControlFrame> formControlFrame = getter_AddRefs(GetFormControlFrame(PR_TRUE));

  nsCOMPtr<nsITextControlFrame>
    textControlFrame(do_QueryInterface(formControlFrame));
    
  if (textControlFrame) {
    PRInt32 selectionEnd;
    return textControlFrame->GetSelectionRange(aSelectionStart, &selectionEnd);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsHTMLTextAreaElement::SetSelectionStart(PRInt32 aSelectionStart)
{
  nsCOMPtr<nsIFormControlFrame> formControlFrame = getter_AddRefs(GetFormControlFrame(PR_TRUE));

  nsCOMPtr<nsITextControlFrame>
    textControlFrame(do_QueryInterface(formControlFrame));

  if (textControlFrame)
    textControlFrame->SetSelectionStart(aSelectionStart);

  return NS_OK;
}

NS_IMETHODIMP
nsHTMLTextAreaElement::GetSelectionEnd(PRInt32 *aSelectionEnd)
{
  NS_ENSURE_ARG_POINTER(aSelectionEnd);
  nsCOMPtr<nsIFormControlFrame> formControlFrame = getter_AddRefs(GetFormControlFrame(PR_TRUE));

  nsCOMPtr<nsITextControlFrame>
    textControlFrame(do_QueryInterface(formControlFrame));
    
  if (textControlFrame) {
    PRInt32 selectionStart;
    return textControlFrame->GetSelectionRange(&selectionStart, aSelectionEnd);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsHTMLTextAreaElement::SetSelectionEnd(PRInt32 aSelectionEnd)
{
  nsCOMPtr<nsIFormControlFrame> formControlFrame = getter_AddRefs(GetFormControlFrame(PR_TRUE));

  nsCOMPtr<nsITextControlFrame>
    textControlFrame(do_QueryInterface(formControlFrame));

  if (textControlFrame)
    textControlFrame->SetSelectionEnd(aSelectionEnd);

  return NS_OK;
}

nsresult
nsHTMLTextAreaElement::Reset()
{
  nsresult rv;
  // If the frame is there, we have to set the value so that it will show up.
  nsIFormControlFrame* formControlFrame = GetFormControlFrame(PR_FALSE);
  if (formControlFrame) {
    nsAutoString resetVal;
    GetDefaultValue(resetVal);
    rv = SetValue(resetVal);
    NS_ENSURE_SUCCESS(rv, rv);
    formControlFrame->OnContentReset();
  }
  SetValueChanged(PR_FALSE);
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLTextAreaElement::SubmitNamesValues(nsIFormSubmission* aFormSubmission,
                                         nsIContent* aSubmitElement)
{
  nsresult rv = NS_OK;

  //
  // Disabled elements don't submit
  //
  PRBool disabled;
  rv = GetDisabled(&disabled);
  if (NS_FAILED(rv) || disabled) {
    return rv;
  }

  //
  // Get the name (if no name, no submit)
  //
  nsAutoString name;
  rv = GetAttr(kNameSpaceID_None, nsHTMLAtoms::name, name);
  if (NS_FAILED(rv) || rv == NS_CONTENT_ATTR_NOT_THERE) {
    return rv;
  }

  //
  // Get the value
  //
  nsAutoString value;
  GetValueInternal(value, PR_FALSE);

  //
  // Submit
  //
  rv = aFormSubmission->AddNameValuePair(this, name, value);

  return rv;
}


NS_IMETHODIMP
nsHTMLTextAreaElement::SaveState()
{
  nsresult rv = NS_OK;

  // Only save if value != defaultValue (bug 62713)
  if (mValueChanged) {
    nsCOMPtr<nsIPresState> state;
    rv = GetPrimaryPresState(this, getter_AddRefs(state));
    if (state) {
      nsAutoString value;
      GetValueInternal(value, PR_TRUE);

      rv = nsLinebreakConverter::ConvertStringLineBreaks(
               value,
               nsLinebreakConverter::eLinebreakPlatform,
               nsLinebreakConverter::eLinebreakContent);
      NS_ASSERTION(NS_SUCCEEDED(rv), "Converting linebreaks failed!");
      rv = state->SetStateProperty(NS_LITERAL_STRING("value"), value);
      NS_ASSERTION(NS_SUCCEEDED(rv), "value save failed!");
    }
  }

  return rv;
}

NS_IMETHODIMP
nsHTMLTextAreaElement::RestoreState(nsIPresState* aState)
{
  nsresult rv = NS_OK;

  nsAutoString value;
  rv = aState->GetStateProperty(NS_LITERAL_STRING("value"), value);
  NS_ASSERTION(NS_SUCCEEDED(rv), "value restore failed!");
  SetValue(value);

  return rv;
}
