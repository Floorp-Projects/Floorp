/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
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
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
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
#include "nsStyleConsts.h"
#include "nsPresContext.h"
#include "nsMappedAttributes.h"
#include "nsIFormControlFrame.h"
#include "nsITextControlFrame.h"
#include "nsIEventStateManager.h"
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

static NS_DEFINE_CID(kXULControllersCID,  NS_XULCONTROLLERS_CID);


class nsHTMLTextAreaElement : public nsGenericHTMLFormElement,
                              public nsIDOMHTMLTextAreaElement,
                              public nsIDOMNSHTMLTextAreaElement,
                              public nsITextControlElement
{
public:
  nsHTMLTextAreaElement(nsINodeInfo *aNodeInfo, PRBool aFromParser = PR_FALSE);
  virtual ~nsHTMLTextAreaElement();

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  // nsIDOMNode
  NS_FORWARD_NSIDOMNODE_NO_CLONENODE(nsGenericHTMLFormElement::)

  // nsIDOMElement
  NS_FORWARD_NSIDOMELEMENT(nsGenericHTMLFormElement::)

  // nsIDOMHTMLElement
  NS_FORWARD_NSIDOMHTMLELEMENT(nsGenericHTMLFormElement::)

  // nsIDOMHTMLTextAreaElement
  NS_DECL_NSIDOMHTMLTEXTAREAELEMENT

  // nsIDOMNSHTMLTextAreaElement
  NS_DECL_NSIDOMNSHTMLTEXTAREAELEMENT

  // nsIFormControl
  NS_IMETHOD_(PRInt32) GetType() { return NS_FORM_TEXTAREA; }
  NS_IMETHOD Reset();
  NS_IMETHOD SubmitNamesValues(nsIFormSubmission* aFormSubmission,
                               nsIContent* aSubmitElement);
  NS_IMETHOD SaveState();
  virtual PRBool RestoreState(nsIPresState* aState);

  // nsITextControlElemet
  NS_IMETHOD TakeTextFrameValue(const nsAString& aValue);
  NS_IMETHOD SetValueChanged(PRBool aValueChanged);

  // nsIContent
  virtual nsresult InsertChildAt(nsIContent* aKid, PRUint32 aIndex,
                                 PRBool aNotify, PRBool aDeepSetDocument);
  virtual nsresult AppendChildTo(nsIContent* aKid, PRBool aNotify,
                                 PRBool aDeepSetDocument);
  virtual nsresult RemoveChildAt(PRUint32 aIndex, PRBool aNotify);
  virtual PRBool ParseAttribute(nsIAtom* aAttribute,
                                const nsAString& aValue,
                                nsAttrValue& aResult);
  NS_IMETHOD GetAttributeMappingFunction(nsMapRuleToAttributesFunc& aMapRuleFunc) const;
  NS_IMETHOD GetAttributeChangeHint(const nsIAtom* aAttribute,
                                    PRInt32 aModType,
                                    nsChangeHint& aHint) const;
  NS_IMETHOD_(PRBool) IsAttributeMapped(const nsIAtom* aAttribute) const;
  virtual nsresult HandleDOMEvent(nsPresContext* aPresContext,
                                  nsEvent* aEvent, nsIDOMEvent** aDOMEvent,
                                  PRUint32 aFlags,
                                  nsEventStatus* aEventStatus);
  virtual void SetFocus(nsPresContext* aPresContext);

  virtual nsresult GetInnerHTML(nsAString& aInnerHTML);
  virtual nsresult SetInnerHTML(const nsAString& aInnerHTML);

  virtual void DoneAddingChildren();
  virtual PRBool IsDoneAddingChildren();

protected:
  nsCOMPtr<nsIControllers> mControllers;
  /** The current value.  This is null if the frame owns the value. */
  char*                    mValue;
  /** Whether or not the value has changed since its default value was given. */
  PRPackedBool             mValueChanged;
  /** Whether or not we are already handling select event. */
  PRPackedBool             mHandlingSelect;
  /** Whether or not we are done adding children (always PR_TRUE if not
      created by a parser */
  PRPackedBool             mDoneAddingChildren;

  NS_IMETHOD SelectAll(nsPresContext* aPresContext);
  /**
   * Get the value, whether it is from the content or the frame.
   * @param aValue the value [out]
   * @param aIgnoreWrap whether to ignore the wrap attribute when getting the
   *        value.  If this is true, linebreaks will not be inserted even if
   *        wrap=hard.
   */
  void GetValueInternal(nsAString& aValue, PRBool aIgnoreWrap);

  nsresult SetValueInternal(const nsAString& aValue,
                            nsITextControlFrame* aFrame);
  nsresult GetSelectionRange(PRInt32* aSelectionStart, PRInt32* aSelectionEnd);
};


NS_IMPL_NS_NEW_HTML_ELEMENT_CHECK_PARSER(TextArea)


nsHTMLTextAreaElement::nsHTMLTextAreaElement(nsINodeInfo *aNodeInfo,
                                             PRBool aFromParser)
  : nsGenericHTMLFormElement(aNodeInfo),
    mValue(nsnull),
    mValueChanged(PR_FALSE),
    mHandlingSelect(PR_FALSE),
    mDoneAddingChildren(!aFromParser)
{
}

nsHTMLTextAreaElement::~nsHTMLTextAreaElement()
{
  if (mValue) {
    nsMemory::Free(mValue);
  }
}


NS_IMPL_ADDREF_INHERITED(nsHTMLTextAreaElement, nsGenericElement) 
NS_IMPL_RELEASE_INHERITED(nsHTMLTextAreaElement, nsGenericElement) 


// QueryInterface implementation for nsHTMLTextAreaElement
NS_HTML_CONTENT_INTERFACE_MAP_BEGIN(nsHTMLTextAreaElement,
                                    nsGenericHTMLFormElement)
  NS_INTERFACE_MAP_ENTRY(nsIDOMHTMLTextAreaElement)
  NS_INTERFACE_MAP_ENTRY(nsIDOMNSHTMLTextAreaElement)
  NS_INTERFACE_MAP_ENTRY(nsITextControlElement)
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO(HTMLTextAreaElement)
NS_HTML_CONTENT_INTERFACE_MAP_END


// nsIDOMHTMLTextAreaElement


NS_IMPL_HTML_DOM_CLONENODE(TextArea)


NS_IMETHODIMP
nsHTMLTextAreaElement::GetForm(nsIDOMHTMLFormElement** aForm)
{
  return nsGenericHTMLFormElement::GetForm(aForm);
}


// nsIContent

NS_IMETHODIMP
nsHTMLTextAreaElement::Blur()
{
  SetElementFocus(PR_FALSE);

  return NS_OK;
}

NS_IMETHODIMP
nsHTMLTextAreaElement::Focus() 
{
  SetElementFocus(PR_TRUE);

  return NS_OK;
}

void
nsHTMLTextAreaElement::SetFocus(nsPresContext* aPresContext)
{
  if (!aPresContext)
    return;

  // first see if we are disabled or not. If disabled then do nothing.
  nsAutoString disabled;

  if (NS_CONTENT_ATTR_HAS_VALUE ==
      nsGenericHTMLFormElement::GetAttr(kNameSpaceID_None,
                                        nsHTMLAtoms::disabled, disabled)) {
    return;
  }

  aPresContext->EventStateManager()->SetContentState(this,
                                                     NS_EVENT_STATE_FOCUS);

  nsIFormControlFrame* formControlFrame = GetFormControlFrame(PR_TRUE);

  if (formControlFrame) {
    formControlFrame->SetFocus(PR_TRUE, PR_TRUE);
    formControlFrame->ScrollIntoView(aPresContext);
    // Could call SelectAll(aPresContext) here to automatically
    // select text when we receive focus.
  }
}

NS_IMETHODIMP
nsHTMLTextAreaElement::Select()
{
  nsresult rv = NS_OK;

  // first see if we are disabled or not. If disabled then do nothing.
  nsAutoString disabled;
  if (NS_CONTENT_ATTR_HAS_VALUE ==
      nsGenericHTMLFormElement::GetAttr(kNameSpaceID_None,
                                        nsHTMLAtoms::disabled, disabled)) {
    return rv;
  }

  // XXX Bug?  We have to give the input focus before contents can be
  // selected

  // Just like SetFocus() but without the ScrollIntoView()!
  nsCOMPtr<nsPresContext> presContext = GetPresContext();

  nsEventStatus status = nsEventStatus_eIgnore;
  nsGUIEvent event(NS_FORM_SELECTED);
  rv = HandleDOMEvent(presContext, &event, nsnull, NS_EVENT_FLAG_INIT, &status);

  // If the DOM event was not canceled (e.g. by a JS event handler
  // returning false)
  if (status == nsEventStatus_eIgnore) {
    presContext->EventStateManager()->SetContentState(this,
                                                      NS_EVENT_STATE_FOCUS);

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
nsHTMLTextAreaElement::SelectAll(nsPresContext* aPresContext)
{
  nsIFormControlFrame* formControlFrame = GetFormControlFrame(PR_TRUE);

  if (formControlFrame) {
    formControlFrame->SetProperty(aPresContext, nsHTMLAtoms::select,
                                  EmptyString());
  }

  return NS_OK;
}

NS_IMPL_STRING_ATTR(nsHTMLTextAreaElement, AccessKey, accesskey)
NS_IMPL_INT_ATTR(nsHTMLTextAreaElement, Cols, cols)
NS_IMPL_BOOL_ATTR(nsHTMLTextAreaElement, Disabled, disabled)
NS_IMPL_STRING_ATTR(nsHTMLTextAreaElement, Name, name)
NS_IMPL_BOOL_ATTR(nsHTMLTextAreaElement, ReadOnly, readonly)
NS_IMPL_INT_ATTR(nsHTMLTextAreaElement, Rows, rows)
NS_IMPL_INT_ATTR_DEFAULT_VALUE(nsHTMLTextAreaElement, TabIndex, tabindex, 0)
  

NS_IMETHODIMP 
nsHTMLTextAreaElement::GetType(nsAString& aType)
{
  aType.AssignLiteral("textarea");

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
      CopyUTF8toUTF16(mValue, aValue);
    }
  }
}

NS_IMETHODIMP
nsHTMLTextAreaElement::TakeTextFrameValue(const nsAString& aValue)
{
  if (mValue) {
    nsMemory::Free(mValue);
  }
  mValue = ToNewUTF8String(aValue);
  SetValueChanged(PR_TRUE);
  return NS_OK;
}

nsresult
nsHTMLTextAreaElement::SetValueInternal(const nsAString& aValue,
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
    formControlFrame->SetProperty(GetPresContext(),
                                  nsHTMLAtoms::value, aValue);
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
  return SetValueInternal(aValue, nsnull);
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
  GetContentsAsText(aDefaultValue);
  return NS_OK;
}  

NS_IMETHODIMP
nsHTMLTextAreaElement::SetDefaultValue(const nsAString& aDefaultValue)
{
  return ReplaceContentsWithText(aDefaultValue, PR_TRUE);
}

nsresult
nsHTMLTextAreaElement::InsertChildAt(nsIContent* aKid, PRUint32 aIndex,
                                     PRBool aNotify, PRBool aDeepSetDocument)
{
  nsresult rv;
  rv = nsGenericHTMLFormElement::InsertChildAt(aKid, aIndex, aNotify,
                                               aDeepSetDocument);
  if (!mValueChanged) {
    Reset();
  }
  return rv;
}

nsresult
nsHTMLTextAreaElement::AppendChildTo(nsIContent* aKid, PRBool aNotify,
                                     PRBool aDeepSetDocument)
{
  nsresult rv;
  rv = nsGenericHTMLFormElement::AppendChildTo(aKid, aNotify,
                                               aDeepSetDocument);
  if (!mValueChanged) {
    Reset();
  }
  return rv;
}

nsresult
nsHTMLTextAreaElement::RemoveChildAt(PRUint32 aIndex, PRBool aNotify)
{
  nsresult rv;
  rv = nsGenericHTMLFormElement::RemoveChildAt(aIndex, aNotify);
  if (!mValueChanged) {
    Reset();
  }
  return rv;
}

PRBool
nsHTMLTextAreaElement::ParseAttribute(nsIAtom* aAttribute,
                                      const nsAString& aValue,
                                      nsAttrValue& aResult)
{
  if (aAttribute == nsHTMLAtoms::cols) {
    return aResult.ParseIntWithBounds(aValue, 0);
  }
  if (aAttribute == nsHTMLAtoms::rows) {
    return aResult.ParseIntWithBounds(aValue, 0);
  }
  return nsGenericHTMLElement::ParseAttribute(aAttribute, aValue, aResult);
}

static void
MapAttributesIntoRule(const nsMappedAttributes* aAttributes,
                      nsRuleData* aData)
{
  nsGenericHTMLFormElement::MapDivAlignAttributeInto(aAttributes, aData);
  nsGenericHTMLFormElement::MapCommonAttributesInto(aAttributes, aData);
}

NS_IMETHODIMP
nsHTMLTextAreaElement::GetAttributeChangeHint(const nsIAtom* aAttribute,
                                              PRInt32 aModType,
                                              nsChangeHint& aHint) const
{
  // XXX Bug 50280 - It is unclear why we need to do this here for 
  // rows and cols and why the AttributeChanged method in
  // nsTextControlFrame does take care of the entire problem, but
  // it doesn't and this makes things better
  nsresult rv =
    nsGenericHTMLFormElement::GetAttributeChangeHint(aAttribute, aModType,
                                                     aHint);
  if (aAttribute == nsHTMLAtoms::rows ||
      aAttribute == nsHTMLAtoms::cols) {
    NS_UpdateHint(aHint, NS_STYLE_HINT_REFLOW);
  }
  return rv;
}

NS_IMETHODIMP_(PRBool)
nsHTMLTextAreaElement::IsAttributeMapped(const nsIAtom* aAttribute) const
{
  static const MappedAttributeEntry* const map[] = {
    sDivAlignAttributeMap,
    sCommonAttributeMap,
  };

  return FindAttributeDependence(aAttribute, map, NS_ARRAY_LENGTH(map));
}

NS_IMETHODIMP
nsHTMLTextAreaElement::GetAttributeMappingFunction(nsMapRuleToAttributesFunc& aMapRuleFunc) const
{
  aMapRuleFunc = &MapAttributesIntoRule;
  return NS_OK;
}


nsresult
nsHTMLTextAreaElement::HandleDOMEvent(nsPresContext* aPresContext,
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
      NS_SUCCEEDED(CallQueryInterface(formControlFrame, &formFrame)) &&
      formFrame) {
    const nsStyleUserInterface* uiStyle = formFrame->GetStyleUserInterface();

    if (uiStyle->mUserInput == NS_STYLE_USER_INPUT_NONE ||
        uiStyle->mUserInput == NS_STYLE_USER_INPUT_DISABLED) {
      return NS_OK;
    }
  }

  PRBool isSelectEvent = (aEvent->message == NS_FORM_SELECTED);
  // Don't dispatch a second select event if we are already handling
  // one.
  if (isSelectEvent && mHandlingSelect) {
    return NS_OK;
  }

  // If NS_EVENT_FLAG_NO_CONTENT_DISPATCH is set we will not allow content to handle
  // this event.  But to allow middle mouse button paste to work we must allow 
  // middle clicks to go to text fields anyway.
  PRBool noContentDispatch = aEvent->flags & NS_EVENT_FLAG_NO_CONTENT_DISPATCH;
  if (aEvent->message == NS_MOUSE_MIDDLE_CLICK) {
    aEvent->flags &= ~NS_EVENT_FLAG_NO_CONTENT_DISPATCH;
  }

  if (isSelectEvent) {
    mHandlingSelect = PR_TRUE;
  }

  rv = nsGenericHTMLFormElement::HandleDOMEvent(aPresContext, aEvent,
                                                aDOMEvent, aFlags,
                                                aEventStatus);

  if (isSelectEvent) {
    mHandlingSelect = PR_FALSE;
  }

  // Reset the flag for other content besides this text field
  aEvent->flags |= noContentDispatch ? NS_EVENT_FLAG_NO_CONTENT_DISPATCH : NS_EVENT_FLAG_NONE;

  return rv;
}

void
nsHTMLTextAreaElement::DoneAddingChildren()
{
  mDoneAddingChildren = PR_TRUE;
  RestoreFormControlState(this, this);
}

PRBool
nsHTMLTextAreaElement::IsDoneAddingChildren()
{
  return mDoneAddingChildren;
}

nsresult
nsHTMLTextAreaElement::GetInnerHTML(nsAString& aInnerHTML)
{
  GetContentsAsText(aInnerHTML);
  return NS_OK;
}

nsresult
nsHTMLTextAreaElement::SetInnerHTML(const nsAString& aInnerHTML)
{
  return ReplaceContentsWithText(aInnerHTML, PR_TRUE);
}

// Controllers Methods

NS_IMETHODIMP
nsHTMLTextAreaElement::GetControllers(nsIControllers** aResult)
{
  NS_ENSURE_ARG_POINTER(aResult);

  if (!mControllers)
  {
    nsresult rv;
    mControllers = do_CreateInstance(kXULControllersCID, &rv);
    NS_ENSURE_SUCCESS(rv, rv);

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
  
  PRInt32 selEnd;
  return GetSelectionRange(aSelectionStart, &selEnd);
}

NS_IMETHODIMP
nsHTMLTextAreaElement::SetSelectionStart(PRInt32 aSelectionStart)
{
  nsresult rv = NS_ERROR_FAILURE;
  nsIFormControlFrame* formControlFrame = GetFormControlFrame(PR_TRUE);

  if (formControlFrame){
    nsITextControlFrame* textControlFrame = nsnull;
    CallQueryInterface(formControlFrame, &textControlFrame);

    if (textControlFrame)
      rv = textControlFrame->SetSelectionStart(aSelectionStart);
  }

  return rv;
}

NS_IMETHODIMP
nsHTMLTextAreaElement::GetSelectionEnd(PRInt32 *aSelectionEnd)
{
  NS_ENSURE_ARG_POINTER(aSelectionEnd);
  
  PRInt32 selStart;
  return GetSelectionRange(&selStart, aSelectionEnd);
}

NS_IMETHODIMP
nsHTMLTextAreaElement::SetSelectionEnd(PRInt32 aSelectionEnd)
{
  nsresult rv = NS_ERROR_FAILURE;
  nsIFormControlFrame* formControlFrame = GetFormControlFrame(PR_TRUE);

  if (formControlFrame) {
    nsITextControlFrame* textControlFrame = nsnull;
    CallQueryInterface(formControlFrame, &textControlFrame);

    if (textControlFrame)
      rv = textControlFrame->SetSelectionEnd(aSelectionEnd);
  }

  return rv;
}

nsresult
nsHTMLTextAreaElement::GetSelectionRange(PRInt32* aSelectionStart,
                                      PRInt32* aSelectionEnd)
{
  nsresult rv = NS_ERROR_FAILURE;
  nsIFormControlFrame* formControlFrame = GetFormControlFrame(PR_TRUE);

  if (formControlFrame) {
    nsITextControlFrame* textControlFrame = nsnull;
    CallQueryInterface(formControlFrame, &textControlFrame);

    if (textControlFrame)
      rv = textControlFrame->GetSelectionRange(aSelectionStart, aSelectionEnd);
  }

  return rv;
}

NS_IMETHODIMP
nsHTMLTextAreaElement::SetSelectionRange(PRInt32 aSelectionStart, PRInt32 aSelectionEnd)
{ 
  nsresult rv = NS_ERROR_FAILURE;
  nsIFormControlFrame* formControlFrame = GetFormControlFrame(PR_TRUE);

  if (formControlFrame) {
    nsITextControlFrame* textControlFrame = nsnull;
    CallQueryInterface(formControlFrame, &textControlFrame);

    if (textControlFrame)
      rv = textControlFrame->SetSelectionRange(aSelectionStart, aSelectionEnd);
  }

  return rv;
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

PRBool
nsHTMLTextAreaElement::RestoreState(nsIPresState* aState)
{
  nsAutoString value;
#ifdef DEBUG
  nsresult rv =
#endif
    aState->GetStateProperty(NS_LITERAL_STRING("value"), value);
  NS_ASSERTION(NS_SUCCEEDED(rv), "value restore failed!");
  SetValue(value);

  return PR_FALSE;
}
