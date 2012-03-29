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

#include "mozilla/Util.h"

#include "nsIDOMHTMLTextAreaElement.h"
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
#include "nsEventStates.h"
#include "nsLinebreakConverter.h"
#include "nsIDocument.h"
#include "nsIFrame.h"
#include "nsIPrivateDOMEvent.h"
#include "nsGUIEvent.h"
#include "nsPresState.h"
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
#include "nsHTMLFormElement.h"

#include "nsTextEditorState.h"

using namespace mozilla;
using namespace mozilla::dom;

static NS_DEFINE_CID(kXULControllersCID,  NS_XULCONTROLLERS_CID);

#define NS_NO_CONTENT_DISPATCH (1 << 0)

class nsHTMLTextAreaElement : public nsGenericHTMLFormElement,
                              public nsIDOMHTMLTextAreaElement,
                              public nsITextControlElement,
                              public nsIDOMNSEditableElement,
                              public nsStubMutationObserver,
                              public nsIConstraintValidation
{
public:
  using nsIConstraintValidation::GetValidationMessage;

  nsHTMLTextAreaElement(already_AddRefed<nsINodeInfo> aNodeInfo,
                        mozilla::dom::FromParser aFromParser = mozilla::dom::NOT_FROM_PARSER);

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  // nsIDOMNode
  NS_FORWARD_NSIDOMNODE(nsGenericHTMLFormElement::)

  // nsIDOMElement
  NS_FORWARD_NSIDOMELEMENT(nsGenericHTMLFormElement::)

  // nsIDOMHTMLElement
  NS_FORWARD_NSIDOMHTMLELEMENT_BASIC(nsGenericHTMLFormElement::)
  NS_SCRIPTABLE NS_IMETHOD Click() {
    return nsGenericHTMLFormElement::Click();
  }
  NS_SCRIPTABLE NS_IMETHOD GetTabIndex(PRInt32* aTabIndex);
  NS_SCRIPTABLE NS_IMETHOD SetTabIndex(PRInt32 aTabIndex);
  NS_SCRIPTABLE NS_IMETHOD Focus() {
    return nsGenericHTMLFormElement::Focus();
  }
  NS_SCRIPTABLE NS_IMETHOD GetDraggable(bool* aDraggable) {
    return nsGenericHTMLFormElement::GetDraggable(aDraggable);
  }
  NS_SCRIPTABLE NS_IMETHOD GetInnerHTML(nsAString& aInnerHTML) {
    return nsGenericHTMLFormElement::GetInnerHTML(aInnerHTML);
  }
  NS_SCRIPTABLE NS_IMETHOD SetInnerHTML(const nsAString& aInnerHTML) {
    return nsGenericHTMLFormElement::SetInnerHTML(aInnerHTML);
  }

  // nsIDOMHTMLTextAreaElement
  NS_DECL_NSIDOMHTMLTEXTAREAELEMENT

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
  virtual bool RestoreState(nsPresState* aState);

  virtual void FieldSetDisabledChanged(bool aNotify);

  virtual nsEventStates IntrinsicState() const;

  // nsITextControlElemet
  NS_IMETHOD SetValueChanged(bool aValueChanged);
  NS_IMETHOD_(bool) IsSingleLineTextControl() const;
  NS_IMETHOD_(bool) IsTextArea() const;
  NS_IMETHOD_(bool) IsPlainTextControl() const;
  NS_IMETHOD_(bool) IsPasswordTextControl() const;
  NS_IMETHOD_(PRInt32) GetCols();
  NS_IMETHOD_(PRInt32) GetWrapCols();
  NS_IMETHOD_(PRInt32) GetRows();
  NS_IMETHOD_(void) GetDefaultValueFromContent(nsAString& aValue);
  NS_IMETHOD_(bool) ValueChanged() const;
  NS_IMETHOD_(void) GetTextEditorValue(nsAString& aValue, bool aIgnoreWrap) const;
  NS_IMETHOD_(void) SetTextEditorValue(const nsAString& aValue, bool aUserInput);
  NS_IMETHOD_(nsIEditor*) GetTextEditor();
  NS_IMETHOD_(nsISelectionController*) GetSelectionController();
  NS_IMETHOD_(nsFrameSelection*) GetConstFrameSelection();
  NS_IMETHOD BindToFrame(nsTextControlFrame* aFrame);
  NS_IMETHOD_(void) UnbindFromFrame(nsTextControlFrame* aFrame);
  NS_IMETHOD CreateEditor();
  NS_IMETHOD_(nsIContent*) GetRootEditorNode();
  NS_IMETHOD_(nsIContent*) CreatePlaceholderNode();
  NS_IMETHOD_(nsIContent*) GetPlaceholderNode();
  NS_IMETHOD_(void) UpdatePlaceholderText(bool aNotify);
  NS_IMETHOD_(void) SetPlaceholderClass(bool aVisible, bool aNotify);
  NS_IMETHOD_(void) InitializeKeyboardEventListeners();
  NS_IMETHOD_(void) OnValueChanged(bool aNotify);
  NS_IMETHOD_(bool) HasCachedSelection();

  // nsIContent
  virtual nsresult BindToTree(nsIDocument* aDocument, nsIContent* aParent,
                               nsIContent* aBindingParent,
                               bool aCompileEventHandlers);
  virtual void UnbindFromTree(bool aDeep = true,
                              bool aNullParent = true);
  virtual bool ParseAttribute(PRInt32 aNamespaceID,
                                nsIAtom* aAttribute,
                                const nsAString& aValue,
                                nsAttrValue& aResult);
  virtual nsMapRuleToAttributesFunc GetAttributeMappingFunction() const;
  virtual nsChangeHint GetAttributeChangeHint(const nsIAtom* aAttribute,
                                              PRInt32 aModType) const;
  NS_IMETHOD_(bool) IsAttributeMapped(const nsIAtom* aAttribute) const;

  virtual nsresult PreHandleEvent(nsEventChainPreVisitor& aVisitor);
  virtual nsresult PostHandleEvent(nsEventChainPostVisitor& aVisitor);

  virtual bool IsHTMLFocusable(bool aWithMouse, bool *aIsFocusable, PRInt32 *aTabIndex);

  virtual void DoneAddingChildren(bool aHaveNotified);
  virtual bool IsDoneAddingChildren();

  virtual nsresult Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const;

  nsresult CopyInnerTo(nsGenericElement* aDest) const;

  /**
   * Called when an attribute is about to be changed
   */
  virtual nsresult BeforeSetAttr(PRInt32 aNameSpaceID, nsIAtom* aName,
                                 const nsAttrValueOrString* aValue,
                                 bool aNotify);

  // nsIMutationObserver
  NS_DECL_NSIMUTATIONOBSERVER_CHARACTERDATACHANGED
  NS_DECL_NSIMUTATIONOBSERVER_CONTENTAPPENDED
  NS_DECL_NSIMUTATIONOBSERVER_CONTENTINSERTED
  NS_DECL_NSIMUTATIONOBSERVER_CONTENTREMOVED

  virtual void UpdateEditableState(bool aNotify)
  {
    return UpdateEditableFormControlState(aNotify);
  }

  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(nsHTMLTextAreaElement,
                                           nsGenericHTMLFormElement)

  virtual nsXPCClassInfo* GetClassInfo();

  // nsIConstraintValidation
  bool     IsTooLong();
  bool     IsValueMissing() const;
  void     UpdateTooLongValidityState();
  void     UpdateValueMissingValidityState();
  void     UpdateBarredFromConstraintValidation();
  nsresult GetValidationMessage(nsAString& aValidationMessage,
                                ValidityStateType aType);

protected:
  using nsGenericHTMLFormElement::IsSingleLineTextControl; // get rid of the compiler warning

  nsCOMPtr<nsIControllers> mControllers;
  /** Whether or not the value has changed since its default value was given. */
  bool                     mValueChanged;
  /** Whether or not we are already handling select event. */
  bool                     mHandlingSelect;
  /** Whether or not we are done adding children (always true if not
      created by a parser */
  bool                     mDoneAddingChildren;
  /** Whether state restoration should be inhibited in DoneAddingChildren. */
  bool                     mInhibitStateRestoration;
  /** Whether our disabled state has changed from the default **/
  bool                     mDisabledChanged;
  /** Whether we should make :-moz-ui-invalid apply on the element. **/
  bool                     mCanShowInvalidUI;
  /** Whether we should make :-moz-ui-valid apply on the element. **/
  bool                     mCanShowValidUI;

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
  void GetValueInternal(nsAString& aValue, bool aIgnoreWrap) const;

  nsresult SetValueInternal(const nsAString& aValue,
                            bool aUserInput);
  nsresult GetSelectionRange(PRInt32* aSelectionStart, PRInt32* aSelectionEnd);

  /**
   * Common method to call from the various mutation observer methods.
   * aContent is a content node that's either the one that changed or its
   * parent; we should only respond to the change if aContent is non-anonymous.
   */
  void ContentChanged(nsIContent* aContent);

  virtual nsresult AfterSetAttr(PRInt32 aNamespaceID, nsIAtom *aName,
                                const nsAttrValue* aValue, bool aNotify);

  /**
   * Return if an element should have a specific validity UI
   * (with :-moz-ui-invalid and :-moz-ui-valid pseudo-classes).
   *
   * @return Whether the elemnet should have a validity UI.
   */
  bool ShouldShowValidityUI() const {
    /**
     * Always show the validity UI if the form has already tried to be submitted
     * but was invalid.
     *
     * Otherwise, show the validity UI if the element's value has been changed.
     */

    if (mForm && mForm->HasEverTriedInvalidSubmit()) {
      return true;
    }

    return mValueChanged;
  }

  /**
   * Get the mutable state of the element.
   */
  bool IsMutable() const;

  /**
   * Returns whether the current value is the empty string.
   *
   * @return whether the current value is the empty string.
   */
  bool IsValueEmpty() const;
};


NS_IMPL_NS_NEW_HTML_ELEMENT_CHECK_PARSER(TextArea)


nsHTMLTextAreaElement::nsHTMLTextAreaElement(already_AddRefed<nsINodeInfo> aNodeInfo,
                                             FromParser aFromParser)
  : nsGenericHTMLFormElement(aNodeInfo),
    mValueChanged(false),
    mHandlingSelect(false),
    mDoneAddingChildren(!aFromParser),
    mInhibitStateRestoration(!!(aFromParser & FROM_PARSER_FRAGMENT)),
    mDisabledChanged(false),
    mCanShowInvalidUI(true),
    mCanShowValidUI(true),
    mState(new nsTextEditorState(this))
{
  AddMutationObserver(this);

  // Set up our default state.  By default we're enabled (since we're
  // a control type that can be disabled but not actually disabled
  // right now), optional, and valid.  We are NOT readwrite by default
  // until someone calls UpdateEditableState on us, apparently!  Also
  // by default we don't have to show validity UI and so forth.
  AddStatesSilently(NS_EVENT_STATE_ENABLED |
                    NS_EVENT_STATE_OPTIONAL |
                    NS_EVENT_STATE_VALID);
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
  NS_HTML_CONTENT_INTERFACE_TABLE5(nsHTMLTextAreaElement,
                                   nsIDOMHTMLTextAreaElement,
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
  nsGUIEvent event(true, NS_FORM_SELECTED, nsnull);
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
  nsIFormControlFrame* formControlFrame = GetFormControlFrame(true);

  if (formControlFrame) {
    formControlFrame->SetFormProperty(nsGkAtoms::select, EmptyString());
  }

  return NS_OK;
}

bool
nsHTMLTextAreaElement::IsHTMLFocusable(bool aWithMouse,
                                       bool *aIsFocusable, PRInt32 *aTabIndex)
{
  if (nsGenericHTMLFormElement::IsHTMLFocusable(aWithMouse, aIsFocusable, aTabIndex)) {
    return true;
  }

  // disabled textareas are not focusable
  *aIsFocusable = !IsDisabled();
  return false;
}

NS_IMPL_BOOL_ATTR(nsHTMLTextAreaElement, Autofocus, autofocus)
NS_IMPL_UINT_ATTR_NON_ZERO_DEFAULT_VALUE(nsHTMLTextAreaElement, Cols, cols, DEFAULT_COLS)
NS_IMPL_BOOL_ATTR(nsHTMLTextAreaElement, Disabled, disabled)
NS_IMPL_NON_NEGATIVE_INT_ATTR(nsHTMLTextAreaElement, MaxLength, maxlength)
NS_IMPL_STRING_ATTR(nsHTMLTextAreaElement, Name, name)
NS_IMPL_BOOL_ATTR(nsHTMLTextAreaElement, ReadOnly, readonly)
NS_IMPL_BOOL_ATTR(nsHTMLTextAreaElement, Required, required)
NS_IMPL_UINT_ATTR_NON_ZERO_DEFAULT_VALUE(nsHTMLTextAreaElement, Rows, rows, DEFAULT_ROWS_TEXTAREA)
NS_IMPL_INT_ATTR(nsHTMLTextAreaElement, TabIndex, tabindex)
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
  GetValueInternal(aValue, true);
  return NS_OK;
}

void
nsHTMLTextAreaElement::GetValueInternal(nsAString& aValue, bool aIgnoreWrap) const
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
nsHTMLTextAreaElement::CreatePlaceholderNode()
{
  NS_ENSURE_SUCCESS(mState->CreatePlaceholderNode(), nsnull);
  return mState->GetPlaceholderNode();
}

NS_IMETHODIMP_(nsIContent*)
nsHTMLTextAreaElement::GetPlaceholderNode()
{
  return mState->GetPlaceholderNode();
}

NS_IMETHODIMP_(void)
nsHTMLTextAreaElement::UpdatePlaceholderText(bool aNotify)
{
  mState->UpdatePlaceholderText(aNotify);
}

NS_IMETHODIMP_(void)
nsHTMLTextAreaElement::SetPlaceholderClass(bool aVisible, bool aNotify)
{
  mState->SetPlaceholderClass(aVisible, aNotify);
}

nsresult
nsHTMLTextAreaElement::SetValueInternal(const nsAString& aValue,
                                        bool aUserInput)
{
  // Need to set the value changed flag here, so that
  // nsTextControlFrame::UpdateValueDisplay retrieves the correct value
  // if needed.
  SetValueChanged(true);
  mState->SetValue(aValue, aUserInput);

  return NS_OK;
}

NS_IMETHODIMP 
nsHTMLTextAreaElement::SetValue(const nsAString& aValue)
{
  return SetValueInternal(aValue, false);
}

NS_IMETHODIMP 
nsHTMLTextAreaElement::SetUserInput(const nsAString& aValue)
{
  if (!nsContentUtils::IsCallerTrustedForWrite()) {
    return NS_ERROR_DOM_SECURITY_ERR;
  }
  SetValueInternal(aValue, true);
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLTextAreaElement::SetValueChanged(bool aValueChanged)
{
  bool previousValue = mValueChanged;

  mValueChanged = aValueChanged;
  if (!aValueChanged && !mState->IsEmpty()) {
    mState->EmptyValue();
  }

  if (mValueChanged != previousValue) {
    UpdateState(true);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsHTMLTextAreaElement::GetDefaultValue(nsAString& aDefaultValue)
{
  nsContentUtils::GetNodeTextContent(this, false, aDefaultValue);
  return NS_OK;
}  

NS_IMETHODIMP
nsHTMLTextAreaElement::SetDefaultValue(const nsAString& aDefaultValue)
{
  nsresult rv = nsContentUtils::SetNodeTextContent(this, aDefaultValue, true);
  if (NS_SUCCEEDED(rv) && !mValueChanged) {
    Reset();
  }
  return rv;
}

bool
nsHTMLTextAreaElement::ParseAttribute(PRInt32 aNamespaceID,
                                      nsIAtom* aAttribute,
                                      const nsAString& aValue,
                                      nsAttrValue& aResult)
{
  if (aNamespaceID == kNameSpaceID_None) {
    if (aAttribute == nsGkAtoms::maxlength) {
      return aResult.ParseNonNegativeIntValue(aValue);
    } else if (aAttribute == nsGkAtoms::cols ||
               aAttribute == nsGkAtoms::rows) {
      return aResult.ParsePositiveIntValue(aValue);
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
  } else if (aAttribute == nsGkAtoms::placeholder) {
    NS_UpdateHint(retval, NS_STYLE_HINT_FRAMECHANGE);
  }
  return retval;
}

NS_IMETHODIMP_(bool)
nsHTMLTextAreaElement::IsAttributeMapped(const nsIAtom* aAttribute) const
{
  static const MappedAttributeEntry* const map[] = {
    sDivAlignAttributeMap,
    sCommonAttributeMap,
  };

  return FindAttributeDependence(aAttribute, map);
}

nsMapRuleToAttributesFunc
nsHTMLTextAreaElement::GetAttributeMappingFunction() const
{
  return &MapAttributesIntoRule;
}

nsresult
nsHTMLTextAreaElement::PreHandleEvent(nsEventChainPreVisitor& aVisitor)
{
  nsIFormControlFrame* formControlFrame = GetFormControlFrame(false);
  nsIFrame* formFrame = NULL;
  if (formControlFrame) {
    formFrame = do_QueryFrame(formControlFrame);
  }

  aVisitor.mCanHandle = false;
  if (IsElementDisabledForEvents(aVisitor.mEvent->message, formFrame)) {
    return NS_OK;
  }

  // Don't dispatch a second select event if we are already handling
  // one.
  if (aVisitor.mEvent->message == NS_FORM_SELECTED) {
    if (mHandlingSelect) {
      return NS_OK;
    }
    mHandlingSelect = true;
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
    mHandlingSelect = false;
  }

  if (aVisitor.mEvent->message == NS_FOCUS_CONTENT ||
      aVisitor.mEvent->message == NS_BLUR_CONTENT) {
    if (aVisitor.mEvent->message == NS_FOCUS_CONTENT) {
      // If the invalid UI is shown, we should show it while focusing (and
      // update). Otherwise, we should not.
      mCanShowInvalidUI = !IsValid() && ShouldShowValidityUI();

      // If neither invalid UI nor valid UI is shown, we shouldn't show the valid
      // UI while typing.
      mCanShowValidUI = ShouldShowValidityUI();
    } else { // NS_BLUR_CONTENT
      mCanShowInvalidUI = true;
      mCanShowValidUI = true;
    }

    UpdateState(true);
  }

  // Reset the flag for other content besides this text field
  aVisitor.mEvent->flags |= (aVisitor.mItemFlags & NS_NO_CONTENT_DISPATCH)
    ? NS_EVENT_FLAG_NO_CONTENT_DISPATCH : NS_EVENT_FLAG_NONE;

  return NS_OK;
}

void
nsHTMLTextAreaElement::DoneAddingChildren(bool aHaveNotified)
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

  mDoneAddingChildren = true;
}

bool
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

    controller = do_CreateInstance("@mozilla.org/editor/editingcontroller;1", &rv);
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
  nsresult rv = GetSelectionRange(aSelectionStart, &selEnd);

  if (NS_FAILED(rv) && mState->IsSelectionCached()) {
    *aSelectionStart = mState->GetSelectionProperties().mStart;
    return NS_OK;
  }
  return rv;
}

NS_IMETHODIMP
nsHTMLTextAreaElement::SetSelectionStart(PRInt32 aSelectionStart)
{
  if (mState->IsSelectionCached()) {
    mState->GetSelectionProperties().mStart = aSelectionStart;
    return NS_OK;
  }

  nsAutoString direction;
  nsresult rv = GetSelectionDirection(direction);
  NS_ENSURE_SUCCESS(rv, rv);
  PRInt32 start, end;
  rv = GetSelectionRange(&start, &end);
  NS_ENSURE_SUCCESS(rv, rv);
  start = aSelectionStart;
  if (end < start) {
    end = start;
  }
  return SetSelectionRange(start, end, direction);
}

NS_IMETHODIMP
nsHTMLTextAreaElement::GetSelectionEnd(PRInt32 *aSelectionEnd)
{
  NS_ENSURE_ARG_POINTER(aSelectionEnd);

  PRInt32 selStart;
  nsresult rv = GetSelectionRange(&selStart, aSelectionEnd);

  if (NS_FAILED(rv) && mState->IsSelectionCached()) {
    *aSelectionEnd = mState->GetSelectionProperties().mEnd;
    return NS_OK;
  }
  return rv;
}

NS_IMETHODIMP
nsHTMLTextAreaElement::SetSelectionEnd(PRInt32 aSelectionEnd)
{
  if (mState->IsSelectionCached()) {
    mState->GetSelectionProperties().mEnd = aSelectionEnd;
    return NS_OK;
  }

  nsAutoString direction;
  nsresult rv = GetSelectionDirection(direction);
  NS_ENSURE_SUCCESS(rv, rv);
  PRInt32 start, end;
  rv = GetSelectionRange(&start, &end);
  NS_ENSURE_SUCCESS(rv, rv);
  end = aSelectionEnd;
  if (start > end) {
    start = end;
  }
  return SetSelectionRange(start, end, direction);
}

nsresult
nsHTMLTextAreaElement::GetSelectionRange(PRInt32* aSelectionStart,
                                      PRInt32* aSelectionEnd)
{
  nsresult rv = NS_ERROR_FAILURE;
  nsIFormControlFrame* formControlFrame = GetFormControlFrame(true);

  if (formControlFrame) {
    nsITextControlFrame* textControlFrame = do_QueryFrame(formControlFrame);
    if (textControlFrame)
      rv = textControlFrame->GetSelectionRange(aSelectionStart, aSelectionEnd);
  }

  return rv;
}

static void
DirectionToName(nsITextControlFrame::SelectionDirection dir, nsAString& aDirection)
{
  if (dir == nsITextControlFrame::eNone) {
    aDirection.AssignLiteral("none");
  } else if (dir == nsITextControlFrame::eForward) {
    aDirection.AssignLiteral("forward");
  } else if (dir == nsITextControlFrame::eBackward) {
    aDirection.AssignLiteral("backward");
  } else {
    NS_NOTREACHED("Invalid SelectionDirection value");
  }
}

nsresult
nsHTMLTextAreaElement::GetSelectionDirection(nsAString& aDirection)
{
  nsresult rv = NS_ERROR_FAILURE;
  nsIFormControlFrame* formControlFrame = GetFormControlFrame(true);

  if (formControlFrame) {
    nsITextControlFrame* textControlFrame = do_QueryFrame(formControlFrame);
    if (textControlFrame) {
      nsITextControlFrame::SelectionDirection dir;
      rv = textControlFrame->GetSelectionRange(nsnull, nsnull, &dir);
      if (NS_SUCCEEDED(rv)) {
        DirectionToName(dir, aDirection);
      }
    }
  }

  if (NS_FAILED(rv)) {
    if (mState->IsSelectionCached()) {
      DirectionToName(mState->GetSelectionProperties().mDirection, aDirection);
      return NS_OK;
    }
  }

  return rv;
}

NS_IMETHODIMP
nsHTMLTextAreaElement::SetSelectionDirection(const nsAString& aDirection) {
  if (mState->IsSelectionCached()) {
    nsITextControlFrame::SelectionDirection dir = nsITextControlFrame::eNone;
    if (aDirection.EqualsLiteral("forward")) {
      dir = nsITextControlFrame::eForward;
    } else if (aDirection.EqualsLiteral("backward")) {
      dir = nsITextControlFrame::eBackward;
    }
    mState->GetSelectionProperties().mDirection = dir;
    return NS_OK;
  }

  PRInt32 start, end;
  nsresult rv = GetSelectionRange(&start, &end);
  if (NS_SUCCEEDED(rv)) {
    rv = SetSelectionRange(start, end, aDirection);
  }

  return rv;
}

NS_IMETHODIMP
nsHTMLTextAreaElement::SetSelectionRange(PRInt32 aSelectionStart,
                                         PRInt32 aSelectionEnd,
                                         const nsAString& aDirection)
{ 
  nsresult rv = NS_ERROR_FAILURE;
  nsIFormControlFrame* formControlFrame = GetFormControlFrame(true);

  if (formControlFrame) {
    nsITextControlFrame* textControlFrame = do_QueryFrame(formControlFrame);
    if (textControlFrame) {
      // Default to forward, even if not specified.
      // Note that we don't currently support directionless selections, so
      // "none" is treated like "forward".
      nsITextControlFrame::SelectionDirection dir = nsITextControlFrame::eForward;
      if (aDirection.EqualsLiteral("backward")) {
        dir = nsITextControlFrame::eBackward;
      }

      rv = textControlFrame->SetSelectionRange(aSelectionStart, aSelectionEnd, dir);
      if (NS_SUCCEEDED(rv)) {
        rv = textControlFrame->ScrollSelectionIntoView();
      }
    }
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

  SetValueChanged(false);
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLTextAreaElement::SubmitNamesValues(nsFormSubmission* aFormSubmission)
{
  // Disabled elements don't submit
  if (IsDisabled()) {
    return NS_OK;
  }

  //
  // Get the name (if no name, no submit)
  //
  nsAutoString name;
  GetAttr(kNameSpaceID_None, nsGkAtoms::name, name);
  if (name.IsEmpty()) {
    return NS_OK;
  }

  //
  // Get the value
  //
  nsAutoString value;
  GetValueInternal(value, false);

  //
  // Submit
  //
  return aFormSubmission->AddNameValuePair(name, value);
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
      GetValueInternal(value, true);

      rv = nsLinebreakConverter::ConvertStringLineBreaks(
               value,
               nsLinebreakConverter::eLinebreakPlatform,
               nsLinebreakConverter::eLinebreakContent);
      NS_ASSERTION(NS_SUCCEEDED(rv), "Converting linebreaks failed!");

      nsCOMPtr<nsISupportsString> pState =
        do_CreateInstance(NS_SUPPORTS_STRING_CONTRACTID);
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
      // We do not want to save the real disabled state but the disabled
      // attribute.
      state->SetDisabled(HasAttr(kNameSpaceID_None, nsGkAtoms::disabled));
    }
  }
  return rv;
}

bool
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

  return false;
}

nsEventStates
nsHTMLTextAreaElement::IntrinsicState() const
{
  nsEventStates state = nsGenericHTMLFormElement::IntrinsicState();

  if (HasAttr(kNameSpaceID_None, nsGkAtoms::required)) {
    state |= NS_EVENT_STATE_REQUIRED;
  } else {
    state |= NS_EVENT_STATE_OPTIONAL;
  }

  if (IsCandidateForConstraintValidation()) {
    if (IsValid()) {
      state |= NS_EVENT_STATE_VALID;
    } else {
      state |= NS_EVENT_STATE_INVALID;
      // :-moz-ui-invalid always apply if the element suffers from a custom
      // error and never applies if novalidate is set on the form owner.
      if ((!mForm || !mForm->HasAttr(kNameSpaceID_None, nsGkAtoms::novalidate)) &&
          (GetValidityState(VALIDITY_STATE_CUSTOM_ERROR) ||
           (mCanShowInvalidUI && ShouldShowValidityUI()))) {
        state |= NS_EVENT_STATE_MOZ_UI_INVALID;
      }
    }

    // :-moz-ui-valid applies if all the following are true:
    // 1. The element is not focused, or had either :-moz-ui-valid or
    //    :-moz-ui-invalid applying before it was focused ;
    // 2. The element is either valid or isn't allowed to have
    //    :-moz-ui-invalid applying ;
    // 3. The element has no form owner or its form owner doesn't have the
    //    novalidate attribute set ;
    // 4. The element has already been modified or the user tried to submit the
    //    form owner while invalid.
    if ((!mForm || !mForm->HasAttr(kNameSpaceID_None, nsGkAtoms::novalidate)) &&
        (mCanShowValidUI && ShouldShowValidityUI() &&
         (IsValid() || (state.HasState(NS_EVENT_STATE_MOZ_UI_INVALID) &&
                        !mCanShowInvalidUI)))) {
      state |= NS_EVENT_STATE_MOZ_UI_VALID;
    }
  }

  if (HasAttr(kNameSpaceID_None, nsGkAtoms::placeholder) &&
      !nsContentUtils::IsFocusedContent((nsIContent*)(this)) &&
      IsValueEmpty()) {
    state |= NS_EVENT_STATE_MOZ_PLACEHOLDER;
  }

  return state;
}

nsresult
nsHTMLTextAreaElement::BindToTree(nsIDocument* aDocument, nsIContent* aParent,
                                  nsIContent* aBindingParent,
                                  bool aCompileEventHandlers)
{
  nsresult rv = nsGenericHTMLFormElement::BindToTree(aDocument, aParent,
                                                     aBindingParent,
                                                     aCompileEventHandlers);
  NS_ENSURE_SUCCESS(rv, rv);

  // If there is a disabled fieldset in the parent chain, the element is now
  // barred from constraint validation and can't suffer from value missing.
  UpdateValueMissingValidityState();
  UpdateBarredFromConstraintValidation();

  // And now make sure our state is up to date
  UpdateState(false);

  return rv;
}

void
nsHTMLTextAreaElement::UnbindFromTree(bool aDeep, bool aNullParent)
{
  nsGenericHTMLFormElement::UnbindFromTree(aDeep, aNullParent);

  // We might be no longer disabled because of parent chain changed.
  UpdateValueMissingValidityState();
  UpdateBarredFromConstraintValidation();

  // And now make sure our state is up to date
  UpdateState(false);
}

nsresult
nsHTMLTextAreaElement::BeforeSetAttr(PRInt32 aNameSpaceID, nsIAtom* aName,
                                     const nsAttrValueOrString* aValue,
                                     bool aNotify)
{
  if (aNotify && aName == nsGkAtoms::disabled &&
      aNameSpaceID == kNameSpaceID_None) {
    mDisabledChanged = true;
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
                                    const nsAttrValue* aValue, bool aNotify)
{
  if (aNameSpaceID == kNameSpaceID_None) {
    if (aName == nsGkAtoms::required || aName == nsGkAtoms::disabled ||
        aName == nsGkAtoms::readonly) {
      UpdateValueMissingValidityState();

      // This *has* to be called *after* validity has changed.
      if (aName == nsGkAtoms::readonly || aName == nsGkAtoms::disabled) {
        UpdateBarredFromConstraintValidation();
      }
    } else if (aName == nsGkAtoms::maxlength) {
      UpdateTooLongValidityState();
    }

    if (aName == nsGkAtoms::readonly) {
      UpdateEditableState(aNotify);
      mState->UpdateEditableState(aNotify);
    }
    UpdateState(aNotify);
  }

  return nsGenericHTMLFormElement::AfterSetAttr(aNameSpaceID, aName, aValue,
                                                aNotify);
}

nsresult
nsHTMLTextAreaElement::CopyInnerTo(nsGenericElement* aDest) const
{
  nsresult rv = nsGenericHTMLFormElement::CopyInnerTo(aDest);
  NS_ENSURE_SUCCESS(rv, rv);

  if (aDest->OwnerDoc()->IsStaticDocument()) {
    nsAutoString value;
    GetValueInternal(value, true);
    static_cast<nsHTMLTextAreaElement*>(aDest)->SetValue(value);
  }
  return NS_OK;
}

bool
nsHTMLTextAreaElement::IsMutable() const
{
  return (!HasAttr(kNameSpaceID_None, nsGkAtoms::readonly) && !IsDisabled());
}

bool
nsHTMLTextAreaElement::IsValueEmpty() const
{
  nsAutoString value;
  GetValueInternal(value, true);

  return value.IsEmpty();
}

// nsIConstraintValidation

NS_IMETHODIMP
nsHTMLTextAreaElement::SetCustomValidity(const nsAString& aError)
{
  nsIConstraintValidation::SetCustomValidity(aError);

  UpdateState(true);

  return NS_OK;
}

bool
nsHTMLTextAreaElement::IsTooLong()
{
  if (!HasAttr(kNameSpaceID_None, nsGkAtoms::maxlength) || !mValueChanged) {
    return false;
  }

  PRInt32 maxLength = -1;
  GetMaxLength(&maxLength);

  // Maxlength of -1 means parsing error.
  if (maxLength == -1) {
    return false;
  }

  PRInt32 textLength = -1;
  GetTextLength(&textLength);

  return textLength > maxLength;
}

bool
nsHTMLTextAreaElement::IsValueMissing() const
{
  if (!HasAttr(kNameSpaceID_None, nsGkAtoms::required) || !IsMutable()) {
    return false;
  }

  return IsValueEmpty();
}

void
nsHTMLTextAreaElement::UpdateTooLongValidityState()
{
  // TODO: this code will be re-enabled with bug 613016 and bug 613019.
#if 0
  SetValidityState(VALIDITY_STATE_TOO_LONG, IsTooLong());
#endif
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
                                    IsDisabled());
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

        const PRUnichar* params[] = { strMaxLength.get(), strTextLength.get() };
        rv = nsContentUtils::FormatLocalizedString(nsContentUtils::eDOM_PROPERTIES,
                                                   "FormValidationTextTooLong",
                                                   params, message);
        aValidationMessage = message;
      }
      break;
    case VALIDITY_STATE_VALUE_MISSING:
      {
        nsXPIDLString message;
        rv = nsContentUtils::GetLocalizedString(nsContentUtils::eDOM_PROPERTIES,
                                                "FormValidationValueMissing",
                                                message);
        aValidationMessage = message;
      }
      break;
    default:
      rv = nsIConstraintValidation::GetValidationMessage(aValidationMessage, aType);
  }

  return rv;
}

NS_IMETHODIMP_(bool)
nsHTMLTextAreaElement::IsSingleLineTextControl() const
{
  return false;
}

NS_IMETHODIMP_(bool)
nsHTMLTextAreaElement::IsTextArea() const
{
  return true;
}

NS_IMETHODIMP_(bool)
nsHTMLTextAreaElement::IsPlainTextControl() const
{
  // need to check our HTML attribute and/or CSS.
  return true;
}

NS_IMETHODIMP_(bool)
nsHTMLTextAreaElement::IsPasswordTextControl() const
{
  return false;
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

NS_IMETHODIMP_(bool)
nsHTMLTextAreaElement::ValueChanged() const
{
  return mValueChanged;
}

NS_IMETHODIMP_(void)
nsHTMLTextAreaElement::GetTextEditorValue(nsAString& aValue,
                                          bool aIgnoreWrap) const
{
  mState->GetValue(aValue, aIgnoreWrap);
}

NS_IMETHODIMP_(void)
nsHTMLTextAreaElement::SetTextEditorValue(const nsAString& aValue,
                                          bool aUserInput)
{
  mState->SetValue(aValue, aUserInput);
}

NS_IMETHODIMP_(void)
nsHTMLTextAreaElement::InitializeKeyboardEventListeners()
{
  mState->InitializeKeyboardEventListeners();
}

NS_IMETHODIMP_(void)
nsHTMLTextAreaElement::OnValueChanged(bool aNotify)
{
  // Update the validity state
  bool validBefore = IsValid();
  UpdateTooLongValidityState();
  UpdateValueMissingValidityState();

  if (validBefore != IsValid() ||
      (HasAttr(kNameSpaceID_None, nsGkAtoms::placeholder)
       && !nsContentUtils::IsFocusedContent((nsIContent*)(this)))) {
    UpdateState(aNotify);
  }
}

NS_IMETHODIMP_(bool)
nsHTMLTextAreaElement::HasCachedSelection()
{
  return mState->IsSelectionCached();
}

void
nsHTMLTextAreaElement::FieldSetDisabledChanged(bool aNotify)
{
  UpdateValueMissingValidityState();
  UpdateBarredFromConstraintValidation();

  nsGenericHTMLFormElement::FieldSetDisabledChanged(aNotify);
}
