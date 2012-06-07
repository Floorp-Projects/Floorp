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
   * @param aURI URI of the current page
   * @param aFile path to the last used directory
   */
  nsresult FetchLastUsedDirectory(nsIURI* aURI, nsIFile** aFile);

  /**
   * Store the last used directory for this location using the
   * content pref service, if it is available
   * @param aURI URI of the current page
   * @param aFile file chosen by the user - the path to the parent of this
   *        file will be stored
   */
  nsresult StoreLastUsedDirectory(nsIURI* aURI, nsIFile* aFile);
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

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  // nsIDOMNode
  NS_FORWARD_NSIDOMNODE(nsGenericHTMLFormElement::)

  // nsIDOMElement
  NS_FORWARD_NSIDOMELEMENT(nsGenericHTMLFormElement::)

  // nsIDOMHTMLElement
  NS_FORWARD_NSIDOMHTMLELEMENT_BASIC(nsGenericHTMLFormElement::)
  NS_SCRIPTABLE NS_IMETHOD Click();
  NS_SCRIPTABLE NS_IMETHOD GetTabIndex(PRInt32* aTabIndex);
  NS_SCRIPTABLE NS_IMETHOD SetTabIndex(PRInt32 aTabIndex);
  NS_SCRIPTABLE NS_IMETHOD Focus();
  NS_SCRIPTABLE NS_IMETHOD GetDraggable(bool* aDraggable) {
    return nsGenericHTMLFormElement::GetDraggable(aDraggable);
  }
  NS_SCRIPTABLE NS_IMETHOD GetInnerHTML(nsAString& aInnerHTML) {
    return nsGenericHTMLFormElement::GetInnerHTML(aInnerHTML);
  }
  NS_SCRIPTABLE NS_IMETHOD SetInnerHTML(const nsAString& aInnerHTML) {
    return nsGenericHTMLFormElement::SetInnerHTML(aInnerHTML);
  }

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
  NS_IMETHOD_(PRUint32) GetType() const { return mType; }
  NS_IMETHOD Reset();
  NS_IMETHOD SubmitNamesValues(nsFormSubmission* aFormSubmission);
  NS_IMETHOD SaveState();
  virtual bool RestoreState(nsPresState* aState);
  virtual bool AllowDrop();

  virtual void FieldSetDisabledChanged(bool aNotify);

  // nsIContent
  virtual bool IsHTMLFocusable(bool aWithMouse, bool *aIsFocusable, PRInt32 *aTabIndex);

  virtual bool ParseAttribute(PRInt32 aNamespaceID,
                                nsIAtom* aAttribute,
                                const nsAString& aValue,
                                nsAttrValue& aResult);
  virtual nsChangeHint GetAttributeChangeHint(const nsIAtom* aAttribute,
                                              PRInt32 aModType) const;
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
  NS_IMETHOD_(PRInt32) GetCols();
  NS_IMETHOD_(PRInt32) GetWrapCols();
  NS_IMETHOD_(PRInt32) GetRows();
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
  NS_IMETHOD_(void) SetPlaceholderClass(bool aVisible, bool aNotify);
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

  static nsHTMLInputElement* FromContent(nsIContent *aContent)
  {
    if (aContent->NodeInfo()->Equals(nsGkAtoms::input, kNameSpaceID_XHTML))
      return static_cast<nsHTMLInputElement*>(aContent);
    return NULL;
  }

  // nsIConstraintValidation
  bool     IsTooLong();
  bool     IsValueMissing() const;
  bool     HasTypeMismatch() const;
  bool     HasPatternMismatch() const;
  void     UpdateTooLongValidityState();
  void     UpdateValueMissingValidityState();
  void     UpdateTypeMismatchValidityState();
  void     UpdatePatternMismatchValidityState();
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
   * @note This method shouldn't be called if the radio elemnet hasn't a group.
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
  PRInt32 GetFilterFromAccept();

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

  nsresult GetSelectionRange(PRInt32* aSelectionStart, PRInt32* aSelectionEnd);

  /**
   * Called when an attribute is about to be changed
   */
  virtual nsresult BeforeSetAttr(PRInt32 aNameSpaceID, nsIAtom* aName,
                                 const nsAttrValueOrString* aValue,
                                 bool aNotify);
  /**
   * Called when an attribute has just been changed
   */
  virtual nsresult AfterSetAttr(PRInt32 aNameSpaceID, nsIAtom* aName,
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
  nsresult DoSetChecked(bool aValue, bool aNotify, bool aSetValueChanged);

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

  nsresult RadioSetChecked(bool aNotify);
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
   * Returns if the maxlength attribute applies for the current type.
   */
  bool MaxLengthApplies() const { return IsSingleLineTextControl(false, mType); }

  void FreeData();
  nsTextEditorState *GetEditorState() const;

  /**
   * Manages the internal data storage across type changes.
   */
  void HandleTypeChange(PRUint8 aNewType);

  /**
   * Sanitize the value of the element depending of its current type.
   * See: http://www.whatwg.org/specs/web-apps/current-work/#value-sanitization-algorithm
   */
  void SanitizeValue(nsAString& aValue);

  /**
   * Returns whether the placeholder attribute applies for the current type.
   */
  bool PlaceholderApplies() const { return IsSingleLineTextControl(false, mType); }

  /**
   * Set the current default value to the value of the input element.
   * @note You should not call this method if GetValueMode() doesn't return
   * VALUE_MODE_VALUE.
   */
  nsresult SetDefaultValueAsValue();

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

  /**
   * The type of this input (<input type=...>) as an integer.
   * @see nsIFormControl.h (specifically NS_FORM_INPUT_*)
   */
  PRUint8                  mType;
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

private:
  struct nsFilePickerFilter {
    nsFilePickerFilter()
      : mFilterMask(0), mIsTrusted(false) {}

    nsFilePickerFilter(PRInt32 aFilterMask)
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
    PRInt32 mFilterMask;
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
};

#endif
