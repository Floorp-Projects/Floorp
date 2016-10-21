/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/HTMLTextAreaElement.h"

#include "mozAutoDocUpdate.h"
#include "mozilla/AsyncEventDispatcher.h"
#include "mozilla/Attributes.h"
#include "mozilla/dom/HTMLFormSubmission.h"
#include "mozilla/dom/HTMLTextAreaElementBinding.h"
#include "mozilla/EventDispatcher.h"
#include "mozilla/EventStates.h"
#include "mozilla/MouseEvents.h"
#include "nsAttrValueInlines.h"
#include "nsContentCID.h"
#include "nsContentCreatorFunctions.h"
#include "nsError.h"
#include "nsFocusManager.h"
#include "nsIComponentManager.h"
#include "nsIConstraintValidation.h"
#include "nsIControllers.h"
#include "nsIDocument.h"
#include "nsIDOMHTMLFormElement.h"
#include "nsIFormControlFrame.h"
#include "nsIFormControl.h"
#include "nsIForm.h"
#include "nsIFrame.h"
#include "nsISupportsPrimitives.h"
#include "nsITextControlFrame.h"
#include "nsLayoutUtils.h"
#include "nsLinebreakConverter.h"
#include "nsMappedAttributes.h"
#include "nsPIDOMWindow.h"
#include "nsPresContext.h"
#include "nsPresState.h"
#include "nsReadableUtils.h"
#include "nsRuleData.h"
#include "nsStyleConsts.h"
#include "nsTextEditorState.h"
#include "nsIController.h"

static NS_DEFINE_CID(kXULControllersCID,  NS_XULCONTROLLERS_CID);

#define NS_NO_CONTENT_DISPATCH (1 << 0)

NS_IMPL_NS_NEW_HTML_ELEMENT_CHECK_PARSER(TextArea)

namespace mozilla {
namespace dom {

HTMLTextAreaElement::HTMLTextAreaElement(already_AddRefed<mozilla::dom::NodeInfo>& aNodeInfo,
                                         FromParser aFromParser)
  : nsGenericHTMLFormElementWithState(aNodeInfo),
    mValueChanged(false),
    mLastValueChangeWasInteractive(false),
    mHandlingSelect(false),
    mDoneAddingChildren(!aFromParser),
    mInhibitStateRestoration(!!(aFromParser & FROM_PARSER_FRAGMENT)),
    mDisabledChanged(false),
    mCanShowInvalidUI(true),
    mCanShowValidUI(true),
    mState(this)
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


NS_IMPL_CYCLE_COLLECTION_INHERITED(HTMLTextAreaElement,
                                   nsGenericHTMLFormElementWithState,
                                   mValidity,
                                   mControllers,
                                   mState)

NS_IMPL_ADDREF_INHERITED(HTMLTextAreaElement, Element)
NS_IMPL_RELEASE_INHERITED(HTMLTextAreaElement, Element)


// QueryInterface implementation for HTMLTextAreaElement
NS_INTERFACE_TABLE_HEAD_CYCLE_COLLECTION_INHERITED(HTMLTextAreaElement)
  NS_INTERFACE_TABLE_INHERITED(HTMLTextAreaElement,
                               nsIDOMHTMLTextAreaElement,
                               nsITextControlElement,
                               nsIDOMNSEditableElement,
                               nsIMutationObserver,
                               nsIConstraintValidation)
NS_INTERFACE_TABLE_TAIL_INHERITING(nsGenericHTMLFormElementWithState)


// nsIDOMHTMLTextAreaElement

nsresult
HTMLTextAreaElement::Clone(mozilla::dom::NodeInfo *aNodeInfo, nsINode **aResult) const
{
  *aResult = nullptr;
  already_AddRefed<mozilla::dom::NodeInfo> ni =
    RefPtr<mozilla::dom::NodeInfo>(aNodeInfo).forget();
  RefPtr<HTMLTextAreaElement> it = new HTMLTextAreaElement(ni);

  nsresult rv = const_cast<HTMLTextAreaElement*>(this)->CopyInnerTo(it);
  NS_ENSURE_SUCCESS(rv, rv);

  if (mValueChanged) {
    // Set our value on the clone.
    nsAutoString value;
    GetValueInternal(value, true);

    // SetValueInternal handles setting mValueChanged for us
    rv = it->SetValueInternal(value, nsTextEditorState::eSetValue_Notify);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  it->mLastValueChangeWasInteractive = mLastValueChangeWasInteractive;
  it.forget(aResult);
  return NS_OK;
}

// nsIConstraintValidation
NS_IMPL_NSICONSTRAINTVALIDATION_EXCEPT_SETCUSTOMVALIDITY(HTMLTextAreaElement)


NS_IMETHODIMP
HTMLTextAreaElement::GetForm(nsIDOMHTMLFormElement** aForm)
{
  return nsGenericHTMLFormElementWithState::GetForm(aForm);
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

  RefPtr<nsPresContext> presContext = GetPresContext(eForComposedDoc);
  if (state == eInactiveWindow) {
    if (fm)
      fm->SetFocus(this, nsIFocusManager::FLAG_NOSCROLL);
    SelectAll(presContext);
    return NS_OK;
  }

  nsEventStatus status = nsEventStatus_eIgnore;
  WidgetGUIEvent event(true, eFormSelect, nullptr);
  // XXXbz HTMLInputElement guards against this reentering; shouldn't we?
  EventDispatcher::Dispatch(static_cast<nsIContent*>(this), presContext,
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
  if (nsGenericHTMLFormElementWithState::IsHTMLFocusable(aWithMouse, aIsFocusable,
                                                         aTabIndex))
  {
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
NS_IMPL_NON_NEGATIVE_INT_ATTR(HTMLTextAreaElement, MinLength, minlength)
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

NS_IMETHODIMP_(Element*)
HTMLTextAreaElement::CreatePlaceholderNode()
{
  NS_ENSURE_SUCCESS(mState.CreatePlaceholderNode(), nullptr);
  return mState.GetPlaceholderNode();
}

NS_IMETHODIMP_(Element*)
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
                                      uint32_t aFlags)
{
  // Need to set the value changed flag here, so that
  // nsTextControlFrame::UpdateValueDisplay retrieves the correct value
  // if needed.
  SetValueChanged(true);
  aFlags |= nsTextEditorState::eSetValue_Notify;
  if (!mState.SetValue(aValue, aFlags)) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

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

  nsresult rv =
    SetValueInternal(aValue, nsTextEditorState::eSetValue_ByContent);
  NS_ENSURE_SUCCESS(rv, rv);

  if (mFocusedValue.Equals(currentValue)) {
    GetValueInternal(mFocusedValue, true);
  }

  return NS_OK;
}

NS_IMETHODIMP 
HTMLTextAreaElement::SetUserInput(const nsAString& aValue)
{
  return SetValueInternal(aValue, nsTextEditorState::eSetValue_BySetUserInput);
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
  if (!nsContentUtils::GetNodeTextContent(this, false, aDefaultValue, fallible)) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return NS_OK;
}  

NS_IMETHODIMP
HTMLTextAreaElement::SetDefaultValue(const nsAString& aDefaultValue)
{
  ErrorResult error;
  SetDefaultValue(aDefaultValue, error);
  return error.StealNSResult();
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
    if (aAttribute == nsGkAtoms::maxlength ||
        aAttribute == nsGkAtoms::minlength) {
      return aResult.ParseNonNegativeIntValue(aValue);
    } else if (aAttribute == nsGkAtoms::cols) {
      aResult.ParseIntWithFallback(aValue, DEFAULT_COLS);
      return true;
    } else if (aAttribute == nsGkAtoms::rows) {
      aResult.ParseIntWithFallback(aValue, DEFAULT_ROWS_TEXTAREA);
      return true;
    }
  }
  return nsGenericHTMLElement::ParseAttribute(aNamespaceID, aAttribute, aValue,
                                              aResult);
}

void
HTMLTextAreaElement::MapAttributesIntoRule(const nsMappedAttributes* aAttributes,
                                           nsRuleData* aData)
{
  if (aData->mSIDs & NS_STYLE_INHERIT_BIT(Text)) {
    // wrap=off
    nsCSSValue* whiteSpace = aData->ValueForWhiteSpace();
    if (whiteSpace->GetUnit() == eCSSUnit_Null) {
      const nsAttrValue* value = aAttributes->GetAttr(nsGkAtoms::wrap);
      if (value && value->Type() == nsAttrValue::eString &&
          value->Equals(nsGkAtoms::OFF, eIgnoreCase)) {
        whiteSpace->SetIntValue(NS_STYLE_WHITESPACE_PRE, eCSSUnit_Enumerated);
      }
    }
  }

  nsGenericHTMLFormElementWithState::MapDivAlignAttributeInto(aAttributes, aData);
  nsGenericHTMLFormElementWithState::MapCommonAttributesInto(aAttributes, aData);
}

nsChangeHint
HTMLTextAreaElement::GetAttributeChangeHint(const nsIAtom* aAttribute,
                                            int32_t aModType) const
{
  nsChangeHint retval =
      nsGenericHTMLFormElementWithState::GetAttributeChangeHint(aAttribute, aModType);
  if (aAttribute == nsGkAtoms::rows ||
      aAttribute == nsGkAtoms::cols) {
    retval |= NS_STYLE_HINT_REFLOW;
  } else if (aAttribute == nsGkAtoms::wrap) {
    retval |= nsChangeHint_ReconstructFrame;
  } else if (aAttribute == nsGkAtoms::placeholder) {
    retval |= nsChangeHint_ReconstructFrame;
  }
  return retval;
}

NS_IMETHODIMP_(bool)
HTMLTextAreaElement::IsAttributeMapped(const nsIAtom* aAttribute) const
{
  static const MappedAttributeEntry attributes[] = {
    { &nsGkAtoms::wrap },
    { nullptr }
  };

  static const MappedAttributeEntry* const map[] = {
    attributes,
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
HTMLTextAreaElement::IsDisabledForEvents(EventMessage aMessage)
{
  nsIFormControlFrame* formControlFrame = GetFormControlFrame(false);
  nsIFrame* formFrame = do_QueryFrame(formControlFrame);
  return IsElementDisabledForEvents(aMessage, formFrame);
}

nsresult
HTMLTextAreaElement::GetEventTargetParent(EventChainPreVisitor& aVisitor)
{
  aVisitor.mCanHandle = false;
  if (IsDisabledForEvents(aVisitor.mEvent->mMessage)) {
    return NS_OK;
  }

  // Don't dispatch a second select event if we are already handling
  // one.
  if (aVisitor.mEvent->mMessage == eFormSelect) {
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
  if (aVisitor.mEvent->mMessage == eMouseClick &&
      aVisitor.mEvent->AsMouseEvent()->button ==
        WidgetMouseEvent::eMiddleButton) {
    aVisitor.mEvent->mFlags.mNoContentDispatch = false;
  }

  if (aVisitor.mEvent->mMessage == eBlur) {
    // Set mWantsPreHandleEvent and fire change event in PreHandleEvent to
    // prevent it breaks event target chain creation.
    aVisitor.mWantsPreHandleEvent = true;
  }

  return nsGenericHTMLFormElementWithState::GetEventTargetParent(aVisitor);
}

nsresult
HTMLTextAreaElement::PreHandleEvent(EventChainVisitor& aVisitor)
{
  if (aVisitor.mEvent->mMessage == eBlur) {
    // Fire onchange (if necessary), before we do the blur, bug 370521.
    FireChangeEventIfNeeded();
  }
  return nsGenericHTMLFormElementWithState::PreHandleEvent(aVisitor);
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
HTMLTextAreaElement::PostHandleEvent(EventChainPostVisitor& aVisitor)
{
  if (aVisitor.mEvent->mMessage == eFormSelect) {
    mHandlingSelect = false;
  }

  if (aVisitor.mEvent->mMessage == eFocus ||
      aVisitor.mEvent->mMessage == eBlur) {
    if (aVisitor.mEvent->mMessage == eFocus) {
      // If the invalid UI is shown, we should show it while focusing (and
      // update). Otherwise, we should not.
      GetValueInternal(mFocusedValue, true);
      mCanShowInvalidUI = !IsValid() && ShouldShowValidityUI();

      // If neither invalid UI nor valid UI is shown, we shouldn't show the valid
      // UI while typing.
      mCanShowValidUI = ShouldShowValidityUI();
    } else { // eBlur
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
      nsresult rv = GenerateStateKey();
      if (NS_SUCCEEDED(rv)) {
        RestoreFormControlState();
      }
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

  return error.StealNSResult();
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
  Nullable<uint32_t> selStart(GetSelectionStart(error));
  if (error.Failed()) {
    return error.StealNSResult();
  }

  *aSelectionStart = int32_t(selStart.Value());
  return error.StealNSResult();
}

Nullable<uint32_t>
HTMLTextAreaElement::GetSelectionStart(ErrorResult& aError)
{
  int32_t selStart, selEnd;
  nsresult rv = GetSelectionRange(&selStart, &selEnd);

  if (NS_FAILED(rv) && mState.IsSelectionCached()) {
    return Nullable<uint32_t>(mState.GetSelectionProperties().GetStart());
  }
  if (NS_FAILED(rv)) {
    aError.Throw(rv);
  }
  return Nullable<uint32_t>(selStart);
}

NS_IMETHODIMP
HTMLTextAreaElement::SetSelectionStart(int32_t aSelectionStart)
{
  ErrorResult error;
  Nullable<uint32_t> selStart(aSelectionStart);
  SetSelectionStart(selStart, error);
  return error.StealNSResult();
}

void
HTMLTextAreaElement::SetSelectionStart(const Nullable<uint32_t>& aSelectionStart,
                                       ErrorResult& aError)
{
  int32_t selStart = 0;
  if (!aSelectionStart.IsNull()) {
    selStart = aSelectionStart.Value();
  }

  if (mState.IsSelectionCached()) {
    mState.GetSelectionProperties().SetStart(selStart);
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
  start = selStart;
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
  Nullable<uint32_t> selEnd(GetSelectionEnd(error));
  if (error.Failed()) {
    return error.StealNSResult();
  }

  *aSelectionEnd = int32_t(selEnd.Value());
  return NS_OK;
}

Nullable<uint32_t>
HTMLTextAreaElement::GetSelectionEnd(ErrorResult& aError)
{
  int32_t selStart, selEnd;
  nsresult rv = GetSelectionRange(&selStart, &selEnd);

  if (NS_FAILED(rv) && mState.IsSelectionCached()) {
    return Nullable<uint32_t>(mState.GetSelectionProperties().GetEnd());
  }
  if (NS_FAILED(rv)) {
    aError.Throw(rv);
  }
  return Nullable<uint32_t>(selEnd);
}

NS_IMETHODIMP
HTMLTextAreaElement::SetSelectionEnd(int32_t aSelectionEnd)
{
  ErrorResult error;
  Nullable<uint32_t> selEnd(aSelectionEnd);
  SetSelectionEnd(selEnd, error);
  return error.StealNSResult();
}

void
HTMLTextAreaElement::SetSelectionEnd(const Nullable<uint32_t>& aSelectionEnd,
                                     ErrorResult& aError)
{
  int32_t selEnd = 0;
  if (!aSelectionEnd.IsNull()) {
    selEnd = aSelectionEnd.Value();
  }

  if (mState.IsSelectionCached()) {
    mState.GetSelectionProperties().SetEnd(selEnd);
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
  end = selEnd;
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
  nsIFormControlFrame* formControlFrame = GetFormControlFrame(true);
  nsITextControlFrame* textControlFrame = do_QueryFrame(formControlFrame);
  if (textControlFrame) {
    return textControlFrame->GetSelectionRange(aSelectionStart, aSelectionEnd);
  }

  return NS_ERROR_FAILURE;
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
  return error.StealNSResult();
}

void
HTMLTextAreaElement::GetSelectionDirection(nsAString& aDirection, ErrorResult& aError)
{
  nsresult rv = NS_ERROR_FAILURE;
  nsIFormControlFrame* formControlFrame = GetFormControlFrame(true);
  nsITextControlFrame* textControlFrame = do_QueryFrame(formControlFrame);
  if (textControlFrame) {
    nsITextControlFrame::SelectionDirection dir;
    rv = textControlFrame->GetSelectionRange(nullptr, nullptr, &dir);
    if (NS_SUCCEEDED(rv)) {
      DirectionToName(dir, aDirection);
    }
  }

  if (NS_FAILED(rv)) {
    if (mState.IsSelectionCached()) {
      DirectionToName(mState.GetSelectionProperties().GetDirection(), aDirection);
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
  return error.StealNSResult();
}

void
HTMLTextAreaElement::SetSelectionDirection(const nsAString& aDirection,
                                           ErrorResult& aError)
{
  if (mState.IsSelectionCached()) {
    nsITextControlFrame::SelectionDirection dir = nsITextControlFrame::eNone;
    if (aDirection.EqualsLiteral("forward")) {
      dir = nsITextControlFrame::eForward;
    } else if (aDirection.EqualsLiteral("backward")) {
      dir = nsITextControlFrame::eBackward;
    }
    mState.GetSelectionProperties().SetDirection(dir);
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
  return error.StealNSResult();
}

void
HTMLTextAreaElement::SetSelectionRange(uint32_t aSelectionStart,
                                       uint32_t aSelectionEnd,
                                       const Optional<nsAString>& aDirection,
                                       ErrorResult& aError)
{
  nsresult rv = NS_ERROR_FAILURE;
  nsIFormControlFrame* formControlFrame = GetFormControlFrame(true);
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
      RefPtr<AsyncEventDispatcher> asyncDispatcher =
        new AsyncEventDispatcher(this, NS_LITERAL_STRING("select"),
                                 true, false);
      asyncDispatcher->PostDOMEvent();
    }
  }

  if (NS_FAILED(rv)) {
    aError.Throw(rv);
  }
}

void
HTMLTextAreaElement::SetRangeText(const nsAString& aReplacement,
                                  ErrorResult& aRv)
{
  int32_t start, end;
  aRv = GetSelectionRange(&start, &end);
  if (aRv.Failed()) {
    if (mState.IsSelectionCached()) {
      start = mState.GetSelectionProperties().GetStart();
      end = mState.GetSelectionProperties().GetEnd();
      aRv = NS_OK;
    }
  }

  SetRangeText(aReplacement, start, end, mozilla::dom::SelectionMode::Preserve,
               aRv, start, end);
}

void
HTMLTextAreaElement::SetRangeText(const nsAString& aReplacement,
                                  uint32_t aStart, uint32_t aEnd,
                                  const SelectionMode& aSelectMode,
                                  ErrorResult& aRv, int32_t aSelectionStart,
                                  int32_t aSelectionEnd)
{
  if (aStart > aEnd) {
    aRv.Throw(NS_ERROR_DOM_INDEX_SIZE_ERR);
    return;
  }

  nsAutoString value;
  GetValueInternal(value, false);
  uint32_t inputValueLength = value.Length();

  if (aStart > inputValueLength) {
    aStart = inputValueLength;
  }

  if (aEnd > inputValueLength) {
    aEnd = inputValueLength;
  }

  if (aSelectionStart == -1 && aSelectionEnd == -1) {
    aRv = GetSelectionRange(&aSelectionStart, &aSelectionEnd);
    if (aRv.Failed()) {
      if (mState.IsSelectionCached()) {
        aSelectionStart = mState.GetSelectionProperties().GetStart();
        aSelectionEnd = mState.GetSelectionProperties().GetEnd();
        aRv = NS_OK;
      }
    }
  }

  if (aStart <= aEnd) {
    value.Replace(aStart, aEnd - aStart, aReplacement);
    nsresult rv =
      SetValueInternal(value, nsTextEditorState::eSetValue_ByContent);
    if (NS_FAILED(rv)) {
      aRv.Throw(rv);
      return;
    }
  }

  uint32_t newEnd = aStart + aReplacement.Length();
  int32_t delta =  aReplacement.Length() - (aEnd - aStart);

  switch (aSelectMode) {
    case mozilla::dom::SelectionMode::Select:
    {
      aSelectionStart = aStart;
      aSelectionEnd = newEnd;
    }
    break;
    case mozilla::dom::SelectionMode::Start:
    {
      aSelectionStart = aSelectionEnd = aStart;
    }
    break;
    case mozilla::dom::SelectionMode::End:
    {
      aSelectionStart = aSelectionEnd = newEnd;
    }
    break;
    case mozilla::dom::SelectionMode::Preserve:
    {
      if ((uint32_t)aSelectionStart > aEnd) {
        aSelectionStart += delta;
      } else if ((uint32_t)aSelectionStart > aStart) {
        aSelectionStart = aStart;
      }

      if ((uint32_t)aSelectionEnd > aEnd) {
        aSelectionEnd += delta;
      } else if ((uint32_t)aSelectionEnd > aStart) {
        aSelectionEnd = newEnd;
      }
    }
    break;
    default:
      MOZ_CRASH("Unknown mode!");
  }

  Optional<nsAString> direction;
  SetSelectionRange(aSelectionStart, aSelectionEnd, direction, aRv);
}

nsresult
HTMLTextAreaElement::Reset()
{
  nsresult rv;

  // To get the initial spellchecking, reset value to
  // empty string before setting the default value.
  rv = SetValue(EmptyString());
  NS_ENSURE_SUCCESS(rv, rv);
  nsAutoString resetVal;
  GetDefaultValue(resetVal);
  rv = SetValue(resetVal);
  NS_ENSURE_SUCCESS(rv, rv);

  SetValueChanged(false);
  return NS_OK;
}

NS_IMETHODIMP
HTMLTextAreaElement::SubmitNamesValues(HTMLFormSubmission* aFormSubmission)
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
    state = GetPrimaryPresState();
    if (state) {
      nsAutoString value;
      GetValueInternal(value, true);

      rv = nsLinebreakConverter::ConvertStringLineBreaks(
               value,
               nsLinebreakConverter::eLinebreakPlatform,
               nsLinebreakConverter::eLinebreakContent);

      if (NS_FAILED(rv)) {
        NS_ERROR("Converting linebreaks failed!");
        return rv;
      }

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
      state = GetPrimaryPresState();
      rv = NS_OK;
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
    nsresult rv = SetValue(data);
    NS_ENSURE_SUCCESS(rv, false);
  }

  if (aState->IsDisabledSet()) {
    SetDisabled(aState->GetDisabled());
  }

  return false;
}

EventStates
HTMLTextAreaElement::IntrinsicState() const
{
  EventStates state = nsGenericHTMLFormElementWithState::IntrinsicState();

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
      IsValueEmpty()) {
    state |= NS_EVENT_STATE_PLACEHOLDERSHOWN;
  }

  return state;
}

nsresult
HTMLTextAreaElement::BindToTree(nsIDocument* aDocument, nsIContent* aParent,
                                nsIContent* aBindingParent,
                                bool aCompileEventHandlers)
{
  nsresult rv = nsGenericHTMLFormElementWithState::BindToTree(aDocument, aParent,
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
  nsGenericHTMLFormElementWithState::UnbindFromTree(aDeep, aNullParent);

  // We might be no longer disabled because of parent chain changed.
  UpdateValueMissingValidityState();
  UpdateBarredFromConstraintValidation();

  // And now make sure our state is up to date
  UpdateState(false);
}

nsresult
HTMLTextAreaElement::BeforeSetAttr(int32_t aNameSpaceID, nsIAtom* aName,
                                   nsAttrValueOrString* aValue,
                                   bool aNotify)
{
  if (aNotify && aName == nsGkAtoms::disabled &&
      aNameSpaceID == kNameSpaceID_None) {
    mDisabledChanged = true;
  }

  return nsGenericHTMLFormElementWithState::BeforeSetAttr(aNameSpaceID, aName,
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
    } else if (aName == nsGkAtoms::minlength) {
      UpdateTooShortValidityState();
    }

    UpdateState(aNotify);
  }

  return nsGenericHTMLFormElementWithState::AfterSetAttr(aNameSpaceID, aName, aValue,
                                                         aNotify);
  }

nsresult
HTMLTextAreaElement::CopyInnerTo(Element* aDest)
{
  nsresult rv = nsGenericHTMLFormElementWithState::CopyInnerTo(aDest);
  NS_ENSURE_SUCCESS(rv, rv);

  if (aDest->OwnerDoc()->IsStaticDocument()) {
    nsAutoString value;
    GetValueInternal(value, true);
    return static_cast<HTMLTextAreaElement*>(aDest)->SetValue(value);
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
  if (!mValueChanged ||
      !mLastValueChangeWasInteractive ||
      !HasAttr(kNameSpaceID_None, nsGkAtoms::maxlength)) {
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
HTMLTextAreaElement::IsTooShort()
{
  if (!mValueChanged ||
      !mLastValueChangeWasInteractive ||
      !HasAttr(kNameSpaceID_None, nsGkAtoms::minlength)) {
    return false;
  }

  int32_t minLength = -1;
  GetMinLength(&minLength);

  // Minlength of -1 means parsing error.
  if (minLength == -1) {
    return false;
  }

  int32_t textLength = -1;
  GetTextLength(&textLength);

  return textLength && textLength < minLength;
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
  SetValidityState(VALIDITY_STATE_TOO_LONG, IsTooLong());
}

void
HTMLTextAreaElement::UpdateTooShortValidityState()
{
  SetValidityState(VALIDITY_STATE_TOO_SHORT, IsTooShort());
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

        const char16_t* params[] = { strMaxLength.get(), strTextLength.get() };
        rv = nsContentUtils::FormatLocalizedString(nsContentUtils::eDOM_PROPERTIES,
                                                   "FormValidationTextTooLong",
                                                   params, message);
        aValidationMessage = message;
      }
      break;
    case VALIDITY_STATE_TOO_SHORT:
      {
        nsXPIDLString message;
        int32_t minLength = -1;
        int32_t textLength = -1;
        nsAutoString strMinLength;
        nsAutoString strTextLength;

        GetMinLength(&minLength);
        GetTextLength(&textLength);

        strMinLength.AppendInt(minLength);
        strTextLength.AppendInt(textLength);

        const char16_t* params[] = { strMinLength.get(), strTextLength.get() };
        rv = nsContentUtils::FormatLocalizedString(nsContentUtils::eDOM_PROPERTIES,
                                                   "FormValidationTextTooShort",
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
  return Cols();
}

NS_IMETHODIMP_(int32_t)
HTMLTextAreaElement::GetWrapCols()
{
  // wrap=off means -1 for wrap width no matter what cols is
  nsHTMLTextWrap wrapProp;
  nsITextControlElement::GetWrapPropertyEnum(this, wrapProp);
  if (wrapProp == nsITextControlElement::eHTMLTextWrap_Off) {
    // do not wrap when wrap=off
    return 0;
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
HTMLTextAreaElement::OnValueChanged(bool aNotify, bool aWasInteractiveUserChange)
{
  mLastValueChangeWasInteractive = aWasInteractiveUserChange;

  // Update the validity state
  bool validBefore = IsValid();
  UpdateTooLongValidityState();
  UpdateTooShortValidityState();
  UpdateValueMissingValidityState();

  if (validBefore != IsValid() ||
      HasAttr(kNameSpaceID_None, nsGkAtoms::placeholder)) {
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

  nsGenericHTMLFormElementWithState::FieldSetDisabledChanged(aNotify);
}

JSObject*
HTMLTextAreaElement::WrapNode(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return HTMLTextAreaElementBinding::Wrap(aCx, this, aGivenProto);
}

} // namespace dom
} // namespace mozilla
