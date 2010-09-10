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
#include "nsFocusManager.h"
#include "nsPIDOMWindow.h"
#include "nsContentCID.h"
#include "nsCOMPtr.h"
#include "nsIComponentManager.h"
#include "nsIDOMHTMLFormElement.h"
#include "nsIFormControl.h"
#include "nsIForm.h"
#include "nsFormSubmission.h"
#include "nsIDOMEventTarget.h"
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
#include "nsDOMError.h"
#include "mozAutoDocUpdate.h"
#include "nsISupportsPrimitives.h"
#include "nsContentCreatorFunctions.h"
#include "nsIConstraintValidation.h"

#include "nsTextEditorState.h"

static NS_DEFINE_CID(kXULControllersCID,  NS_XULCONTROLLERS_CID);

#define NS_NO_CONTENT_DISPATCH (1 << 0)

class nsHTMLTextAreaElement : public nsGenericHTMLFormElement,
                              public nsIDOMHTMLTextAreaElement,
                              public nsIDOMNSHTMLTextAreaElement,
                              public nsITextControlElement,
                              public nsIDOMNSEditableElement,
                              public nsStubMutationObserver,
                              public nsIConstraintValidation
{
public:
  using nsIConstraintValidation::GetValidationMessage;

  nsHTMLTextAreaElement(already_AddRefed<nsINodeInfo> aNodeInfo,
                        PRUint32 aFromParser = 0);

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
  NS_IMETHOD GetEditor(nsIEditor** aEditor)
  {
    return nsGenericHTMLElement::GetEditor(aEditor);
  }
  NS_IMETHOD SetUserInput(const nsAString& aInput);

  // nsIFormControl
  NS_IMETHOD_(PRUint32) GetType() const { return NS_FORM_TEXTAREA; }
  NS_IMETHOD Reset();
  NS_IMETHOD SubmitNamesValues(nsFormSubmission* aFormSubmission);
  NS_IMETHOD SaveState();
  virtual PRBool RestoreState(nsPresState* aState);

  virtual PRInt32 IntrinsicState() const;

  // nsITextControlElemet
  NS_IMETHOD SetValueChanged(PRBool aValueChanged);
  NS_IMETHOD_(PRBool) IsSingleLineTextControl() const;
  NS_IMETHOD_(PRBool) IsTextArea() const;
  NS_IMETHOD_(PRBool) IsPlainTextControl() const;
  NS_IMETHOD_(PRBool) IsPasswordTextControl() const;
  NS_IMETHOD_(PRInt32) GetCols();
  NS_IMETHOD_(PRInt32) GetWrapCols();
  NS_IMETHOD_(PRInt32) GetRows();
  NS_IMETHOD_(void) GetDefaultValueFromContent(nsAString& aValue);
  NS_IMETHOD_(PRBool) ValueChanged() const;
  NS_IMETHOD_(void) GetTextEditorValue(nsAString& aValue, PRBool aIgnoreWrap) const;
  NS_IMETHOD_(void) SetTextEditorValue(const nsAString& aValue, PRBool aUserInput);
  NS_IMETHOD_(nsIEditor*) GetTextEditor();
  NS_IMETHOD_(nsISelectionController*) GetSelectionController();
  NS_IMETHOD_(nsFrameSelection*) GetConstFrameSelection();
  NS_IMETHOD BindToFrame(nsTextControlFrame* aFrame);
  NS_IMETHOD_(void) UnbindFromFrame(nsTextControlFrame* aFrame);
  NS_IMETHOD CreateEditor();
  NS_IMETHOD_(nsIContent*) GetRootEditorNode();
  NS_IMETHOD_(nsIContent*) GetPlaceholderNode();
  NS_IMETHOD_(void) UpdatePlaceholderText(PRBool aNotify);
  NS_IMETHOD_(void) SetPlaceholderClass(PRBool aVisible, PRBool aNotify);
  NS_IMETHOD_(void) InitializeKeyboardEventListeners();
  NS_IMETHOD_(void) OnValueChanged(PRBool aNotify);

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

  virtual PRBool IsHTMLFocusable(PRBool aWithMouse, PRBool *aIsFocusable, PRInt32 *aTabIndex);

  virtual nsresult DoneAddingChildren(PRBool aHaveNotified);
  virtual PRBool IsDoneAddingChildren();

  virtual nsresult Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const;

  nsresult CopyInnerTo(nsGenericElement* aDest) const;

  /**
   * Called when an attribute is about to be changed
   */
  virtual nsresult BeforeSetAttr(PRInt32 aNameSpaceID, nsIAtom* aName,
                                 const nsAString* aValue, PRBool aNotify);

  // nsIMutationObserver
  NS_DECL_NSIMUTATIONOBSERVER_CHARACTERDATACHANGED
  NS_DECL_NSIMUTATIONOBSERVER_CONTENTAPPENDED
  NS_DECL_NSIMUTATIONOBSERVER_CONTENTINSERTED
  NS_DECL_NSIMUTATIONOBSERVER_CONTENTREMOVED

  virtual void UpdateEditableState()
  {
    return UpdateEditableFormControlState();
  }

  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(nsHTMLTextAreaElement,
                                           nsGenericHTMLFormElement)

  virtual nsXPCClassInfo* GetClassInfo();

  // nsIConstraintValidation
  PRBool   IsTooLong();
  PRBool   IsValueMissing() const;
  void     UpdateTooLongValidityState();
  void     UpdateValueMissingValidityState();
  void     UpdateBarredFromConstraintValidation();
  nsresult GetValidationMessage(nsAString& aValidationMessage,
                                ValidityStateType aType);

protected:
  using nsGenericHTMLFormElement::IsSingleLineTextControl; // get rid of the compiler warning

  nsCOMPtr<nsIControllers> mControllers;
  /** Whether or not the value has changed since its default value was given. */
  PRPackedBool             mValueChanged;
  /** Whether or not we are already handling select event. */
  PRPackedBool             mHandlingSelect;
  /** Whether or not we are done adding children (always PR_TRUE if not
      created by a parser */
  PRPackedBool             mDoneAddingChildren;
  /** Whether state restoration should be inhibited in DoneAddingChildren. */
  PRPackedBool             mInhibitStateRestoration;
  /** Whether our disabled state has changed from the default **/
  PRPackedBool             mDisabledChanged;
  /** The state of the text editor (selection controller and the editor) **/
  nsRefPtr<nsTextEditorState> mState;
  
  NS_IMETHOD SelectAll(nsPresContext* aPresContext);
  /**
   * Get the value, whether it is from the content or the frame.
   * @param aValue the value [out]
   * @param aIgnoreWrap whether to ignore the wrap attribute when getting the
   *        value.  If this is true, linebreaks will not be inserted even if
   *        wrap=hard.
   */
  void GetValueInternal(nsAString& aValue, PRBool aIgnoreWrap) const;

  nsresult SetValueInternal(const nsAString& aValue,
                            PRBool aUserInput);
  nsresult GetSelectionRange(PRInt32* aSelectionStart, PRInt32* aSelectionEnd);

  virtual PRBool AcceptAutofocus() const
  {
    return PR_TRUE;
  }

  /**
   * Common method to call from the various mutation observer methods.
   * aContent is a content node that's either the one that changed or its
   * parent; we should only respond to the change if aContent is non-anonymous.
   */
  void ContentChanged(nsIContent* aContent);

  virtual nsresult AfterSetAttr(PRInt32 aNamespaceID, nsIAtom *aName,
                                const nsAString* aValue, PRBool aNotify);

  /**
   * Get the mutable state of the element.
   */
  PRBool IsMutable() const;
};


NS_IMPL_NS_NEW_HTML_ELEMENT_CHECK_PARSER(TextArea)


nsHTMLTextAreaElement::nsHTMLTextAreaElement(already_AddRefed<nsINodeInfo> aNodeInfo,
                                             PRUint32 aFromParser)
  : nsGenericHTMLFormElement(aNodeInfo),
    mValueChanged(PR_FALSE),
    mHandlingSelect(PR_FALSE),
    mDoneAddingChildren(!aFromParser),
    mInhibitStateRestoration(!!(aFromParser & NS_FROM_PARSER_FRAGMENT)),
    mDisabledChanged(PR_FALSE),
    mState(new nsTextEditorState(this))
{
  AddMutationObserver(this);
}


NS_IMPL_CYCLE_COLLECTION_CLASS(nsHTMLTextAreaElement)
NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(nsHTMLTextAreaElement,
                                                nsGenericHTMLFormElement)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mControllers)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(nsHTMLTextAreaElement,
                                                  nsGenericHTMLFormElement)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mControllers)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NATIVE_MEMBER(mState, nsTextEditorState)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_ADDREF_INHERITED(nsHTMLTextAreaElement, nsGenericElement) 
NS_IMPL_RELEASE_INHERITED(nsHTMLTextAreaElement, nsGenericElement) 


DOMCI_NODE_DATA(HTMLTextAreaElement, nsHTMLTextAreaElement)

// QueryInterface implementation for nsHTMLTextAreaElement
NS_INTERFACE_TABLE_HEAD_CYCLE_COLLECTION_INHERITED(nsHTMLTextAreaElement)
  NS_HTML_CONTENT_INTERFACE_TABLE6(nsHTMLTextAreaElement,
                                   nsIDOMHTMLTextAreaElement,
                                   nsIDOMNSHTMLTextAreaElement,
                                   nsITextControlElement,
                                   nsIDOMNSEditableElement,
                                   nsIMutationObserver,
                                   nsIConstraintValidation)
  NS_HTML_CONTENT_INTERFACE_TABLE_TO_MAP_SEGUE(nsHTMLTextAreaElement,
                                               nsGenericHTMLFormElement)
NS_HTML_CONTENT_INTERFACE_TABLE_TAIL_CLASSINFO(HTMLTextAreaElement)


// nsIDOMHTMLTextAreaElement


NS_IMPL_ELEMENT_CLONE(nsHTMLTextAreaElement)

// nsIConstraintValidation
NS_IMPL_NSICONSTRAINTVALIDATION_EXCEPT_SETCUSTOMVALIDITY(nsHTMLTextAreaElement)


NS_IMETHODIMP
nsHTMLTextAreaElement::GetForm(nsIDOMHTMLFormElement** aForm)
{
  return nsGenericHTMLFormElement::GetForm(aForm);
}


// nsIContent

NS_IMETHODIMP
nsHTMLTextAreaElement::Blur()
{
  return nsGenericHTMLElement::Blur();
}

NS_IMETHODIMP
nsHTMLTextAreaElement::Focus()
{
  return nsGenericHTMLElement::Focus();
}

NS_IMETHODIMP
nsHTMLTextAreaElement::Select()
{
  // XXX Bug?  We have to give the input focus before contents can be
  // selected

  FocusTristate state = FocusState();
  if (state == eUnfocusable) {
    return NS_OK;
  }

  nsIFocusManager* fm = nsFocusManager::GetFocusManager();

  nsRefPtr<nsPresContext> presContext = GetPresContext();
  if (state == eInactiveWindow) {
    if (fm)
      fm->SetFocus(this, nsIFocusManager::FLAG_NOSCROLL);
    SelectAll(presContext);
    return NS_OK;
  }

  nsEventStatus status = nsEventStatus_eIgnore;
  nsGUIEvent event(PR_TRUE, NS_FORM_SELECTED, nsnull);
  // XXXbz nsHTMLInputElement guards against this reentering; shouldn't we?
  nsEventDispatcher::Dispatch(static_cast<nsIContent*>(this), presContext,
                              &event, nsnull, &status);

  // If the DOM event was not canceled (e.g. by a JS event handler
  // returning false)
  if (status == nsEventStatus_eIgnore) {
    if (fm) {
      fm->SetFocus(this, nsIFocusManager::FLAG_NOSCROLL);

      // ensure that the element is actually focused
      nsCOMPtr<nsIDOMElement> focusedElement;
      fm->GetFocusedElement(getter_AddRefs(focusedElement));
      if (SameCOMIdentity(static_cast<nsIDOMNode*>(this), focusedElement)) {
        // Now Select all the text!
        SelectAll(presContext);
      }
    }
  }

  return NS_OK;
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

PRBool
nsHTMLTextAreaElement::IsHTMLFocusable(PRBool aWithMouse,
                                       PRBool *aIsFocusable, PRInt32 *aTabIndex)
{
  if (nsGenericHTMLElement::IsHTMLFocusable(aWithMouse, aIsFocusable, aTabIndex)) {
    return PR_TRUE;
  }

  // disabled textareas are not focusable
  *aIsFocusable = !HasAttr(kNameSpaceID_None, nsGkAtoms::disabled);
  return PR_FALSE;
}

NS_IMPL_STRING_ATTR(nsHTMLTextAreaElement, AccessKey, accesskey)
NS_IMPL_BOOL_ATTR(nsHTMLTextAreaElement, Autofocus, autofocus)
NS_IMPL_INT_ATTR(nsHTMLTextAreaElement, Cols, cols)
NS_IMPL_BOOL_ATTR(nsHTMLTextAreaElement, Disabled, disabled)
NS_IMPL_NON_NEGATIVE_INT_ATTR(nsHTMLTextAreaElement, MaxLength, maxlength)
NS_IMPL_STRING_ATTR(nsHTMLTextAreaElement, Name, name)
NS_IMPL_BOOL_ATTR(nsHTMLTextAreaElement, ReadOnly, readonly)
NS_IMPL_BOOL_ATTR(nsHTMLTextAreaElement, Required, required)
NS_IMPL_INT_ATTR(nsHTMLTextAreaElement, Rows, rows)
NS_IMPL_INT_ATTR_DEFAULT_VALUE(nsHTMLTextAreaElement, TabIndex, tabindex, 0)
NS_IMPL_STRING_ATTR(nsHTMLTextAreaElement, Wrap, wrap)
NS_IMPL_STRING_ATTR(nsHTMLTextAreaElement, Placeholder, placeholder)
  

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
nsHTMLTextAreaElement::GetValueInternal(nsAString& aValue, PRBool aIgnoreWrap) const
{
  mState->GetValue(aValue, aIgnoreWrap);
}

NS_IMETHODIMP_(nsIEditor*)
nsHTMLTextAreaElement::GetTextEditor()
{
  return mState->GetEditor();
}

NS_IMETHODIMP_(nsISelectionController*)
nsHTMLTextAreaElement::GetSelectionController()
{
  return mState->GetSelectionController();
}

NS_IMETHODIMP_(nsFrameSelection*)
nsHTMLTextAreaElement::GetConstFrameSelection()
{
  return mState->GetConstFrameSelection();
}

NS_IMETHODIMP
nsHTMLTextAreaElement::BindToFrame(nsTextControlFrame* aFrame)
{
  return mState->BindToFrame(aFrame);
}

NS_IMETHODIMP_(void)
nsHTMLTextAreaElement::UnbindFromFrame(nsTextControlFrame* aFrame)
{
  if (aFrame) {
    mState->UnbindFromFrame(aFrame);
  }
}

NS_IMETHODIMP
nsHTMLTextAreaElement::CreateEditor()
{
  return mState->PrepareEditor();
}

NS_IMETHODIMP_(nsIContent*)
nsHTMLTextAreaElement::GetRootEditorNode()
{
  return mState->GetRootNode();
}

NS_IMETHODIMP_(nsIContent*)
nsHTMLTextAreaElement::GetPlaceholderNode()
{
  return mState->GetPlaceholderNode();
}

NS_IMETHODIMP_(void)
nsHTMLTextAreaElement::UpdatePlaceholderText(PRBool aNotify)
{
  mState->UpdatePlaceholderText(aNotify);
}

NS_IMETHODIMP_(void)
nsHTMLTextAreaElement::SetPlaceholderClass(PRBool aVisible, PRBool aNotify)
{
  mState->SetPlaceholderClass(aVisible, aNotify);
}

nsresult
nsHTMLTextAreaElement::SetValueInternal(const nsAString& aValue,
                                        PRBool aUserInput)
{
  // Need to set the value changed flag here, so that
  // nsTextControlFrame::UpdateValueDisplay retrieves the correct value
  // if needed.
  SetValueChanged(PR_TRUE);
  mState->SetValue(aValue, aUserInput);

  return NS_OK;
}

NS_IMETHODIMP 
nsHTMLTextAreaElement::SetValue(const nsAString& aValue)
{
  return SetValueInternal(aValue, PR_FALSE);
}

NS_IMETHODIMP 
nsHTMLTextAreaElement::SetUserInput(const nsAString& aValue)
{
  if (!nsContentUtils::IsCallerTrustedForWrite()) {
    return NS_ERROR_DOM_SECURITY_ERR;
  }
  SetValueInternal(aValue, PR_TRUE);
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLTextAreaElement::SetValueChanged(PRBool aValueChanged)
{
  mValueChanged = aValueChanged;
  if (!aValueChanged && !mState->IsEmpty()) {
    mState->EmptyValue();
  }

  if (HasAttr(kNameSpaceID_None, nsGkAtoms::placeholder)) {
    nsIDocument* doc = GetCurrentDoc();
    if (doc) {
      mozAutoDocUpdate upd(doc, UPDATE_CONTENT_STATE, PR_TRUE);
      doc->ContentStatesChanged(this, nsnull, NS_EVENT_STATE_MOZ_PLACEHOLDER);
    }
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
    if (aAttribute == nsGkAtoms::maxlength) {
      return aResult.ParseNonNegativeIntValue(aValue);
    }
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
  } else if (aAttribute == nsGkAtoms::wrap) {
    NS_UpdateHint(retval, nsChangeHint_ReconstructFrame);
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
      (formFrame = do_QueryFrame(formControlFrame))) {
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
      static_cast<nsMouseEvent*>(aVisitor.mEvent)->button ==
        nsMouseEvent::eMiddleButton) {
    aVisitor.mEvent->flags &= ~NS_EVENT_FLAG_NO_CONTENT_DISPATCH;
  }

  // Fire onchange (if necessary), before we do the blur, bug 370521.
  if (aVisitor.mEvent->message == NS_BLUR_CONTENT) {
    nsIFrame* primaryFrame = GetPrimaryFrame();
    if (primaryFrame) {
      nsITextControlFrame* textFrame = do_QueryFrame(primaryFrame);
      if (textFrame) {
        textFrame->CheckFireOnChange();
      }
    }
  }

  return nsGenericHTMLFormElement::PreHandleEvent(aVisitor);
}

nsresult
nsHTMLTextAreaElement::PostHandleEvent(nsEventChainPostVisitor& aVisitor)
{
  if (aVisitor.mEvent->message == NS_FORM_SELECTED) {
    mHandlingSelect = PR_FALSE;
  }

  if (HasAttr(kNameSpaceID_None, nsGkAtoms::placeholder) &&
      // TODO: checking if the value is empty could be a good idea but we do not
      // have a simple way to do that, see bug 585100
      (aVisitor.mEvent->message == NS_FOCUS_CONTENT ||
       aVisitor.mEvent->message == NS_BLUR_CONTENT)) {
    nsIDocument* doc = GetCurrentDoc();
    if (doc) {
      MOZ_AUTO_DOC_UPDATE(doc, UPDATE_CONTENT_STATE, PR_TRUE);
      doc->ContentStatesChanged(this, nsnull, NS_EVENT_STATE_MOZ_PLACEHOLDER);
    }
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
    if (!mInhibitStateRestoration) {
      RestoreFormControlState(this, this);
    }
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
    nsITextControlFrame* textControlFrame = do_QueryFrame(formControlFrame);
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
    nsITextControlFrame* textControlFrame = do_QueryFrame(formControlFrame);
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
    nsITextControlFrame* textControlFrame = do_QueryFrame(formControlFrame);
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
    nsITextControlFrame* textControlFrame = do_QueryFrame(formControlFrame);
    if (textControlFrame)
      rv = textControlFrame->SetSelectionRange(aSelectionStart, aSelectionEnd);
  }

  return rv;
} 

nsresult
nsHTMLTextAreaElement::Reset()
{
  nsresult rv;

  // To get the initial spellchecking, reset value to
  // empty string before setting the default value.
  SetValue(EmptyString());
  nsAutoString resetVal;
  GetDefaultValue(resetVal);
  rv = SetValue(resetVal);
  NS_ENSURE_SUCCESS(rv, rv);

  SetValueChanged(PR_FALSE);
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLTextAreaElement::SubmitNamesValues(nsFormSubmission* aFormSubmission)
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
  rv = aFormSubmission->AddNameValuePair(name, value);

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

      nsCOMPtr<nsISupportsString> pState
        (do_CreateInstance(NS_SUPPORTS_STRING_CONTRACTID));
      if (!pState) {
        return NS_ERROR_OUT_OF_MEMORY;
      }
      pState->SetData(value);
      state->SetStateProperty(pState);
    }
  }

  if (mDisabledChanged) {
    if (!state) {
      rv = GetPrimaryPresState(this, &state);
    }
    if (state) {
      PRBool disabled;
      GetDisabled(&disabled);
      state->SetDisabled(disabled);
    }
  }
  return rv;
}

PRBool
nsHTMLTextAreaElement::RestoreState(nsPresState* aState)
{
  nsCOMPtr<nsISupportsString> state
    (do_QueryInterface(aState->GetStateProperty()));
  
  if (state) {
    nsAutoString data;
    state->GetData(data);
    SetValue(data);
  }

  if (aState->IsDisabledSet()) {
    SetDisabled(aState->GetDisabled());
  }

  return PR_FALSE;
}

PRInt32
nsHTMLTextAreaElement::IntrinsicState() const
{
  PRInt32 state = nsGenericHTMLFormElement::IntrinsicState();

  if (HasAttr(kNameSpaceID_None, nsGkAtoms::required)) {
    state |= NS_EVENT_STATE_REQUIRED;
  } else {
    state |= NS_EVENT_STATE_OPTIONAL;
  }

  if (IsCandidateForConstraintValidation()) {
    state |= IsValid() ? NS_EVENT_STATE_VALID : NS_EVENT_STATE_INVALID;
  }

  if (HasAttr(kNameSpaceID_None, nsGkAtoms::placeholder) &&
      !nsContentUtils::IsFocusedContent((nsIContent*)(this))) {
    nsAutoString value;
    GetValueInternal(value, PR_TRUE);
    if (value.IsEmpty()) {
      state |= NS_EVENT_STATE_MOZ_PLACEHOLDER;
    }
  }

  return state;
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
                                       nsIContent* aFirstNewContent,
                                       PRInt32 /* unused */)
{
  ContentChanged(aFirstNewContent);
}

void
nsHTMLTextAreaElement::ContentInserted(nsIDocument* aDocument,
                                       nsIContent* aContainer,
                                       nsIContent* aChild,
                                       PRInt32 /* unused */)
{
  ContentChanged(aChild);
}

void
nsHTMLTextAreaElement::ContentRemoved(nsIDocument* aDocument,
                                      nsIContent* aContainer,
                                      nsIContent* aChild,
                                      PRInt32 aIndexInContainer,
                                      nsIContent* aPreviousSibling)
{
  ContentChanged(aChild);
}

void
nsHTMLTextAreaElement::ContentChanged(nsIContent* aContent)
{
  if (!mValueChanged && mDoneAddingChildren &&
      nsContentUtils::IsInSameAnonymousTree(this, aContent)) {
    // Hard to say what the reset can trigger, so be safe pending
    // further auditing.
    nsCOMPtr<nsIMutationObserver> kungFuDeathGrip(this);
    Reset();
  }
}

nsresult
nsHTMLTextAreaElement::AfterSetAttr(PRInt32 aNameSpaceID, nsIAtom* aName,
                                    const nsAString* aValue, PRBool aNotify)
{
  PRInt32 states = 0;

  if (aNameSpaceID == kNameSpaceID_None) {
    if (aName == nsGkAtoms::required || aName == nsGkAtoms::disabled ||
        aName == nsGkAtoms::readonly) {
      UpdateValueMissingValidityState();

      // This *has* to be called *after* validity has changed.
      if (aName == nsGkAtoms::readonly || aName == nsGkAtoms::disabled) {
        UpdateBarredFromConstraintValidation();
      }

      states |= NS_EVENT_STATE_VALID | NS_EVENT_STATE_INVALID |
                NS_EVENT_STATE_MOZ_SUBMITINVALID;
    } else if (aName == nsGkAtoms::maxlength) {
      UpdateTooLongValidityState();
      states |= NS_EVENT_STATE_VALID | NS_EVENT_STATE_INVALID;
    }

    if (aNotify) {
      nsIDocument* doc = GetCurrentDoc();

      if (aName == nsGkAtoms::readonly) {
        UpdateEditableState();
        states |= NS_EVENT_STATE_MOZ_READONLY | NS_EVENT_STATE_MOZ_READWRITE;
      }

      if (doc && states) {
        MOZ_AUTO_DOC_UPDATE(doc, UPDATE_CONTENT_STATE, PR_TRUE);
        doc->ContentStatesChanged(this, nsnull, states);
      }
    }
  }

  return nsGenericHTMLFormElement::AfterSetAttr(aNameSpaceID, aName, aValue,
                                                aNotify);
}

nsresult
nsHTMLTextAreaElement::CopyInnerTo(nsGenericElement* aDest) const
{
  nsresult rv = nsGenericHTMLFormElement::CopyInnerTo(aDest);
  NS_ENSURE_SUCCESS(rv, rv);

  if (aDest->GetOwnerDoc()->IsStaticDocument()) {
    nsAutoString value;
    const_cast<nsHTMLTextAreaElement*>(this)->GetValue(value);
    static_cast<nsHTMLTextAreaElement*>(aDest)->SetValue(value);
  }
  return NS_OK;
}

PRBool
nsHTMLTextAreaElement::IsMutable() const
{
  return (!HasAttr(kNameSpaceID_None, nsGkAtoms::readonly) &&
          !HasAttr(kNameSpaceID_None, nsGkAtoms::disabled));
}

// nsIConstraintValidation

NS_IMETHODIMP
nsHTMLTextAreaElement::SetCustomValidity(const nsAString& aError)
{
  nsIConstraintValidation::SetCustomValidity(aError);

  nsIDocument* doc = GetCurrentDoc();
  if (doc) {
    MOZ_AUTO_DOC_UPDATE(doc, UPDATE_CONTENT_STATE, PR_TRUE);
    doc->ContentStatesChanged(this, nsnull, NS_EVENT_STATE_INVALID |
                                            NS_EVENT_STATE_VALID);
  }

  return NS_OK;
}

PRBool
nsHTMLTextAreaElement::IsTooLong()
{
  if (!mValueChanged) {
    return PR_FALSE;
  }

  PRInt32 maxLength = -1;
  PRInt32 textLength = -1;

  GetMaxLength(&maxLength);
  GetTextLength(&textLength);

  return maxLength >= 0 && textLength > maxLength;
}

PRBool
nsHTMLTextAreaElement::IsValueMissing() const
{
  if (!HasAttr(kNameSpaceID_None, nsGkAtoms::required) || !IsMutable()) {
    return PR_FALSE;
  }

  nsAutoString value;
  GetValueInternal(value, PR_TRUE);

  return value.IsEmpty();
}

void
nsHTMLTextAreaElement::UpdateTooLongValidityState()
{
  SetValidityState(VALIDITY_STATE_TOO_LONG, IsTooLong());
}

void
nsHTMLTextAreaElement::UpdateValueMissingValidityState()
{
  SetValidityState(VALIDITY_STATE_VALUE_MISSING, IsValueMissing());
}

void
nsHTMLTextAreaElement::UpdateBarredFromConstraintValidation()
{
  SetBarredFromConstraintValidation(HasAttr(kNameSpaceID_None,
                                            nsGkAtoms::readonly) ||
                                    HasAttr(kNameSpaceID_None,
                                            nsGkAtoms::disabled));
}

nsresult
nsHTMLTextAreaElement::GetValidationMessage(nsAString& aValidationMessage,
                                            ValidityStateType aType)
{
  nsresult rv = NS_OK;

  switch (aType)
  {
    case VALIDITY_STATE_TOO_LONG:
      {
        nsXPIDLString message;
        PRInt32 maxLength = -1;
        PRInt32 textLength = -1;
        nsAutoString strMaxLength;
        nsAutoString strTextLength;

        GetMaxLength(&maxLength);
        GetTextLength(&textLength);

        strMaxLength.AppendInt(maxLength);
        strTextLength.AppendInt(textLength);

        const PRUnichar* params[] = { strTextLength.get(), strMaxLength.get() };
        rv = nsContentUtils::FormatLocalizedString(nsContentUtils::eDOM_PROPERTIES,
                                                   "ElementSuffersFromBeingTooLong",
                                                   params, 2, message);
        aValidationMessage = message;
      }
      break;
    case VALIDITY_STATE_VALUE_MISSING:
      {
        nsXPIDLString message;
        rv = nsContentUtils::GetLocalizedString(nsContentUtils::eDOM_PROPERTIES,
                                                "TextElementSuffersFromBeingMissing",
                                                message);
        aValidationMessage = message;
      }
      break;
    default:
      rv = nsIConstraintValidation::GetValidationMessage(aValidationMessage, aType);
  }

  return rv;
}

NS_IMETHODIMP_(PRBool)
nsHTMLTextAreaElement::IsSingleLineTextControl() const
{
  return PR_FALSE;
}

NS_IMETHODIMP_(PRBool)
nsHTMLTextAreaElement::IsTextArea() const
{
  return PR_TRUE;
}

NS_IMETHODIMP_(PRBool)
nsHTMLTextAreaElement::IsPlainTextControl() const
{
  // need to check our HTML attribute and/or CSS.
  return PR_TRUE;
}

NS_IMETHODIMP_(PRBool)
nsHTMLTextAreaElement::IsPasswordTextControl() const
{
  return PR_FALSE;
}

NS_IMETHODIMP_(PRInt32)
nsHTMLTextAreaElement::GetCols()
{
  const nsAttrValue* attr = GetParsedAttr(nsGkAtoms::cols);
  if (attr) {
    PRInt32 cols = attr->Type() == nsAttrValue::eInteger ?
                   attr->GetIntegerValue() : 0;
    // XXX why a default of 1 char, why hide it
    return (cols <= 0) ? 1 : cols;
  }

  return DEFAULT_COLS;
}

NS_IMETHODIMP_(PRInt32)
nsHTMLTextAreaElement::GetWrapCols()
{
  // wrap=off means -1 for wrap width no matter what cols is
  nsHTMLTextWrap wrapProp;
  nsITextControlElement::GetWrapPropertyEnum(this, wrapProp);
  if (wrapProp == nsITextControlElement::eHTMLTextWrap_Off) {
    // do not wrap when wrap=off
    return -1;
  }

  // Otherwise we just wrap at the given number of columns
  return GetCols();
}


NS_IMETHODIMP_(PRInt32)
nsHTMLTextAreaElement::GetRows()
{
  const nsAttrValue* attr = GetParsedAttr(nsGkAtoms::rows);
  if (attr && attr->Type() == nsAttrValue::eInteger) {
    PRInt32 rows = attr->GetIntegerValue();
    return (rows <= 0) ? DEFAULT_ROWS_TEXTAREA : rows;
  }

  return DEFAULT_ROWS_TEXTAREA;
}

NS_IMETHODIMP_(void)
nsHTMLTextAreaElement::GetDefaultValueFromContent(nsAString& aValue)
{
  GetDefaultValue(aValue);
}

NS_IMETHODIMP_(PRBool)
nsHTMLTextAreaElement::ValueChanged() const
{
  return mValueChanged;
}

NS_IMETHODIMP_(void)
nsHTMLTextAreaElement::GetTextEditorValue(nsAString& aValue,
                                          PRBool aIgnoreWrap) const
{
  mState->GetValue(aValue, aIgnoreWrap);
}

NS_IMETHODIMP_(void)
nsHTMLTextAreaElement::SetTextEditorValue(const nsAString& aValue,
                                          PRBool aUserInput)
{
  mState->SetValue(aValue, aUserInput);
}

NS_IMETHODIMP_(void)
nsHTMLTextAreaElement::InitializeKeyboardEventListeners()
{
  mState->InitializeKeyboardEventListeners();
}

NS_IMETHODIMP_(void)
nsHTMLTextAreaElement::OnValueChanged(PRBool aNotify)
{
  // Update the validity state
  UpdateTooLongValidityState();
  UpdateValueMissingValidityState();

  if (aNotify) {
    nsIDocument* doc = GetCurrentDoc();
    if (doc) {
      MOZ_AUTO_DOC_UPDATE(doc, UPDATE_CONTENT_STATE, PR_TRUE);
      doc->ContentStatesChanged(this, nsnull, NS_EVENT_STATE_VALID |
                                              NS_EVENT_STATE_INVALID |
                                              // We could check if that is
                                              // really needed but considering
                                              // we are already updating the
                                              // state for valid/invalid...
                                              NS_EVENT_STATE_MOZ_PLACEHOLDER);
    }
  }
}

