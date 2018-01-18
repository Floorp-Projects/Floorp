/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_HTMLInputElement_h
#define mozilla_dom_HTMLInputElement_h

#include "mozilla/Attributes.h"
#include "nsGenericHTMLElement.h"
#include "nsImageLoadingContent.h"
#include "nsIDOMHTMLInputElement.h"
#include "nsITextControlElement.h"
#include "nsITimer.h"
#include "nsIDOMNSEditableElement.h"
#include "nsCOMPtr.h"
#include "nsIConstraintValidation.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/dom/BindingDeclarations.h"
#include "mozilla/dom/HTMLFormElement.h" // for HasEverTriedInvalidSubmit()
#include "mozilla/dom/HTMLInputElementBinding.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/UnionTypes.h"
#include "nsIFilePicker.h"
#include "nsIContentPrefService2.h"
#include "mozilla/Decimal.h"
#include "nsContentUtils.h"
#include "nsTextEditorState.h"
#include "mozilla/Variant.h"
#include "SingleLineTextInputTypes.h"
#include "NumericInputTypes.h"
#include "CheckableInputTypes.h"
#include "ButtonInputTypes.h"
#include "DateTimeInputTypes.h"
#include "ColorInputType.h"
#include "FileInputType.h"
#include "HiddenInputType.h"

static constexpr size_t INPUT_TYPE_SIZE = sizeof(
  mozilla::Variant<TextInputType, SearchInputType, TelInputType, URLInputType,
                   EmailInputType, PasswordInputType, NumberInputType,
                   RangeInputType, RadioInputType, CheckboxInputType,
                   ButtonInputType, ImageInputType, ResetInputType,
                   SubmitInputType, DateInputType, TimeInputType, WeekInputType,
                   MonthInputType, DateTimeLocalInputType, FileInputType,
                   ColorInputType, HiddenInputType> );

class InputType;
struct DoNotDelete;
class nsIRadioGroupContainer;
class nsIRadioVisitor;

namespace mozilla {

class EventChainPostVisitor;
class EventChainPreVisitor;

namespace dom {

class AfterSetFilesOrDirectoriesRunnable;
class Date;
class DispatchChangeEventCallback;
class File;
class FileList;
class FileSystemEntry;
class GetFilesHelper;

/**
 * A class we use to create a singleton object that is used to keep track of
 * the last directory from which the user has picked files (via
 * <input type=file>) on a per-domain basis. The implementation uses
 * nsIContentPrefService2/NS_CONTENT_PREF_SERVICE_CONTRACTID to store the last
 * directory per-domain, and to ensure that whether the directories are
 * persistently saved (saved across sessions) or not honors whether or not the
 * page is being viewed in private browsing.
 */
class UploadLastDir final : public nsIObserver, public nsSupportsWeakReference
{
  ~UploadLastDir() {}

public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER

  /**
   * Fetch the last used directory for this location from the content
   * pref service, and display the file picker opened in that directory.
   *
   * @param aDoc          current document
   * @param aFilePicker   the file picker to open
   * @param aFpCallback   the callback object to be run when the file is shown.
   */
  nsresult FetchDirectoryAndDisplayPicker(nsIDocument* aDoc,
                                          nsIFilePicker* aFilePicker,
                                          nsIFilePickerShownCallback* aFpCallback);

  /**
   * Store the last used directory for this location using the
   * content pref service, if it is available
   * @param aURI URI of the current page
   * @param aDir Parent directory of the file(s)/directory chosen by the user
   */
  nsresult StoreLastUsedDirectory(nsIDocument* aDoc, nsIFile* aDir);

  class ContentPrefCallback final : public nsIContentPrefCallback2
  {
    virtual ~ContentPrefCallback()
    { }

  public:
    ContentPrefCallback(nsIFilePicker* aFilePicker, nsIFilePickerShownCallback* aFpCallback)
    : mFilePicker(aFilePicker)
    , mFpCallback(aFpCallback)
    { }

    NS_DECL_ISUPPORTS
    NS_DECL_NSICONTENTPREFCALLBACK2

    nsCOMPtr<nsIFilePicker> mFilePicker;
    nsCOMPtr<nsIFilePickerShownCallback> mFpCallback;
    nsCOMPtr<nsIContentPref> mResult;
  };
};

class HTMLInputElement final : public nsGenericHTMLFormElementWithState,
                               public nsImageLoadingContent,
                               public nsIDOMHTMLInputElement,
                               public nsITextControlElement,
                               public nsIDOMNSEditableElement,
                               public nsIConstraintValidation
{
  friend class AfterSetFilesOrDirectoriesCallback;
  friend class DispatchChangeEventCallback;
  friend class ::InputType;

public:
  using nsIConstraintValidation::GetValidationMessage;
  using nsGenericHTMLFormElementWithState::GetForm;
  using nsGenericHTMLFormElementWithState::GetFormAction;

  enum class FromClone { no, yes };

  HTMLInputElement(already_AddRefed<mozilla::dom::NodeInfo>& aNodeInfo,
                   mozilla::dom::FromParser aFromParser,
                   FromClone aFromClone = FromClone::no);

  NS_IMPL_FROMCONTENT_HTML_WITH_TAG(HTMLInputElement, input)

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  virtual int32_t TabIndexDefault() override;
  using nsGenericHTMLElement::Focus;
  virtual void Blur(ErrorResult& aError) override;
  virtual void Focus(ErrorResult& aError) override;

  // nsINode
#if !defined(ANDROID) && !defined(XP_MACOSX)
  virtual bool IsNodeApzAwareInternal() const override;
#endif

  // Element
  virtual bool IsInteractiveHTMLContent(bool aIgnoreTabindex) const override;

  // EventTarget
  virtual void AsyncEventRunning(AsyncEventDispatcher* aEvent) override;

  // nsIDOMHTMLInputElement
  NS_DECL_NSIDOMHTMLINPUTELEMENT

  // nsIDOMNSEditableElement
  NS_IMETHOD GetEditor(nsIEditor** aEditor) override
  {
    nsCOMPtr<nsIEditor> editor = GetEditor();
    editor.forget(aEditor);
    return NS_OK;
  }

  NS_IMETHOD SetUserInput(const nsAString& aInput) override;

  // Overriden nsIFormControl methods
  NS_IMETHOD Reset() override;
  NS_IMETHOD SubmitNamesValues(HTMLFormSubmission* aFormSubmission) override;
  NS_IMETHOD SaveState() override;
  virtual bool RestoreState(nsPresState* aState) override;
  virtual bool AllowDrop() override;
  virtual bool IsDisabledForEvents(EventMessage aMessage) override;

  virtual void FieldSetDisabledChanged(bool aNotify) override;

  // nsIContent
  virtual bool IsHTMLFocusable(bool aWithMouse, bool *aIsFocusable, int32_t *aTabIndex) override;

  virtual bool ParseAttribute(int32_t aNamespaceID,
                                nsAtom* aAttribute,
                                const nsAString& aValue,
                                nsIPrincipal* aMaybeScriptedPrincipal,
                                nsAttrValue& aResult) override;
  virtual nsChangeHint GetAttributeChangeHint(const nsAtom* aAttribute,
                                              int32_t aModType) const override;
  NS_IMETHOD_(bool) IsAttributeMapped(const nsAtom* aAttribute) const override;
  virtual nsMapRuleToAttributesFunc GetAttributeMappingFunction() const override;

  virtual nsresult GetEventTargetParent(
                     EventChainPreVisitor& aVisitor) override;
  virtual nsresult PreHandleEvent(EventChainVisitor& aVisitor) override;
  virtual nsresult PostHandleEvent(
                     EventChainPostVisitor& aVisitor) override;
  void PostHandleEventForRangeThumb(EventChainPostVisitor& aVisitor);
  void StartRangeThumbDrag(WidgetGUIEvent* aEvent);
  void FinishRangeThumbDrag(WidgetGUIEvent* aEvent = nullptr);
  void CancelRangeThumbDrag(bool aIsForUserEvent = true);
  void SetValueOfRangeForUserEvent(Decimal aValue);

  virtual nsresult BindToTree(nsIDocument* aDocument, nsIContent* aParent,
                              nsIContent* aBindingParent,
                              bool aCompileEventHandlers) override;
  virtual void UnbindFromTree(bool aDeep = true,
                              bool aNullParent = true) override;

  virtual void DoneCreatingElement() override;

  virtual EventStates IntrinsicState() const override;

  // Element
private:
  virtual void AddStates(EventStates aStates) override;
  virtual void RemoveStates(EventStates aStates) override;

public:

  // nsITextControlElement
  NS_IMETHOD SetValueChanged(bool aValueChanged) override;
  NS_IMETHOD_(bool) IsSingleLineTextControl() const override;
  NS_IMETHOD_(bool) IsTextArea() const override;
  NS_IMETHOD_(bool) IsPasswordTextControl() const override;
  NS_IMETHOD_(int32_t) GetCols() override;
  NS_IMETHOD_(int32_t) GetWrapCols() override;
  NS_IMETHOD_(int32_t) GetRows() override;
  NS_IMETHOD_(void) GetDefaultValueFromContent(nsAString& aValue) override;
  NS_IMETHOD_(bool) ValueChanged() const override;
  NS_IMETHOD_(void) GetTextEditorValue(nsAString& aValue, bool aIgnoreWrap) const override;
  NS_IMETHOD_(mozilla::TextEditor*) GetTextEditor() override;
  NS_IMETHOD_(nsISelectionController*) GetSelectionController() override;
  NS_IMETHOD_(nsFrameSelection*) GetConstFrameSelection() override;
  NS_IMETHOD BindToFrame(nsTextControlFrame* aFrame) override;
  NS_IMETHOD_(void) UnbindFromFrame(nsTextControlFrame* aFrame) override;
  NS_IMETHOD CreateEditor() override;
  NS_IMETHOD_(Element*) GetRootEditorNode() override;
  NS_IMETHOD_(Element*) GetPlaceholderNode() override;
  NS_IMETHOD_(Element*) GetPreviewNode() override;
  NS_IMETHOD_(void) UpdateOverlayTextVisibility(bool aNotify) override;
  NS_IMETHOD_(void) SetPreviewValue(const nsAString& aValue) override;
  NS_IMETHOD_(void) GetPreviewValue(nsAString& aValue) override;
  NS_IMETHOD_(void) EnablePreview() override;
  NS_IMETHOD_(bool) IsPreviewEnabled() override;
  NS_IMETHOD_(bool) GetPlaceholderVisibility() override;
  NS_IMETHOD_(bool) GetPreviewVisibility() override;
  NS_IMETHOD_(void) InitializeKeyboardEventListeners() override;
  NS_IMETHOD_(void) OnValueChanged(bool aNotify, bool aWasInteractiveUserChange) override;
  virtual void GetValueFromSetRangeText(nsAString& aValue) override;
  virtual nsresult SetValueFromSetRangeText(const nsAString& aValue) override;
  NS_IMETHOD_(bool) HasCachedSelection() override;

  // Methods for nsFormFillController so it can do selection operations on input
  // types the HTML spec doesn't support them on, like "email".
  uint32_t GetSelectionStartIgnoringType(ErrorResult& aRv);
  uint32_t GetSelectionEndIgnoringType(ErrorResult& aRv);

  void GetDisplayFileName(nsAString& aFileName) const;

  const nsTArray<OwningFileOrDirectory>& GetFilesOrDirectoriesInternal() const;

  void SetFilesOrDirectories(const nsTArray<OwningFileOrDirectory>& aFilesOrDirectories,
                             bool aSetValueChanged);
  void SetFiles(nsIDOMFileList* aFiles, bool aSetValueChanged);

  // This method is used for test only. Onces the data is set, a 'change' event
  // is dispatched.
  void MozSetDndFilesAndDirectories(const nsTArray<OwningFileOrDirectory>& aSequence);

  // Called when a nsIFilePicker or a nsIColorPicker terminate.
  void PickerClosed();

  void SetCheckedChangedInternal(bool aCheckedChanged);
  bool GetCheckedChanged() const {
    return mCheckedChanged;
  }
  void AddedToRadioGroup();
  void WillRemoveFromRadioGroup();

 /**
   * Helper function returning the currently selected button in the radio group.
   * Returning null if the element is not a button or if there is no selectied
   * button in the group.
   *
   * @return the selected button (or null).
   */
  already_AddRefed<nsIDOMHTMLInputElement> GetSelectedRadioButton() const;

  virtual nsresult Clone(mozilla::dom::NodeInfo *aNodeInfo, nsINode **aResult,
                         bool aPreallocateChildren) const override;

  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(HTMLInputElement,
                                           nsGenericHTMLFormElementWithState)

  static UploadLastDir* gUploadLastDir;
  // create and destroy the static UploadLastDir object for remembering
  // which directory was last used on a site-by-site basis
  static void InitUploadLastDir();
  static void DestroyUploadLastDir();

  //If the valueAsDate attribute should be enabled in webIDL
  static bool ValueAsDateEnabled(JSContext* cx, JSObject* obj);

  void MaybeLoadImage();

  void SetSelectionCached()
  {
    MOZ_ASSERT(mType == NS_FORM_INPUT_NUMBER);
    mSelectionCached = true;
  }
  bool IsSelectionCached() const
  {
    MOZ_ASSERT(mType == NS_FORM_INPUT_NUMBER);
    return mSelectionCached;
  }
  void ClearSelectionCached()
  {
    MOZ_ASSERT(mType == NS_FORM_INPUT_NUMBER);
    mSelectionCached = false;
  }
  nsTextEditorState::SelectionProperties& GetSelectionProperties()
  {
    MOZ_ASSERT(mType == NS_FORM_INPUT_NUMBER);
    return mSelectionProperties;
  }

  bool HasPatternAttribute() const
  {
    return mHasPatternAttribute;
  }

  virtual already_AddRefed<nsITextControlElement> GetAsTextControlElement() override
  {
    nsCOMPtr<nsITextControlElement> txt = this;
    return txt.forget();
  }

  // nsIConstraintValidation
  bool     IsTooLong();
  bool     IsTooShort();
  bool     IsValueMissing() const;
  bool     HasTypeMismatch() const;
  bool     HasPatternMismatch() const;
  bool     IsRangeOverflow() const;
  bool     IsRangeUnderflow() const;
  bool     HasStepMismatch(bool aUseZeroIfValueNaN = false) const;
  bool     HasBadInput() const;
  void     UpdateTooLongValidityState();
  void     UpdateTooShortValidityState();
  void     UpdateValueMissingValidityState();
  void     UpdateTypeMismatchValidityState();
  void     UpdatePatternMismatchValidityState();
  void     UpdateRangeOverflowValidityState();
  void     UpdateRangeUnderflowValidityState();
  void     UpdateStepMismatchValidityState();
  void     UpdateBadInputValidityState();
  // Update all our validity states and then update our element state
  // as needed.  aNotify controls whether the element state update
  // needs to notify.
  void     UpdateAllValidityStates(bool aNotify);
  // Update all our validity states without updating element state.
  // This should be called instead of UpdateAllValidityStates any time
  // we're guaranteed that element state will be updated anyway.
  void     UpdateAllValidityStatesButNotElementState();
  void     UpdateBarredFromConstraintValidation();
  nsresult GetValidationMessage(nsAString& aValidationMessage,
                                ValidityStateType aType) override;

  // Override SetCustomValidity so we update our state properly when it's called
  // via bindings.
  void SetCustomValidity(const nsAString& aError);

  /**
   * Update the value missing validity state for radio elements when they have
   * a group.
   *
   * @param aIgnoreSelf Whether the required attribute and the checked state
   * of the current radio should be ignored.
   * @note This method shouldn't be called if the radio element hasn't a group.
   */
  void     UpdateValueMissingValidityStateForRadio(bool aIgnoreSelf);

  /**
   * Set filters to the filePicker according to the accept attribute value.
   *
   * See:
   * http://dev.w3.org/html5/spec/forms.html#attr-input-accept
   *
   * @note You should not call this function if the element has no @accept.
   * @note "All Files" filter is always set, no matter if there is a valid
   * filter specified or not.
   * @note If more than one valid filter is found, the "All Supported Types"
   * filter is added, which is the concatenation of all valid filters.
   * @note Duplicate filters and similar filters (i.e. filters whose file
   * extensions already exist in another filter) are ignored.
   * @note "All Files" filter will be selected by default if unknown mime types
   * have been specified and no file extension filter has been specified.
   * Otherwise, specified filter or "All Supported Types" filter will be
   * selected by default.
   * The logic behind is that having unknown mime type means we might restrict
   * user's input too much, as some filters will be missing.
   * However, if author has also specified some file extension filters, it's
   * likely those are fallback for the unusual mime type we haven't been able
   * to resolve; so it's better to select author specified filters in that case.
   */
  void SetFilePickerFiltersFromAccept(nsIFilePicker* filePicker);

  /**
   * The form might need to request an update of the UI bits
   * (BF_CAN_SHOW_INVALID_UI and BF_CAN_SHOW_VALID_UI) when an invalid form
   * submission is tried.
   *
   * @param aIsFocused Whether the element is currently focused.
   *
   * @note The caller is responsible to call ContentStatesChanged.
   */
  void UpdateValidityUIBits(bool aIsFocused);

  /**
   * Fires change event if mFocusedValue and current value held are unequal.
   */
  void FireChangeEventIfNeeded();

  /**
   * Returns the input element's value as a Decimal.
   * Returns NaN if the current element's value is not a floating point number.
   *
   * @return the input element's value as a Decimal.
   */
  Decimal GetValueAsDecimal() const;

  /**
   * Returns the input's "minimum" (as defined by the HTML5 spec) as a double.
   * Note this takes account of any default minimum that the type may have.
   * Returns NaN if the min attribute isn't a valid floating point number and
   * the input's type does not have a default minimum.
   *
   * NOTE: Only call this if you know DoesMinMaxApply() returns true.
   */
  Decimal GetMinimum() const;

  /**
   * Returns the input's "maximum" (as defined by the HTML5 spec) as a double.
   * Note this takes account of any default maximum that the type may have.
   * Returns NaN if the max attribute isn't a valid floating point number and
   * the input's type does not have a default maximum.
   *
   * NOTE:Only call this if you know DoesMinMaxApply() returns true.
   */
  Decimal GetMaximum() const;

  // WebIDL

  void GetAccept(nsAString& aValue)
  {
    GetHTMLAttr(nsGkAtoms::accept, aValue);
  }
  void SetAccept(const nsAString& aValue, ErrorResult& aRv)
  {
    SetHTMLAttr(nsGkAtoms::accept, aValue, aRv);
  }

  void GetAlt(nsAString& aValue)
  {
    GetHTMLAttr(nsGkAtoms::alt, aValue);
  }
  void SetAlt(const nsAString& aValue, ErrorResult& aRv)
  {
    SetHTMLAttr(nsGkAtoms::alt, aValue, aRv);
  }

  // XPCOM GetAutocomplete() is OK
  void SetAutocomplete(const nsAString& aValue, ErrorResult& aRv)
  {
    SetHTMLAttr(nsGkAtoms::autocomplete, aValue, aRv);
  }

  void GetAutocompleteInfo(Nullable<AutocompleteInfo>& aInfo);

  bool Autofocus() const
  {
    return GetBoolAttr(nsGkAtoms::autofocus);
  }

  void SetAutofocus(bool aValue, ErrorResult& aRv)
  {
    SetHTMLBoolAttr(nsGkAtoms::autofocus, aValue, aRv);
  }

  bool DefaultChecked() const
  {
    return HasAttr(kNameSpaceID_None, nsGkAtoms::checked);
  }

  void SetDefaultChecked(bool aValue, ErrorResult& aRv)
  {
    SetHTMLBoolAttr(nsGkAtoms::checked, aValue, aRv);
  }

  bool Checked() const
  {
    return mChecked;
  }
  // XPCOM SetChecked() is OK

  bool Disabled() const
  {
    return GetBoolAttr(nsGkAtoms::disabled);
  }

  void SetDisabled(bool aValue, ErrorResult& aRv)
  {
    SetHTMLBoolAttr(nsGkAtoms::disabled, aValue, aRv);
  }

  // XPCOM GetForm() is OK

  FileList* GetFiles();
  void SetFiles(FileList* aFiles);

  // XPCOM GetFormAction() is OK
  void SetFormAction(const nsAString& aValue, ErrorResult& aRv)
  {
    SetHTMLAttr(nsGkAtoms::formaction, aValue, aRv);
  }

  void GetFormEnctype(nsAString& aValue);
  void SetFormEnctype(const nsAString& aValue, ErrorResult& aRv)
  {
    SetHTMLAttr(nsGkAtoms::formenctype, aValue, aRv);
  }

  void GetFormMethod(nsAString& aValue);
  void SetFormMethod(const nsAString& aValue, ErrorResult& aRv)
  {
    SetHTMLAttr(nsGkAtoms::formmethod, aValue, aRv);
  }

  bool FormNoValidate() const
  {
    return GetBoolAttr(nsGkAtoms::formnovalidate);
  }

  void SetFormNoValidate(bool aValue, ErrorResult& aRv)
  {
    SetHTMLBoolAttr(nsGkAtoms::formnovalidate, aValue, aRv);
  }

  void GetFormTarget(nsAString& aValue)
  {
    GetHTMLAttr(nsGkAtoms::formtarget, aValue);
  }
  void SetFormTarget(const nsAString& aValue, ErrorResult& aRv)
  {
    SetHTMLAttr(nsGkAtoms::formtarget, aValue, aRv);
  }

  MOZ_CAN_RUN_SCRIPT uint32_t Height();

  void SetHeight(uint32_t aValue, ErrorResult& aRv)
  {
    SetUnsignedIntAttr(nsGkAtoms::height, aValue, 0, aRv);
  }

  bool Indeterminate() const
  {
    return mIndeterminate;
  }

  bool IsDraggingRange() const
  {
    return mIsDraggingRange;
  }

  // XPCOM SetIndeterminate() is OK

  void GetInputMode(nsAString& aValue);
  void SetInputMode(const nsAString& aValue, ErrorResult& aRv)
  {
    SetHTMLAttr(nsGkAtoms::inputmode, aValue, aRv);
  }

  nsGenericHTMLElement* GetList() const;

  void GetMax(nsAString& aValue)
  {
    GetHTMLAttr(nsGkAtoms::max, aValue);
  }
  void SetMax(const nsAString& aValue, ErrorResult& aRv)
  {
    SetHTMLAttr(nsGkAtoms::max, aValue, aRv);
  }

  int32_t MaxLength() const
  {
    return GetIntAttr(nsGkAtoms::maxlength, -1);
  }

  void SetMaxLength(int32_t aValue, ErrorResult& aRv)
  {
    int32_t minLength = MinLength();
    if (aValue < 0 || (minLength >= 0 && aValue < minLength)) {
      aRv.Throw(NS_ERROR_DOM_INDEX_SIZE_ERR);
      return;
    }

    SetHTMLIntAttr(nsGkAtoms::maxlength, aValue, aRv);
  }

  int32_t MinLength() const
  {
    return GetIntAttr(nsGkAtoms::minlength, -1);
  }

  void SetMinLength(int32_t aValue, ErrorResult& aRv)
  {
    int32_t maxLength = MaxLength();
    if (aValue < 0 || (maxLength >= 0 && aValue > maxLength)) {
      aRv.Throw(NS_ERROR_DOM_INDEX_SIZE_ERR);
      return;
    }

    SetHTMLIntAttr(nsGkAtoms::minlength, aValue, aRv);
  }

  void GetMin(nsAString& aValue)
  {
    GetHTMLAttr(nsGkAtoms::min, aValue);
  }
  void SetMin(const nsAString& aValue, ErrorResult& aRv)
  {
    SetHTMLAttr(nsGkAtoms::min, aValue, aRv);
  }

  bool Multiple() const
  {
    return GetBoolAttr(nsGkAtoms::multiple);
  }

  void SetMultiple(bool aValue, ErrorResult& aRv)
  {
    SetHTMLBoolAttr(nsGkAtoms::multiple, aValue, aRv);
  }

  // XPCOM GetName() is OK
  void SetName(const nsAString& aValue, ErrorResult& aRv)
  {
    SetHTMLAttr(nsGkAtoms::name, aValue, aRv);
  }

  void GetPattern(nsAString& aValue)
  {
    GetHTMLAttr(nsGkAtoms::pattern, aValue);
  }
  void SetPattern(const nsAString& aValue, ErrorResult& aRv)
  {
    SetHTMLAttr(nsGkAtoms::pattern, aValue, aRv);
  }

  void GetPlaceholder(nsAString& aValue)
  {
    GetHTMLAttr(nsGkAtoms::placeholder, aValue);
  }
  void SetPlaceholder(const nsAString& aValue, ErrorResult& aRv)
  {
    SetHTMLAttr(nsGkAtoms::placeholder, aValue, aRv);
  }

  bool ReadOnly() const
  {
    return GetBoolAttr(nsGkAtoms::readonly);
  }

  void SetReadOnly(bool aValue, ErrorResult& aRv)
  {
    SetHTMLBoolAttr(nsGkAtoms::readonly, aValue, aRv);
  }

  bool Required() const
  {
    return GetBoolAttr(nsGkAtoms::required);
  }

  void SetRequired(bool aValue, ErrorResult& aRv)
  {
    SetHTMLBoolAttr(nsGkAtoms::required, aValue, aRv);
  }

  uint32_t Size() const
  {
    return GetUnsignedIntAttr(nsGkAtoms::size, DEFAULT_COLS);
  }

  void SetSize(uint32_t aValue, ErrorResult& aRv)
  {
    if (aValue == 0) {
      aRv.Throw(NS_ERROR_DOM_INDEX_SIZE_ERR);
      return;
    }

    SetUnsignedIntAttr(nsGkAtoms::size, aValue, DEFAULT_COLS, aRv);
  }

  void GetSrc(nsAString& aValue)
  {
    GetURIAttr(nsGkAtoms::src, nullptr, aValue);
  }
  void SetSrc(const nsAString& aValue, nsIPrincipal* aTriggeringPrincipal, ErrorResult& aRv)
  {
    SetHTMLAttr(nsGkAtoms::src, aValue, aTriggeringPrincipal, aRv);
  }

  void GetStep(nsAString& aValue)
  {
    GetHTMLAttr(nsGkAtoms::step, aValue);
  }
  void SetStep(const nsAString& aValue, ErrorResult& aRv)
  {
    SetHTMLAttr(nsGkAtoms::step, aValue, aRv);
  }

  void GetType(nsAString& aValue);
  void SetType(const nsAString& aValue, ErrorResult& aRv)
  {
    SetHTMLAttr(nsGkAtoms::type, aValue, aRv);
  }

  void GetDefaultValue(nsAString& aValue)
  {
    GetHTMLAttr(nsGkAtoms::value, aValue);
  }
  void SetDefaultValue(const nsAString& aValue, ErrorResult& aRv)
  {
    SetHTMLAttr(nsGkAtoms::value, aValue, aRv);
  }

  void SetValue(const nsAString& aValue, CallerType aCallerType,
                ErrorResult& aRv);
  void GetValue(nsAString& aValue, CallerType aCallerType);

  Nullable<Date> GetValueAsDate(ErrorResult& aRv);

  void SetValueAsDate(const Nullable<Date>& aDate, ErrorResult& aRv);

  double ValueAsNumber() const
  {
    return DoesValueAsNumberApply() ? GetValueAsDecimal().toDouble()
                                    : UnspecifiedNaN<double>();
  }

  void SetValueAsNumber(double aValue, ErrorResult& aRv);

  MOZ_CAN_RUN_SCRIPT uint32_t Width();

  void SetWidth(uint32_t aValue, ErrorResult& aRv)
  {
    SetUnsignedIntAttr(nsGkAtoms::width, aValue, 0, aRv);
  }

  void StepUp(int32_t aN, ErrorResult& aRv)
  {
    aRv = ApplyStep(aN);
  }

  void StepDown(int32_t aN, ErrorResult& aRv)
  {
    aRv = ApplyStep(-aN);
  }

  /**
   * Returns the current step value.
   * Returns kStepAny if the current step is "any" string.
   *
   * @return the current step value.
   */
  Decimal GetStep() const;

  already_AddRefed<nsINodeList> GetLabels();

  void Select();

  Nullable<uint32_t> GetSelectionStart(ErrorResult& aRv);
  void SetSelectionStart(const Nullable<uint32_t>& aValue, ErrorResult& aRv);

  Nullable<uint32_t> GetSelectionEnd(ErrorResult& aRv);
  void SetSelectionEnd(const Nullable<uint32_t>& aValue, ErrorResult& aRv);

  void GetSelectionDirection(nsAString& aValue, ErrorResult& aRv);
  void SetSelectionDirection(const nsAString& aValue, ErrorResult& aRv);

  void SetSelectionRange(uint32_t aStart, uint32_t aEnd,
                         const Optional< nsAString >& direction,
                         ErrorResult& aRv);

  void SetRangeText(const nsAString& aReplacement, ErrorResult& aRv);

  void SetRangeText(const nsAString& aReplacement, uint32_t aStart,
                    uint32_t aEnd, SelectionMode aSelectMode,
                    ErrorResult& aRv);

  bool Allowdirs() const
  {
    return HasAttr(kNameSpaceID_None, nsGkAtoms::allowdirs);
  }

  void SetAllowdirs(bool aValue, ErrorResult& aRv)
  {
    SetHTMLBoolAttr(nsGkAtoms::allowdirs, aValue, aRv);
  }

  bool WebkitDirectoryAttr() const
  {
    return HasAttr(kNameSpaceID_None, nsGkAtoms::webkitdirectory);
  }

  void SetWebkitDirectoryAttr(bool aValue, ErrorResult& aRv)
  {
    SetHTMLBoolAttr(nsGkAtoms::webkitdirectory, aValue, aRv);
  }

  void GetWebkitEntries(nsTArray<RefPtr<FileSystemEntry>>& aSequence);

  bool IsFilesAndDirectoriesSupported() const;

  already_AddRefed<Promise> GetFilesAndDirectories(ErrorResult& aRv);

  already_AddRefed<Promise> GetFiles(bool aRecursiveFlag, ErrorResult& aRv);

  void ChooseDirectory(ErrorResult& aRv);

  void GetAlign(nsAString& aValue)
  {
    GetHTMLAttr(nsGkAtoms::align, aValue);
  }
  void SetAlign(const nsAString& aValue, ErrorResult& aRv)
  {
    SetHTMLAttr(nsGkAtoms::align, aValue, aRv);
  }

  void GetUseMap(nsAString& aValue)
  {
    GetHTMLAttr(nsGkAtoms::usemap, aValue);
  }
  void SetUseMap(const nsAString& aValue, ErrorResult& aRv)
  {
    SetHTMLAttr(nsGkAtoms::usemap, aValue, aRv);
  }

  nsIControllers* GetControllers(ErrorResult& aRv);

  int32_t InputTextLength(CallerType aCallerType);

  void MozGetFileNameArray(nsTArray<nsString>& aFileNames, ErrorResult& aRv);

  void MozSetFileNameArray(const Sequence< nsString >& aFileNames, ErrorResult& aRv);
  void MozSetFileArray(const Sequence<OwningNonNull<File>>& aFiles);
  void MozSetDirectory(const nsAString& aDirectoryPath, ErrorResult& aRv);

  /*
   * The following functions are called from datetime picker to let input box
   * know the current state of the picker or to update the input box on changes.
   */
  void GetDateTimeInputBoxValue(DateTimeValue& aValue);
  void UpdateDateTimeInputBox(const DateTimeValue& aValue);
  void SetDateTimePickerState(bool aOpen);

  /*
   * The following functions are called from datetime input box XBL to control
   * and update the picker.
   */
  void OpenDateTimePicker(const DateTimeValue& aInitialValue);
  void UpdateDateTimePicker(const DateTimeValue& aValue);
  void CloseDateTimePicker();

  /*
   * Called from datetime input box binding when inner text fields are focused
   * or blurred.
   */
  void SetFocusState(bool aIsFocused);

  /*
   * Called from datetime input box binding when the the user entered value
   * becomes valid/invalid.
   */
  void UpdateValidityState();

  /*
   * The following are called from datetime input box binding to get the
   * corresponding computed values.
   */
  double GetStepAsDouble() { return GetStep().toDouble(); }
  double GetStepBaseAsDouble() { return GetStepBase().toDouble(); }
  double GetMinimumAsDouble() { return GetMinimum().toDouble(); }
  double GetMaximumAsDouble() { return GetMaximum().toDouble(); }

  HTMLInputElement* GetOwnerNumberControl();

  void StartNumberControlSpinnerSpin();
  enum SpinnerStopState {
    eAllowDispatchingEvents,
    eDisallowDispatchingEvents
  };
  void StopNumberControlSpinnerSpin(SpinnerStopState aState =
                                      eAllowDispatchingEvents);
  void StepNumberControlForUserEvent(int32_t aDirection);

  /**
   * The callback function used by the nsRepeatService that we use to spin the
   * spinner for <input type=number>.
   */
  static void HandleNumberControlSpin(void* aData);

  bool NumberSpinnerUpButtonIsDepressed() const
  {
    return mNumberControlSpinnerIsSpinning && mNumberControlSpinnerSpinsUp;
  }

  bool NumberSpinnerDownButtonIsDepressed() const
  {
    return mNumberControlSpinnerIsSpinning && !mNumberControlSpinnerSpinsUp;
  }

  bool MozIsTextField(bool aExcludePassword);

  /**
   * GetEditor() is for webidl bindings.
   */
  nsIEditor* GetEditor();

  void SetUserInput(const nsAString& aInput,
                    nsIPrincipal& aSubjectPrincipal);

  /**
   * If aValue contains a valid floating-point number in the format specified
   * by the HTML 5 spec:
   *
   *   http://www.whatwg.org/specs/web-apps/current-work/multipage/common-microsyntaxes.html#floating-point-numbers
   *
   * then this function will return the number parsed as a Decimal, otherwise
   * it will return a Decimal for which Decimal::isFinite() will return false.
   */
  static Decimal StringToDecimal(const nsAString& aValue);

  void UpdateEntries(const nsTArray<OwningFileOrDirectory>& aFilesOrDirectories);

  static void Shutdown();

  /**
   * Returns if the required attribute applies for the current type.
   */
  bool DoesRequiredApply() const;

  /**
   * Returns the current required state of the element. This function differs
   * from Required() in that this function only returns true for input types
   * that @required attribute applies and the attribute is set; in contrast,
   * Required() returns true whenever @required attribute is set.
   */
  bool IsRequired() const
  {
    return State().HasState(NS_EVENT_STATE_REQUIRED);
  }

protected:
  virtual ~HTMLInputElement();

  virtual JSObject* WrapNode(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

  // Pull IsSingleLineTextControl into our scope, otherwise it'd be hidden
  // by the nsITextControlElement version.
  using nsGenericHTMLFormElementWithState::IsSingleLineTextControl;

  /**
   * The ValueModeType specifies how the value IDL attribute should behave.
   *
   * See: http://dev.w3.org/html5/spec/forms.html#dom-input-value
   */
  enum ValueModeType
  {
    // On getting, returns the value.
    // On setting, sets value.
    VALUE_MODE_VALUE,
    // On getting, returns the value if present or the empty string.
    // On setting, sets the value.
    VALUE_MODE_DEFAULT,
    // On getting, returns the value if present or "on".
    // On setting, sets the value.
    VALUE_MODE_DEFAULT_ON,
    // On getting, returns "C:\fakepath\" followed by the file name of the
    // first file of the selected files if any.
    // On setting the empty string, empties the selected files list, otherwise
    // throw the INVALID_STATE_ERR exception.
    VALUE_MODE_FILENAME
  };

  /**
   * This helper method convert a sub-string that contains only digits to a
   * number (unsigned int given that it can't contain a minus sign).
   * This method will return whether the sub-string is correctly formatted
   * (ie. contains only digit) and it can be successfuly parsed to generate a
   * number).
   * If the method returns true, |aResult| will contained the parsed number.
   *
   * @param aValue  the string on which the sub-string will be extracted and parsed.
   * @param aStart  the beginning of the sub-string in aValue.
   * @param aLen    the length of the sub-string.
   * @param aResult the parsed number.
   * @return whether the sub-string has been parsed successfully.
   */
  static bool DigitSubStringToNumber(const nsAString& aValue, uint32_t aStart,
                                     uint32_t aLen, uint32_t* aResult);

  // Helper method

  /**
   * Setting the value.
   *
   * @param aValue      String to set.
   * @param aOldValue   Previous value before setting aValue.
                        If previous value is unknown, aOldValue can be nullptr.
   * @param aFlags      See nsTextEditorState::SetValueFlags.
   */
  nsresult SetValueInternal(const nsAString& aValue,
                            const nsAString* aOldValue,
                            uint32_t aFlags);

  nsresult SetValueInternal(const nsAString& aValue,
                            uint32_t aFlags)
  {
    return SetValueInternal(aValue, nullptr, aFlags);
  }

  // Generic getter for the value that doesn't do experimental control type
  // sanitization.
  void GetValueInternal(nsAString& aValue, CallerType aCallerType) const;

  // A getter for callers that know we're not dealing with a file input, so they
  // don't have to think about the caller type.
  void GetNonFileValueInternal(nsAString& aValue) const;

  /**
   * Returns whether the current value is the empty string.  This only makes
   * sense for some input types; does NOT make sense for file inputs.
   *
   * @return whether the current value is the empty string.
   */
  bool IsValueEmpty() const;

  /**
   * Returns whether the current placeholder value should be shown.
   */
  bool ShouldShowPlaceholder() const;

  void ClearFiles(bool aSetValueChanged);

  void SetIndeterminateInternal(bool aValue,
                                bool aShouldInvalidate);

  /**
   * Called when an attribute is about to be changed
   */
  virtual nsresult BeforeSetAttr(int32_t aNameSpaceID, nsAtom* aName,
                                 const nsAttrValueOrString* aValue,
                                 bool aNotify) override;
  /**
   * Called when an attribute has just been changed
   */
  virtual nsresult AfterSetAttr(int32_t aNameSpaceID, nsAtom* aName,
                                const nsAttrValue* aValue,
                                const nsAttrValue* aOldValue,
                                nsIPrincipal* aSubjectPrincipal,
                                bool aNotify) override;

  virtual void BeforeSetForm(bool aBindToTree) override;

  virtual void AfterClearForm(bool aUnbindOrDelete) override;

  /**
   * Dispatch a select event. Returns true if the event was not cancelled.
   */
  bool DispatchSelectEvent(nsPresContext* aPresContext);

  void SelectAll(nsPresContext* aPresContext);
  bool IsImage() const
  {
    return AttrValueIs(kNameSpaceID_None, nsGkAtoms::type,
                       nsGkAtoms::image, eIgnoreCase);
  }

  /**
   * Visit the group of radio buttons this radio belongs to
   * @param aVisitor the visitor to visit with
   */
  nsresult VisitGroup(nsIRadioVisitor* aVisitor, bool aFlushContent);

  /**
   * Do all the work that |SetChecked| does (radio button handling, etc.), but
   * take an |aNotify| parameter.
   */
  void DoSetChecked(bool aValue, bool aNotify, bool aSetValueChanged);

  /**
   * Do all the work that |SetCheckedChanged| does (radio button handling,
   * etc.), but take an |aNotify| parameter that lets it avoid flushing content
   * when it can.
   */
  void DoSetCheckedChanged(bool aCheckedChanged, bool aNotify);

  /**
   * Actually set checked and notify the frame of the change.
   * @param aValue the value of checked to set
   */
  void SetCheckedInternal(bool aValue, bool aNotify);

  void RadioSetChecked(bool aNotify);
  void SetCheckedChanged(bool aCheckedChanged);

  /**
   * MaybeSubmitForm looks for a submit input or a single text control
   * and submits the form if either is present.
   */
  nsresult MaybeSubmitForm(nsPresContext* aPresContext);

  /**
   * Update mFileList with the currently selected file.
   */
  void UpdateFileList();

  /**
   * Called after calling one of the SetFilesOrDirectories() functions.
   * This method can explore the directory recursively if needed.
   */
  void AfterSetFilesOrDirectories(bool aSetValueChanged);

  /**
   * Recursively explore the directory and populate mFileOrDirectories correctly
   * for webkitdirectory.
   */
  void ExploreDirectoryRecursively(bool aSetValuechanged);

  /**
   * Determine whether the editor needs to be initialized explicitly for
   * a particular event.
   */
  bool NeedToInitializeEditorForEvent(EventChainPreVisitor& aVisitor) const;

  /**
   * Get the value mode of the element, depending of the type.
   */
  ValueModeType GetValueMode() const;

  /**
   * Get the mutable state of the element.
   * When the element isn't mutable (immutable), the value or checkedness
   * should not be changed by the user.
   *
   * See: http://dev.w3.org/html5/spec/forms.html#concept-input-mutable
   */
  bool IsMutable() const;

  /**
   * Returns if the readonly attribute applies for the current type.
   */
  bool DoesReadOnlyApply() const;

  /**
   * Returns if the min and max attributes apply for the current type.
   */
  bool DoesMinMaxApply() const;

  /**
   * Returns if the step attribute apply for the current type.
   */
  bool DoesStepApply() const { return DoesMinMaxApply(); }

  /**
   * Returns if stepDown and stepUp methods apply for the current type.
   */
  bool DoStepDownStepUpApply() const { return DoesStepApply(); }

  /**
   * Returns if valueAsNumber attribute applies for the current type.
   */
  bool DoesValueAsNumberApply() const { return DoesMinMaxApply(); }

  /**
   * Returns if autocomplete attribute applies for the current type.
   */
  bool DoesAutocompleteApply() const;

  void FreeData();
  nsTextEditorState *GetEditorState() const;

  mozilla::TextEditor* GetTextEditorFromState();

  /**
   * Manages the internal data storage across type changes.
   */
  void HandleTypeChange(uint8_t aNewType, bool aNotify);

  /**
   * Sanitize the value of the element depending of its current type.
   * See: http://www.whatwg.org/specs/web-apps/current-work/#value-sanitization-algorithm
   */
  void SanitizeValue(nsAString& aValue);

  /**
   * Returns whether the placeholder attribute applies for the current type.
   */
  bool PlaceholderApplies() const;

  /**
   * Set the current default value to the value of the input element.
   * @note You should not call this method if GetValueMode() doesn't return
   * VALUE_MODE_VALUE.
   */
  nsresult SetDefaultValueAsValue();

  void SetDirectionFromValue(bool aNotify);

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

    switch (GetValueMode()) {
      case VALUE_MODE_DEFAULT:
        return true;
      case VALUE_MODE_DEFAULT_ON:
        return GetCheckedChanged();
      case VALUE_MODE_VALUE:
      case VALUE_MODE_FILENAME:
        return mValueChanged;
      default:
        NS_NOTREACHED("We should not be there: there are no other modes.");
        return false;
    }
  }

  /**
   * Returns the radio group container if the element has one, null otherwise.
   * The radio group container will be the form owner if there is one.
   * The current document otherwise.
   * @return the radio group container if the element has one, null otherwise.
   */
  nsIRadioGroupContainer* GetRadioGroupContainer() const;

  /**
   * Parse a color string of the form #XXXXXX where X should be hexa characters
   * @param the string to be parsed.
   * @return whether the string is a valid simple color.
   * Note : this function does not consider the empty string as valid.
   */
  bool IsValidSimpleColor(const nsAString& aValue) const;

  /**
   * Parse a week string of the form yyyy-Www
   * @param the string to be parsed.
   * @return whether the string is a valid week.
   * Note : this function does not consider the empty string as valid.
   */
  bool IsValidWeek(const nsAString& aValue) const;

  /**
   * Parse a month string of the form yyyy-mm
   * @param the string to be parsed.
   * @return whether the string is a valid month.
   * Note : this function does not consider the empty string as valid.
   */
  bool IsValidMonth(const nsAString& aValue) const;

  /**
   * Parse a date string of the form yyyy-mm-dd
   * @param the string to be parsed.
   * @return whether the string is a valid date.
   * Note : this function does not consider the empty string as valid.
   */
  bool IsValidDate(const nsAString& aValue) const;

  /**
   * Parse a datetime-local string of the form yyyy-mm-ddThh:mm[:ss.s] or
   * yyyy-mm-dd hh:mm[:ss.s], where fractions of seconds can be 1 to 3 digits.
   *
   * @param the string to be parsed.
   * @return whether the string is a valid datetime-local string.
   * Note : this function does not consider the empty string as valid.
   */
  bool IsValidDateTimeLocal(const nsAString& aValue) const;

  /**
   * Parse a year string of the form yyyy
   *
   * @param the string to be parsed.
   *
   * @return the year in aYear.
   * @return whether the parsing was successful.
   */
  bool ParseYear(const nsAString& aValue, uint32_t* aYear) const;

  /**
   * Parse a month string of the form yyyy-mm
   *
   * @param the string to be parsed.
   * @return the year and month in aYear and aMonth.
   * @return whether the parsing was successful.
   */
  bool ParseMonth(const nsAString& aValue,
                  uint32_t* aYear,
                  uint32_t* aMonth) const;

  /**
   * Parse a week string of the form yyyy-Www
   *
   * @param the string to be parsed.
   * @return the year and week in aYear and aWeek.
   * @return whether the parsing was successful.
   */
  bool ParseWeek(const nsAString& aValue,
                 uint32_t* aYear,
                 uint32_t* aWeek) const;
  /**
   * Parse a date string of the form yyyy-mm-dd
   *
   * @param the string to be parsed.
   * @return the date in aYear, aMonth, aDay.
   * @return whether the parsing was successful.
   */
  bool ParseDate(const nsAString& aValue,
                 uint32_t* aYear,
                 uint32_t* aMonth,
                 uint32_t* aDay) const;

  /**
   * Parse a datetime-local string of the form yyyy-mm-ddThh:mm[:ss.s] or
   * yyyy-mm-dd hh:mm[:ss.s], where fractions of seconds can be 1 to 3 digits.
   *
   * @param the string to be parsed.
   * @return the date in aYear, aMonth, aDay and time expressed in milliseconds
   *         in aTime.
   * @return whether the parsing was successful.
   */
  bool ParseDateTimeLocal(const nsAString& aValue,
                          uint32_t* aYear,
                          uint32_t* aMonth,
                          uint32_t* aDay,
                          uint32_t* aTime) const;

  /**
   * Normalize the datetime-local string following the HTML specifications:
   * https://html.spec.whatwg.org/multipage/infrastructure.html#valid-normalised-local-date-and-time-string
   */
  void NormalizeDateTimeLocal(nsAString& aValue) const;

  /**
   * This methods returns the number of days since epoch for a given year and
   * week.
   */
  double DaysSinceEpochFromWeek(uint32_t aYear, uint32_t aWeek) const;

  /**
   * This methods returns the number of days in a given month, for a given year.
   */
  uint32_t NumberOfDaysInMonth(uint32_t aMonth, uint32_t aYear) const;

  /**
   * This methods returns the number of months between January 1970 and the
   * given year and month.
   */
  int32_t MonthsSinceJan1970(uint32_t aYear, uint32_t aMonth) const;

  /**
   * This methods returns the day of the week given a date. If @isoWeek is true,
   * 7=Sunday, otherwise, 0=Sunday.
   */
  uint32_t DayOfWeek(uint32_t aYear, uint32_t aMonth, uint32_t aDay,
                     bool isoWeek) const;

  /**
   * This methods returns the maximum number of week in a given year, the
   * result is either 52 or 53.
   */
  uint32_t MaximumWeekInYear(uint32_t aYear) const;

  /**
   * This methods returns true if it's a leap year.
   */
  bool IsLeapYear(uint32_t aYear) const;

  /**
   * Returns whether aValue is a valid time as described by HTML specifications:
   * http://www.whatwg.org/specs/web-apps/current-work/multipage/common-microsyntaxes.html#valid-time-string
   *
   * @param aValue the string to be tested.
   * @return Whether the string is a valid time per HTML specifications.
   */
  bool IsValidTime(const nsAString& aValue) const;

  /**
   * Returns the time expressed in milliseconds of |aValue| being parsed as a
   * time following the HTML specifications:
   * http://www.whatwg.org/specs/web-apps/current-work/#parse-a-time-string
   *
   * Note: |aResult| can be null.
   *
   * @param aValue  the string to be parsed.
   * @param aResult the time expressed in milliseconds representing the time [out]
   * @return Whether the parsing was successful.
   */
  static bool ParseTime(const nsAString& aValue, uint32_t* aResult);

  /**
   * Sets the value of the element to the string representation of the Decimal.
   *
   * @param aValue The Decimal that will be used to set the value.
   */
  void SetValue(Decimal aValue, CallerType aCallerType);

  /**
   * Update the HAS_RANGE bit field value.
   */
  void UpdateHasRange();

   /**
    * Get the step scale value for the current type.
    * See:
    * http://www.whatwg.org/specs/web-apps/current-work/multipage/common-input-element-attributes.html#concept-input-step-scale
    */
  Decimal GetStepScaleFactor() const;

  /**
   * Return the base used to compute if a value matches step.
   * Basically, it's the min attribute if present and a default value otherwise.
   *
   * @return The step base.
   */
  Decimal GetStepBase() const;

  /**
   * Returns the default step for the current type.
   * @return the default step for the current type.
   */
  Decimal GetDefaultStep() const;

  enum StepCallerType {
    CALLED_FOR_USER_EVENT,
    CALLED_FOR_SCRIPT
  };

  /**
   * Sets the aValue outparam to the value that this input would take if
   * someone tries to step aStep steps and this input's value would change as
   * a result. Leaves aValue untouched if this inputs value would not change
   * (e.g. already at max, and asking for the next step up).
   *
   * Negative aStep means step down, positive means step up.
   *
   * Returns NS_OK or else the error values that should be thrown if this call
   * was initiated by a stepUp()/stepDown() call from script under conditions
   * that such a call should throw.
   */
  nsresult GetValueIfStepped(int32_t aStepCount,
                             StepCallerType aCallerType,
                             Decimal* aNextStep);

  /**
   * Apply a step change from stepUp or stepDown by multiplying aStep by the
   * current step value.
   *
   * @param aStep The value used to be multiplied against the step value.
   */
  nsresult ApplyStep(int32_t aStep);

  /**
   * Returns if the current type is an experimental mobile type.
   */
  static bool IsExperimentalMobileType(uint8_t aType);

  /*
   * Returns if the current type is one of the date/time input types: date,
   * time and month. TODO: week and datetime-local.
   */
  static bool IsDateTimeInputType(uint8_t aType);

  /**
   * Flushes the layout frame tree to make sure we have up-to-date frames.
   */
  void FlushFrames();

  /**
   * Returns true if the element should prevent dispatching another DOMActivate.
   * This is used in situations where the anonymous subtree should already have
   * sent a DOMActivate and prevents firing more than once.
   */
  bool ShouldPreventDOMActivateDispatch(EventTarget* aOriginalTarget);

  /**
   * Some input type (color and file) let user choose a value using a picker:
   * this function checks if it is needed, and if so, open the corresponding
   * picker (color picker or file picker).
   */
  nsresult MaybeInitPickers(EventChainPostVisitor& aVisitor);

  enum FilePickerType {
    FILE_PICKER_FILE,
    FILE_PICKER_DIRECTORY
  };
  nsresult InitFilePicker(FilePickerType aType);
  nsresult InitColorPicker();

  /**
   * Use this function before trying to open a picker.
   * It checks if the page is allowed to open a new pop-up.
   * If it returns true, you should not create the picker.
   *
   * @return true if popup should be blocked, false otherwise
   */
  bool IsPopupBlocked() const;

  GetFilesHelper* GetOrCreateGetFilesHelper(bool aRecursiveFlag,
                                            ErrorResult& aRv);

  void ClearGetFilesHelpers();

  /**
   * nsINode::SetMayBeApzAware() will be invoked in this function if necessary
   * to prevent default action of APZC so that we can increase/decrease the
   * value of this InputElement when mouse wheel event comes without scrolling
   * the page.
   *
   * SetMayBeApzAware() will set flag MayBeApzAware which is checked by apzc to
   * decide whether to add this element into its dispatch-to-content region.
   */
  void UpdateApzAwareFlag();

  /**
   * A helper to get the current selection range.  Will throw on the ErrorResult
   * if we have no editor state.
   */
  void GetSelectionRange(uint32_t* aSelectionStart,
                         uint32_t* aSelectionEnd,
                         ErrorResult& aRv);

  /**
   * Override for nsImageLoadingContent.
   */
  nsIContent* AsContent() override { return this; }

  nsCOMPtr<nsIControllers> mControllers;

  /*
   * In mInputData, the mState field is used if IsSingleLineTextControl returns
   * true and mValue is used otherwise.  We have to be careful when handling it
   * on a type change.
   *
   * Accessing the mState member should be done using the GetEditorState function,
   * which returns null if the state is not present.
   */
  union InputData {
    /**
     * The current value of the input if it has been changed from the default
     */
    char16_t*               mValue;
    /**
     * The state of the text editor associated with the text/password input
     */
    nsTextEditorState*       mState;
  } mInputData;

  struct FileData;
  UniquePtr<FileData> mFileData;

  /**
   * The value of the input element when first initialized and it is updated
   * when the element is either changed through a script, focused or dispatches
   * a change event. This is to ensure correct future change event firing.
   * NB: This is ONLY applicable where the element is a text control. ie,
   * where type= "text", "email", "search", "tel", "url" or "password".
   */
  nsString mFocusedValue;

  /**
   * If mIsDraggingRange is true, this is the value that the input had before
   * the drag started. Used to reset the input to its old value if the drag is
   * canceled.
   */
  Decimal mRangeThumbDragStartValue;

  /**
   * Current value in the input box, in DateTimeValue dictionary format, see
   * HTMLInputElement.webidl for details.
   */
  nsAutoPtr<DateTimeValue> mDateTimeInputBoxValue;

  /**
   * The selection properties cache for number controls.  This is needed because
   * the number controls don't recycle their text field, so the normal cache in
   * nsTextEditorState cannot do its job.
   */
  nsTextEditorState::SelectionProperties mSelectionProperties;

  /**
   * The triggering principal for the src attribute.
   */
  nsCOMPtr<nsIPrincipal> mSrcTriggeringPrincipal;

  /*
   * InputType object created based on input type.
   */
  UniquePtr<InputType, DoNotDelete> mInputType;

  // Memory allocated for mInputType, reused when type changes.
  char mInputTypeMem[INPUT_TYPE_SIZE];

  // Step scale factor values, for input types that have one.
  static const Decimal kStepScaleFactorDate;
  static const Decimal kStepScaleFactorNumberRange;
  static const Decimal kStepScaleFactorTime;
  static const Decimal kStepScaleFactorMonth;
  static const Decimal kStepScaleFactorWeek;

  // Default step base value when a type do not have specific one.
  static const Decimal kDefaultStepBase;
  // Default step base value when type=week does not not have a specific one,
  // which is −259200000, the start of week 1970-W01.
  static const Decimal kDefaultStepBaseWeek;

  // Default step used when there is no specified step.
  static const Decimal kDefaultStep;
  static const Decimal kDefaultStepTime;

  // Float value returned by GetStep() when the step attribute is set to 'any'.
  static const Decimal kStepAny;

  // Minimum year limited by HTML standard, year >= 1.
  static const double kMinimumYear;
  // Maximum year limited by ECMAScript date object range, year <= 275760.
  static const double kMaximumYear;
  // Maximum valid week is 275760-W37.
  static const double kMaximumWeekInMaximumYear;
  // Maximum valid day is 275760-09-13.
  static const double kMaximumDayInMaximumYear;
  // Maximum valid month is 275760-09.
  static const double kMaximumMonthInMaximumYear;
  // Long years in a ISO calendar have 53 weeks in them.
  static const double kMaximumWeekInYear;
  // Milliseconds in a day.
  static const double kMsPerDay;

  nsContentUtils::AutocompleteAttrState mAutocompleteAttrState;
  nsContentUtils::AutocompleteAttrState mAutocompleteInfoState;
  bool                     mDisabledChanged     : 1;
  bool                     mValueChanged        : 1;
  bool                     mLastValueChangeWasInteractive : 1;
  bool                     mCheckedChanged      : 1;
  bool                     mChecked             : 1;
  bool                     mHandlingSelectEvent : 1;
  bool                     mShouldInitChecked   : 1;
  bool                     mDoneCreating        : 1;
  bool                     mInInternalActivate  : 1;
  bool                     mCheckedIsToggled    : 1;
  bool                     mIndeterminate       : 1;
  bool                     mInhibitRestoration  : 1;
  bool                     mCanShowValidUI      : 1;
  bool                     mCanShowInvalidUI    : 1;
  bool                     mHasRange            : 1;
  bool                     mIsDraggingRange     : 1;
  bool                     mNumberControlSpinnerIsSpinning : 1;
  bool                     mNumberControlSpinnerSpinsUp : 1;
  bool                     mPickerRunning : 1;
  bool                     mSelectionCached : 1;
  bool                     mIsPreviewEnabled : 1;
  bool                     mHasPatternAttribute : 1;

private:
  static void MapAttributesIntoRule(const nsMappedAttributes* aAttributes,
                                    GenericSpecifiedValues* aGenericData);

  /**
   * Returns true if this input's type will fire a DOM "change" event when it
   * loses focus if its value has changed since it gained focus.
   */
  bool MayFireChangeOnBlur() const {
    return MayFireChangeOnBlur(mType);
  }

  /**
   * Returns true if selection methods can be called on element
   */
  bool SupportsTextSelection() const {
    return mType == NS_FORM_INPUT_TEXT || mType == NS_FORM_INPUT_SEARCH ||
           mType == NS_FORM_INPUT_URL || mType == NS_FORM_INPUT_TEL ||
           mType == NS_FORM_INPUT_PASSWORD;
  }

  static bool MayFireChangeOnBlur(uint8_t aType) {
    return IsSingleLineTextControl(false, aType) ||
           aType == NS_FORM_INPUT_RANGE ||
           aType == NS_FORM_INPUT_NUMBER ||
           aType == NS_FORM_INPUT_TIME ||
           aType == NS_FORM_INPUT_DATE;
  }

  /**
   * Checks if aDateTimeInputType should be supported based on "dom.forms.datetime",
   * and "dom.experimental_forms".
   */
  static bool
  IsDateTimeTypeSupported(uint8_t aDateTimeInputType);

  /**
   * Checks preference "dom.webkitBlink.filesystem.enabled" to determine if
   * webkitEntries should be supported.
   */
  static bool
  IsWebkitFileSystemEnabled();

  /**
   * Checks preference "dom.input.dirpicker" to determine if file and directory
   * entries API should be supported.
   */
  static bool
  IsDirPickerEnabled();

  /**
   * Checks preference "dom.experimental_forms" to determine if experimental
   * implementation of input element should be enabled.
   */
  static bool
  IsExperimentalFormsEnabled();

  /**
   * Checks preference "dom.forms.datetime" to determine if input date and time
   * should be supported.
   */
  static bool
  IsInputDateTimeEnabled();

  /**
   * Checks preference "dom.forms.datetime.others" to determine if input week,
   * month and datetime-local should be supported.
   */
  static bool
  IsInputDateTimeOthersEnabled();

  /**
   * Checks preference "dom.forms.number" to determine if input type=number
   * should be supported.
   */
  static bool
  IsInputNumberEnabled();

  /**
   * Checks preference "dom.forms.color" to determine if date/time related
   * types should be supported.
   */
  static bool
  IsInputColorEnabled();

  struct nsFilePickerFilter {
    nsFilePickerFilter()
      : mFilterMask(0) {}

    explicit nsFilePickerFilter(int32_t aFilterMask)
      : mFilterMask(aFilterMask) {}

    nsFilePickerFilter(const nsString& aTitle,
                       const nsString& aFilter)
      : mFilterMask(0), mTitle(aTitle), mFilter(aFilter) {}

    nsFilePickerFilter(const nsFilePickerFilter& other) {
      mFilterMask = other.mFilterMask;
      mTitle = other.mTitle;
      mFilter = other.mFilter;
    }

    bool operator== (const nsFilePickerFilter& other) const {
      if ((mFilter == other.mFilter) && (mFilterMask == other.mFilterMask)) {
        return true;
      } else {
        return false;
      }
    }

    // Filter mask, using values defined in nsIFilePicker
    int32_t mFilterMask;
    // If mFilterMask is defined, mTitle and mFilter are useless and should be
    // ignored
    nsString mTitle;
    nsString mFilter;
  };

  class nsFilePickerShownCallback
    : public nsIFilePickerShownCallback
  {
    virtual ~nsFilePickerShownCallback()
    { }

  public:
    nsFilePickerShownCallback(HTMLInputElement* aInput,
                              nsIFilePicker* aFilePicker);
    NS_DECL_ISUPPORTS

    NS_IMETHOD Done(int16_t aResult) override;

  private:
    nsCOMPtr<nsIFilePicker> mFilePicker;
    RefPtr<HTMLInputElement> mInput;
  };

  static void ReleaseTextEditorState(nsTextEditorState* aState);

  static nsTextEditorState* sCachedTextEditorState;
  static bool sShutdown;
};

} // namespace dom
} // namespace mozilla

#endif
