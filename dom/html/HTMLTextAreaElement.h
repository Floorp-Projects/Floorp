/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_HTMLTextAreaElement_h
#define mozilla_dom_HTMLTextAreaElement_h

#include "mozilla/Attributes.h"
#include "mozilla/TextControlElement.h"
#include "mozilla/TextControlState.h"
#include "mozilla/TextEditor.h"
#include "mozilla/dom/ConstraintValidation.h"
#include "mozilla/dom/HTMLFormElement.h"
#include "mozilla/dom/HTMLInputElementBinding.h"
#include "nsIControllers.h"
#include "nsCOMPtr.h"
#include "nsGenericHTMLElement.h"
#include "nsStubMutationObserver.h"
#include "nsGkAtoms.h"

class nsIControllers;
class nsPresContext;

namespace mozilla {

class EventChainPostVisitor;
class EventChainPreVisitor;
class EventStates;
class PresState;

namespace dom {

class FormData;

class HTMLTextAreaElement final : public TextControlElement,
                                  public nsStubMutationObserver,
                                  public ConstraintValidation {
 public:
  using ConstraintValidation::GetValidationMessage;
  using ValueSetterOption = TextControlState::ValueSetterOption;
  using ValueSetterOptions = TextControlState::ValueSetterOptions;

  explicit HTMLTextAreaElement(
      already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo,
      FromParser aFromParser = NOT_FROM_PARSER);

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  NS_IMPL_FROMNODE_HTML_WITH_TAG(HTMLTextAreaElement, textarea)

  virtual int32_t TabIndexDefault() override;

  // Element
  virtual bool IsInteractiveHTMLContent() const override { return true; }

  // nsGenericHTMLElement
  virtual bool IsDisabledForEvents(WidgetEvent* aEvent) override;

  // nsGenericHTMLFormElement
  void SaveState() override;
  bool RestoreState(PresState* aState) override;

  // nsIFormControl
  MOZ_CAN_RUN_SCRIPT_BOUNDARY
  NS_IMETHOD Reset() override;
  NS_IMETHOD SubmitNamesValues(FormData* aFormData) override;

  virtual void FieldSetDisabledChanged(bool aNotify) override;

  virtual EventStates IntrinsicState() const override;

  void SetLastValueChangeWasInteractive(bool);

  // TextControlElement
  virtual nsresult SetValueChanged(bool aValueChanged) override;
  virtual bool IsSingleLineTextControl() const override;
  virtual bool IsTextArea() const override;
  virtual bool IsPasswordTextControl() const override;
  virtual int32_t GetCols() override;
  virtual int32_t GetWrapCols() override;
  virtual int32_t GetRows() override;
  virtual void GetDefaultValueFromContent(nsAString& aValue) override;
  virtual bool ValueChanged() const override;
  virtual void GetTextEditorValue(nsAString& aValue,
                                  bool aIgnoreWrap) const override;
  MOZ_CAN_RUN_SCRIPT virtual TextEditor* GetTextEditor() override;
  virtual TextEditor* GetTextEditorWithoutCreation() override;
  virtual nsISelectionController* GetSelectionController() override;
  virtual nsFrameSelection* GetConstFrameSelection() override;
  virtual TextControlState* GetTextControlState() const override {
    return mState;
  }
  virtual nsresult BindToFrame(nsTextControlFrame* aFrame) override;
  MOZ_CAN_RUN_SCRIPT virtual void UnbindFromFrame(
      nsTextControlFrame* aFrame) override;
  MOZ_CAN_RUN_SCRIPT virtual nsresult CreateEditor() override;
  virtual void SetPreviewValue(const nsAString& aValue) override;
  virtual void GetPreviewValue(nsAString& aValue) override;
  virtual void EnablePreview() override;
  virtual bool IsPreviewEnabled() override;
  virtual void InitializeKeyboardEventListeners() override;
  virtual void OnValueChanged(ValueChangeKind) override;
  virtual void GetValueFromSetRangeText(nsAString& aValue) override;
  MOZ_CAN_RUN_SCRIPT virtual nsresult SetValueFromSetRangeText(
      const nsAString& aValue) override;
  virtual bool HasCachedSelection() override;

  // nsIContent
  virtual nsresult BindToTree(BindContext&, nsINode& aParent) override;
  virtual void UnbindFromTree(bool aNullParent = true) override;
  virtual bool ParseAttribute(int32_t aNamespaceID, nsAtom* aAttribute,
                              const nsAString& aValue,
                              nsIPrincipal* aMaybeScriptedPrincipal,
                              nsAttrValue& aResult) override;
  virtual nsMapRuleToAttributesFunc GetAttributeMappingFunction()
      const override;
  virtual nsChangeHint GetAttributeChangeHint(const nsAtom* aAttribute,
                                              int32_t aModType) const override;
  NS_IMETHOD_(bool) IsAttributeMapped(const nsAtom* aAttribute) const override;

  void GetEventTargetParent(EventChainPreVisitor& aVisitor) override;
  virtual nsresult PreHandleEvent(EventChainVisitor& aVisitor) override;
  virtual nsresult PostHandleEvent(EventChainPostVisitor& aVisitor) override;

  virtual bool IsHTMLFocusable(bool aWithMouse, bool* aIsFocusable,
                               int32_t* aTabIndex) override;

  virtual void DoneAddingChildren(bool aHaveNotified) override;
  virtual bool IsDoneAddingChildren() override;

  MOZ_CAN_RUN_SCRIPT_BOUNDARY
  virtual nsresult Clone(dom::NodeInfo*, nsINode** aResult) const override;

  nsresult CopyInnerTo(Element* aDest);

  /**
   * Called when an attribute is about to be changed
   */
  virtual nsresult BeforeSetAttr(int32_t aNameSpaceID, nsAtom* aName,
                                 const nsAttrValueOrString* aValue,
                                 bool aNotify) override;

  // nsIMutationObserver
  NS_DECL_NSIMUTATIONOBSERVER_CHARACTERDATACHANGED
  NS_DECL_NSIMUTATIONOBSERVER_CONTENTAPPENDED
  NS_DECL_NSIMUTATIONOBSERVER_CONTENTINSERTED
  NS_DECL_NSIMUTATIONOBSERVER_CONTENTREMOVED

  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(HTMLTextAreaElement,
                                           TextControlElement)

  // nsIConstraintValidation
  bool IsTooLong();
  bool IsTooShort();
  bool IsValueMissing() const;
  void UpdateTooLongValidityState();
  void UpdateTooShortValidityState();
  void UpdateValueMissingValidityState();
  void UpdateBarredFromConstraintValidation();

  // ConstraintValidation
  nsresult GetValidationMessage(nsAString& aValidationMessage,
                                ValidityStateType aType) override;

  // Web IDL binding methods
  void GetAutocomplete(DOMString& aValue);
  void SetAutocomplete(const nsAString& aValue, ErrorResult& aRv) {
    SetHTMLAttr(nsGkAtoms::autocomplete, aValue, aRv);
  }
  bool Autofocus() { return GetBoolAttr(nsGkAtoms::autofocus); }
  void SetAutofocus(bool aAutoFocus, ErrorResult& aError) {
    SetHTMLBoolAttr(nsGkAtoms::autofocus, aAutoFocus, aError);
  }
  uint32_t Cols() { return GetUnsignedIntAttr(nsGkAtoms::cols, DEFAULT_COLS); }
  void SetCols(uint32_t aCols, ErrorResult& aError) {
    uint32_t cols = aCols ? aCols : DEFAULT_COLS;
    SetUnsignedIntAttr(nsGkAtoms::cols, cols, DEFAULT_COLS, aError);
  }
  bool Disabled() { return GetBoolAttr(nsGkAtoms::disabled); }
  void SetDisabled(bool aDisabled, ErrorResult& aError) {
    SetHTMLBoolAttr(nsGkAtoms::disabled, aDisabled, aError);
  }
  // nsGenericHTMLFormControlElementWithState::GetForm is fine
  using nsGenericHTMLFormControlElementWithState::GetForm;
  int32_t MaxLength() const { return GetIntAttr(nsGkAtoms::maxlength, -1); }
  int32_t UsedMaxLength() const final { return MaxLength(); }
  void SetMaxLength(int32_t aMaxLength, ErrorResult& aError) {
    int32_t minLength = MinLength();
    if (aMaxLength < 0 || (minLength >= 0 && aMaxLength < minLength)) {
      aError.Throw(NS_ERROR_DOM_INDEX_SIZE_ERR);
    } else {
      SetHTMLIntAttr(nsGkAtoms::maxlength, aMaxLength, aError);
    }
  }
  int32_t MinLength() const { return GetIntAttr(nsGkAtoms::minlength, -1); }
  void SetMinLength(int32_t aMinLength, ErrorResult& aError) {
    int32_t maxLength = MaxLength();
    if (aMinLength < 0 || (maxLength >= 0 && aMinLength > maxLength)) {
      aError.Throw(NS_ERROR_DOM_INDEX_SIZE_ERR);
    } else {
      SetHTMLIntAttr(nsGkAtoms::minlength, aMinLength, aError);
    }
  }
  void GetName(nsAString& aName) { GetHTMLAttr(nsGkAtoms::name, aName); }
  void SetName(const nsAString& aName, ErrorResult& aError) {
    SetHTMLAttr(nsGkAtoms::name, aName, aError);
  }
  void GetPlaceholder(nsAString& aPlaceholder) {
    GetHTMLAttr(nsGkAtoms::placeholder, aPlaceholder);
  }
  void SetPlaceholder(const nsAString& aPlaceholder, ErrorResult& aError) {
    SetHTMLAttr(nsGkAtoms::placeholder, aPlaceholder, aError);
  }
  bool ReadOnly() { return GetBoolAttr(nsGkAtoms::readonly); }
  void SetReadOnly(bool aReadOnly, ErrorResult& aError) {
    SetHTMLBoolAttr(nsGkAtoms::readonly, aReadOnly, aError);
  }
  bool Required() const { return State().HasState(NS_EVENT_STATE_REQUIRED); }

  MOZ_CAN_RUN_SCRIPT void SetRangeText(const nsAString& aReplacement,
                                       ErrorResult& aRv);

  MOZ_CAN_RUN_SCRIPT void SetRangeText(const nsAString& aReplacement,
                                       uint32_t aStart, uint32_t aEnd,
                                       SelectionMode aSelectMode,
                                       ErrorResult& aRv);

  void SetRequired(bool aRequired, ErrorResult& aError) {
    SetHTMLBoolAttr(nsGkAtoms::required, aRequired, aError);
  }
  uint32_t Rows() {
    return GetUnsignedIntAttr(nsGkAtoms::rows, DEFAULT_ROWS_TEXTAREA);
  }
  void SetRows(uint32_t aRows, ErrorResult& aError) {
    uint32_t rows = aRows ? aRows : DEFAULT_ROWS_TEXTAREA;
    SetUnsignedIntAttr(nsGkAtoms::rows, rows, DEFAULT_ROWS_TEXTAREA, aError);
  }
  void GetWrap(nsAString& aWrap) { GetHTMLAttr(nsGkAtoms::wrap, aWrap); }
  void SetWrap(const nsAString& aWrap, ErrorResult& aError) {
    SetHTMLAttr(nsGkAtoms::wrap, aWrap, aError);
  }
  void GetType(nsAString& aType);
  void GetDefaultValue(nsAString& aDefaultValue, ErrorResult& aError);
  void SetDefaultValue(const nsAString& aDefaultValue, ErrorResult& aError);
  void GetValue(nsAString& aValue);
  /**
   * ValueEquals() is designed for internal use so that aValue shouldn't
   * include \r character.  It should be handled before calling this with
   * nsContentUtils::PlatformToDOMLineBreaks().
   */
  bool ValueEquals(const nsAString& aValue) const;
  MOZ_CAN_RUN_SCRIPT_BOUNDARY void SetValue(const nsAString& aValue,
                                            ErrorResult& aError);

  uint32_t GetTextLength();

  // Override SetCustomValidity so we update our state properly when it's called
  // via bindings.
  void SetCustomValidity(const nsAString& aError);

  MOZ_CAN_RUN_SCRIPT void Select();
  Nullable<uint32_t> GetSelectionStart(ErrorResult& aError);
  MOZ_CAN_RUN_SCRIPT void SetSelectionStart(
      const Nullable<uint32_t>& aSelectionStart, ErrorResult& aError);
  Nullable<uint32_t> GetSelectionEnd(ErrorResult& aError);
  MOZ_CAN_RUN_SCRIPT void SetSelectionEnd(
      const Nullable<uint32_t>& aSelectionEnd, ErrorResult& aError);
  void GetSelectionDirection(nsAString& aDirection, ErrorResult& aError);
  MOZ_CAN_RUN_SCRIPT void SetSelectionDirection(const nsAString& aDirection,
                                                ErrorResult& aError);
  MOZ_CAN_RUN_SCRIPT void SetSelectionRange(
      uint32_t aSelectionStart, uint32_t aSelectionEnd,
      const Optional<nsAString>& aDirecton, ErrorResult& aError);
  nsIControllers* GetControllers(ErrorResult& aError);
  // XPCOM adapter function widely used throughout code, leaving it as is.
  nsresult GetControllers(nsIControllers** aResult);

  MOZ_CAN_RUN_SCRIPT nsIEditor* GetEditorForBindings();
  bool HasEditor() {
    MOZ_ASSERT(mState);
    return !!mState->GetTextEditorWithoutCreation();
  }

  bool IsInputEventTarget() const { return true; }

  MOZ_CAN_RUN_SCRIPT_BOUNDARY void SetUserInput(
      const nsAString& aValue, nsIPrincipal& aSubjectPrincipal);

 protected:
  MOZ_CAN_RUN_SCRIPT_BOUNDARY virtual ~HTMLTextAreaElement();

  // get rid of the compiler warning
  using nsGenericHTMLFormControlElementWithState::IsSingleLineTextControl;

  virtual JSObject* WrapNode(JSContext* aCx,
                             JS::Handle<JSObject*> aGivenProto) override;

  nsCOMPtr<nsIControllers> mControllers;
  /** Whether or not the value has changed since its default value was given. */
  bool mValueChanged;
  /** Whether or not the last change to the value was made interactively by the
   * user. */
  bool mLastValueChangeWasInteractive;
  /** Whether or not we are already handling select event. */
  bool mHandlingSelect;
  /** Whether or not we are done adding children (always true if not
      created by a parser */
  bool mDoneAddingChildren;
  /** Whether state restoration should be inhibited in DoneAddingChildren. */
  bool mInhibitStateRestoration;
  /** Whether our disabled state has changed from the default **/
  bool mDisabledChanged;
  /** Whether we should make :-moz-ui-invalid apply on the element. **/
  bool mCanShowInvalidUI;
  /** Whether we should make :-moz-ui-valid apply on the element. **/
  bool mCanShowValidUI;
  bool mIsPreviewEnabled;

  nsContentUtils::AutocompleteAttrState mAutocompleteAttrState;

  void FireChangeEventIfNeeded();

  nsString mFocusedValue;

  /** The state of the text editor (selection controller and the editor) **/
  TextControlState* mState;

  NS_IMETHOD SelectAll(nsPresContext* aPresContext);
  /**
   * Get the value, whether it is from the content or the frame.
   * @param aValue the value [out]
   * @param aIgnoreWrap whether to ignore the wrap attribute when getting the
   *        value.  If this is true, linebreaks will not be inserted even if
   *        wrap=hard.
   */
  void GetValueInternal(nsAString& aValue, bool aIgnoreWrap) const;

  /**
   * Setting the value.
   *
   * @param aValue      String to set.
   * @param aOptions    See TextControlState::ValueSetterOption.
   */
  MOZ_CAN_RUN_SCRIPT nsresult
  SetValueInternal(const nsAString& aValue, const ValueSetterOptions& aOptions);

  /**
   * Common method to call from the various mutation observer methods.
   * aContent is a content node that's either the one that changed or its
   * parent; we should only respond to the change if aContent is non-anonymous.
   */
  void ContentChanged(nsIContent* aContent);

  virtual nsresult AfterSetAttr(int32_t aNamespaceID, nsAtom* aName,
                                const nsAttrValue* aValue,
                                const nsAttrValue* aOldValue,
                                nsIPrincipal* aSubjectPrincipal,
                                bool aNotify) override;

  /**
   * Return if an element should have a specific validity UI
   * (with :-moz-ui-invalid and :-moz-ui-valid pseudo-classes).
   *
   * @return Whether the element should have a validity UI.
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

  /**
   * A helper to get the current selection range.  Will throw on the ErrorResult
   * if we have no editor state.
   */
  void GetSelectionRange(uint32_t* aSelectionStart, uint32_t* aSelectionEnd,
                         ErrorResult& aRv);

 private:
  static void MapAttributesIntoRule(const nsMappedAttributes* aAttributes,
                                    MappedDeclarations&);
};

}  // namespace dom
}  // namespace mozilla

#endif
