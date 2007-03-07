/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=2 et tw=80: */
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
#include "nsIDOMNSEditableElement.h"
#include "nsIControllers.h"
#include "nsIFocusController.h"
#include "nsPIDOMWindow.h"
#include "nsContentCID.h"
#include "nsCOMPtr.h"
#include "nsIComponentManager.h"
#include "nsIDOMHTMLFormElement.h"
#include "nsIFormControl.h"
#include "nsIForm.h"
#include "nsIFormSubmission.h"
#include "nsIDOMEventReceiver.h"
#include "nsGenericHTMLElement.h"
#include "nsGkAtoms.h"
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
#include "nsPresState.h"
#include "nsIDOMText.h"
#include "nsReadableUtils.h"
#include "nsEventDispatcher.h"
#include "nsLayoutUtils.h"
#include "nsLayoutErrors.h"
#include "nsStubMutationObserver.h"

static NS_DEFINE_CID(kXULControllersCID,  NS_XULCONTROLLERS_CID);

#define NS_NO_CONTENT_DISPATCH (1 << 0)

class nsHTMLTextAreaElement : public nsGenericHTMLFormElement,
                              public nsIDOMHTMLTextAreaElement,
                              public nsIDOMNSHTMLTextAreaElement,
                              public nsITextControlElement,
                              public nsIDOMNSEditableElement,
                              public nsStubMutationObserver
{
public:
  nsHTMLTextAreaElement(nsINodeInfo *aNodeInfo, PRBool aFromParser = PR_FALSE);
  virtual ~nsHTMLTextAreaElement();

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  // nsIDOMNode
  NS_FORWARD_NSIDOMNODE(nsGenericHTMLFormElement::)

  // nsIDOMElement
  NS_FORWARD_NSIDOMELEMENT(nsGenericHTMLFormElement::)

  // nsIDOMHTMLElement
  NS_FORWARD_NSIDOMHTMLELEMENT(nsGenericHTMLFormElement::)

  // nsIDOMHTMLTextAreaElement
  NS_DECL_NSIDOMHTMLTEXTAREAELEMENT

  // nsIDOMNSHTMLTextAreaElement
  NS_DECL_NSIDOMNSHTMLTEXTAREAELEMENT

  // nsIDOMNSEditableElement
  NS_FORWARD_NSIDOMNSEDITABLEELEMENT(nsGenericHTMLElement::)

  // nsIFormControl
  NS_IMETHOD_(PRInt32) GetType() const { return NS_FORM_TEXTAREA; }
  NS_IMETHOD Reset();
  NS_IMETHOD SubmitNamesValues(nsIFormSubmission* aFormSubmission,
                               nsIContent* aSubmitElement);
  NS_IMETHOD SaveState();
  virtual PRBool RestoreState(nsPresState* aState);

  // nsITextControlElemet
  NS_IMETHOD TakeTextFrameValue(const nsAString& aValue);
  NS_IMETHOD SetValueChanged(PRBool aValueChanged);

  // nsIContent
  virtual PRBool ParseAttribute(PRInt32 aNamespaceID,
                                nsIAtom* aAttribute,
                                const nsAString& aValue,
                                nsAttrValue& aResult);
  virtual nsMapRuleToAttributesFunc GetAttributeMappingFunction() const;
  virtual nsChangeHint GetAttributeChangeHint(const nsIAtom* aAttribute,
                                              PRInt32 aModType) const;
  NS_IMETHOD_(PRBool) IsAttributeMapped(const nsIAtom* aAttribute) const;

  virtual nsresult PreHandleEvent(nsEventChainPreVisitor& aVisitor);
  virtual nsresult PostHandleEvent(nsEventChainPostVisitor& aVisitor);

  virtual void SetFocus(nsPresContext* aPresContext);

  virtual nsresult DoneAddingChildren(PRBool aHaveNotified);
  virtual PRBool IsDoneAddingChildren();

  virtual nsresult Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const;

  /**
   * Called when an attribute is about to be changed
   */
  virtual nsresult BeforeSetAttr(PRInt32 aNameSpaceID, nsIAtom* aName,
                                 const nsAString* aValue, PRBool aNotify);

  // nsIMutationObserver
  virtual void CharacterDataChanged(nsIDocument* aDocument,
                                    nsIContent* aContent,
                                    CharacterDataChangeInfo* aInfo);
  virtual void ContentAppended(nsIDocument* aDocument,
                                nsIContent* aContainer,
                               PRInt32 aNewIndexInContainer);
  virtual void ContentInserted(nsIDocument* aDocument,
                               nsIContent* aContainer,
                               nsIContent* aChild,
                               PRInt32 aIndexInContainer);
  virtual void ContentRemoved(nsIDocument* aDocument,
                              nsIContent* aContainer,
                              nsIContent* aChild,
                              PRInt32 aIndexInContainer);

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
  /** Whether our disabled state has changed from the default **/
  PRPackedBool             mDisabledChanged;
  
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

  /**
   * Common method to call from the various mutation observer methods.
   * aContent is a content node that's either the one that changed or its
   * parent; we should only respond to the change if aContent is non-anonymous.
   */
  void ContentChanged(nsIContent* aContent);
};


NS_IMPL_NS_NEW_HTML_ELEMENT_CHECK_PARSER(TextArea)


nsHTMLTextAreaElement::nsHTMLTextAreaElement(nsINodeInfo *aNodeInfo,
                                             PRBool aFromParser)
  : nsGenericHTMLFormElement(aNodeInfo),
    mValue(nsnull),
    mValueChanged(PR_FALSE),
    mHandlingSelect(PR_FALSE),
    mDoneAddingChildren(!aFromParser),
    mDisabledChanged(PR_FALSE)
{
  AddMutationObserver(this);
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
  NS_INTERFACE_MAP_ENTRY(nsIDOMNSEditableElement)
  NS_INTERFACE_MAP_ENTRY(nsIMutationObserver)
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO(HTMLTextAreaElement)
NS_HTML_CONTENT_INTERFACE_MAP_END


// nsIDOMHTMLTextAreaElement


NS_IMPL_ELEMENT_CLONE(nsHTMLTextAreaElement)


NS_IMETHODIMP
nsHTMLTextAreaElement::GetForm(nsIDOMHTMLFormElement** aForm)
{
  return nsGenericHTMLFormElement::GetForm(aForm);
}


// nsIContent

NS_IMETHODIMP
nsHTMLTextAreaElement::Blur()
{
  if (ShouldBlur(this)) {
    SetElementFocus(PR_FALSE);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsHTMLTextAreaElement::Focus() 
{
  if (ShouldFocus(this)) {
    SetElementFocus(PR_TRUE);
  }

  return NS_OK;
}

void
nsHTMLTextAreaElement::SetFocus(nsPresContext* aPresContext)
{
  if (!aPresContext)
    return;

  // first see if we are disabled or not. If disabled then do nothing.
  if (HasAttr(kNameSpaceID_None, nsGkAtoms::disabled)) {
    return;
  }

  // We can't be focus'd if we aren't in a document
  nsIDocument* doc = GetCurrentDoc();
  if (!doc)
    return;

  // If the window is not active, do not allow the focus to bring the
  // window to the front.  We update the focus controller, but do
  // nothing else.
  nsPIDOMWindow* win = doc->GetWindow();
  if (win) {
    nsIFocusController *focusController = win->GetRootFocusController();
    PRBool isActive = PR_FALSE;
    focusController->GetActive(&isActive);
    if (!isActive) {
      focusController->SetFocusedWindow(win);
      focusController->SetFocusedElement(this);

      return;
    }
  }

  SetFocusAndScrollIntoView(aPresContext);
}

NS_IMETHODIMP
nsHTMLTextAreaElement::Select()
{
  nsresult rv = NS_OK;

  // first see if we are disabled or not. If disabled then do nothing.
  if (HasAttr(kNameSpaceID_None, nsGkAtoms::disabled)) {
    return rv;
  }

  // We can't be focus'd if we aren't in a document
  nsIDocument* doc = GetCurrentDoc();
  if (!doc)
    return rv;

  // If the window is not active, do not allow the focus to bring the
  // window to the front.  We update the focus controller, but do
  // nothing else.
  nsPIDOMWindow* win = doc->GetWindow();
  if (win) {
    nsIFocusController *focusController = win->GetRootFocusController();
    PRBool isActive = PR_FALSE;
    focusController->GetActive(&isActive);
    if (!isActive) {
      focusController->SetFocusedWindow(win);
      focusController->SetFocusedElement(this);

      return rv;
    }
  }

  // XXX Bug?  We have to give the input focus before contents can be
  // selected

  // Just like SetFocus() but without the ScrollIntoView()!
  nsCOMPtr<nsPresContext> presContext = GetPresContext();

  nsEventStatus status = nsEventStatus_eIgnore;
  nsGUIEvent event(PR_TRUE, NS_FORM_SELECTED, nsnull);
  nsEventDispatcher::Dispatch(NS_STATIC_CAST(nsIContent*, this), presContext,
                              &event, nsnull, &status);

  // If the DOM event was not canceled (e.g. by a JS event handler
  // returning false)
  if (status == nsEventStatus_eIgnore) {
    PRBool shouldFocus = ShouldFocus(this);

    if (shouldFocus &&
        !presContext->EventStateManager()->SetContentState(this,
                                                           NS_EVENT_STATE_FOCUS)) {
      return rv; // We ended up unfocused, e.g. due to a DOM event handler.
    }

    nsIFormControlFrame* formControlFrame = GetFormControlFrame(PR_TRUE);

    if (formControlFrame) {
      if (shouldFocus) {
        formControlFrame->SetFocus(PR_TRUE, PR_TRUE);
      }

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
    formControlFrame->SetFormProperty(nsGkAtoms::select, EmptyString());
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
  nsIFrame* primaryFrame = GetPrimaryFrame();
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
    formControlFrame->SetFormProperty(nsGkAtoms::value, aValue);
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
  nsContentUtils::GetNodeTextContent(this, PR_FALSE, aDefaultValue);
  return NS_OK;
}  

NS_IMETHODIMP
nsHTMLTextAreaElement::SetDefaultValue(const nsAString& aDefaultValue)
{
  nsresult rv = nsContentUtils::SetNodeTextContent(this, aDefaultValue, PR_TRUE);
  if (NS_SUCCEEDED(rv) && !mValueChanged) {
    Reset();
  }
  return rv;
}

PRBool
nsHTMLTextAreaElement::ParseAttribute(PRInt32 aNamespaceID,
                                      nsIAtom* aAttribute,
                                      const nsAString& aValue,
                                      nsAttrValue& aResult)
{
  if (aNamespaceID == kNameSpaceID_None) {
    if (aAttribute == nsGkAtoms::cols) {
      return aResult.ParseIntWithBounds(aValue, 0);
    }
    if (aAttribute == nsGkAtoms::rows) {
      return aResult.ParseIntWithBounds(aValue, 0);
    }
  }
  return nsGenericHTMLElement::ParseAttribute(aNamespaceID, aAttribute, aValue,
                                              aResult);
}

static void
MapAttributesIntoRule(const nsMappedAttributes* aAttributes,
                      nsRuleData* aData)
{
  nsGenericHTMLFormElement::MapDivAlignAttributeInto(aAttributes, aData);
  nsGenericHTMLFormElement::MapCommonAttributesInto(aAttributes, aData);
}

nsChangeHint
nsHTMLTextAreaElement::GetAttributeChangeHint(const nsIAtom* aAttribute,
                                              PRInt32 aModType) const
{
  nsChangeHint retval =
      nsGenericHTMLFormElement::GetAttributeChangeHint(aAttribute, aModType);
  if (aAttribute == nsGkAtoms::rows ||
      aAttribute == nsGkAtoms::cols) {
    NS_UpdateHint(retval, NS_STYLE_HINT_REFLOW);
  }
  return retval;
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

nsMapRuleToAttributesFunc
nsHTMLTextAreaElement::GetAttributeMappingFunction() const
{
  return &MapAttributesIntoRule;
}

nsresult
nsHTMLTextAreaElement::PreHandleEvent(nsEventChainPreVisitor& aVisitor)
{
  // Do not process any DOM events if the element is disabled
  aVisitor.mCanHandle = PR_FALSE;
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

  // Don't dispatch a second select event if we are already handling
  // one.
  if (aVisitor.mEvent->message == NS_FORM_SELECTED) {
    if (mHandlingSelect) {
      return NS_OK;
    }
    mHandlingSelect = PR_TRUE;
  }

  // If NS_EVENT_FLAG_NO_CONTENT_DISPATCH is set we will not allow content to handle
  // this event.  But to allow middle mouse button paste to work we must allow 
  // middle clicks to go to text fields anyway.
  if (aVisitor.mEvent->flags & NS_EVENT_FLAG_NO_CONTENT_DISPATCH)
    aVisitor.mItemFlags |= NS_NO_CONTENT_DISPATCH;
  if (aVisitor.mEvent->message == NS_MOUSE_CLICK &&
      aVisitor.mEvent->eventStructType == NS_MOUSE_EVENT &&
      NS_STATIC_CAST(nsMouseEvent*, aVisitor.mEvent)->button ==
        nsMouseEvent::eMiddleButton) {
    aVisitor.mEvent->flags &= ~NS_EVENT_FLAG_NO_CONTENT_DISPATCH;
  }

  // Fire onchange (if necessary), before we do the blur, bug 370521.
  if (aVisitor.mEvent->message == NS_BLUR_CONTENT) {
    nsIFrame* primaryFrame = GetPrimaryFrame();
    if (primaryFrame) {
      nsITextControlFrame* textFrame = nsnull;
      CallQueryInterface(primaryFrame, &textFrame);
      if (textFrame) {
        textFrame->CheckFireOnChange();
      }
    }
  }

  return nsGenericHTMLElement::PreHandleEvent(aVisitor);
}

nsresult
nsHTMLTextAreaElement::PostHandleEvent(nsEventChainPostVisitor& aVisitor)
{
  if (aVisitor.mEvent->message == NS_FORM_SELECTED) {
    mHandlingSelect = PR_FALSE;
  }

  // Reset the flag for other content besides this text field
  aVisitor.mEvent->flags |= (aVisitor.mItemFlags & NS_NO_CONTENT_DISPATCH)
    ? NS_EVENT_FLAG_NO_CONTENT_DISPATCH : NS_EVENT_FLAG_NONE;

  return NS_OK;
}

nsresult
nsHTMLTextAreaElement::DoneAddingChildren(PRBool aHaveNotified)
{
  if (!mValueChanged) {
    if (!mDoneAddingChildren) {
      // Reset now that we're done adding children if the content sink tried to
      // sneak some text in without calling AppendChildTo.
      Reset();
    }

    RestoreFormControlState(this, this);
  }

  mDoneAddingChildren = PR_TRUE;

  return NS_OK;
}

PRBool
nsHTMLTextAreaElement::IsDoneAddingChildren()
{
  return mDoneAddingChildren;
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
  if (!GetAttr(kNameSpaceID_None, nsGkAtoms::name, name)) {
    return NS_OK;
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
  nsPresState *state = nsnull;
  if (mValueChanged) {
    rv = GetPrimaryPresState(this, &state);
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

  if (mDisabledChanged) {
    if (!state) {
      rv = GetPrimaryPresState(this, &state);
    }
    if (state) {
      PRBool disabled;
      GetDisabled(&disabled);
      if (disabled) {
        rv |= state->SetStateProperty(NS_LITERAL_STRING("disabled"),
                                      NS_LITERAL_STRING("t"));
      } else {
        rv |= state->SetStateProperty(NS_LITERAL_STRING("disabled"),
                                      NS_LITERAL_STRING("f"));
      }
      NS_ASSERTION(NS_SUCCEEDED(rv), "disabled save failed!");
    }
  }
  return rv;
}

PRBool
nsHTMLTextAreaElement::RestoreState(nsPresState* aState)
{
  nsAutoString value;
  nsresult rv =
    aState->GetStateProperty(NS_LITERAL_STRING("value"), value);
  NS_ASSERTION(NS_SUCCEEDED(rv), "value restore failed!");
  SetValue(value);

  nsAutoString disabled;
  rv = aState->GetStateProperty(NS_LITERAL_STRING("disabled"), disabled);
  NS_ASSERTION(NS_SUCCEEDED(rv), "disabled restore failed!");
  if (rv == NS_STATE_PROPERTY_EXISTS) {
    SetDisabled(disabled.EqualsLiteral("t"));
  }

  return PR_FALSE;
}

nsresult
nsHTMLTextAreaElement::BeforeSetAttr(PRInt32 aNameSpaceID, nsIAtom* aName,
                                     const nsAString* aValue, PRBool aNotify)
{
  if (aNotify && aName == nsGkAtoms::disabled &&
      aNameSpaceID == kNameSpaceID_None) {
    mDisabledChanged = PR_TRUE;
  }

  return nsGenericHTMLFormElement::BeforeSetAttr(aNameSpaceID, aName,
                                                 aValue, aNotify);
}

void
nsHTMLTextAreaElement::CharacterDataChanged(nsIDocument* aDocument,
                                            nsIContent* aContent,
                                            CharacterDataChangeInfo* aInfo)
{
  ContentChanged(aContent);
}

void
nsHTMLTextAreaElement::ContentAppended(nsIDocument* aDocument,
                                       nsIContent* aContainer,
                                       PRInt32 aNewIndexInContainer)
{
  ContentChanged(aContainer);
}

void
nsHTMLTextAreaElement::ContentInserted(nsIDocument* aDocument,
                                       nsIContent* aContainer,
                                       nsIContent* aChild,
                                       PRInt32 aIndexInContainer)
{
  ContentChanged(aChild);
}

void
nsHTMLTextAreaElement::ContentRemoved(nsIDocument* aDocument,
                                      nsIContent* aContainer,
                                      nsIContent* aChild,
                                      PRInt32 aIndexInContainer)
{
  ContentChanged(aChild);
}

void
nsHTMLTextAreaElement::ContentChanged(nsIContent* aContent)
{
  if (!mValueChanged && mDoneAddingChildren &&
      nsContentUtils::IsInSameAnonymousTree(this, aContent)) {
    Reset();
  }
}
