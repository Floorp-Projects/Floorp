/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/HTMLTextAreaElement.h"
#include "mozilla/dom/HTMLTextAreaElementBinding.h"
#include "mozilla/Util.h"
#include "base/compiler_specific.h"

#include "nsIControllers.h"
#include "nsFocusManager.h"
#include "nsPIDOMWindow.h"
#include "nsContentCID.h"
#include "nsIComponentManager.h"
#include "nsIDOMHTMLFormElement.h"
#include "nsIFormControl.h"
#include "nsIForm.h"
#include "nsFormSubmission.h"
#include "nsAttrValueInlines.h"
#include "nsStyleConsts.h"
#include "nsPresContext.h"
#include "nsMappedAttributes.h"
#include "nsIFormControlFrame.h"
#include "nsITextControlFrame.h"
#include "nsLinebreakConverter.h"
#include "nsIDocument.h"
#include "nsIFrame.h"
#include "nsGUIEvent.h"
#include "nsPresState.h"
#include "nsReadableUtils.h"
#include "nsEventDispatcher.h"
#include "nsLayoutUtils.h"
#include "nsError.h"
#include "mozAutoDocUpdate.h"
#include "nsISupportsPrimitives.h"
#include "nsContentCreatorFunctions.h"
#include "nsIConstraintValidation.h"

#include "nsTextEditorState.h"

static NS_DEFINE_CID(kXULControllersCID,  NS_XULCONTROLLERS_CID);

#define NS_NO_CONTENT_DISPATCH (1 << 0)

NS_IMPL_NS_NEW_HTML_ELEMENT_CHECK_PARSER(TextArea)

namespace mozilla {
namespace dom {

HTMLTextAreaElement::HTMLTextAreaElement(already_AddRefed<nsINodeInfo> aNodeInfo,
                                         FromParser aFromParser)
  : nsGenericHTMLFormElement(aNodeInfo),
    mValueChanged(false),
    mHandlingSelect(false),
    mDoneAddingChildren(!aFromParser),
    mInhibitStateRestoration(!!(aFromParser & FROM_PARSER_FRAGMENT)),
    mDisabledChanged(false),
    mCanShowInvalidUI(true),
    mCanShowValidUI(true),
    ALLOW_THIS_IN_INITIALIZER_LIST(mState(this))
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

  SetIsDOMBinding();
}


NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(HTMLTextAreaElement,
                                                nsGenericHTMLFormElement)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mValidity)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mControllers)
  tmp->mState.Unlink();
NS_IMPL_CYCLE_COLLECTION_UNLINK_END
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(HTMLTextAreaElement,
                                                  nsGenericHTMLFormElement)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mValidity)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mControllers)
  tmp->mState.Traverse(cb);
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_ADDREF_INHERITED(HTMLTextAreaElement, Element)
NS_IMPL_RELEASE_INHERITED(HTMLTextAreaElement, Element)


// QueryInterface implementation for HTMLTextAreaElement
NS_INTERFACE_TABLE_HEAD_CYCLE_COLLECTION_INHERITED(HTMLTextAreaElement)
  NS_HTML_CONTENT_INTERFACE_TABLE5(HTMLTextAreaElement,
                                   nsIDOMHTMLTextAreaElement,
                                   nsITextControlElement,
                                   nsIDOMNSEditableElement,
                                   nsIMutationObserver,
                                   nsIConstraintValidation)
  NS_HTML_CONTENT_INTERFACE_TABLE_TO_MAP_SEGUE(HTMLTextAreaElement,
                                               nsGenericHTMLFormElement)
NS_HTML_CONTENT_INTERFACE_MAP_END


// nsIDOMHTMLTextAreaElement


NS_IMPL_ELEMENT_CLONE(HTMLTextAreaElement)

// nsIConstraintValidation
NS_IMPL_NSICONSTRAINTVALIDATION_EXCEPT_SETCUSTOMVALIDITY(HTMLTextAreaElement)


NS_IMETHODIMP
HTMLTextAreaElement::GetForm(nsIDOMHTMLFormElement** aForm)
{
  return nsGenericHTMLFormElement::GetForm(aForm);
}


// nsIContent

NS_IMETHODIMP
HTMLTextAreaElement::Select()
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
  nsGUIEvent event(true, NS_FORM_SELECTED, nullptr);
  // XXXbz HTMLInputElement guards against this reentering; shouldn't we?
  nsEventDispatcher::Dispatch(static_cast<nsIContent*>(this), presContext,
                              &event, nullptr, &status);

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
HTMLTextAreaElement::SelectAll(nsPresContext* aPresContext)
{
  nsIFormControlFrame* formControlFrame = GetFormControlFrame(true);

  if (formControlFrame) {
    formControlFrame->SetFormProperty(nsGkAtoms::select, EmptyString());
  }

  return NS_OK;
}

bool
HTMLTextAreaElement::IsHTMLFocusable(bool aWithMouse,
                                     bool *aIsFocusable, int32_t *aTabIndex)
{
  if (nsGenericHTMLFormElement::IsHTMLFocusable(aWithMouse, aIsFocusable, aTabIndex)) {
    return true;
  }

  // disabled textareas are not focusable
  *aIsFocusable = !IsDisabled();
  return false;
}

NS_IMPL_BOOL_ATTR(HTMLTextAreaElement, Autofocus, autofocus)
NS_IMPL_UINT_ATTR_NON_ZERO_DEFAULT_VALUE(HTMLTextAreaElement, Cols, cols, DEFAULT_COLS)
NS_IMPL_BOOL_ATTR(HTMLTextAreaElement, Disabled, disabled)
NS_IMPL_NON_NEGATIVE_INT_ATTR(HTMLTextAreaElement, MaxLength, maxlength)
NS_IMPL_STRING_ATTR(HTMLTextAreaElement, Name, name)
NS_IMPL_BOOL_ATTR(HTMLTextAreaElement, ReadOnly, readonly)
NS_IMPL_BOOL_ATTR(HTMLTextAreaElement, Required, required)
NS_IMPL_UINT_ATTR_NON_ZERO_DEFAULT_VALUE(HTMLTextAreaElement, Rows, rows, DEFAULT_ROWS_TEXTAREA)
NS_IMPL_STRING_ATTR(HTMLTextAreaElement, Wrap, wrap)
NS_IMPL_STRING_ATTR(HTMLTextAreaElement, Placeholder, placeholder)
  
int32_t
HTMLTextAreaElement::TabIndexDefault()
{
  return 0;
}

NS_IMETHODIMP 
HTMLTextAreaElement::GetType(nsAString& aType)
{
  aType.AssignLiteral("textarea");

  return NS_OK;
}

NS_IMETHODIMP 
HTMLTextAreaElement::GetValue(nsAString& aValue)
{
  GetValueInternal(aValue, true);
  return NS_OK;
}

void
HTMLTextAreaElement::GetValueInternal(nsAString& aValue, bool aIgnoreWrap) const
{
  mState.GetValue(aValue, aIgnoreWrap);
}

NS_IMETHODIMP_(nsIEditor*)
HTMLTextAreaElement::GetTextEditor()
{
  return GetEditor();
}

NS_IMETHODIMP_(nsISelectionController*)
HTMLTextAreaElement::GetSelectionController()
{
  return mState.GetSelectionController();
}

NS_IMETHODIMP_(nsFrameSelection*)
HTMLTextAreaElement::GetConstFrameSelection()
{
  return mState.GetConstFrameSelection();
}

NS_IMETHODIMP
HTMLTextAreaElement::BindToFrame(nsTextControlFrame* aFrame)
{
  return mState.BindToFrame(aFrame);
}

NS_IMETHODIMP_(void)
HTMLTextAreaElement::UnbindFromFrame(nsTextControlFrame* aFrame)
{
  if (aFrame) {
    mState.UnbindFromFrame(aFrame);
  }
}

NS_IMETHODIMP
HTMLTextAreaElement::CreateEditor()
{
  return mState.PrepareEditor();
}

NS_IMETHODIMP_(nsIContent*)
HTMLTextAreaElement::GetRootEditorNode()
{
  return mState.GetRootNode();
}

NS_IMETHODIMP_(nsIContent*)
HTMLTextAreaElement::CreatePlaceholderNode()
{
  NS_ENSURE_SUCCESS(mState.CreatePlaceholderNode(), nullptr);
  return mState.GetPlaceholderNode();
}

NS_IMETHODIMP_(nsIContent*)
HTMLTextAreaElement::GetPlaceholderNode()
{
  return mState.GetPlaceholderNode();
}

NS_IMETHODIMP_(void)
HTMLTextAreaElement::UpdatePlaceholderVisibility(bool aNotify)
{
  mState.UpdatePlaceholderVisibility(aNotify);
}

NS_IMETHODIMP_(bool)
HTMLTextAreaElement::GetPlaceholderVisibility()
{
  return mState.GetPlaceholderVisibility();
}

nsresult
HTMLTextAreaElement::SetValueInternal(const nsAString& aValue,
                                      bool aUserInput)
{
  // Need to set the value changed flag here, so that
  // nsTextControlFrame::UpdateValueDisplay retrieves the correct value
  // if needed.
  SetValueChanged(true);
  mState.SetValue(aValue, aUserInput, true);

  return NS_OK;
}

NS_IMETHODIMP 
HTMLTextAreaElement::SetValue(const nsAString& aValue)
{
  // If the value has been set by a script, we basically want to keep the
  // current change event state. If the element is ready to fire a change
  // event, we should keep it that way. Otherwise, we should make sure the
  // element will not fire any event because of the script interaction.
  //
  // NOTE: this is currently quite expensive work (too much string
  // manipulation). We should probably optimize that.
  nsAutoString currentValue;
  GetValueInternal(currentValue, true);

  SetValueInternal(aValue, false);

  if (mFocusedValue.Equals(currentValue)) {
    GetValueInternal(mFocusedValue, true);
  }

  return NS_OK;
}

NS_IMETHODIMP 
HTMLTextAreaElement::SetUserInput(const nsAString& aValue)
{
  if (!nsContentUtils::IsCallerChrome()) {
    return NS_ERROR_DOM_SECURITY_ERR;
  }
  SetValueInternal(aValue, true);
  return NS_OK;
}

NS_IMETHODIMP
HTMLTextAreaElement::SetValueChanged(bool aValueChanged)
{
  bool previousValue = mValueChanged;

  mValueChanged = aValueChanged;
  if (!aValueChanged && !mState.IsEmpty()) {
    mState.EmptyValue();
  }

  if (mValueChanged != previousValue) {
    UpdateState(true);
  }

  return NS_OK;
}

NS_IMETHODIMP
HTMLTextAreaElement::GetDefaultValue(nsAString& aDefaultValue)
{
  nsContentUtils::GetNodeTextContent(this, false, aDefaultValue);
  return NS_OK;
}  

NS_IMETHODIMP
HTMLTextAreaElement::SetDefaultValue(const nsAString& aDefaultValue)
{
  ErrorResult error;
  SetDefaultValue(aDefaultValue, error);
  return error.ErrorCode();
}

void
HTMLTextAreaElement::SetDefaultValue(const nsAString& aDefaultValue, ErrorResult& aError)
{
  nsresult rv = nsContentUtils::SetNodeTextContent(this, aDefaultValue, true);
  if (NS_SUCCEEDED(rv) && !mValueChanged) {
    Reset();
  }
  if (NS_FAILED(rv)) {
    aError.Throw(rv);
  }
}

bool
HTMLTextAreaElement::ParseAttribute(int32_t aNamespaceID,
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
HTMLTextAreaElement::GetAttributeChangeHint(const nsIAtom* aAttribute,
                                            int32_t aModType) const
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
HTMLTextAreaElement::IsAttributeMapped(const nsIAtom* aAttribute) const
{
  static const MappedAttributeEntry* const map[] = {
    sDivAlignAttributeMap,
    sCommonAttributeMap,
  };

  return FindAttributeDependence(aAttribute, map);
}

nsMapRuleToAttributesFunc
HTMLTextAreaElement::GetAttributeMappingFunction() const
{
  return &MapAttributesIntoRule;
}

bool
HTMLTextAreaElement::IsDisabledForEvents(uint32_t aMessage)
{
  nsIFormControlFrame* formControlFrame = GetFormControlFrame(false);
  nsIFrame* formFrame = nullptr;
  if (formControlFrame) {
    formFrame = do_QueryFrame(formControlFrame);
  }
  return IsElementDisabledForEvents(aMessage, formFrame);
}

nsresult
HTMLTextAreaElement::PreHandleEvent(nsEventChainPreVisitor& aVisitor)
{
  aVisitor.mCanHandle = false;
  if (IsDisabledForEvents(aVisitor.mEvent->message)) {
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

  // If noContentDispatch is true we will not allow content to handle
  // this event.  But to allow middle mouse button paste to work we must allow 
  // middle clicks to go to text fields anyway.
  if (aVisitor.mEvent->mFlags.mNoContentDispatch) {
    aVisitor.mItemFlags |= NS_NO_CONTENT_DISPATCH;
  }
  if (aVisitor.mEvent->message == NS_MOUSE_CLICK &&
      aVisitor.mEvent->eventStructType == NS_MOUSE_EVENT &&
      static_cast<nsMouseEvent*>(aVisitor.mEvent)->button ==
        nsMouseEvent::eMiddleButton) {
    aVisitor.mEvent->mFlags.mNoContentDispatch = false;
  }

  // Fire onchange (if necessary), before we do the blur, bug 370521.
  if (aVisitor.mEvent->message == NS_BLUR_CONTENT) {
    FireChangeEventIfNeeded();
  }

  return nsGenericHTMLFormElement::PreHandleEvent(aVisitor);
}

void
HTMLTextAreaElement::FireChangeEventIfNeeded()
{
  nsString value;
  GetValueInternal(value, true);

  if (mFocusedValue.Equals(value)) {
    return;
  }

  // Dispatch the change event.
  mFocusedValue = value;
  nsContentUtils::DispatchTrustedEvent(OwnerDoc(),
                                       static_cast<nsIContent*>(this),
                                       NS_LITERAL_STRING("change"), true,
                                       false);
}

nsresult
HTMLTextAreaElement::PostHandleEvent(nsEventChainPostVisitor& aVisitor)
{
  if (aVisitor.mEvent->message == NS_FORM_SELECTED) {
    mHandlingSelect = false;
  }

  if (aVisitor.mEvent->message == NS_FOCUS_CONTENT ||
      aVisitor.mEvent->message == NS_BLUR_CONTENT) {
    if (aVisitor.mEvent->message == NS_FOCUS_CONTENT) {
      // If the invalid UI is shown, we should show it while focusing (and
      // update). Otherwise, we should not.
      GetValueInternal(mFocusedValue, true);
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
  aVisitor.mEvent->mFlags.mNoContentDispatch =
    ((aVisitor.mItemFlags & NS_NO_CONTENT_DISPATCH) != 0);

  return NS_OK;
}

void
HTMLTextAreaElement::DoneAddingChildren(bool aHaveNotified)
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
HTMLTextAreaElement::IsDoneAddingChildren()
{
  return mDoneAddingChildren;
}

// Controllers Methods

nsIControllers*
HTMLTextAreaElement::GetControllers(ErrorResult& aError)
{
  if (!mControllers)
  {
    nsresult rv;
    mControllers = do_CreateInstance(kXULControllersCID, &rv);
    if (NS_FAILED(rv)) {
      aError.Throw(rv);
      return nullptr;
    }

    nsCOMPtr<nsIController> controller = do_CreateInstance("@mozilla.org/editor/editorcontroller;1", &rv);
    if (NS_FAILED(rv)) {
      aError.Throw(rv);
      return nullptr;
    }

    mControllers->AppendController(controller);

    controller = do_CreateInstance("@mozilla.org/editor/editingcontroller;1", &rv);
    if (NS_FAILED(rv)) {
      aError.Throw(rv);
      return nullptr;
    }

    mControllers->AppendController(controller);
  }

  return mControllers;
}

NS_IMETHODIMP
HTMLTextAreaElement::GetControllers(nsIControllers** aResult)
{
  NS_ENSURE_ARG_POINTER(aResult);

  ErrorResult error;
  *aResult = GetControllers(error);
  NS_IF_ADDREF(*aResult);

  return error.ErrorCode();
}

uint32_t
HTMLTextAreaElement::GetTextLength()
{
  nsAutoString val;
  GetValue(val);
  return val.Length();
}

NS_IMETHODIMP
HTMLTextAreaElement::GetTextLength(int32_t *aTextLength)
{
  NS_ENSURE_ARG_POINTER(aTextLength);
  *aTextLength = GetTextLength();

  return NS_OK;
}

NS_IMETHODIMP
HTMLTextAreaElement::GetSelectionStart(int32_t *aSelectionStart)
{
  NS_ENSURE_ARG_POINTER(aSelectionStart);

  ErrorResult error;
  *aSelectionStart = GetSelectionStart(error);
  return error.ErrorCode();
}

uint32_t
HTMLTextAreaElement::GetSelectionStart(ErrorResult& aError)
{
  int32_t selStart, selEnd;
  nsresult rv = GetSelectionRange(&selStart, &selEnd);

  if (NS_FAILED(rv) && mState.IsSelectionCached()) {
    return mState.GetSelectionProperties().mStart;
  }
  if (NS_FAILED(rv)) {
    aError.Throw(rv);
  }
  return selStart;
}

NS_IMETHODIMP
HTMLTextAreaElement::SetSelectionStart(int32_t aSelectionStart)
{
  ErrorResult error;
  SetSelectionStart(aSelectionStart, error);
  return error.ErrorCode();
}

void
HTMLTextAreaElement::SetSelectionStart(uint32_t aSelectionStart, ErrorResult& aError)
{
  if (mState.IsSelectionCached()) {
    mState.GetSelectionProperties().mStart = aSelectionStart;
    return;
  }

  nsAutoString direction;
  nsresult rv = GetSelectionDirection(direction);
  if (NS_FAILED(rv)) {
    aError.Throw(rv);
    return;
  }
  int32_t start, end;
  rv = GetSelectionRange(&start, &end);
  if (NS_FAILED(rv)) {
    aError.Throw(rv);
    return;
  }
  start = aSelectionStart;
  if (end < start) {
    end = start;
  }
  rv = SetSelectionRange(start, end, direction);
  if (NS_FAILED(rv)) {
    aError.Throw(rv);
  }
}

NS_IMETHODIMP
HTMLTextAreaElement::GetSelectionEnd(int32_t *aSelectionEnd)
{
  NS_ENSURE_ARG_POINTER(aSelectionEnd);

  ErrorResult error;
  *aSelectionEnd = GetSelectionEnd(error);
  return error.ErrorCode();
}

uint32_t
HTMLTextAreaElement::GetSelectionEnd(ErrorResult& aError)
{
  int32_t selStart, selEnd;
  nsresult rv = GetSelectionRange(&selStart, &selEnd);

  if (NS_FAILED(rv) && mState.IsSelectionCached()) {
    return mState.GetSelectionProperties().mEnd;
  }
  if (NS_FAILED(rv)) {
    aError.Throw(rv);
  }
  return selEnd;
}

NS_IMETHODIMP
HTMLTextAreaElement::SetSelectionEnd(int32_t aSelectionEnd)
{
  ErrorResult error;
  SetSelectionEnd(aSelectionEnd, error);
  return error.ErrorCode();
}

void
HTMLTextAreaElement::SetSelectionEnd(uint32_t aSelectionEnd, ErrorResult& aError)
{
  if (mState.IsSelectionCached()) {
    mState.GetSelectionProperties().mEnd = aSelectionEnd;
    return;
  }

  nsAutoString direction;
  nsresult rv = GetSelectionDirection(direction);
  if (NS_FAILED(rv)) {
    aError.Throw(rv);
    return;
  }
  int32_t start, end;
  rv = GetSelectionRange(&start, &end);
  if (NS_FAILED(rv)) {
    aError.Throw(rv);
    return;
  }
  end = aSelectionEnd;
  if (start > end) {
    start = end;
  }
  rv = SetSelectionRange(start, end, direction);
  if (NS_FAILED(rv)) {
    aError.Throw(rv);
  }
}

nsresult
HTMLTextAreaElement::GetSelectionRange(int32_t* aSelectionStart,
                                       int32_t* aSelectionEnd)
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
HTMLTextAreaElement::GetSelectionDirection(nsAString& aDirection)
{
  ErrorResult error;
  GetSelectionDirection(aDirection, error);
  return error.ErrorCode();
}

void
HTMLTextAreaElement::GetSelectionDirection(nsAString& aDirection, ErrorResult& aError)
{
  nsresult rv = NS_ERROR_FAILURE;
  nsIFormControlFrame* formControlFrame = GetFormControlFrame(true);

  if (formControlFrame) {
    nsITextControlFrame* textControlFrame = do_QueryFrame(formControlFrame);
    if (textControlFrame) {
      nsITextControlFrame::SelectionDirection dir;
      rv = textControlFrame->GetSelectionRange(nullptr, nullptr, &dir);
      if (NS_SUCCEEDED(rv)) {
        DirectionToName(dir, aDirection);
      }
    }
  }

  if (NS_FAILED(rv)) {
    if (mState.IsSelectionCached()) {
      DirectionToName(mState.GetSelectionProperties().mDirection, aDirection);
      return;
    }
    aError.Throw(rv);
  }
}

NS_IMETHODIMP
HTMLTextAreaElement::SetSelectionDirection(const nsAString& aDirection)
{
  ErrorResult error;
  SetSelectionDirection(aDirection, error);
  return error.ErrorCode();
}

void
HTMLTextAreaElement::SetSelectionDirection(const nsAString& aDirection, ErrorResult& aError)
{
  if (mState.IsSelectionCached()) {
    nsITextControlFrame::SelectionDirection dir = nsITextControlFrame::eNone;
    if (aDirection.EqualsLiteral("forward")) {
      dir = nsITextControlFrame::eForward;
    } else if (aDirection.EqualsLiteral("backward")) {
      dir = nsITextControlFrame::eBackward;
    }
    mState.GetSelectionProperties().mDirection = dir;
    return;
  }

  int32_t start, end;
  nsresult rv = GetSelectionRange(&start, &end);
  if (NS_SUCCEEDED(rv)) {
    rv = SetSelectionRange(start, end, aDirection);
  }
  if (NS_FAILED(rv)) {
    aError.Throw(rv);
  }
}

NS_IMETHODIMP
HTMLTextAreaElement::SetSelectionRange(int32_t aSelectionStart,
                                       int32_t aSelectionEnd,
                                       const nsAString& aDirection)
{
  ErrorResult error;
  Optional<nsAString> dir;
  dir = &aDirection;
  SetSelectionRange(aSelectionStart, aSelectionEnd, dir, error);
  return error.ErrorCode();
}

void
HTMLTextAreaElement::SetSelectionRange(uint32_t aSelectionStart,
                                       uint32_t aSelectionEnd,
                                       const Optional<nsAString>& aDirection,
                                       ErrorResult& aError)
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
      if (aDirection.WasPassed() && aDirection.Value().EqualsLiteral("backward")) {
        dir = nsITextControlFrame::eBackward;
      }

      rv = textControlFrame->SetSelectionRange(aSelectionStart, aSelectionEnd, dir);
      if (NS_SUCCEEDED(rv)) {
        rv = textControlFrame->ScrollSelectionIntoView();
      }
    }
  }

  if (NS_FAILED(rv)) {
    aError.Throw(rv);
  }
}

nsresult
HTMLTextAreaElement::Reset()
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
HTMLTextAreaElement::SubmitNamesValues(nsFormSubmission* aFormSubmission)
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
HTMLTextAreaElement::SaveState()
{
  nsresult rv = NS_OK;

  // Only save if value != defaultValue (bug 62713)
  nsPresState *state = nullptr;
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
HTMLTextAreaElement::RestoreState(nsPresState* aState)
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
HTMLTextAreaElement::IntrinsicState() const
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

  return state;
}

nsresult
HTMLTextAreaElement::BindToTree(nsIDocument* aDocument, nsIContent* aParent,
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
HTMLTextAreaElement::UnbindFromTree(bool aDeep, bool aNullParent)
{
  nsGenericHTMLFormElement::UnbindFromTree(aDeep, aNullParent);

  // We might be no longer disabled because of parent chain changed.
  UpdateValueMissingValidityState();
  UpdateBarredFromConstraintValidation();

  // And now make sure our state is up to date
  UpdateState(false);
}

nsresult
HTMLTextAreaElement::BeforeSetAttr(int32_t aNameSpaceID, nsIAtom* aName,
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
HTMLTextAreaElement::CharacterDataChanged(nsIDocument* aDocument,
                                          nsIContent* aContent,
                                          CharacterDataChangeInfo* aInfo)
{
  ContentChanged(aContent);
}

void
HTMLTextAreaElement::ContentAppended(nsIDocument* aDocument,
                                     nsIContent* aContainer,
                                     nsIContent* aFirstNewContent,
                                     int32_t /* unused */)
{
  ContentChanged(aFirstNewContent);
}

void
HTMLTextAreaElement::ContentInserted(nsIDocument* aDocument,
                                     nsIContent* aContainer,
                                     nsIContent* aChild,
                                     int32_t /* unused */)
{
  ContentChanged(aChild);
}

void
HTMLTextAreaElement::ContentRemoved(nsIDocument* aDocument,
                                    nsIContent* aContainer,
                                    nsIContent* aChild,
                                    int32_t aIndexInContainer,
                                    nsIContent* aPreviousSibling)
{
  ContentChanged(aChild);
}

void
HTMLTextAreaElement::ContentChanged(nsIContent* aContent)
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
HTMLTextAreaElement::AfterSetAttr(int32_t aNameSpaceID, nsIAtom* aName,
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

    UpdateState(aNotify);
  }

  return nsGenericHTMLFormElement::AfterSetAttr(aNameSpaceID, aName, aValue,
                                                aNotify);
}

nsresult
HTMLTextAreaElement::CopyInnerTo(Element* aDest)
{
  nsresult rv = nsGenericHTMLFormElement::CopyInnerTo(aDest);
  NS_ENSURE_SUCCESS(rv, rv);

  if (aDest->OwnerDoc()->IsStaticDocument()) {
    nsAutoString value;
    GetValueInternal(value, true);
    static_cast<HTMLTextAreaElement*>(aDest)->SetValue(value);
  }
  return NS_OK;
}

bool
HTMLTextAreaElement::IsMutable() const
{
  return (!HasAttr(kNameSpaceID_None, nsGkAtoms::readonly) && !IsDisabled());
}

bool
HTMLTextAreaElement::IsValueEmpty() const
{
  nsAutoString value;
  GetValueInternal(value, true);

  return value.IsEmpty();
}

// nsIConstraintValidation

NS_IMETHODIMP
HTMLTextAreaElement::SetCustomValidity(const nsAString& aError)
{
  nsIConstraintValidation::SetCustomValidity(aError);

  UpdateState(true);

  return NS_OK;
}

bool
HTMLTextAreaElement::IsTooLong()
{
  if (!HasAttr(kNameSpaceID_None, nsGkAtoms::maxlength) || !mValueChanged) {
    return false;
  }

  int32_t maxLength = -1;
  GetMaxLength(&maxLength);

  // Maxlength of -1 means parsing error.
  if (maxLength == -1) {
    return false;
  }

  int32_t textLength = -1;
  GetTextLength(&textLength);

  return textLength > maxLength;
}

bool
HTMLTextAreaElement::IsValueMissing() const
{
  if (!HasAttr(kNameSpaceID_None, nsGkAtoms::required) || !IsMutable()) {
    return false;
  }

  return IsValueEmpty();
}

void
HTMLTextAreaElement::UpdateTooLongValidityState()
{
  // TODO: this code will be re-enabled with bug 613016 and bug 613019.
#if 0
  SetValidityState(VALIDITY_STATE_TOO_LONG, IsTooLong());
#endif
}

void
HTMLTextAreaElement::UpdateValueMissingValidityState()
{
  SetValidityState(VALIDITY_STATE_VALUE_MISSING, IsValueMissing());
}

void
HTMLTextAreaElement::UpdateBarredFromConstraintValidation()
{
  SetBarredFromConstraintValidation(HasAttr(kNameSpaceID_None,
                                            nsGkAtoms::readonly) ||
                                    IsDisabled());
}

nsresult
HTMLTextAreaElement::GetValidationMessage(nsAString& aValidationMessage,
                                          ValidityStateType aType)
{
  nsresult rv = NS_OK;

  switch (aType)
  {
    case VALIDITY_STATE_TOO_LONG:
      {
        nsXPIDLString message;
        int32_t maxLength = -1;
        int32_t textLength = -1;
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
HTMLTextAreaElement::IsSingleLineTextControl() const
{
  return false;
}

NS_IMETHODIMP_(bool)
HTMLTextAreaElement::IsTextArea() const
{
  return true;
}

NS_IMETHODIMP_(bool)
HTMLTextAreaElement::IsPlainTextControl() const
{
  // need to check our HTML attribute and/or CSS.
  return true;
}

NS_IMETHODIMP_(bool)
HTMLTextAreaElement::IsPasswordTextControl() const
{
  return false;
}

NS_IMETHODIMP_(int32_t)
HTMLTextAreaElement::GetCols()
{
  const nsAttrValue* attr = GetParsedAttr(nsGkAtoms::cols);
  if (attr) {
    int32_t cols = attr->Type() == nsAttrValue::eInteger ?
                   attr->GetIntegerValue() : 0;
    // XXX why a default of 1 char, why hide it
    return (cols <= 0) ? 1 : cols;
  }

  return DEFAULT_COLS;
}

NS_IMETHODIMP_(int32_t)
HTMLTextAreaElement::GetWrapCols()
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


NS_IMETHODIMP_(int32_t)
HTMLTextAreaElement::GetRows()
{
  const nsAttrValue* attr = GetParsedAttr(nsGkAtoms::rows);
  if (attr && attr->Type() == nsAttrValue::eInteger) {
    int32_t rows = attr->GetIntegerValue();
    return (rows <= 0) ? DEFAULT_ROWS_TEXTAREA : rows;
  }

  return DEFAULT_ROWS_TEXTAREA;
}

NS_IMETHODIMP_(void)
HTMLTextAreaElement::GetDefaultValueFromContent(nsAString& aValue)
{
  GetDefaultValue(aValue);
}

NS_IMETHODIMP_(bool)
HTMLTextAreaElement::ValueChanged() const
{
  return mValueChanged;
}

NS_IMETHODIMP_(void)
HTMLTextAreaElement::GetTextEditorValue(nsAString& aValue,
                                        bool aIgnoreWrap) const
{
  mState.GetValue(aValue, aIgnoreWrap);
}

NS_IMETHODIMP_(void)
HTMLTextAreaElement::InitializeKeyboardEventListeners()
{
  mState.InitializeKeyboardEventListeners();
}

NS_IMETHODIMP_(void)
HTMLTextAreaElement::OnValueChanged(bool aNotify)
{
  // Update the validity state
  bool validBefore = IsValid();
  UpdateTooLongValidityState();
  UpdateValueMissingValidityState();

  if (validBefore != IsValid()) {
    UpdateState(aNotify);
  }
}

NS_IMETHODIMP_(bool)
HTMLTextAreaElement::HasCachedSelection()
{
  return mState.IsSelectionCached();
}

void
HTMLTextAreaElement::FieldSetDisabledChanged(bool aNotify)
{
  UpdateValueMissingValidityState();
  UpdateBarredFromConstraintValidation();

  nsGenericHTMLFormElement::FieldSetDisabledChanged(aNotify);
}

JSObject*
HTMLTextAreaElement::WrapNode(JSContext* aCx, JSObject* aScope)
{
  return HTMLTextAreaElementBinding::Wrap(aCx, aScope, this);
}

} // namespace dom
} // namespace mozilla
