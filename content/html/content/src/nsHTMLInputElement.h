/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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

//
// Accessors for mBitField
//
#define BF_DISABLED_CHANGED 0
#define BF_HANDLING_CLICK 1
#define BF_VALUE_CHANGED 2
#define BF_CHECKED_CHANGED 3
#define BF_CHECKED 4
#define BF_HANDLING_SELECT_EVENT 5
#define BF_SHOULD_INIT_CHECKED 6
#define BF_PARSER_CREATING 7
#define BF_IN_INTERNAL_ACTIVATE 8
#define BF_CHECKED_IS_TOGGLED 9
#define BF_INDETERMINATE 10
#define BF_INHIBIT_RESTORATION 11

#define GET_BOOLBIT(bitfield, field) (((bitfield) & (0x01 << (field))) \
                                        ? PR_TRUE : PR_FALSE)
#define SET_BOOLBIT(bitfield, field, b) ((b) \
                                        ? ((bitfield) |=  (0x01 << (field))) \
                                        : ((bitfield) &= ~(0x01 << (field))))

class nsDOMFileList;
class nsIRadioGroupContainer;
class nsIRadioGroupVisitor;
class nsIRadioVisitor;

class UploadLastDir : public nsIObserver, public nsSupportsWeakReference {
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER

  UploadLastDir();

  /**
   * Fetch the last used directory for this location from the content
   * pref service, if it is available.
   *
   * @param aURI URI of the current page
   * @param aFile path to the last used directory
   */
  nsresult FetchLastUsedDirectory(nsIURI* aURI, nsILocalFile** aFile);

  /**
   * Store the last used directory for this location using the
   * content pref service, if it is available
   * @param aURI URI of the current page
   * @param aFile file chosen by the user - the path to the parent of this
   *        file will be stored
   */
  nsresult StoreLastUsedDirectory(nsIURI* aURI, nsILocalFile* aFile);
private:
  // Directories are stored here during private browsing mode
  nsInterfaceHashtable<nsStringHashKey, nsILocalFile> mUploadLastDirStore;
  PRBool mInPrivateBrowsing;
};

class nsIRadioGroupContainer;
class nsIRadioVisitor;

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
  NS_FORWARD_NSIDOMHTMLELEMENT(nsGenericHTMLFormElement::)

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
  virtual PRBool RestoreState(nsPresState* aState);
  virtual PRBool AllowDrop();

  virtual void FieldSetDisabledChanged(nsEventStates aStates, PRBool aNotify);

  // nsIContent
  virtual PRBool IsHTMLFocusable(PRBool aWithMouse, PRBool *aIsFocusable, PRInt32 *aTabIndex);

  virtual PRBool ParseAttribute(PRInt32 aNamespaceID,
                                nsIAtom* aAttribute,
                                const nsAString& aValue,
                                nsAttrValue& aResult);
  virtual nsChangeHint GetAttributeChangeHint(const nsIAtom* aAttribute,
                                              PRInt32 aModType) const;
  NS_IMETHOD_(PRBool) IsAttributeMapped(const nsIAtom* aAttribute) const;
  virtual nsMapRuleToAttributesFunc GetAttributeMappingFunction() const;

  virtual nsresult PreHandleEvent(nsEventChainPreVisitor& aVisitor);
  virtual nsresult PostHandleEvent(nsEventChainPostVisitor& aVisitor);

  virtual nsresult BindToTree(nsIDocument* aDocument, nsIContent* aParent,
                              nsIContent* aBindingParent,
                              PRBool aCompileEventHandlers);
  virtual void UnbindFromTree(PRBool aDeep = PR_TRUE,
                              PRBool aNullParent = PR_TRUE);

  virtual void DoneCreatingElement();

  virtual nsEventStates IntrinsicState() const;

  // nsITextControlElement
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
  NS_IMETHOD_(nsIContent*) CreatePlaceholderNode();
  NS_IMETHOD_(nsIContent*) GetPlaceholderNode();
  NS_IMETHOD_(void) UpdatePlaceholderText(PRBool aNotify);
  NS_IMETHOD_(void) SetPlaceholderClass(PRBool aVisible, PRBool aNotify);
  NS_IMETHOD_(void) InitializeKeyboardEventListeners();
  NS_IMETHOD_(void) OnValueChanged(PRBool aNotify);

  // nsIFileControlElement
  void GetDisplayFileName(nsAString& aFileName) const;
  const nsCOMArray<nsIDOMFile>& GetFiles();
  void SetFiles(const nsCOMArray<nsIDOMFile>& aFiles);

  void SetCheckedChangedInternal(PRBool aCheckedChanged);
  PRBool GetCheckedChanged();
  void AddedToRadioGroup(PRBool aNotify = PR_TRUE);
  void WillRemoveFromRadioGroup(PRBool aNotify);
  /**
   * Get the radio group container for this button (form or document)
   * @return the radio group container (or null if no form or document)
   */
  virtual already_AddRefed<nsIRadioGroupContainer> GetRadioGroupContainer();

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

  virtual void UpdateEditableState()
  {
    return UpdateEditableFormControlState();
  }

  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED_NO_UNLINK(nsHTMLInputElement,
                                                     nsGenericHTMLFormElement)

  static UploadLastDir* gUploadLastDir;
  // create and destroy the static UploadLastDir object for remembering
  // which directory was last used on a site-by-site basis
  static void InitUploadLastDir();
  static void DestroyUploadLastDir();

  void MaybeLoadImage();

  virtual nsXPCClassInfo* GetClassInfo();

  static nsHTMLInputElement* FromContent(nsIContent *aContent)
  {
    if (aContent->NodeInfo()->Equals(nsGkAtoms::input, kNameSpaceID_XHTML))
      return static_cast<nsHTMLInputElement*>(aContent);
    return NULL;
  }

  // nsIConstraintValidation
  PRBool   IsTooLong();
  PRBool   IsValueMissing();
  PRBool   HasTypeMismatch();
  PRBool   HasPatternMismatch();
  void     UpdateTooLongValidityState();
  void     UpdateValueMissingValidityState();
  void     UpdateTypeMismatchValidityState();
  void     UpdatePatternMismatchValidityState();
  void     UpdateAllValidityStates(PRBool aNotify);
  void     UpdateBarredFromConstraintValidation();
  nsresult GetValidationMessage(nsAString& aValidationMessage,
                                ValidityStateType aType);

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
  static PRBool IsValidEmailAddress(const nsAString& aValue);

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
  static PRBool IsValidEmailAddressList(const nsAString& aValue);

  /**
   * This helper method returns true if the aPattern pattern matches aValue.
   * aPattern should not contain leading and trailing slashes (/).
   * The pattern has to match the entire value not just a subset.
   * aDocument must be a valid pointer (not null).
   *
   * This is following the HTML5 specification:
   * http://dev.w3.org/html5/spec/forms.html#attr-input-pattern
   *
   * @param aValue    the string to check.
   * @param aPattern  the string defining the pattern.
   * @param aDocument the owner document of the element.
   * @result          whether the given string is matches the pattern.
   */
  static PRBool IsPatternMatching(nsAString& aValue, nsAString& aPattern,
                                  nsIDocument* aDocument);

  // Helper method
  nsresult SetValueInternal(const nsAString& aValue,
                            PRBool aUserInput,
                            PRBool aSetValueChanged);

  void ClearFiles() {
    nsCOMArray<nsIDOMFile> files;
    SetFiles(files);
  }

  void SetSingleFile(nsIDOMFile* aFile) {
    nsCOMArray<nsIDOMFile> files;
    nsCOMPtr<nsIDOMFile> file = aFile;
    files.AppendObject(file);
    SetFiles(files);
  }

  nsresult SetIndeterminateInternal(PRBool aValue,
                                    PRBool aShouldInvalidate);

  nsresult GetSelectionRange(PRInt32* aSelectionStart, PRInt32* aSelectionEnd);

  /**
   * Get the name if it exists and return whether it did exist
   * @param aName the name returned [OUT]
   * @param true if the name is empty, false otherwise
   */
  PRBool GetNameIfExists(nsAString& aName) {
    GetAttr(kNameSpaceID_None, nsGkAtoms::name, aName);
    return !aName.IsEmpty();
  }

  /**
   * Called when an attribute is about to be changed
   */
  virtual nsresult BeforeSetAttr(PRInt32 aNameSpaceID, nsIAtom* aName,
                                 const nsAString* aValue, PRBool aNotify);
  /**
   * Called when an attribute has just been changed
   */
  virtual nsresult AfterSetAttr(PRInt32 aNameSpaceID, nsIAtom* aName,
                                const nsAString* aValue, PRBool aNotify);

  /**
   * Dispatch a select event. Returns true if the event was not cancelled.
   */
  PRBool DispatchSelectEvent(nsPresContext* aPresContext);

  void SelectAll(nsPresContext* aPresContext);
  PRBool IsImage() const
  {
    return AttrValueIs(kNameSpaceID_None, nsGkAtoms::type,
                       nsGkAtoms::image, eIgnoreCase);
  }

  virtual PRBool AcceptAutofocus() const
  {
    return PR_TRUE;
  }

  /**
   * Fire the onChange event
   */
  void FireOnChange();

  /**
   * Visit the group of radio buttons this radio belongs to
   * @param aVisitor the visitor to visit with
   */
  nsresult VisitGroup(nsIRadioVisitor* aVisitor, PRBool aFlushContent);

  /**
   * Do all the work that |SetChecked| does (radio button handling, etc.), but
   * take an |aNotify| parameter.
   */
  nsresult DoSetChecked(PRBool aValue, PRBool aNotify, PRBool aSetValueChanged);

  /**
   * Do all the work that |SetCheckedChanged| does (radio button handling,
   * etc.), but take an |aNotify| parameter that lets it avoid flushing content
   * when it can.
   */
  void DoSetCheckedChanged(PRBool aCheckedChanged, PRBool aNotify);

  /**
   * Actually set checked and notify the frame of the change.
   * @param aValue the value of checked to set
   */
  void SetCheckedInternal(PRBool aValue, PRBool aNotify);

  /**
   * Syntax sugar to make it easier to check for checked
   */
  PRBool GetChecked() const
  {
    return GET_BOOLBIT(mBitField, BF_CHECKED);
  }

  nsresult RadioSetChecked(PRBool aNotify);
  void SetCheckedChanged(PRBool aCheckedChanged);

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
   * Determine whether the editor needs to be initialized explicitly for
   * a particular event.
   */
  PRBool NeedToInitializeEditorForEvent(nsEventChainPreVisitor& aVisitor) const;

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
  PRBool IsMutable() const;

  /**
   * Returns if the readonly attribute applies for the current type.
   */
  PRBool DoesReadOnlyApply() const;

  /**
   * Returns if the required attribute applies for the current type.
   */
  PRBool DoesRequiredApply() const;

  /**
   * Returns if the pattern attribute applies for the current type.
   */
  PRBool DoesPatternApply() const;

  /**
   * Returns if the maxlength attribute applies for the current type.
   */
  bool MaxLengthApplies() const { return IsSingleLineTextControlInternal(PR_FALSE, mType); }

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
  bool PlaceholderApplies() const { return IsSingleLineTextControlInternal(PR_FALSE, mType); }

  /**
   * Set the current default value to the value of the input element.
   * @note You should not call this method if GetValueMode() doesn't return
   * VALUE_MODE_VALUE.
   */
  nsresult SetDefaultValueAsValue();

  nsCOMPtr<nsIControllers> mControllers;

  /**
   * The type of this input (<input type=...>) as an integer.
   * @see nsIFormControl.h (specifically NS_FORM_INPUT_*)
   */
  PRUint8                  mType;
  /**
   * A bitfield containing our booleans
   * @see GET_BOOLBIT / SET_BOOLBIT macros and BF_* field identifiers
   */
  PRInt16                  mBitField;
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
};

#endif
