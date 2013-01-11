/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsHTMLInputElement_h__
#define nsHTMLInputElement_h__

#include "nsGenericHTMLElement.h"
#include "nsImageLoadingContent.h"
#include "nsIDOMHTMLInputElement.h"
#include "nsITextControlElement.h"
#include "nsIPhonetic.h"
#include "nsIDOMNSEditableElement.h"
#include "nsTextEditorState.h"
#include "nsCOMPtr.h"
#include "nsIConstraintValidation.h"
#include "nsDOMFile.h"
#include "nsHTMLFormElement.h" // for ShouldShowInvalidUI()
#include "nsIFile.h"
#include "nsIFilePicker.h"

class nsDOMFileList;
class nsIFilePicker;
class nsIRadioGroupContainer;
class nsIRadioGroupVisitor;
class nsIRadioVisitor;

class UploadLastDir MOZ_FINAL : public nsIObserver, public nsSupportsWeakReference {
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER

  /**
   * Fetch the last used directory for this location from the content
   * pref service, if it is available.
   *
   * @param aDoc  current document
   * @param aFile path to the last used directory
   */
  nsresult FetchLastUsedDirectory(nsIDocument* aDoc, nsIFile** aFile);

  /**
   * Store the last used directory for this location using the
   * content pref service, if it is available
   * @param aURI URI of the current page
   * @param aFile file chosen by the user - the path to the parent of this
   *        file will be stored
   */
  nsresult StoreLastUsedDirectory(nsIDocument* aDoc, nsIFile* aFile);
};

class nsHTMLInputElement : public nsGenericHTMLFormElement,
                           public nsImageLoadingContent,
                           public nsIDOMHTMLInputElement,
                           public nsITextControlElement,
                           public nsIPhonetic,
                           public nsIDOMNSEditableElement,
                           public nsIConstraintValidation
{
public:
  using nsIConstraintValidation::GetValidationMessage;

  nsHTMLInputElement(already_AddRefed<nsINodeInfo> aNodeInfo,
                     mozilla::dom::FromParser aFromParser);
  virtual ~nsHTMLInputElement();

  NS_IMPL_FROMCONTENT_HTML_WITH_TAG(nsHTMLInputElement, input)

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  // nsIDOMNode
  NS_FORWARD_NSIDOMNODE_TO_NSINODE

  // nsIDOMElement
  NS_FORWARD_NSIDOMELEMENT_TO_GENERIC

  // nsIDOMHTMLElement
  NS_FORWARD_NSIDOMHTMLELEMENT_TO_GENERIC

  virtual void Click() MOZ_OVERRIDE;
  virtual int32_t TabIndexDefault() MOZ_OVERRIDE;
  virtual void Focus(mozilla::ErrorResult& aError) MOZ_OVERRIDE;

  // nsIDOMHTMLInputElement
  NS_DECL_NSIDOMHTMLINPUTELEMENT

  // nsIPhonetic
  NS_DECL_NSIPHONETIC

  // nsIDOMNSEditableElement
  NS_IMETHOD GetEditor(nsIEditor** aEditor)
  {
    return nsGenericHTMLElement::GetEditor(aEditor);
  }

  NS_IMETHOD SetUserInput(const nsAString& aInput);

  // Overriden nsIFormControl methods
  NS_IMETHOD_(uint32_t) GetType() const { return mType; }
  NS_IMETHOD Reset();
  NS_IMETHOD SubmitNamesValues(nsFormSubmission* aFormSubmission);
  NS_IMETHOD SaveState();
  virtual bool RestoreState(nsPresState* aState);
  virtual bool AllowDrop();
  virtual bool IsDisabledForEvents(uint32_t aMessage);

  virtual void FieldSetDisabledChanged(bool aNotify);

  // nsIContent
  virtual bool IsHTMLFocusable(bool aWithMouse, bool *aIsFocusable, int32_t *aTabIndex);

  virtual bool ParseAttribute(int32_t aNamespaceID,
                                nsIAtom* aAttribute,
                                const nsAString& aValue,
                                nsAttrValue& aResult);
  virtual nsChangeHint GetAttributeChangeHint(const nsIAtom* aAttribute,
                                              int32_t aModType) const;
  NS_IMETHOD_(bool) IsAttributeMapped(const nsIAtom* aAttribute) const;
  virtual nsMapRuleToAttributesFunc GetAttributeMappingFunction() const;

  virtual nsresult PreHandleEvent(nsEventChainPreVisitor& aVisitor);
  virtual nsresult PostHandleEvent(nsEventChainPostVisitor& aVisitor);

  virtual nsresult BindToTree(nsIDocument* aDocument, nsIContent* aParent,
                              nsIContent* aBindingParent,
                              bool aCompileEventHandlers);
  virtual void UnbindFromTree(bool aDeep = true,
                              bool aNullParent = true);

  virtual void DoneCreatingElement();

  virtual nsEventStates IntrinsicState() const;

  // nsITextControlElement
  NS_IMETHOD SetValueChanged(bool aValueChanged);
  NS_IMETHOD_(bool) IsSingleLineTextControl() const;
  NS_IMETHOD_(bool) IsTextArea() const;
  NS_IMETHOD_(bool) IsPlainTextControl() const;
  NS_IMETHOD_(bool) IsPasswordTextControl() const;
  NS_IMETHOD_(int32_t) GetCols();
  NS_IMETHOD_(int32_t) GetWrapCols();
  NS_IMETHOD_(int32_t) GetRows();
  NS_IMETHOD_(void) GetDefaultValueFromContent(nsAString& aValue);
  NS_IMETHOD_(bool) ValueChanged() const;
  NS_IMETHOD_(void) GetTextEditorValue(nsAString& aValue, bool aIgnoreWrap) const;
  NS_IMETHOD_(nsIEditor*) GetTextEditor();
  NS_IMETHOD_(nsISelectionController*) GetSelectionController();
  NS_IMETHOD_(nsFrameSelection*) GetConstFrameSelection();
  NS_IMETHOD BindToFrame(nsTextControlFrame* aFrame);
  NS_IMETHOD_(void) UnbindFromFrame(nsTextControlFrame* aFrame);
  NS_IMETHOD CreateEditor();
  NS_IMETHOD_(nsIContent*) GetRootEditorNode();
  NS_IMETHOD_(nsIContent*) CreatePlaceholderNode();
  NS_IMETHOD_(nsIContent*) GetPlaceholderNode();
  NS_IMETHOD_(void) UpdatePlaceholderVisibility(bool aNotify);
  NS_IMETHOD_(bool) GetPlaceholderVisibility();
  NS_IMETHOD_(void) InitializeKeyboardEventListeners();
  NS_IMETHOD_(void) OnValueChanged(bool aNotify);
  NS_IMETHOD_(bool) HasCachedSelection();

  void GetDisplayFileName(nsAString& aFileName) const;
  const nsCOMArray<nsIDOMFile>& GetFiles() const;
  void SetFiles(const nsCOMArray<nsIDOMFile>& aFiles, bool aSetValueChanged);
  void SetFiles(nsIDOMFileList* aFiles, bool aSetValueChanged);

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
  already_AddRefed<nsIDOMHTMLInputElement> GetSelectedRadioButton();

  virtual nsresult Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const;

  NS_IMETHOD FireAsyncClickHandler();

  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(nsHTMLInputElement,
                                           nsGenericHTMLFormElement)

  static UploadLastDir* gUploadLastDir;
  // create and destroy the static UploadLastDir object for remembering
  // which directory was last used on a site-by-site basis
  static void InitUploadLastDir();
  static void DestroyUploadLastDir();

  void MaybeLoadImage();

  virtual nsXPCClassInfo* GetClassInfo();

  virtual nsIDOMNode* AsDOMNode() { return this; }

  // nsIConstraintValidation
  bool     IsTooLong();
  bool     IsValueMissing() const;
  bool     HasTypeMismatch() const;
  bool     HasPatternMismatch() const;
  bool     IsRangeOverflow() const;
  bool     IsRangeUnderflow() const;
  bool     HasStepMismatch() const;
  void     UpdateTooLongValidityState();
  void     UpdateValueMissingValidityState();
  void     UpdateTypeMismatchValidityState();
  void     UpdatePatternMismatchValidityState();
  void     UpdateRangeOverflowValidityState();
  void     UpdateRangeUnderflowValidityState();
  void     UpdateStepMismatchValidityState();
  void     UpdateAllValidityStates(bool aNotify);
  void     UpdateBarredFromConstraintValidation();
  nsresult GetValidationMessage(nsAString& aValidationMessage,
                                ValidityStateType aType);
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
   * filter specifed or not.
   * @note If there is only one valid filter that is audio or video or image,
   * it will be selected as the default filter. Otherwise "All files" remains
   * the default filter.
   * @note If more than one valid filter is found, the "All Supported Types"
   * filter is added, which is the concatenation of all valid filters.
   */
  void SetFilePickerFiltersFromAccept(nsIFilePicker* filePicker);

  /**
   * Returns the filter which should be used for the file picker according to
   * the accept attribute value.
   *
   * See:
   * http://dev.w3.org/html5/spec/forms.html#attr-input-accept
   *
   * @return Filter to use on the file picker with AppendFilters, 0 if none.
   *
   * @note You should not call this function if the element has no @accept.
   * @note This will only filter for one type of file. If more than one filter
   * is specified by the accept attribute they will *all* be ignored.
   */
  int32_t GetFilterFromAccept();

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

  bool DefaultChecked() const {
    return HasAttr(kNameSpaceID_None, nsGkAtoms::checked);
  }

  bool Indeterminate() const { return mIndeterminate; }
  bool Checked() const { return mChecked; }

  /**
   * Fires change event if mFocusedValue and current value held are unequal.
   */
  void FireChangeEventIfNeeded();

protected:
  // Pull IsSingleLineTextControl into our scope, otherwise it'd be hidden
  // by the nsITextControlElement version.
  using nsGenericHTMLFormElement::IsSingleLineTextControl;

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
   * This helper method returns true if aValue is a valid email address.
   * This is following the HTML5 specification:
   * http://dev.w3.org/html5/spec/forms.html#valid-e-mail-address
   *
   * @param aValue  the email address to check.
   * @result        whether the given string is a valid email address.
   */
  static bool IsValidEmailAddress(const nsAString& aValue);

  /**
   * This helper method returns true if aValue is a valid email address list.
   * Email address list is a list of email address separated by comas (,) which
   * can be surrounded by space charecters.
   * This is following the HTML5 specification:
   * http://dev.w3.org/html5/spec/forms.html#valid-e-mail-address-list
   *
   * @param aValue  the email address list to check.
   * @result        whether the given string is a valid email address list.
   */
  static bool IsValidEmailAddressList(const nsAString& aValue);

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
  nsresult SetValueInternal(const nsAString& aValue,
                            bool aUserInput,
                            bool aSetValueChanged);

  nsresult GetValueInternal(nsAString& aValue) const;

  /**
   * Returns whether the current value is the empty string.
   *
   * @return whether the current value is the empty string.
   */
  bool IsValueEmpty() const;

  void ClearFiles(bool aSetValueChanged) {
    nsCOMArray<nsIDOMFile> files;
    SetFiles(files, aSetValueChanged);
  }

  nsresult SetIndeterminateInternal(bool aValue,
                                    bool aShouldInvalidate);

  nsresult GetSelectionRange(int32_t* aSelectionStart, int32_t* aSelectionEnd);

  /**
   * Called when an attribute is about to be changed
   */
  virtual nsresult BeforeSetAttr(int32_t aNameSpaceID, nsIAtom* aName,
                                 const nsAttrValueOrString* aValue,
                                 bool aNotify);
  /**
   * Called when an attribute has just been changed
   */
  virtual nsresult AfterSetAttr(int32_t aNameSpaceID, nsIAtom* aName,
                                const nsAttrValue* aValue, bool aNotify);

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
  nsresult UpdateFileList();

  /**
   * Called after calling one of the SetFiles() functions.
   */
  void AfterSetFiles(bool aSetValueChanged);

  /**
   * Determine whether the editor needs to be initialized explicitly for
   * a particular event.
   */
  bool NeedToInitializeEditorForEvent(nsEventChainPreVisitor& aVisitor) const;

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
   * Returns if the required attribute applies for the current type.
   */
  bool DoesRequiredApply() const;

  /**
   * Returns if the pattern attribute applies for the current type.
   */
  bool DoesPatternApply() const;

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
   * Returns if the maxlength attribute applies for the current type.
   */
  bool MaxLengthApplies() const { return IsSingleLineTextControl(false, mType); }

  void FreeData();
  nsTextEditorState *GetEditorState() const;

  /**
   * Manages the internal data storage across type changes.
   */
  void HandleTypeChange(uint8_t aNewType);

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

  virtual void SetDirectionIfAuto(bool aAuto, bool aNotify);

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
   * Returns the input element's value as a double-precision float.
   * Returns NaN if the current element's value is not a floating point number.
   *
   * @return the input element's value as a double-precision float.
   */
  double GetValueAsDouble() const;

  /**
   * Convert a string to a number in a type specific way,
   * http://www.whatwg.org/specs/web-apps/current-work/multipage/the-input-element.html#concept-input-value-string-number
   * ie parse a date string to a timestamp if type=date,
   * or parse a number string to its value if type=number.
   * @param aValue the string to be parsed.
   * @param aResultValue the timestamp as a double.
   * @result whether the parsing was successful.
   */
  bool ConvertStringToNumber(nsAString& aValue, double& aResultValue) const;

  /**
   * Convert a double to a string in a type specific way, ie convert a timestamp
   * to a date string if type=date or append the number string representing the
   * value if type=number.
   *
   * @param aValue the double to be converted
   * @param aResultString [out] the string representing the double
   * @return whether the function succeded, it will fail if the current input's
   *         type is not supported or the number can't be converted to a string
   *         as expected by the type.
   */
  bool ConvertNumberToString(double aValue, nsAString& aResultString) const;

  /**
   * Parse a date string of the form yyyy-mm-dd
   * @param the string to be parsed.
   * @return whether the string is a valid date.
   * Note : this function does not consider the empty string as valid.
   */
  bool IsValidDate(nsAString& aValue) const;

  /**
   * Parse a date string of the form yyyy-mm-dd
   * @param the string to be parsed.
   * @return the date in aYear, aMonth, aDay.
   * @return whether the parsing was successful.
   */
  bool GetValueAsDate(nsAString& aValue,
                      uint32_t& aYear,
                      uint32_t& aMonth,
                      uint32_t& aDay) const;

  /**
   * This methods returns the number of days in a given month, for a given year.
   */
  uint32_t NumberOfDaysInMonth(uint32_t aMonth, uint32_t aYear) const;

  /**
   * Returns whether aValue is a valid time as described by HTML specifications:
   * http://www.whatwg.org/specs/web-apps/current-work/multipage/common-microsyntaxes.html#valid-time-string
   *
   * @param aValue the string to be tested.
   * @return Whether the string is a valid time per HTML specifications.
   */
  bool IsValidTime(const nsAString& aValue) const;

  /**
   * Sets the value of the element to the string representation of the double.
   *
   * @param aValue The double that will be used to set the value.
   */
  void SetValue(double aValue);

  /**
   * Update the HAS_RANGE bit field value.
   */
  void UpdateHasRange();

  /**
   * Returns the min attribute as a double.
   * Returns NaN if the min attribute isn't a valid floating point number.
   */
  double GetMinAsDouble() const;

  /**
   * Returns the max attribute as a double.
   * Returns NaN if the max attribute isn't a valid floating point number.
   */
  double GetMaxAsDouble() const;

   /**
    * Get the step scale value for the current type.
    * See:
    * http://www.whatwg.org/specs/web-apps/current-work/multipage/common-input-element-attributes.html#concept-input-step-scale
    */
  double GetStepScaleFactor() const;

  /**
   * Returns the current step value.
   * Returns kStepAny if the current step is "any" string.
   *
   * @return the current step value.
   */
  double GetStep() const;

  /**
   * Return the base used to compute if a value matches step.
   * Basically, it's the min attribute if present and a default value otherwise.
   *
   * @return The step base.
   */
  double GetStepBase() const;

  /**
   * Apply a step change from stepUp or stepDown by multiplying aStep by the
   * current step value.
   *
   * @param aStep The value used to be multiplied against the step value.
   */
  nsresult ApplyStep(int32_t aStep);

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
    char*                    mValue;
    /**
     * The state of the text editor associated with the text/password input
     */
    nsTextEditorState*       mState;
  } mInputData;
  /**
   * The value of the input if it is a file input. This is the list of filenames
   * used when uploading a file. It is vital that this is kept separate from
   * mValue so that it won't be possible to 'leak' the value from a text-input
   * to a file-input. Additionally, the logic for this value is kept as simple
   * as possible to avoid accidental errors where the wrong filename is used.
   * Therefor the list of filenames is always owned by this member, never by
   * the frame. Whenever the frame wants to change the filename it has to call
   * SetFileNames to update this member.
   */
  nsCOMArray<nsIDOMFile>   mFiles;

  nsRefPtr<nsDOMFileList>  mFileList;

  nsString mStaticDocFileList;
  
  /** 
   * The value of the input element when first initialized and it is updated
   * when the element is either changed through a script, focused or dispatches   
   * a change event. This is to ensure correct future change event firing.
   * NB: This is ONLY applicable where the element is a text control. ie,
   * where type= "text", "email", "search", "tel", "url" or "password".
   */
  nsString mFocusedValue;  

  // Step scale factor values, for input types that have one.
  static const double kStepScaleFactorDate;
  static const double kStepScaleFactorNumber;

  // Default step base value when a type do not have specific one.
  static const double kDefaultStepBase;
  // Float alue returned by GetStep() when the step attribute is set to 'any'.
  static const double kStepAny;

  /**
   * The type of this input (<input type=...>) as an integer.
   * @see nsIFormControl.h (specifically NS_FORM_INPUT_*)
   */
  uint8_t                  mType;
  bool                     mDisabledChanged     : 1;
  bool                     mValueChanged        : 1;
  bool                     mCheckedChanged      : 1;
  bool                     mChecked             : 1;
  bool                     mHandlingSelectEvent : 1;
  bool                     mShouldInitChecked   : 1;
  bool                     mParserCreating      : 1;
  bool                     mInInternalActivate  : 1;
  bool                     mCheckedIsToggled    : 1;
  bool                     mIndeterminate       : 1;
  bool                     mInhibitRestoration  : 1;
  bool                     mCanShowValidUI      : 1;
  bool                     mCanShowInvalidUI    : 1;
  bool                     mHasRange            : 1;

private:
  struct nsFilePickerFilter {
    nsFilePickerFilter()
      : mFilterMask(0), mIsTrusted(false) {}

    nsFilePickerFilter(int32_t aFilterMask)
      : mFilterMask(aFilterMask), mIsTrusted(true) {}

    nsFilePickerFilter(const nsString& aTitle,
                       const nsString& aFilter,
                       const bool aIsTrusted = false)
      : mFilterMask(0), mTitle(aTitle), mFilter(aFilter), mIsTrusted(aIsTrusted) {}

    nsFilePickerFilter(const nsFilePickerFilter& other) {
      mFilterMask = other.mFilterMask;
      mTitle = other.mTitle;
      mFilter = other.mFilter;
      mIsTrusted = other.mIsTrusted;
    }

    bool operator== (const nsFilePickerFilter& other) const {
      if ((mFilter == other.mFilter) && (mFilterMask == other.mFilterMask)) {
        NS_ASSERTION(mIsTrusted == other.mIsTrusted,
                     "Filter with similar list of extensions and mask should"
                     " have the same trusted flag value");
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
    // mIsTrusted is true if mime type comes from a "trusted" source (e.g. our
    // hard-coded set).
    // false means it may come from an "untrusted" source (e.g. OS mime types
    // mapping, which can be different accross OS, user's personal configuration, ...)
    // For now, only mask filters are considered to be "trusted".
    bool mIsTrusted; 
  };

  class AsyncClickHandler
    : public nsRunnable
  {
  public:
    AsyncClickHandler(nsHTMLInputElement* aInput);
    NS_IMETHOD Run();

  protected:
    nsRefPtr<nsHTMLInputElement> mInput;
    PopupControlState mPopupControlState;
  };

  class nsFilePickerShownCallback
    : public nsIFilePickerShownCallback
  {
  public:
    nsFilePickerShownCallback(nsHTMLInputElement* aInput,
                              nsIFilePicker* aFilePicker,
                              bool aMulti);
    virtual ~nsFilePickerShownCallback()
    { }

    NS_DECL_ISUPPORTS

    NS_IMETHOD Done(int16_t aResult);

  private:
    nsCOMPtr<nsIFilePicker> mFilePicker;
    nsRefPtr<nsHTMLInputElement> mInput;
    bool mMulti;
  };
};

#endif
