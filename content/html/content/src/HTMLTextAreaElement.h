/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_HTMLTextAreaElement_h
#define mozilla_dom_HTMLTextAreaElement_h

#include "nsIDOMHTMLTextAreaElement.h"
#include "nsITextControlElement.h"
#include "nsIDOMNSEditableElement.h"
#include "nsCOMPtr.h"
#include "nsGenericHTMLElement.h"
#include "nsEventStates.h"
#include "nsStubMutationObserver.h"
#include "nsIConstraintValidation.h"
#include "nsHTMLFormElement.h"
#include "nsGkAtoms.h"

#include "nsTextEditorState.h"

class nsFormSubmission;
class nsIControllers;
class nsIDocument;
class nsPresContext;
class nsPresState;

namespace mozilla {
namespace dom {

class HTMLTextAreaElement MOZ_FINAL : public nsGenericHTMLFormElement,
                                      public nsIDOMHTMLTextAreaElement,
                                      public nsITextControlElement,
                                      public nsIDOMNSEditableElement,
                                      public nsStubMutationObserver,
                                      public nsIConstraintValidation
{
public:
  using nsIConstraintValidation::GetValidationMessage;

  HTMLTextAreaElement(already_AddRefed<nsINodeInfo> aNodeInfo,
                      FromParser aFromParser = NOT_FROM_PARSER);

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  // nsIDOMNode
  NS_FORWARD_NSIDOMNODE_TO_NSINODE

  // nsIDOMElement
  NS_FORWARD_NSIDOMELEMENT_TO_GENERIC

  // nsIDOMHTMLElement
  NS_FORWARD_NSIDOMHTMLELEMENT_TO_GENERIC

  virtual int32_t TabIndexDefault() MOZ_OVERRIDE;

  // nsIDOMHTMLTextAreaElement
  NS_DECL_NSIDOMHTMLTEXTAREAELEMENT

  // nsIDOMNSEditableElement
  NS_IMETHOD GetEditor(nsIEditor** aEditor)
  {
    return nsGenericHTMLElement::GetEditor(aEditor);
  }
  NS_IMETHOD SetUserInput(const nsAString& aInput);

  // nsIFormControl
  NS_IMETHOD_(uint32_t) GetType() const { return NS_FORM_TEXTAREA; }
  NS_IMETHOD Reset();
  NS_IMETHOD SubmitNamesValues(nsFormSubmission* aFormSubmission);
  NS_IMETHOD SaveState();
  virtual bool RestoreState(nsPresState* aState);
  virtual bool IsDisabledForEvents(uint32_t aMessage);

  virtual void FieldSetDisabledChanged(bool aNotify);

  virtual nsEventStates IntrinsicState() const;

  // nsITextControlElemet
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

  // nsIContent
  virtual nsresult BindToTree(nsIDocument* aDocument, nsIContent* aParent,
                               nsIContent* aBindingParent,
                               bool aCompileEventHandlers);
  virtual void UnbindFromTree(bool aDeep = true,
                              bool aNullParent = true);
  virtual bool ParseAttribute(int32_t aNamespaceID,
                                nsIAtom* aAttribute,
                                const nsAString& aValue,
                                nsAttrValue& aResult);
  virtual nsMapRuleToAttributesFunc GetAttributeMappingFunction() const;
  virtual nsChangeHint GetAttributeChangeHint(const nsIAtom* aAttribute,
                                              int32_t aModType) const;
  NS_IMETHOD_(bool) IsAttributeMapped(const nsIAtom* aAttribute) const;

  virtual nsresult PreHandleEvent(nsEventChainPreVisitor& aVisitor);
  virtual nsresult PostHandleEvent(nsEventChainPostVisitor& aVisitor);

  virtual bool IsHTMLFocusable(bool aWithMouse, bool *aIsFocusable, int32_t *aTabIndex);

  virtual void DoneAddingChildren(bool aHaveNotified);
  virtual bool IsDoneAddingChildren();

  virtual nsresult Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const;

  nsresult CopyInnerTo(Element* aDest);

  /**
   * Called when an attribute is about to be changed
   */
  virtual nsresult BeforeSetAttr(int32_t aNameSpaceID, nsIAtom* aName,
                                 const nsAttrValueOrString* aValue,
                                 bool aNotify);

  // nsIMutationObserver
  NS_DECL_NSIMUTATIONOBSERVER_CHARACTERDATACHANGED
  NS_DECL_NSIMUTATIONOBSERVER_CONTENTAPPENDED
  NS_DECL_NSIMUTATIONOBSERVER_CONTENTINSERTED
  NS_DECL_NSIMUTATIONOBSERVER_CONTENTREMOVED

  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(HTMLTextAreaElement,
                                           nsGenericHTMLFormElement)

  virtual nsIDOMNode* AsDOMNode() { return this; }

  // nsIConstraintValidation
  bool     IsTooLong();
  bool     IsValueMissing() const;
  void     UpdateTooLongValidityState();
  void     UpdateValueMissingValidityState();
  void     UpdateBarredFromConstraintValidation();
  nsresult GetValidationMessage(nsAString& aValidationMessage,
                                ValidityStateType aType);

  // Web IDL binding methods
  bool Autofocus()
  {
    return GetBoolAttr(nsGkAtoms::autofocus);
  }
  void SetAutofocus(bool aAutoFocus, ErrorResult& aError)
  {
    SetHTMLBoolAttr(nsGkAtoms::autofocus, aAutoFocus, aError);
  }
  uint32_t Cols()
  {
    return GetIntAttr(nsGkAtoms::cols, DEFAULT_COLS);
  }
  void SetCols(uint32_t aCols, ErrorResult& aError)
  {
    if (aCols == 0) {
      aError.Throw(NS_ERROR_DOM_INDEX_SIZE_ERR);
    } else {
      SetHTMLUnsignedIntAttr(nsGkAtoms::cols, aCols, aError);
    }
  }
  bool Disabled()
  {
    return GetBoolAttr(nsGkAtoms::disabled);
  }
  void SetDisabled(bool aDisabled, ErrorResult& aError)
  {
    SetHTMLBoolAttr(nsGkAtoms::disabled, aDisabled, aError);
  }
  // nsGenericHTMLFormElement::GetForm is fine
  using nsGenericHTMLFormElement::GetForm;
  int32_t MaxLength()
  {
    return GetIntAttr(nsGkAtoms::maxlength, -1);
  }
  void SetMaxLength(int32_t aMaxLength, ErrorResult& aError)
  {
    if (aMaxLength < 0) {
      aError.Throw(NS_ERROR_DOM_INDEX_SIZE_ERR);
    } else {
      SetHTMLIntAttr(nsGkAtoms::maxlength, aMaxLength, aError);
    }
  }
  // XPCOM GetName is fine
  void SetName(const nsAString& aName, ErrorResult& aError)
  {
    SetHTMLAttr(nsGkAtoms::name, aName, aError);
  }
  // XPCOM GetPlaceholder is fine
  void SetPlaceholder(const nsAString& aPlaceholder, ErrorResult& aError)
  {
    SetHTMLAttr(nsGkAtoms::placeholder, aPlaceholder, aError);
  }
  bool ReadOnly()
  {
    return GetBoolAttr(nsGkAtoms::readonly);
  }
  void SetReadOnly(bool aReadOnly, ErrorResult& aError)
  {
    SetHTMLBoolAttr(nsGkAtoms::readonly, aReadOnly, aError);
  }
  bool Required()
  {
    return GetBoolAttr(nsGkAtoms::required);
  }
  void SetRequired(bool aRequired, ErrorResult& aError)
  {
    SetHTMLBoolAttr(nsGkAtoms::required, aRequired, aError);
  }
  uint32_t Rows()
  {
    return GetIntAttr(nsGkAtoms::rows, DEFAULT_ROWS_TEXTAREA);
  }
  void SetRows(uint32_t aRows, ErrorResult& aError)
  {
    if (aRows == 0) {
      aError.Throw(NS_ERROR_DOM_INDEX_SIZE_ERR);
    } else {
      SetHTMLUnsignedIntAttr(nsGkAtoms::rows, aRows, aError);
    }
  }
  // XPCOM GetWrap is fine
  void SetWrap(const nsAString& aWrap, ErrorResult& aError)
  {
    SetHTMLAttr(nsGkAtoms::wrap, aWrap, aError);
  }
  // XPCOM GetType is fine
  // XPCOM GetDefaultValue is fine
  void SetDefaultValue(const nsAString& aDefaultValue, ErrorResult& aError);
  // XPCOM GetValue/SetValue are fine
  uint32_t GetTextLength();
  // nsIConstraintValidation::WillValidate is fine.
  // nsIConstraintValidation::Validity() is fine.
  // nsIConstraintValidation::GetValidationMessage() is fine.
  // nsIConstraintValidation::CheckValidity() is fine.
  using nsIConstraintValidation::CheckValidity;
  // nsIConstraintValidation::SetCustomValidity() is fine.
  // XPCOM Select is fine
  uint32_t GetSelectionStart(ErrorResult& aError);
  void SetSelectionStart(uint32_t aSelectionStart, ErrorResult& aError);
  uint32_t GetSelectionEnd(ErrorResult& aError);
  void SetSelectionEnd(uint32_t aSelectionEnd, ErrorResult& aError);
  void GetSelectionDirection(nsAString& aDirection, ErrorResult& aError);
  void SetSelectionDirection(const nsAString& aDirection, ErrorResult& aError);
  void SetSelectionRange(uint32_t aSelectionStart, uint32_t aSelectionEnd, const Optional<nsAString>& aDirecton, ErrorResult& aError);
  nsIControllers* GetControllers(ErrorResult& aError);
  nsIEditor* GetEditor()
  {
    return mState.GetEditor();
  }

protected:
  using nsGenericHTMLFormElement::IsSingleLineTextControl; // get rid of the compiler warning

  virtual JSObject* WrapNode(JSContext *aCx, JSObject *aScope) MOZ_OVERRIDE;

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
  
  void FireChangeEventIfNeeded();
  
  nsString mFocusedValue;

  /** The state of the text editor (selection controller and the editor) **/
  nsTextEditorState mState;

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
  nsresult GetSelectionRange(int32_t* aSelectionStart, int32_t* aSelectionEnd);

  /**
   * Common method to call from the various mutation observer methods.
   * aContent is a content node that's either the one that changed or its
   * parent; we should only respond to the change if aContent is non-anonymous.
   */
  void ContentChanged(nsIContent* aContent);

  virtual nsresult AfterSetAttr(int32_t aNamespaceID, nsIAtom *aName,
                                const nsAttrValue* aValue, bool aNotify);

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
};

} // namespace dom
} // namespace mozilla

#endif

