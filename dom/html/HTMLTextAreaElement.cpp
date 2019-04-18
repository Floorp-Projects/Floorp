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
#include "mozilla/MappedDeclarations.h"
#include "mozilla/MouseEvents.h"
#include "nsAttrValueInlines.h"
#include "nsBaseCommandController.h"
#include "nsContentCID.h"
#include "nsContentCreatorFunctions.h"
#include "nsError.h"
#include "nsFocusManager.h"
#include "nsIComponentManager.h"
#include "nsIConstraintValidation.h"
#include "nsIControllers.h"
#include "mozilla/dom/Document.h"
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
#include "mozilla/PresState.h"
#include "nsReadableUtils.h"
#include "nsStyleConsts.h"
#include "nsTextEditorState.h"
#include "nsBaseCommandController.h"
#include "nsXULControllers.h"

NS_IMPL_NS_NEW_HTML_ELEMENT_CHECK_PARSER(TextArea)

namespace mozilla {
namespace dom {

HTMLTextAreaElement::HTMLTextAreaElement(
    already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo,
    FromParser aFromParser)
    : nsGenericHTMLFormElementWithState(std::move(aNodeInfo), NS_FORM_TEXTAREA),
      mValueChanged(false),
      mLastValueChangeWasInteractive(false),
      mHandlingSelect(false),
      mDoneAddingChildren(!aFromParser),
      mInhibitStateRestoration(!!(aFromParser & FROM_PARSER_FRAGMENT)),
      mDisabledChanged(false),
      mCanShowInvalidUI(true),
      mCanShowValidUI(true),
      mIsPreviewEnabled(false),
      mAutocompleteAttrState(nsContentUtils::eAutocompleteAttrState_Unknown),
      mState(this) {
  AddMutationObserver(this);

  // Set up our default state.  By default we're enabled (since we're
  // a control type that can be disabled but not actually disabled
  // right now), optional, and valid.  We are NOT readwrite by default
  // until someone calls UpdateEditableState on us, apparently!  Also
  // by default we don't have to show validity UI and so forth.
  AddStatesSilently(NS_EVENT_STATE_ENABLED | NS_EVENT_STATE_OPTIONAL |
                    NS_EVENT_STATE_VALID);
}

NS_IMPL_CYCLE_COLLECTION_INHERITED(HTMLTextAreaElement,
                                   nsGenericHTMLFormElementWithState, mValidity,
                                   mControllers, mState)

NS_IMPL_ISUPPORTS_CYCLE_COLLECTION_INHERITED(HTMLTextAreaElement,
                                             nsGenericHTMLFormElementWithState,
                                             nsITextControlElement,
                                             nsIMutationObserver,
                                             nsIConstraintValidation)

// nsIDOMHTMLTextAreaElement

nsresult HTMLTextAreaElement::Clone(dom::NodeInfo* aNodeInfo,
                                    nsINode** aResult) const {
  *aResult = nullptr;
  RefPtr<HTMLTextAreaElement> it =
      new HTMLTextAreaElement(do_AddRef(aNodeInfo));

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

// nsIContent

void HTMLTextAreaElement::Select() {
  // XXX Bug?  We have to give the input focus before contents can be
  // selected

  FocusTristate state = FocusState();
  if (state == eUnfocusable) {
    return;
  }

  nsFocusManager* fm = nsFocusManager::GetFocusManager();

  RefPtr<nsPresContext> presContext = GetPresContext(eForComposedDoc);
  if (state == eInactiveWindow) {
    if (fm) fm->SetFocus(this, nsIFocusManager::FLAG_NOSCROLL);
    SelectAll(presContext);
    return;
  }

  nsEventStatus status = nsEventStatus_eIgnore;
  WidgetGUIEvent event(true, eFormSelect, nullptr);
  // XXXbz HTMLInputElement guards against this reentering; shouldn't we?
  EventDispatcher::Dispatch(static_cast<nsIContent*>(this), presContext, &event,
                            nullptr, &status);

  // If the DOM event was not canceled (e.g. by a JS event handler
  // returning false)
  if (status == nsEventStatus_eIgnore) {
    if (fm) {
      fm->SetFocus(this, nsIFocusManager::FLAG_NOSCROLL);

      // ensure that the element is actually focused
      if (this == fm->GetFocusedElement()) {
        // Now Select all the text!
        SelectAll(presContext);
      }
    }
  }
}

NS_IMETHODIMP
HTMLTextAreaElement::SelectAll(nsPresContext* aPresContext) {
  nsIFormControlFrame* formControlFrame = GetFormControlFrame(true);

  if (formControlFrame) {
    formControlFrame->SetFormProperty(nsGkAtoms::select, EmptyString());
  }

  return NS_OK;
}

bool HTMLTextAreaElement::IsHTMLFocusable(bool aWithMouse, bool* aIsFocusable,
                                          int32_t* aTabIndex) {
  if (nsGenericHTMLFormElementWithState::IsHTMLFocusable(
          aWithMouse, aIsFocusable, aTabIndex)) {
    return true;
  }

  // disabled textareas are not focusable
  *aIsFocusable = !IsDisabled();
  return false;
}

int32_t HTMLTextAreaElement::TabIndexDefault() { return 0; }

void HTMLTextAreaElement::GetType(nsAString& aType) {
  aType.AssignLiteral("textarea");
}

void HTMLTextAreaElement::GetValue(nsAString& aValue) {
  nsAutoString value;
  GetValueInternal(value, true);

  // Normalize CRLF and CR to LF
  nsContentUtils::PlatformToDOMLineBreaks(value);

  aValue = value;
}

void HTMLTextAreaElement::GetValueInternal(nsAString& aValue,
                                           bool aIgnoreWrap) const {
  mState.GetValue(aValue, aIgnoreWrap);
}

NS_IMETHODIMP_(TextEditor*)
HTMLTextAreaElement::GetTextEditor() { return mState.GetTextEditor(); }

NS_IMETHODIMP_(TextEditor*)
HTMLTextAreaElement::GetTextEditorWithoutCreation() {
  return mState.GetTextEditorWithoutCreation();
}

NS_IMETHODIMP_(nsISelectionController*)
HTMLTextAreaElement::GetSelectionController() {
  return mState.GetSelectionController();
}

NS_IMETHODIMP_(nsFrameSelection*)
HTMLTextAreaElement::GetConstFrameSelection() {
  return mState.GetConstFrameSelection();
}

NS_IMETHODIMP
HTMLTextAreaElement::BindToFrame(nsTextControlFrame* aFrame) {
  return mState.BindToFrame(aFrame);
}

NS_IMETHODIMP_(void)
HTMLTextAreaElement::UnbindFromFrame(nsTextControlFrame* aFrame) {
  if (aFrame) {
    mState.UnbindFromFrame(aFrame);
  }
}

NS_IMETHODIMP
HTMLTextAreaElement::CreateEditor() { return mState.PrepareEditor(); }

NS_IMETHODIMP_(void)
HTMLTextAreaElement::UpdateOverlayTextVisibility(bool aNotify) {
  mState.UpdateOverlayTextVisibility(aNotify);
}

NS_IMETHODIMP_(bool)
HTMLTextAreaElement::GetPlaceholderVisibility() {
  return mState.GetPlaceholderVisibility();
}

NS_IMETHODIMP_(void)
HTMLTextAreaElement::SetPreviewValue(const nsAString& aValue) {
  mState.SetPreviewText(aValue, true);
}

NS_IMETHODIMP_(void)
HTMLTextAreaElement::GetPreviewValue(nsAString& aValue) {
  mState.GetPreviewText(aValue);
}

NS_IMETHODIMP_(void)
HTMLTextAreaElement::EnablePreview() {
  if (mIsPreviewEnabled) {
    return;
  }

  mIsPreviewEnabled = true;
  // Reconstruct the frame to append an anonymous preview node
  nsLayoutUtils::PostRestyleEvent(this, RestyleHint{0},
                                  nsChangeHint_ReconstructFrame);
}

NS_IMETHODIMP_(bool)
HTMLTextAreaElement::IsPreviewEnabled() { return mIsPreviewEnabled; }

NS_IMETHODIMP_(bool)
HTMLTextAreaElement::GetPreviewVisibility() {
  return mState.GetPreviewVisibility();
}

nsresult HTMLTextAreaElement::SetValueInternal(const nsAString& aValue,
                                               uint32_t aFlags) {
  // Need to set the value changed flag here if our value has in fact changed
  // (i.e. if eSetValue_Notify is in aFlags), so that
  // nsTextControlFrame::UpdateValueDisplay retrieves the correct value if
  // needed.
  if (aFlags & nsTextEditorState::eSetValue_Notify) {
    SetValueChanged(true);
  }

  if (!mState.SetValue(aValue, aFlags)) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  return NS_OK;
}

void HTMLTextAreaElement::SetValue(const nsAString& aValue,
                                   ErrorResult& aError) {
  // If the value has been set by a script, we basically want to keep the
  // current change event state. If the element is ready to fire a change
  // event, we should keep it that way. Otherwise, we should make sure the
  // element will not fire any event because of the script interaction.
  //
  // NOTE: this is currently quite expensive work (too much string
  // manipulation). We should probably optimize that.
  nsAutoString currentValue;
  GetValueInternal(currentValue, true);

  nsresult rv = SetValueInternal(
      aValue, nsTextEditorState::eSetValue_ByContent |
                  nsTextEditorState::eSetValue_Notify |
                  nsTextEditorState::eSetValue_MoveCursorToEndIfValueChanged);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    aError.Throw(rv);
    return;
  }

  if (mFocusedValue.Equals(currentValue)) {
    GetValueInternal(mFocusedValue, true);
  }
}

void HTMLTextAreaElement::SetUserInput(const nsAString& aValue,
                                       nsIPrincipal& aSubjectPrincipal) {
  SetValueInternal(
      aValue, nsTextEditorState::eSetValue_BySetUserInput |
                  nsTextEditorState::eSetValue_Notify |
                  nsTextEditorState::eSetValue_MoveCursorToEndIfValueChanged);
}

NS_IMETHODIMP
HTMLTextAreaElement::SetValueChanged(bool aValueChanged) {
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

void HTMLTextAreaElement::GetDefaultValue(nsAString& aDefaultValue,
                                          ErrorResult& aError) {
  if (!nsContentUtils::GetNodeTextContent(this, false, aDefaultValue,
                                          fallible)) {
    aError.Throw(NS_ERROR_OUT_OF_MEMORY);
  }
}

void HTMLTextAreaElement::SetDefaultValue(const nsAString& aDefaultValue,
                                          ErrorResult& aError) {
  nsresult rv = nsContentUtils::SetNodeTextContent(this, aDefaultValue, true);
  if (NS_SUCCEEDED(rv) && !mValueChanged) {
    Reset();
  }
  if (NS_FAILED(rv)) {
    aError.Throw(rv);
  }
}

bool HTMLTextAreaElement::ParseAttribute(int32_t aNamespaceID,
                                         nsAtom* aAttribute,
                                         const nsAString& aValue,
                                         nsIPrincipal* aMaybeScriptedPrincipal,
                                         nsAttrValue& aResult) {
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
    } else if (aAttribute == nsGkAtoms::autocomplete) {
      aResult.ParseAtomArray(aValue);
      return true;
    }
  }
  return nsGenericHTMLElement::ParseAttribute(aNamespaceID, aAttribute, aValue,
                                              aMaybeScriptedPrincipal, aResult);
}

void HTMLTextAreaElement::MapAttributesIntoRule(
    const nsMappedAttributes* aAttributes, MappedDeclarations& aDecls) {
  // wrap=off
  if (!aDecls.PropertyIsSet(eCSSProperty_white_space)) {
    const nsAttrValue* value = aAttributes->GetAttr(nsGkAtoms::wrap);
    if (value && value->Type() == nsAttrValue::eString &&
        value->Equals(nsGkAtoms::OFF, eIgnoreCase)) {
      aDecls.SetKeywordValue(eCSSProperty_white_space, StyleWhiteSpace::Pre);
    }
  }

  nsGenericHTMLFormElementWithState::MapDivAlignAttributeInto(aAttributes,
                                                              aDecls);
  nsGenericHTMLFormElementWithState::MapCommonAttributesInto(aAttributes,
                                                             aDecls);
}

nsChangeHint HTMLTextAreaElement::GetAttributeChangeHint(
    const nsAtom* aAttribute, int32_t aModType) const {
  nsChangeHint retval =
      nsGenericHTMLFormElementWithState::GetAttributeChangeHint(aAttribute,
                                                                aModType);
  if (aAttribute == nsGkAtoms::rows || aAttribute == nsGkAtoms::cols) {
    retval |= NS_STYLE_HINT_REFLOW;
  } else if (aAttribute == nsGkAtoms::wrap) {
    retval |= nsChangeHint_ReconstructFrame;
  } else if (aAttribute == nsGkAtoms::placeholder) {
    retval |= nsChangeHint_ReconstructFrame;
  }
  return retval;
}

NS_IMETHODIMP_(bool)
HTMLTextAreaElement::IsAttributeMapped(const nsAtom* aAttribute) const {
  static const MappedAttributeEntry attributes[] = {{nsGkAtoms::wrap},
                                                    {nullptr}};

  static const MappedAttributeEntry* const map[] = {
      attributes,
      sDivAlignAttributeMap,
      sCommonAttributeMap,
  };

  return FindAttributeDependence(aAttribute, map);
}

nsMapRuleToAttributesFunc HTMLTextAreaElement::GetAttributeMappingFunction()
    const {
  return &MapAttributesIntoRule;
}

bool HTMLTextAreaElement::IsDisabledForEvents(WidgetEvent* aEvent) {
  nsIFormControlFrame* formControlFrame = GetFormControlFrame(false);
  nsIFrame* formFrame = do_QueryFrame(formControlFrame);
  return IsElementDisabledForEvents(aEvent, formFrame);
}

void HTMLTextAreaElement::GetEventTargetParent(EventChainPreVisitor& aVisitor) {
  aVisitor.mCanHandle = false;
  if (IsDisabledForEvents(aVisitor.mEvent)) {
    return;
  }

  // Don't dispatch a second select event if we are already handling
  // one.
  if (aVisitor.mEvent->mMessage == eFormSelect) {
    if (mHandlingSelect) {
      return;
    }
    mHandlingSelect = true;
  }

  if (aVisitor.mEvent->mMessage == eBlur) {
    // Set mWantsPreHandleEvent and fire change event in PreHandleEvent to
    // prevent it breaks event target chain creation.
    aVisitor.mWantsPreHandleEvent = true;
  }

  nsGenericHTMLFormElementWithState::GetEventTargetParent(aVisitor);
}

nsresult HTMLTextAreaElement::PreHandleEvent(EventChainVisitor& aVisitor) {
  if (aVisitor.mEvent->mMessage == eBlur) {
    // Fire onchange (if necessary), before we do the blur, bug 370521.
    FireChangeEventIfNeeded();
  }
  return nsGenericHTMLFormElementWithState::PreHandleEvent(aVisitor);
}

void HTMLTextAreaElement::FireChangeEventIfNeeded() {
  nsString value;
  GetValueInternal(value, true);

  if (mFocusedValue.Equals(value)) {
    return;
  }

  // Dispatch the change event.
  mFocusedValue = value;
  nsContentUtils::DispatchTrustedEvent(
      OwnerDoc(), static_cast<nsIContent*>(this), NS_LITERAL_STRING("change"),
      CanBubble::eYes, Cancelable::eNo);
}

nsresult HTMLTextAreaElement::PostHandleEvent(EventChainPostVisitor& aVisitor) {
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

      // If neither invalid UI nor valid UI is shown, we shouldn't show the
      // valid UI while typing.
      mCanShowValidUI = ShouldShowValidityUI();
    } else {  // eBlur
      mCanShowInvalidUI = true;
      mCanShowValidUI = true;
    }

    UpdateState(true);
  }

  return NS_OK;
}

void HTMLTextAreaElement::DoneAddingChildren(bool aHaveNotified) {
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

bool HTMLTextAreaElement::IsDoneAddingChildren() { return mDoneAddingChildren; }

// Controllers Methods

nsIControllers* HTMLTextAreaElement::GetControllers(ErrorResult& aError) {
  if (!mControllers) {
    mControllers = new nsXULControllers();
    if (!mControllers) {
      aError.Throw(NS_ERROR_FAILURE);
      return nullptr;
    }

    RefPtr<nsBaseCommandController> commandController =
        nsBaseCommandController::CreateEditorController();
    if (!commandController) {
      aError.Throw(NS_ERROR_FAILURE);
      return nullptr;
    }

    mControllers->AppendController(commandController);

    commandController = nsBaseCommandController::CreateEditingController();
    if (!commandController) {
      aError.Throw(NS_ERROR_FAILURE);
      return nullptr;
    }

    mControllers->AppendController(commandController);
  }

  return mControllers;
}

nsresult HTMLTextAreaElement::GetControllers(nsIControllers** aResult) {
  NS_ENSURE_ARG_POINTER(aResult);

  ErrorResult error;
  *aResult = GetControllers(error);
  NS_IF_ADDREF(*aResult);

  return error.StealNSResult();
}

uint32_t HTMLTextAreaElement::GetTextLength() {
  nsAutoString val;
  GetValue(val);
  return val.Length();
}

Nullable<uint32_t> HTMLTextAreaElement::GetSelectionStart(ErrorResult& aError) {
  uint32_t selStart, selEnd;
  GetSelectionRange(&selStart, &selEnd, aError);
  return Nullable<uint32_t>(selStart);
}

void HTMLTextAreaElement::SetSelectionStart(
    const Nullable<uint32_t>& aSelectionStart, ErrorResult& aError) {
  mState.SetSelectionStart(aSelectionStart, aError);
}

Nullable<uint32_t> HTMLTextAreaElement::GetSelectionEnd(ErrorResult& aError) {
  uint32_t selStart, selEnd;
  GetSelectionRange(&selStart, &selEnd, aError);
  return Nullable<uint32_t>(selEnd);
}

void HTMLTextAreaElement::SetSelectionEnd(
    const Nullable<uint32_t>& aSelectionEnd, ErrorResult& aError) {
  mState.SetSelectionEnd(aSelectionEnd, aError);
}

void HTMLTextAreaElement::GetSelectionRange(uint32_t* aSelectionStart,
                                            uint32_t* aSelectionEnd,
                                            ErrorResult& aRv) {
  return mState.GetSelectionRange(aSelectionStart, aSelectionEnd, aRv);
}

void HTMLTextAreaElement::GetSelectionDirection(nsAString& aDirection,
                                                ErrorResult& aError) {
  mState.GetSelectionDirectionString(aDirection, aError);
}

void HTMLTextAreaElement::SetSelectionDirection(const nsAString& aDirection,
                                                ErrorResult& aError) {
  mState.SetSelectionDirection(aDirection, aError);
}

void HTMLTextAreaElement::SetSelectionRange(
    uint32_t aSelectionStart, uint32_t aSelectionEnd,
    const Optional<nsAString>& aDirection, ErrorResult& aError) {
  mState.SetSelectionRange(aSelectionStart, aSelectionEnd, aDirection, aError);
}

void HTMLTextAreaElement::SetRangeText(const nsAString& aReplacement,
                                       ErrorResult& aRv) {
  mState.SetRangeText(aReplacement, aRv);
}

void HTMLTextAreaElement::SetRangeText(const nsAString& aReplacement,
                                       uint32_t aStart, uint32_t aEnd,
                                       SelectionMode aSelectMode,
                                       ErrorResult& aRv) {
  mState.SetRangeText(aReplacement, aStart, aEnd, aSelectMode, aRv);
}

void HTMLTextAreaElement::GetValueFromSetRangeText(nsAString& aValue) {
  GetValueInternal(aValue, false);
}

nsresult HTMLTextAreaElement::SetValueFromSetRangeText(
    const nsAString& aValue) {
  return SetValueInternal(aValue, nsTextEditorState::eSetValue_ByContent |
                                      nsTextEditorState::eSetValue_Notify);
}

nsresult HTMLTextAreaElement::Reset() {
  nsAutoString resetVal;
  GetDefaultValue(resetVal, IgnoreErrors());
  SetValueChanged(false);

  nsresult rv =
      SetValueInternal(resetVal, nsTextEditorState::eSetValue_Internal);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

NS_IMETHODIMP
HTMLTextAreaElement::SubmitNamesValues(HTMLFormSubmission* aFormSubmission) {
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
HTMLTextAreaElement::SaveState() {
  nsresult rv = NS_OK;

  // Only save if value != defaultValue (bug 62713)
  PresState* state = nullptr;
  if (mValueChanged) {
    state = GetPrimaryPresState();
    if (state) {
      nsAutoString value;
      GetValueInternal(value, true);

      rv = nsLinebreakConverter::ConvertStringLineBreaks(
          value, nsLinebreakConverter::eLinebreakPlatform,
          nsLinebreakConverter::eLinebreakContent);

      if (NS_FAILED(rv)) {
        NS_ERROR("Converting linebreaks failed!");
        return rv;
      }

      state->contentData() = std::move(value);
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
      state->disabled() = HasAttr(kNameSpaceID_None, nsGkAtoms::disabled);
      state->disabledSet() = true;
    }
  }
  return rv;
}

bool HTMLTextAreaElement::RestoreState(PresState* aState) {
  const PresContentData& state = aState->contentData();

  if (state.type() == PresContentData::TnsString) {
    ErrorResult rv;
    SetValue(state.get_nsString(), rv);
    ENSURE_SUCCESS(rv, false);
  }

  if (aState->disabledSet() && !aState->disabled()) {
    SetDisabled(false, IgnoreErrors());
  }

  return false;
}

EventStates HTMLTextAreaElement::IntrinsicState() const {
  EventStates state = nsGenericHTMLFormElementWithState::IntrinsicState();

  if (IsCandidateForConstraintValidation()) {
    if (IsValid()) {
      state |= NS_EVENT_STATE_VALID;
    } else {
      state |= NS_EVENT_STATE_INVALID;
      // :-moz-ui-invalid always apply if the element suffers from a custom
      // error and never applies if novalidate is set on the form owner.
      if ((!mForm ||
           !mForm->HasAttr(kNameSpaceID_None, nsGkAtoms::novalidate)) &&
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

  if (HasAttr(kNameSpaceID_None, nsGkAtoms::placeholder) && IsValueEmpty()) {
    state |= NS_EVENT_STATE_PLACEHOLDERSHOWN;
  }

  return state;
}

nsresult HTMLTextAreaElement::BindToTree(Document* aDocument,
                                         nsIContent* aParent,
                                         nsIContent* aBindingParent) {
  nsresult rv = nsGenericHTMLFormElementWithState::BindToTree(
      aDocument, aParent, aBindingParent);
  NS_ENSURE_SUCCESS(rv, rv);

  // If there is a disabled fieldset in the parent chain, the element is now
  // barred from constraint validation and can't suffer from value missing.
  UpdateValueMissingValidityState();
  UpdateBarredFromConstraintValidation();

  // And now make sure our state is up to date
  UpdateState(false);

  return rv;
}

void HTMLTextAreaElement::UnbindFromTree(bool aDeep, bool aNullParent) {
  nsGenericHTMLFormElementWithState::UnbindFromTree(aDeep, aNullParent);

  // We might be no longer disabled because of parent chain changed.
  UpdateValueMissingValidityState();
  UpdateBarredFromConstraintValidation();

  // And now make sure our state is up to date
  UpdateState(false);
}

nsresult HTMLTextAreaElement::BeforeSetAttr(int32_t aNameSpaceID, nsAtom* aName,
                                            const nsAttrValueOrString* aValue,
                                            bool aNotify) {
  if (aNotify && aName == nsGkAtoms::disabled &&
      aNameSpaceID == kNameSpaceID_None) {
    mDisabledChanged = true;
  }

  return nsGenericHTMLFormElementWithState::BeforeSetAttr(aNameSpaceID, aName,
                                                          aValue, aNotify);
}

void HTMLTextAreaElement::CharacterDataChanged(nsIContent* aContent,
                                               const CharacterDataChangeInfo&) {
  ContentChanged(aContent);
}

void HTMLTextAreaElement::ContentAppended(nsIContent* aFirstNewContent) {
  ContentChanged(aFirstNewContent);
}

void HTMLTextAreaElement::ContentInserted(nsIContent* aChild) {
  ContentChanged(aChild);
}

void HTMLTextAreaElement::ContentRemoved(nsIContent* aChild,
                                         nsIContent* aPreviousSibling) {
  ContentChanged(aChild);
}

void HTMLTextAreaElement::ContentChanged(nsIContent* aContent) {
  if (!mValueChanged && mDoneAddingChildren &&
      nsContentUtils::IsInSameAnonymousTree(this, aContent)) {
    // Hard to say what the reset can trigger, so be safe pending
    // further auditing.
    nsCOMPtr<nsIMutationObserver> kungFuDeathGrip(this);
    Reset();
  }
}

nsresult HTMLTextAreaElement::AfterSetAttr(int32_t aNameSpaceID, nsAtom* aName,
                                           const nsAttrValue* aValue,
                                           const nsAttrValue* aOldValue,
                                           nsIPrincipal* aSubjectPrincipal,
                                           bool aNotify) {
  if (aNameSpaceID == kNameSpaceID_None) {
    if (aName == nsGkAtoms::required || aName == nsGkAtoms::disabled ||
        aName == nsGkAtoms::readonly) {
      if (aName == nsGkAtoms::disabled) {
        // This *has* to be called *before* validity state check because
        // UpdateBarredFromConstraintValidation and
        // UpdateValueMissingValidityState depend on our disabled state.
        UpdateDisabledState(aNotify);
      }

      if (aName == nsGkAtoms::required) {
        // This *has* to be called *before* UpdateValueMissingValidityState
        // because UpdateValueMissingValidityState depends on our required
        // state.
        UpdateRequiredState(!!aValue, aNotify);
      }

      UpdateValueMissingValidityState();

      // This *has* to be called *after* validity has changed.
      if (aName == nsGkAtoms::readonly || aName == nsGkAtoms::disabled) {
        UpdateBarredFromConstraintValidation();
      }
    } else if (aName == nsGkAtoms::autocomplete) {
      // Clear the cached @autocomplete attribute state.
      mAutocompleteAttrState = nsContentUtils::eAutocompleteAttrState_Unknown;
    } else if (aName == nsGkAtoms::maxlength) {
      UpdateTooLongValidityState();
    } else if (aName == nsGkAtoms::minlength) {
      UpdateTooShortValidityState();
    }
  }

  return nsGenericHTMLFormElementWithState::AfterSetAttr(
      aNameSpaceID, aName, aValue, aOldValue, aSubjectPrincipal, aNotify);
}

nsresult HTMLTextAreaElement::CopyInnerTo(Element* aDest) {
  nsresult rv = nsGenericHTMLFormElementWithState::CopyInnerTo(aDest);
  NS_ENSURE_SUCCESS(rv, rv);

  if (aDest->OwnerDoc()->IsStaticDocument()) {
    nsAutoString value;
    GetValueInternal(value, true);
    ErrorResult ret;
    static_cast<HTMLTextAreaElement*>(aDest)->SetValue(value, ret);
    return ret.StealNSResult();
  }
  return NS_OK;
}

bool HTMLTextAreaElement::IsMutable() const {
  return (!HasAttr(kNameSpaceID_None, nsGkAtoms::readonly) && !IsDisabled());
}

bool HTMLTextAreaElement::IsValueEmpty() const {
  nsAutoString value;
  GetValueInternal(value, true);

  return value.IsEmpty();
}

void HTMLTextAreaElement::SetCustomValidity(const nsAString& aError) {
  nsIConstraintValidation::SetCustomValidity(aError);

  UpdateState(true);
}

bool HTMLTextAreaElement::IsTooLong() {
  if (!mValueChanged || !mLastValueChangeWasInteractive ||
      !HasAttr(kNameSpaceID_None, nsGkAtoms::maxlength)) {
    return false;
  }

  int32_t maxLength = MaxLength();

  // Maxlength of -1 means parsing error.
  if (maxLength == -1) {
    return false;
  }

  int32_t textLength = GetTextLength();

  return textLength > maxLength;
}

bool HTMLTextAreaElement::IsTooShort() {
  if (!mValueChanged || !mLastValueChangeWasInteractive ||
      !HasAttr(kNameSpaceID_None, nsGkAtoms::minlength)) {
    return false;
  }

  int32_t minLength = MinLength();

  // Minlength of -1 means parsing error.
  if (minLength == -1) {
    return false;
  }

  int32_t textLength = GetTextLength();

  return textLength && textLength < minLength;
}

bool HTMLTextAreaElement::IsValueMissing() const {
  if (!Required() || !IsMutable()) {
    return false;
  }

  return IsValueEmpty();
}

void HTMLTextAreaElement::UpdateTooLongValidityState() {
  SetValidityState(VALIDITY_STATE_TOO_LONG, IsTooLong());
}

void HTMLTextAreaElement::UpdateTooShortValidityState() {
  SetValidityState(VALIDITY_STATE_TOO_SHORT, IsTooShort());
}

void HTMLTextAreaElement::UpdateValueMissingValidityState() {
  SetValidityState(VALIDITY_STATE_VALUE_MISSING, IsValueMissing());
}

void HTMLTextAreaElement::UpdateBarredFromConstraintValidation() {
  SetBarredFromConstraintValidation(
      HasAttr(kNameSpaceID_None, nsGkAtoms::readonly) || IsDisabled());
}

nsresult HTMLTextAreaElement::GetValidationMessage(
    nsAString& aValidationMessage, ValidityStateType aType) {
  nsresult rv = NS_OK;

  switch (aType) {
    case VALIDITY_STATE_TOO_LONG: {
      nsAutoString message;
      int32_t maxLength = MaxLength();
      int32_t textLength = GetTextLength();
      nsAutoString strMaxLength;
      nsAutoString strTextLength;

      strMaxLength.AppendInt(maxLength);
      strTextLength.AppendInt(textLength);

      const char16_t* params[] = {strMaxLength.get(), strTextLength.get()};
      rv = nsContentUtils::FormatLocalizedString(
          nsContentUtils::eDOM_PROPERTIES, "FormValidationTextTooLong", params,
          message);
      aValidationMessage = message;
    } break;
    case VALIDITY_STATE_TOO_SHORT: {
      nsAutoString message;
      int32_t minLength = MinLength();
      int32_t textLength = GetTextLength();
      nsAutoString strMinLength;
      nsAutoString strTextLength;

      strMinLength.AppendInt(minLength);
      strTextLength.AppendInt(textLength);

      const char16_t* params[] = {strMinLength.get(), strTextLength.get()};
      rv = nsContentUtils::FormatLocalizedString(
          nsContentUtils::eDOM_PROPERTIES, "FormValidationTextTooShort", params,
          message);
      aValidationMessage = message;
    } break;
    case VALIDITY_STATE_VALUE_MISSING: {
      nsAutoString message;
      rv = nsContentUtils::GetLocalizedString(nsContentUtils::eDOM_PROPERTIES,
                                              "FormValidationValueMissing",
                                              message);
      aValidationMessage = message;
    } break;
    default:
      rv = nsIConstraintValidation::GetValidationMessage(aValidationMessage,
                                                         aType);
  }

  return rv;
}

NS_IMETHODIMP_(bool)
HTMLTextAreaElement::IsSingleLineTextControl() const { return false; }

NS_IMETHODIMP_(bool)
HTMLTextAreaElement::IsTextArea() const { return true; }

NS_IMETHODIMP_(bool)
HTMLTextAreaElement::IsPasswordTextControl() const { return false; }

NS_IMETHODIMP_(int32_t)
HTMLTextAreaElement::GetCols() { return Cols(); }

NS_IMETHODIMP_(int32_t)
HTMLTextAreaElement::GetWrapCols() {
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
HTMLTextAreaElement::GetRows() {
  const nsAttrValue* attr = GetParsedAttr(nsGkAtoms::rows);
  if (attr && attr->Type() == nsAttrValue::eInteger) {
    int32_t rows = attr->GetIntegerValue();
    return (rows <= 0) ? DEFAULT_ROWS_TEXTAREA : rows;
  }

  return DEFAULT_ROWS_TEXTAREA;
}

NS_IMETHODIMP_(void)
HTMLTextAreaElement::GetDefaultValueFromContent(nsAString& aValue) {
  GetDefaultValue(aValue, IgnoreErrors());
}

NS_IMETHODIMP_(bool)
HTMLTextAreaElement::ValueChanged() const { return mValueChanged; }

NS_IMETHODIMP_(void)
HTMLTextAreaElement::GetTextEditorValue(nsAString& aValue,
                                        bool aIgnoreWrap) const {
  mState.GetValue(aValue, aIgnoreWrap);
}

NS_IMETHODIMP_(void)
HTMLTextAreaElement::InitializeKeyboardEventListeners() {
  mState.InitializeKeyboardEventListeners();
}

NS_IMETHODIMP_(void)
HTMLTextAreaElement::OnValueChanged(bool aNotify, ValueChangeKind aKind) {
  if (aKind != ValueChangeKind::Internal) {
    mLastValueChangeWasInteractive = aKind == ValueChangeKind::UserInteraction;
  }

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
HTMLTextAreaElement::HasCachedSelection() { return mState.IsSelectionCached(); }

void HTMLTextAreaElement::FieldSetDisabledChanged(bool aNotify) {
  // This *has* to be called before UpdateBarredFromConstraintValidation and
  // UpdateValueMissingValidityState because these two functions depend on our
  // disabled state.
  nsGenericHTMLFormElementWithState::FieldSetDisabledChanged(aNotify);

  UpdateValueMissingValidityState();
  UpdateBarredFromConstraintValidation();
  UpdateState(aNotify);
}

JSObject* HTMLTextAreaElement::WrapNode(JSContext* aCx,
                                        JS::Handle<JSObject*> aGivenProto) {
  return HTMLTextAreaElement_Binding::Wrap(aCx, this, aGivenProto);
}

void HTMLTextAreaElement::GetAutocomplete(DOMString& aValue) {
  const nsAttrValue* attributeVal = GetParsedAttr(nsGkAtoms::autocomplete);

  mAutocompleteAttrState = nsContentUtils::SerializeAutocompleteAttribute(
      attributeVal, aValue, mAutocompleteAttrState);
}

}  // namespace dom
}  // namespace mozilla
