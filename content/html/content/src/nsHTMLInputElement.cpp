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
#include "nsCOMPtr.h"
#include "nsIDOMHTMLInputElement.h"
#include "nsIDOMNSHTMLInputElement.h"
#include "nsITextControlElement.h"
#include "nsIRadioControlElement.h"
#include "nsIRadioVisitor.h"
#include "nsIPhonetic.h"

#include "nsIControllers.h"
#include "nsIFocusController.h"
#include "nsPIDOMWindow.h"
#include "nsIScriptGlobalObject.h"
#include "nsContentCID.h"
#include "nsIComponentManager.h"
#include "nsIDOMHTMLFormElement.h"
#include "nsIDOMEventReceiver.h"
#include "nsIHTMLContent.h"
#include "nsGenericHTMLElement.h"
#include "nsHTMLAtoms.h"
#include "nsStyleConsts.h"
#include "nsIPresContext.h"
#include "nsMappedAttributes.h"
#include "nsIFormControl.h"
#include "nsIForm.h"
#include "nsIFormSubmission.h"
#include "nsITextControlFrame.h"
#include "nsIRadioControlFrame.h"
#include "nsIDocument.h"
#include "nsIPresShell.h"
#include "nsIFormControlFrame.h"
#include "nsIFrame.h"
#include "nsIEventStateManager.h"
#include "nsIServiceManager.h"
#include "nsIScriptSecurityManager.h"
#include "nsDOMError.h"
#include "nsIPrivateDOMEvent.h"
#include "nsIEditor.h"
#include "nsGUIEvent.h"

#include "nsIPresState.h"
#include "nsLayoutErrors.h"
#include "nsIDOMEvent.h"
#include "nsIDOMNSEvent.h"
#include "nsIDOMNodeList.h"
#include "nsIDOMHTMLCollection.h"
#include "nsICheckboxControlFrame.h"
#include "nsIImageControlFrame.h"
#include "nsLinebreakConverter.h" //to strip out carriage returns
#include "nsReadableUtils.h"
#include "nsUnicharUtils.h"

#include "nsIDOMMutationEvent.h"
#include "nsIDOMEventReceiver.h"
#include "nsMutationEvent.h"

#include "nsRuleData.h"

// input type=radio
#include "nsIRadioControlFrame.h"
#include "nsIRadioGroupContainer.h"

// input type=file
#include "nsIMIMEService.h"
#include "nsCExternalHandlerService.h"
#include "nsIFile.h"
#include "nsILocalFile.h"
#include "nsIFileStreams.h"
#include "nsNetUtil.h"

// input type=image
#include "nsImageLoadingContent.h"
#include "nsIDOMWindowInternal.h"

// XXX align=left, hspace, vspace, border? other nav4 attrs

static NS_DEFINE_CID(kXULControllersCID,  NS_XULCONTROLLERS_CID);
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

#define GET_BOOLBIT(bitfield, field) (((bitfield) & (0x01 << (field))) \
                                        ? PR_TRUE : PR_FALSE)
#define SET_BOOLBIT(bitfield, field, b) ((b) \
                                        ? ((bitfield) |=  (0x01 << (field))) \
                                        : ((bitfield) &= ~(0x01 << (field))))

class nsHTMLInputElement : public nsGenericHTMLFormElement,
                           public nsImageLoadingContent,
                           public nsIDOMHTMLInputElement,
                           public nsIDOMNSHTMLInputElement,
                           public nsITextControlElement,
                           public nsIRadioControlElement,
                           public nsIPhonetic
{
public:
  nsHTMLInputElement(nsINodeInfo *aNodeInfo, PRBool aFromParser);
  virtual ~nsHTMLInputElement();

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  // nsIDOMNode
  NS_FORWARD_NSIDOMNODE_NO_CLONENODE(nsGenericHTMLFormElement::)

  // nsIDOMElement
  NS_FORWARD_NSIDOMELEMENT(nsGenericHTMLFormElement::)

  // nsIDOMHTMLElement
  NS_FORWARD_NSIDOMHTMLELEMENT(nsGenericHTMLFormElement::)

  // nsIDOMHTMLInputElement
  NS_DECL_NSIDOMHTMLINPUTELEMENT

  // nsIDOMNSHTMLInputElement
  NS_DECL_NSIDOMNSHTMLINPUTELEMENT

  // nsIPhonetic
  NS_DECL_NSIPHONETIC

  // Overriden nsIFormControl methods
  NS_IMETHOD_(PRInt32) GetType() { return mType; }
  NS_IMETHOD Reset();
  NS_IMETHOD SubmitNamesValues(nsIFormSubmission* aFormSubmission,
                               nsIContent* aSubmitElement);
  NS_IMETHOD SaveState();
  NS_IMETHOD RestoreState(nsIPresState* aState);
  virtual PRBool AllowDrop();

  // nsIContent
  virtual void SetFocus(nsIPresContext* aPresContext);
  virtual PRBool IsFocusable(PRInt32 *aTabIndex = nsnull);

  virtual PRBool ParseAttribute(nsIAtom* aAttribute,
                                const nsAString& aValue,
                                nsAttrValue& aResult);
  NS_IMETHOD AttributeToString(nsIAtom* aAttribute,
                               const nsHTMLValue& aValue,
                               nsAString& aResult) const;
  NS_IMETHOD GetAttributeChangeHint(const nsIAtom* aAttribute,
                                    PRInt32 aModType,
                                    nsChangeHint& aHint) const;
  NS_IMETHOD_(PRBool) IsAttributeMapped(const nsIAtom* aAttribute) const;
  NS_IMETHOD GetAttributeMappingFunction(nsMapRuleToAttributesFunc& aMapRuleFunc) const;
  virtual nsresult HandleDOMEvent(nsIPresContext* aPresContext,
                                  nsEvent* aEvent, nsIDOMEvent** aDOMEvent,
                                  PRUint32 aFlags,
                                  nsEventStatus* aEventStatus);
  virtual void SetDocument(nsIDocument* aDocument, PRBool aDeep,
                           PRBool aCompileEventHandlers);
  virtual void SetParent(nsIContent* aParent);

  nsresult SetAttr(PRInt32 aNameSpaceID, nsIAtom* aName,
                   const nsAString& aValue, PRBool aNotify)
  {
    return SetAttr(aNameSpaceID, aName, nsnull, aValue, aNotify);
  }
  virtual nsresult SetAttr(PRInt32 aNameSpaceID, nsIAtom* aName,
                           nsIAtom* aPrefix, const nsAString& aValue,
                           PRBool aNotify)
  {
    BeforeSetAttr(aNameSpaceID, aName, &aValue, aNotify);

    nsresult rv = nsGenericHTMLFormElement::SetAttr(aNameSpaceID, aName,
                                                    aPrefix, aValue, aNotify);

    AfterSetAttr(aNameSpaceID, aName, &aValue, aNotify);
    return rv;
  }

  virtual nsresult UnsetAttr(PRInt32 aNameSpaceID, nsIAtom* aAttribute,
                             PRBool aNotify)
  {
    BeforeSetAttr(aNameSpaceID, aAttribute, nsnull, aNotify);

    nsresult rv = nsGenericHTMLFormElement::UnsetAttr(aNameSpaceID, aAttribute,
                                                      aNotify);

    AfterSetAttr(aNameSpaceID, aAttribute, nsnull, aNotify);
    return rv;
  }

  virtual void DoneCreatingElement();

  // nsITextControlElement
  NS_IMETHOD TakeTextFrameValue(const nsAString& aValue);
  NS_IMETHOD SetValueChanged(PRBool aValueChanged);

  // nsIRadioControlElement
  NS_IMETHOD RadioSetChecked(PRBool aNotify);
  NS_IMETHOD SetCheckedChanged(PRBool aCheckedChanged);
  NS_IMETHOD SetCheckedChangedInternal(PRBool aCheckedChanged);
  NS_IMETHOD GetCheckedChanged(PRBool* aCheckedChanged);
  NS_IMETHOD AddedToRadioGroup(PRBool aNotify = PR_TRUE);
  NS_IMETHOD WillRemoveFromRadioGroup();

protected:
  // Helper method
  nsresult SetValueInternal(const nsAString& aValue,
                            nsITextControlFrame* aFrame);

  nsresult GetSelectionRange(PRInt32* aSelectionStart, PRInt32* aSelectionEnd);
  //Helper method
#ifdef ACCESSIBILITY
  nsresult FireEventForAccessibility(nsIPresContext* aPresContext,
                                     const nsAString& aEventType);
#endif

  /**
   * Get the name if it exists and return whether it did exist
   * @param aName the name returned [OUT]
   * @param true if the name existed, false if not
   */
  PRBool GetNameIfExists(nsAString& aName) {
    return GetAttr(kNameSpaceID_None, nsHTMLAtoms::name, aName) !=
           NS_CONTENT_ATTR_NOT_THERE;
  }

  /**
   * Called when an attribute is about to be changed
   */
  void BeforeSetAttr(PRInt32 aNameSpaceID, nsIAtom* aName,
                     const nsAString* aValue, PRBool aNotify);
  /**
   * Called when an attribute has just been changed
   */
  void AfterSetAttr(PRInt32 aNameSpaceID, nsIAtom* aName,
                    const nsAString* aValue, PRBool aNotify);

  void SelectAll(nsIPresContext* aPresContext);
  PRBool IsImage() const
  {
    nsAutoString tmp;

    GetAttr(kNameSpaceID_None, nsHTMLAtoms::type, tmp);

    return tmp.LowerCaseEqualsLiteral("image");
  }

  /**
   * Fire the onChange event
   */
  void FireOnChange();

  /**
   * Visit a the group of radio buttons this radio belongs to
   * @param aVisitor the visitor to visit with
   */
  nsresult VisitGroup(nsIRadioVisitor* aVisitor);

  /**
   * Do all the work that |SetChecked| does (radio button handling, etc.), but
   * take an |aNotify| parameter.
   */
  nsresult DoSetChecked(PRBool aValue, PRBool aNotify = PR_TRUE);

  /**
   * Actually set checked and notify the frame of the change.
   * @param aValue the value of checked to set
   */
  nsresult SetCheckedInternal(PRBool aValue, PRBool aNotify);

  /**
   * Get the radio group container for this button (form or document)
   * @return the radio group container (or null if no form or document)
   */
  already_AddRefed<nsIRadioGroupContainer> GetRadioGroupContainer();

  /**
   * MaybeSubmitForm looks for a submit input or a single text control
   * and submits the form if either is present.
   */
  nsresult MaybeSubmitForm(nsIPresContext* aPresContext);
  
  nsCOMPtr<nsIControllers> mControllers;

  /**
   * The type of this input (<input type=...>) as an integer.
   * @see nsIFormControl.h (specifically NS_FORM_INPUT_*)
   */
  PRInt8                   mType;
  /**
   * A bitfield containing our booleans
   * @see GET_BOOLBIT / SET_BOOLBIT macros and BF_* field identifiers
   */
  PRInt16                  mBitField;
  /**
   * The current value of the input if it has been changed from the deafault
   */
  char*                    mValue;
};

//
// construction, destruction
//

NS_IMPL_NS_NEW_HTML_ELEMENT_CHECK_PARSER(Input)

nsHTMLInputElement::nsHTMLInputElement(nsINodeInfo *aNodeInfo,
                                       PRBool aFromParser)
  : nsGenericHTMLFormElement(aNodeInfo),
    mType(NS_FORM_INPUT_TEXT), // default value
    mBitField(0),
    mValue(nsnull)
{
  SET_BOOLBIT(mBitField, BF_PARSER_CREATING, aFromParser);
}

nsHTMLInputElement::~nsHTMLInputElement()
{
  if (mValue) {
    nsMemory::Free(mValue);
  }
}


// nsISupports

NS_IMPL_ADDREF_INHERITED(nsHTMLInputElement, nsGenericElement) 
NS_IMPL_RELEASE_INHERITED(nsHTMLInputElement, nsGenericElement) 


// QueryInterface implementation for nsHTMLInputElement
NS_HTML_CONTENT_INTERFACE_MAP_BEGIN(nsHTMLInputElement,
                                    nsGenericHTMLFormElement)
  NS_INTERFACE_MAP_ENTRY(nsIDOMHTMLInputElement)
  NS_INTERFACE_MAP_ENTRY(nsIDOMNSHTMLInputElement)
  NS_INTERFACE_MAP_ENTRY(nsITextControlElement)
  NS_INTERFACE_MAP_ENTRY(nsIRadioControlElement)
  NS_INTERFACE_MAP_ENTRY(nsIPhonetic)
  NS_INTERFACE_MAP_ENTRY(imgIDecoderObserver)
  NS_INTERFACE_MAP_ENTRY(nsIImageLoadingContent)
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO(HTMLInputElement)
NS_HTML_CONTENT_INTERFACE_MAP_END


// nsIDOMNode

nsresult
nsHTMLInputElement::CloneNode(PRBool aDeep, nsIDOMNode** aReturn)
{
  *aReturn = nsnull;

  nsHTMLInputElement* it = new nsHTMLInputElement(mNodeInfo, PR_FALSE);
  if (!it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  nsCOMPtr<nsIDOMNode> kungFuDeathGrip(it);

  CopyInnerTo(it, aDeep);

  switch (mType) {
    case NS_FORM_INPUT_TEXT:
    case NS_FORM_INPUT_PASSWORD:
    case NS_FORM_INPUT_FILE:
      if (GET_BOOLBIT(mBitField, BF_VALUE_CHANGED)) {
        // We don't have our default value anymore.  Set our value on
        // the clone.
        nsAutoString value;
        GetValue(value);
        // SetValueInternal handles setting the VALUE_CHANGED bit for us
        it->SetValueInternal(value, nsnull);
      }
      break;
    case NS_FORM_INPUT_RADIO:
    case NS_FORM_INPUT_CHECKBOX:
      if (GET_BOOLBIT(mBitField, BF_CHECKED_CHANGED)) {
        // We no longer have our original checked state.  Set our
        // checked state on the clone.
        PRBool checked;
        GetChecked(&checked);
        it->DoSetChecked(checked, PR_FALSE);
      }
      break;
    default:
      break;
  }
          
  kungFuDeathGrip.swap(*aReturn);

  return NS_OK;
}


void
nsHTMLInputElement::BeforeSetAttr(PRInt32 aNameSpaceID, nsIAtom* aName,
                                  const nsAString* aValue,
                                  PRBool aNotify)
{
  if (aNameSpaceID != kNameSpaceID_None) {
    return;
  }

  //
  // When name or type changes, radio should be removed from radio group.
  // (type changes are handled in the form itself currently)
  // If the parser is not done creating the radio, we also should not do it.
  //
  if ((aName == nsHTMLAtoms::name || (aName == nsHTMLAtoms::type && !mForm)) &&
      mType == NS_FORM_INPUT_RADIO &&
      (mForm || !(GET_BOOLBIT(mBitField, BF_PARSER_CREATING)))) {
    WillRemoveFromRadioGroup();
  } else if (aNotify && aName == nsHTMLAtoms::src &&
             aValue && mType == NS_FORM_INPUT_IMAGE) {
    // Null value means the attr got unset; don't trigger on that
    ImageURIChanged(*aValue);
  } else if (aNotify && aName == nsHTMLAtoms::disabled) {
    SET_BOOLBIT(mBitField, BF_DISABLED_CHANGED, PR_TRUE);
  }
}

void
nsHTMLInputElement::AfterSetAttr(PRInt32 aNameSpaceID, nsIAtom* aName,
                                 const nsAString* aValue,
                                 PRBool aNotify)
{
  if (aNameSpaceID != kNameSpaceID_None) {
    return;
  }

  //
  // When name or type changes, radio should be added to radio group.
  // (type changes are handled in the form itself currently)
  // If the parser is not done creating the radio, we also should not do it.
  //
  if ((aName == nsHTMLAtoms::name || (aName == nsHTMLAtoms::type && !mForm)) &&
      mType == NS_FORM_INPUT_RADIO &&
      (mForm || !(GET_BOOLBIT(mBitField, BF_PARSER_CREATING)))) {
    AddedToRadioGroup();
  }

  //
  // Some elements have to change their value when the value and checked
  // attributes change (but they only do so when ValueChanged() and
  // CheckedChanged() are false--i.e. the value has not been changed by the
  // user or by JS)
  //
  // We only really need to call reset for the value so that the text control
  // knows the new value.  No other reason.
  //
  if (aName == nsHTMLAtoms::value &&
      !GET_BOOLBIT(mBitField, BF_VALUE_CHANGED) &&
      (mType == NS_FORM_INPUT_TEXT ||
       mType == NS_FORM_INPUT_PASSWORD ||
       mType == NS_FORM_INPUT_FILE)) {
    Reset();
  }
  //
  // Checked must be set no matter what type of control it is, since
  // GetChecked() must reflect the new value
  //
  if (aName == nsHTMLAtoms::checked &&
      !GET_BOOLBIT(mBitField, BF_CHECKED_CHANGED)) {
    // Delay setting checked if the parser is creating this element (wait until
    // everything is set)
    if (GET_BOOLBIT(mBitField, BF_PARSER_CREATING)) {
      SET_BOOLBIT(mBitField, BF_SHOULD_INIT_CHECKED, PR_TRUE);
    } else {
      PRBool defaultChecked;
      GetDefaultChecked(&defaultChecked);
      DoSetChecked(defaultChecked);
      SetCheckedChanged(PR_FALSE);
    }
  }

  if (aName == nsHTMLAtoms::type) {
    // If we are changing type from File/Text/Passwd to other input types
    // we need save the mValue into value attribute
    if (mValue &&
        mType != NS_FORM_INPUT_TEXT &&
        mType != NS_FORM_INPUT_PASSWORD &&
        mType != NS_FORM_INPUT_FILE) {
      SetAttr(kNameSpaceID_None, nsHTMLAtoms::value,
              NS_ConvertUTF8toUCS2(mValue), PR_FALSE);
      if (mValue) {
        nsMemory::Free(mValue);
        mValue = nsnull;
      }
    }

    if (aNotify && mType == NS_FORM_INPUT_IMAGE && !mCurrentRequest) {
      // We just got switched to be an image input; we should see
      // whether we have an image to load;
      nsAutoString src;
      nsresult rv = GetAttr(kNameSpaceID_None, nsHTMLAtoms::src, src);
      if (rv == NS_CONTENT_ATTR_HAS_VALUE) {
        ImageURIChanged(src);
      }
    }
  }
}

// nsIDOMHTMLInputElement

NS_IMETHODIMP
nsHTMLInputElement::GetForm(nsIDOMHTMLFormElement** aForm)
{
  return nsGenericHTMLFormElement::GetForm(aForm);
}

NS_IMPL_STRING_ATTR(nsHTMLInputElement, DefaultValue, value)
NS_IMPL_BOOL_ATTR(nsHTMLInputElement, DefaultChecked, checked)
NS_IMPL_STRING_ATTR(nsHTMLInputElement, Accept, accept)
NS_IMPL_STRING_ATTR(nsHTMLInputElement, AccessKey, accesskey)
NS_IMPL_STRING_ATTR(nsHTMLInputElement, Align, align)
NS_IMPL_STRING_ATTR(nsHTMLInputElement, Alt, alt)
//NS_IMPL_BOOL_ATTR(nsHTMLInputElement, Checked, checked)
NS_IMPL_BOOL_ATTR(nsHTMLInputElement, Disabled, disabled)
NS_IMPL_INT_ATTR(nsHTMLInputElement, MaxLength, maxlength)
NS_IMPL_STRING_ATTR(nsHTMLInputElement, Name, name)
NS_IMPL_BOOL_ATTR(nsHTMLInputElement, ReadOnly, readonly)
NS_IMPL_STRING_ATTR(nsHTMLInputElement, Src, src)
NS_IMPL_INT_ATTR_DEFAULT_VALUE(nsHTMLInputElement, TabIndex, tabindex, 0)
NS_IMPL_STRING_ATTR(nsHTMLInputElement, UseMap, usemap)
//NS_IMPL_STRING_ATTR(nsHTMLInputElement, Value, value)
//NS_IMPL_INT_ATTR_DEFAULT_VALUE(nsHTMLInputElement, Size, size, 0)
//NS_IMPL_STRING_ATTR_DEFAULT_VALUE(nsHTMLInputElement, Type, type, "text")

NS_IMETHODIMP
nsHTMLInputElement::GetSize(PRUint32* aValue)
{
  *aValue = 0;

  nsHTMLValue value;
  if (NS_CONTENT_ATTR_HAS_VALUE ==
      GetHTMLAttribute(nsHTMLAtoms::size, value) &&
      value.GetUnit() == eHTMLUnit_Integer) {
    *aValue = value.GetIntValue();
  }

  return NS_OK;
}

NS_IMETHODIMP
nsHTMLInputElement::SetSize(PRUint32 aValue)
{
  nsAutoString val;
  val.AppendInt(aValue);

  return SetAttr(kNameSpaceID_None, nsHTMLAtoms::size, val, PR_TRUE);
}

NS_IMETHODIMP 
nsHTMLInputElement::GetValue(nsAString& aValue)
{
  if (mType == NS_FORM_INPUT_TEXT || mType == NS_FORM_INPUT_PASSWORD ||
      mType == NS_FORM_INPUT_FILE) {
    // No need to flush here, if there's no frame created for this
    // input yet, there won't be a value in it (that we don't already
    // have) even if we force it to be created
    nsIFormControlFrame* formControlFrame = GetFormControlFrame(PR_FALSE);

    PRBool frameOwnsValue = PR_FALSE;
    if (formControlFrame) {
      nsITextControlFrame* textControlFrame = nsnull;
      CallQueryInterface(formControlFrame, &textControlFrame);

      if (textControlFrame) {
        textControlFrame->OwnsValue(&frameOwnsValue);
      } else {
        // We assume if it's not a text control frame that it owns the value
        frameOwnsValue = PR_TRUE;
      }
    }

    if (frameOwnsValue) {
      formControlFrame->GetProperty(nsHTMLAtoms::value, aValue);
    } else {
      if (!GET_BOOLBIT(mBitField, BF_VALUE_CHANGED) || !mValue) {
        GetDefaultValue(aValue);
      } else {
        CopyUTF8toUTF16(mValue, aValue);
      }
    }

    return NS_OK;
  }

  // Treat value == defaultValue for other input elements
  nsresult rv = GetAttr(kNameSpaceID_None, nsHTMLAtoms::value, aValue);

  if (rv == NS_CONTENT_ATTR_NOT_THERE &&
      (mType == NS_FORM_INPUT_RADIO || mType == NS_FORM_INPUT_CHECKBOX)) {
    // The default value of a radio or checkbox input is "on".
    aValue.AssignLiteral("on");

    return NS_OK;
  }

  return rv;
}

NS_IMETHODIMP 
nsHTMLInputElement::SetValue(const nsAString& aValue)
{
  //check secuity
  if (mType == NS_FORM_INPUT_FILE) {
    nsIScriptSecurityManager *securityManager =
      nsContentUtils::GetSecurityManager();

    PRBool enabled;
    nsresult rv =
      securityManager->IsCapabilityEnabled("UniversalFileRead", &enabled);
    NS_ENSURE_SUCCESS(rv, rv);

    if (!enabled) {
      // setting the value of a "FILE" input widget requires the
      // UniversalFileRead privilege
      return NS_ERROR_DOM_SECURITY_ERR;
    }
  }
  SetValueInternal(aValue, nsnull);
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLInputElement::TakeTextFrameValue(const nsAString& aValue)
{
  if (mValue) {
    nsMemory::Free(mValue);
  }
  mValue = ToNewUTF8String(aValue);
  SetValueChanged(PR_TRUE);
  return NS_OK;
}

nsresult
nsHTMLInputElement::SetValueInternal(const nsAString& aValue,
                                     nsITextControlFrame* aFrame)
{
  if (mType == NS_FORM_INPUT_TEXT || mType == NS_FORM_INPUT_PASSWORD ||
      mType == NS_FORM_INPUT_FILE) {

    nsITextControlFrame* textControlFrame = aFrame;
    nsIFormControlFrame* formControlFrame = textControlFrame;
    if (!textControlFrame) {
      // No need to flush here, if there's no frame at this point we
      // don't need to force creation of one just to tell it about this
      // new value.
      formControlFrame = GetFormControlFrame(PR_FALSE);

      if (formControlFrame) {
        CallQueryInterface(formControlFrame, &textControlFrame);
      }
    }

    // File frames always own the value (if the frame is there).
    // Text frames have a bit that says whether they own the value.
    PRBool frameOwnsValue = PR_FALSE;
    if (mType == NS_FORM_INPUT_FILE && formControlFrame) {
      frameOwnsValue = PR_TRUE;
    }
    if (textControlFrame) {
      textControlFrame->OwnsValue(&frameOwnsValue);
    }
    // If the frame owns the value, set the value in the frame
    if (frameOwnsValue) {
      nsCOMPtr<nsIPresContext> presContext;
      GetPresContext(this, getter_AddRefs(presContext));
      formControlFrame->SetProperty(presContext, nsHTMLAtoms::value, aValue);
      return NS_OK;
    }

    // If the frame does not own the value, set mValue
    if (mValue) {
      nsMemory::Free(mValue);
    }

    mValue = ToNewUTF8String(aValue);

    SetValueChanged(PR_TRUE);
    return mValue ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
  }

  // If the value of a hidden input was changed, we mark it changed so that we
  // will know we need to save / restore the value.  Yes, we are overloading
  // the meaning of ValueChanged just a teensy bit to save a measly byte of
  // storage space in nsHTMLInputElement.  Yes, you are free to make a new flag,
  // NEED_TO_SAVE_VALUE, at such time as mBitField becomes a 16-bit value.
  if (mType == NS_FORM_INPUT_HIDDEN) {
    SetValueChanged(PR_TRUE);
  }

  // Treat value == defaultValue for other input elements.
  return nsGenericHTMLFormElement::SetAttr(kNameSpaceID_None,
                                           nsHTMLAtoms::value, aValue,
                                           PR_TRUE);
}

NS_IMETHODIMP
nsHTMLInputElement::SetValueChanged(PRBool aValueChanged)
{
  SET_BOOLBIT(mBitField, BF_VALUE_CHANGED, aValueChanged);
  if (!aValueChanged) {
    if (mValue) {
      nsMemory::Free(mValue);
      mValue = nsnull;
    }
  }
  return NS_OK;
}

NS_IMETHODIMP 
nsHTMLInputElement::GetChecked(PRBool* aChecked)
{
  *aChecked = GET_BOOLBIT(mBitField, BF_CHECKED);
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLInputElement::SetCheckedChanged(PRBool aCheckedChanged)
{
  if (mType == NS_FORM_INPUT_RADIO) {
    if (GET_BOOLBIT(mBitField, BF_CHECKED_CHANGED) != aCheckedChanged) {
      nsCOMPtr<nsIRadioVisitor> visitor;
      NS_GetRadioSetCheckedChangedVisitor(aCheckedChanged,
                                          getter_AddRefs(visitor));
      VisitGroup(visitor);
    }
  } else {
    SetCheckedChangedInternal(aCheckedChanged);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLInputElement::SetCheckedChangedInternal(PRBool aCheckedChanged)
{
  SET_BOOLBIT(mBitField, BF_CHECKED_CHANGED, aCheckedChanged);
  return NS_OK;
}


NS_IMETHODIMP
nsHTMLInputElement::GetCheckedChanged(PRBool* aCheckedChanged)
{
  *aCheckedChanged = GET_BOOLBIT(mBitField, BF_CHECKED_CHANGED);
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLInputElement::SetChecked(PRBool aChecked)
{
  return DoSetChecked(aChecked);
}

nsresult
nsHTMLInputElement::DoSetChecked(PRBool aChecked, PRBool aNotify)
{
  nsresult rv = NS_OK;

  //
  // If the user or JS attempts to set checked, whether it actually changes the
  // value or not, we say the value was changed so that defaultValue don't
  // affect it no more.
  //
  SetCheckedChanged(PR_TRUE);

  //
  // Don't do anything if we're not changing whether it's checked (it would
  // screw up state actually, especially when you are setting radio button to
  // false)
  //
  PRBool checked = PR_FALSE;
  GetChecked(&checked);
  if (checked == aChecked) {
    return NS_OK;
  }

  //
  // Set checked
  //
  if (mType == NS_FORM_INPUT_RADIO) {
    //
    // For radio button, we need to do some extra fun stuff
    //
    if (aChecked) {
      rv = RadioSetChecked(aNotify);
    } else {
      rv = SetCheckedInternal(PR_FALSE, aNotify);
      nsCOMPtr<nsIRadioGroupContainer> container = GetRadioGroupContainer();
      if (container) {
        nsAutoString name;
        if (GetNameIfExists(name)) {
          container->SetCurrentRadioButton(name, nsnull);
        }
      }
    }
  } else {
    rv = SetCheckedInternal(aChecked, aNotify);
  }

  return rv;
}

NS_IMETHODIMP
nsHTMLInputElement::RadioSetChecked(PRBool aNotify)
{
  nsresult rv = NS_OK;

  //
  // Find the selected radio button so we can deselect it
  //
  nsCOMPtr<nsIDOMHTMLInputElement> currentlySelected;
  nsCOMPtr<nsIRadioGroupContainer> container = GetRadioGroupContainer();
  // This is ONLY INITIALIZED IF container EXISTS
  nsAutoString name;
  PRBool nameExists = PR_FALSE;
  if (container) {
    nameExists = GetNameIfExists(name);
    if (nameExists) {
      container->GetCurrentRadioButton(name, getter_AddRefs(currentlySelected));
    }
  }

  //
  // Deselect the currently selected radio button
  //
  if (currentlySelected) {
    // Pass PR_TRUE for the aNotify parameter since the currently selected
    // button is already in the document.
    rv = NS_STATIC_CAST(nsHTMLInputElement*,
                        NS_STATIC_CAST(nsIDOMHTMLInputElement*, currentlySelected)
         )->SetCheckedInternal(PR_FALSE, PR_TRUE);
  }

  //
  // Actually select this one
  //
  if (NS_SUCCEEDED(rv)) {
    rv = SetCheckedInternal(PR_TRUE, aNotify);
  }

  //
  // Let the group know that we are now the One True Radio Button
  //
  NS_ENSURE_SUCCESS(rv, rv);
  if (container && nameExists) {
    rv = container->SetCurrentRadioButton(name, this);
  }

  return rv;
}

already_AddRefed<nsIRadioGroupContainer>
nsHTMLInputElement::GetRadioGroupContainer()
{
  nsIRadioGroupContainer* retval = nsnull;
  if (mForm) {
    CallQueryInterface(mForm, &retval);
  } else if (mDocument && GetParent()) {
    CallQueryInterface(mDocument, &retval);
  }
  return retval;
}

nsresult
nsHTMLInputElement::MaybeSubmitForm(nsIPresContext* aPresContext)
{
  if (!mForm) {
    // Nothing to do here.
    return NS_OK;
  }
  
  // Find the first submit control in elements[]
  // and also check how many text controls we have in the form
  nsCOMPtr<nsIContent> submitControl;
  PRInt32 numTextControlsFound = 0;

  nsCOMPtr<nsISimpleEnumerator> formControls;
  mForm->GetControlEnumerator(getter_AddRefs(formControls));

  nsCOMPtr<nsISupports> currentControlSupports;
  nsCOMPtr<nsIFormControl> currentControl;
  PRBool hasMoreElements;
  nsresult rv;
  while (NS_SUCCEEDED(rv = formControls->HasMoreElements(&hasMoreElements)) &&
         hasMoreElements) {
    rv = formControls->GetNext(getter_AddRefs(currentControlSupports));
    NS_ENSURE_SUCCESS(rv, rv);

    currentControl = do_QueryInterface(currentControlSupports);
    if (currentControl) {
      PRInt32 type = currentControl->GetType();
      if (!submitControl &&
          (type == NS_FORM_INPUT_SUBMIT ||
           type == NS_FORM_BUTTON_SUBMIT ||
           type == NS_FORM_INPUT_IMAGE)) {
        submitControl = do_QueryInterface(currentControl);
        // We know as soon as we find a submit control that it no
        // longer matters how many text controls there are--we are
        // going to fire the onClick handler.
        break;
      } else if (type == NS_FORM_INPUT_TEXT ||
                 type == NS_FORM_INPUT_PASSWORD) {
        numTextControlsFound++;
      }
    }
  }
  NS_ENSURE_SUCCESS(rv, rv);
            
  nsIPresShell* shell = aPresContext->GetPresShell();
  if (shell) {
    if (submitControl) {
      // Fire the button's onclick handler and let the button handle
      // submitting the form.
      nsGUIEvent event(NS_MOUSE_LEFT_CLICK);
      nsEventStatus status = nsEventStatus_eIgnore;
      shell->HandleDOMEventWithTarget(submitControl, &event, &status);
    } else if (numTextControlsFound == 1) {
      // If there's only one text control, just submit the form
      nsCOMPtr<nsIContent> form = do_QueryInterface(mForm);
      nsFormEvent event(NS_FORM_SUBMIT);
      nsEventStatus status  = nsEventStatus_eIgnore;
      shell->HandleDOMEventWithTarget(form, &event, &status);
    }
  }

  return NS_OK;
}

nsresult
nsHTMLInputElement::SetCheckedInternal(PRBool aChecked, PRBool aNotify)
{
  //
  // Set the value
  //
  SET_BOOLBIT(mBitField, BF_CHECKED, aChecked);

  //
  // Notify the frame
  //
  nsIFrame* frame = GetPrimaryFrame(PR_FALSE);
  if (frame) {
    nsCOMPtr<nsIPresContext> presContext;
    GetPresContext(this, getter_AddRefs(presContext));

    if (mType == NS_FORM_INPUT_CHECKBOX) {
      nsICheckboxControlFrame* checkboxFrame = nsnull;
      CallQueryInterface(frame, &checkboxFrame);
      if (checkboxFrame) {
        checkboxFrame->OnChecked(presContext, aChecked);
      }
    } else if (mType == NS_FORM_INPUT_RADIO) {
      nsIRadioControlFrame* radioFrame = nsnull;
      CallQueryInterface(frame, &radioFrame);
      if (radioFrame) {
        radioFrame->OnChecked(presContext, aChecked);
      }
    }
  }

  // Notify the document that the CSS :checked pseudoclass for this element
  // has changed state.
  if (mDocument && aNotify) {
    mozAutoDocUpdate(mDocument, UPDATE_CONTENT_STATE, aNotify);
    mDocument->ContentStatesChanged(this, nsnull, NS_EVENT_STATE_CHECKED);
  }

  return NS_OK;
}


void
nsHTMLInputElement::FireOnChange()
{
  //
  // Since the value is changing, send out an onchange event (bug 23571)
  //
  nsCOMPtr<nsIPresContext> presContext;
  GetPresContext(this, getter_AddRefs(presContext));
  nsEventStatus status = nsEventStatus_eIgnore;
  nsEvent event(NS_FORM_CHANGE);
  HandleDOMEvent(presContext, &event, nsnull, NS_EVENT_FLAG_INIT, &status);
}

NS_IMETHODIMP
nsHTMLInputElement::Blur()
{
  SetElementFocus(PR_FALSE);

  return NS_OK;
}

NS_IMETHODIMP
nsHTMLInputElement::Focus()
{
  SetElementFocus(PR_TRUE);

  return NS_OK;
}

void
nsHTMLInputElement::SetFocus(nsIPresContext* aPresContext)
{
  if (!aPresContext)
    return;

  // We can't be focus'd if we don't have a document
  if (!mDocument)
    return;

  // first see if we are disabled or not. If disabled then do nothing.
  nsAutoString disabled;
  if (NS_CONTENT_ATTR_HAS_VALUE == GetAttr(kNameSpaceID_None,
                                           nsHTMLAtoms::disabled, disabled)) {
    return;
  }
 
  // If the window is not active, do not allow the focus to bring the
  // window to the front.  We update the focus controller, but do
  // nothing else.
  nsCOMPtr<nsPIDOMWindow> win(do_QueryInterface(mDocument->GetScriptGlobalObject()));
  nsIFocusController *focusController = win->GetRootFocusController();
  PRBool isActive = PR_FALSE;
  focusController->GetActive(&isActive);
  if (!isActive) {
    focusController->SetFocusedWindow(win);
    focusController->SetFocusedElement(this);

    return;
  }

  aPresContext->EventStateManager()->SetContentState(this,
                                                     NS_EVENT_STATE_FOCUS);

  nsIFormControlFrame* formControlFrame = GetFormControlFrame(PR_TRUE);

  if (formControlFrame) {
    formControlFrame->SetFocus(PR_TRUE, PR_TRUE);
    formControlFrame->ScrollIntoView(aPresContext);
    // Could call SelectAll(aPresContext) here to automatically
    // select text when we receive focus - only for text and password!
  }
}

NS_IMETHODIMP
nsHTMLInputElement::Select()
{
  nsresult rv = NS_OK;
  if (!mDocument)
    return NS_OK;

  // first see if we are disabled or not. If disabled then do nothing.
  nsAutoString disabled;
  if (NS_CONTENT_ATTR_HAS_VALUE == GetAttr(kNameSpaceID_None,
                                           nsHTMLAtoms::disabled, disabled)) {
    return rv;
  }

  if (mType == NS_FORM_INPUT_PASSWORD || mType == NS_FORM_INPUT_TEXT) {
    // XXX Bug?  We have to give the input focus before contents can be
    // selected

    nsCOMPtr<nsIPresContext> presContext;
    GetPresContext(this, getter_AddRefs(presContext)); 

    // If the window is not active, do not allow the select to bring the
    // window to the front.  We update the focus controller, but do
    // nothing else.
    nsCOMPtr<nsPIDOMWindow> win(do_QueryInterface(mDocument->GetScriptGlobalObject()));
    nsIFocusController *focusController = win->GetRootFocusController();
    PRBool isActive = PR_FALSE;
    focusController->GetActive(&isActive);
    if (!isActive) {
      focusController->SetFocusedWindow(win);
      focusController->SetFocusedElement(this);
      SelectAll(presContext);
      return NS_OK;
    }

    // Just like SetFocus() but without the ScrollIntoView()!
    nsEventStatus status = nsEventStatus_eIgnore;
    
    //If already handling select event, don't dispatch a second.
    if (!GET_BOOLBIT(mBitField, BF_HANDLING_SELECT_EVENT)) {
      nsEvent event(NS_FORM_SELECTED);
  
      SET_BOOLBIT(mBitField, BF_HANDLING_SELECT_EVENT, PR_TRUE);
      rv = HandleDOMEvent(presContext, &event, nsnull, NS_EVENT_FLAG_INIT,
                          &status);
      SET_BOOLBIT(mBitField, BF_HANDLING_SELECT_EVENT, PR_FALSE);
    }

    // If the DOM event was not canceled (e.g. by a JS event handler
    // returning false)
    if (status == nsEventStatus_eIgnore) {
      if (presContext) {
        nsIEventStateManager *esm = presContext->EventStateManager();
        // XXX Fix for bug 135345 - ESM currently does not check to see if we
        // have focus before attempting to set focus again and may cause
        // infinite recursion.  For now check if we have focus and do not set
        // focus again if already focused.
        PRInt32 currentState;
        esm->GetContentState(this, currentState);
        if (!(currentState & NS_EVENT_STATE_FOCUS)) {
          esm->SetContentState(this, NS_EVENT_STATE_FOCUS);
        }
      }

      nsIFormControlFrame* formControlFrame = GetFormControlFrame(PR_TRUE);

      if (formControlFrame) {
        formControlFrame->SetFocus(PR_TRUE, PR_TRUE);

        // Now Select all the text!
        SelectAll(presContext);
      }
    }
  }

  return rv;
}

void
nsHTMLInputElement::SelectAll(nsIPresContext* aPresContext)
{
  nsIFormControlFrame* formControlFrame = GetFormControlFrame(PR_TRUE);

  if (formControlFrame) {
    formControlFrame->SetProperty(aPresContext, nsHTMLAtoms::select,
                                  EmptyString());
  }
}

NS_IMETHODIMP
nsHTMLInputElement::Click()
{
  nsresult rv = NS_OK;

  if (GET_BOOLBIT(mBitField, BF_HANDLING_CLICK)) // Fixes crash as in bug 41599
      return rv;                      // --heikki@netscape.com

  // first see if we are disabled or not. If disabled then do nothing.
  nsAutoString disabled;
  if (NS_CONTENT_ATTR_HAS_VALUE == GetAttr(kNameSpaceID_None,
                                           nsHTMLAtoms::disabled, disabled)) {
    return rv;
  }

  // see what type of input we are.  Only click button, checkbox, radio,
  // reset, submit, & image
  if (mDocument &&
      (mType == NS_FORM_INPUT_BUTTON   ||
       mType == NS_FORM_INPUT_CHECKBOX ||
       mType == NS_FORM_INPUT_RADIO    ||
       mType == NS_FORM_INPUT_RESET    ||
       mType == NS_FORM_INPUT_SUBMIT   ||
       mType == NS_FORM_INPUT_IMAGE)) {

    nsCOMPtr<nsIDocument> doc = mDocument; // Strong in case the event kills it
    nsCOMPtr<nsIPresContext> context;

    nsIPresShell *shell = doc->GetShellAt(0);

    if (shell) {
      shell->GetPresContext(getter_AddRefs(context));

      if (context) {
        nsEventStatus status = nsEventStatus_eIgnore;
        nsMouseEvent event(NS_MOUSE_LEFT_CLICK);

        SET_BOOLBIT(mBitField, BF_HANDLING_CLICK, PR_TRUE);

        rv = HandleDOMEvent(context, &event, nsnull, NS_EVENT_FLAG_INIT,
                            &status);

        SET_BOOLBIT(mBitField, BF_HANDLING_CLICK, PR_FALSE);
      }
    }
  }
  
  return NS_OK;
}

nsresult
nsHTMLInputElement::HandleDOMEvent(nsIPresContext* aPresContext,
                                   nsEvent* aEvent,
                                   nsIDOMEvent** aDOMEvent,
                                   PRUint32 aFlags,
                                   nsEventStatus* aEventStatus)
{
  NS_ENSURE_ARG_POINTER(aEventStatus);

  // Do not process any DOM events if the element is disabled
  PRBool disabled;
  nsresult rv = GetDisabled(&disabled);
  NS_ENSURE_SUCCESS(rv, rv);
  if (disabled) {
    return NS_OK;
  }
  
  // For some reason or another we also need to check if the style shows us
  // as disabled.
  nsIFormControlFrame* formControlFrame = GetFormControlFrame(PR_FALSE);
  if (formControlFrame) {
    nsIFrame* formFrame = nsnull;
    CallQueryInterface(formControlFrame, &formFrame);
    if (formFrame) {
      const nsStyleUserInterface* uiStyle = formFrame->GetStyleUserInterface();

      if (uiStyle->mUserInput == NS_STYLE_USER_INPUT_NONE ||
          uiStyle->mUserInput == NS_STYLE_USER_INPUT_DISABLED) {
        return NS_OK;
      }
    }
  }

  //
  // Web pages expect the value of a radio button or checkbox to be set
  // *before* onclick and DOMActivate fire, and they expect that if they set
  // the value explicitly during onclick or DOMActivate it will not be toggled
  // or any such nonsense.
  // In order to support that (bug 57137 and 58460 are examples) we toggle
  // the checked attribute *first*, and then fire onclick.  If the user
  // returns false, we reset the control to the old checked value.  Otherwise,
  // we dispatch DOMActivate.  If DOMActivate is cancelled, we also reset
  // the control to the old checked value.  We need to keep track of whether
  // we've already toggled the state from onclick since the user could
  // explicitly dispatch DOMActivate on the element.
  //
  // This is a compatibility hack.
  //

  // Track whether we're in the "outer" HandleDOMEvent invocation
  PRBool clickOrExtActivate =
    (aEvent->message == NS_MOUSE_LEFT_CLICK ||
     (aEvent->message == NS_DOMUI_ACTIVATE &&
      !GET_BOOLBIT(mBitField, BF_IN_INTERNAL_ACTIVATE)));

  PRBool originalCheckedValue = PR_FALSE;

  nsCOMPtr<nsIDOMHTMLInputElement> selectedRadioButton;

  if (!(aFlags & NS_EVENT_FLAG_CAPTURE) &&
      !(aFlags & NS_EVENT_FLAG_SYSTEM_EVENT) &&
      clickOrExtActivate) {

    SET_BOOLBIT(mBitField, BF_CHECKED_IS_TOGGLED, PR_FALSE);

    switch(mType) {
      case NS_FORM_INPUT_CHECKBOX:
        {
          GetChecked(&originalCheckedValue);
          DoSetChecked(!originalCheckedValue);
          SET_BOOLBIT(mBitField, BF_CHECKED_IS_TOGGLED, PR_TRUE);
        }
        break;

      case NS_FORM_INPUT_RADIO:
        {
          nsCOMPtr<nsIRadioGroupContainer> container = GetRadioGroupContainer();
          if (container) {
            nsAutoString name;
            if (GetNameIfExists(name)) {
              container->GetCurrentRadioButton(name,
                                               getter_AddRefs(selectedRadioButton));
            }
          }

          GetChecked(&originalCheckedValue);
          if (!originalCheckedValue) {
            DoSetChecked(PR_TRUE);
            SET_BOOLBIT(mBitField, BF_CHECKED_IS_TOGGLED, PR_TRUE);
          }
        }
        break;

      case NS_FORM_INPUT_SUBMIT:
      case NS_FORM_INPUT_IMAGE:
        if(mForm) {
          // tell the form that we are about to enter a click handler.
          // that means that if there are scripted submissions, the
          // latest one will be deferred until after the exit point of the handler. 
          mForm->OnSubmitClickBegin();
        }
        break;

      default:
        break;
    } //switch
  }

  // If NS_EVENT_FLAG_NO_CONTENT_DISPATCH is set we will not allow content to handle
  // this event.  But to allow middle mouse button paste to work we must allow 
  // middle clicks to go to text fields anyway.
  PRBool noContentDispatch = aEvent->flags & NS_EVENT_FLAG_NO_CONTENT_DISPATCH;
  if ((mType == NS_FORM_INPUT_TEXT || mType == NS_FORM_INPUT_PASSWORD) &&
      aEvent->message == NS_MOUSE_MIDDLE_CLICK) {
    aEvent->flags &= ~NS_EVENT_FLAG_NO_CONTENT_DISPATCH;
  }

  // We must cache type because mType may change during JS event (bug 2369)
  //
  PRInt32 oldType = mType;

  // Try script event handlers first if its not a focus/blur event
  //we dont want the doc to get these
  rv = nsGenericHTMLFormElement::HandleDOMEvent(aPresContext, aEvent,
                                                aDOMEvent, aFlags,
                                                aEventStatus);

  // Ideally we would make the default action for click and space just dispatch
  // DOMActivate, and the default action for DOMActivate flip the checkbox/
  // radio state and fire onchange.  However, for backwards compatibility, we
  // need to flip the state before firing click, and we need to fire click
  // when space is pressed.  So, we just nest the firing of DOMActivate inside
  // the click event handling, and allow cancellation of DOMActivate to cancel
  // the click.
  if (*aEventStatus != nsEventStatus_eConsumeNoDefault &&
      !(aFlags & NS_EVENT_FLAG_SYSTEM_EVENT) &&
      aEvent->message == NS_MOUSE_LEFT_CLICK && mType != NS_FORM_INPUT_TEXT) {
    nsDOMUIEvent actEvent(NS_DOMUI_ACTIVATE, 1);

    nsIPresShell *shell = aPresContext->GetPresShell();
    if (shell) {
      nsEventStatus status = nsEventStatus_eIgnore;
      SET_BOOLBIT(mBitField, BF_IN_INTERNAL_ACTIVATE, PR_TRUE);
      rv = shell->HandleDOMEventWithTarget(this, &actEvent, &status);
      SET_BOOLBIT(mBitField, BF_IN_INTERNAL_ACTIVATE, PR_FALSE);

      // If activate is cancelled, we must do the same as when click is
      // cancelled (revert the checkbox to its original value).
      if (status == nsEventStatus_eConsumeNoDefault)
        *aEventStatus = status;
    }
  }

  if (!(aFlags & NS_EVENT_FLAG_CAPTURE) &&
      !(aFlags & NS_EVENT_FLAG_SYSTEM_EVENT) &&
      clickOrExtActivate) {
    switch(oldType) {
      case NS_FORM_INPUT_SUBMIT:
      case NS_FORM_INPUT_IMAGE:
        if(mForm) {
          // tell the form that we are about to exit a click handler
          // so the form knows not to defer subsequent submissions
          // the pending ones that were created during the handler
          // will be flushed or forgoten.
          mForm->OnSubmitClickEnd();
        }
        break;
    } //switch
  }

  // Reset the flag for other content besides this text field
  aEvent->flags |= noContentDispatch ? NS_EVENT_FLAG_NO_CONTENT_DISPATCH : NS_EVENT_FLAG_NONE;

  // now check to see if the event was "cancelled"
  if (GET_BOOLBIT(mBitField, BF_CHECKED_IS_TOGGLED) && clickOrExtActivate) {
    if (*aEventStatus == nsEventStatus_eConsumeNoDefault) {
      // if it was cancelled and a radio button, then set the old
      // selected btn to TRUE. if it is a checkbox then set it to its
      // original value
      if (selectedRadioButton) {
        selectedRadioButton->SetChecked(PR_TRUE);
        // If this one is no longer a radio button we must reset it back to
        // false to cancel the action.  See how the web of hack grows?
        if (mType != NS_FORM_INPUT_RADIO) {
          DoSetChecked(PR_FALSE);
        }
      } else if (oldType == NS_FORM_INPUT_CHECKBOX) {
        DoSetChecked(originalCheckedValue);
      }
    } else {
      FireOnChange();
#ifdef ACCESSIBILITY
      // Fire an event to notify accessibility
      if (mType == NS_FORM_INPUT_CHECKBOX) {
        FireEventForAccessibility(aPresContext,
                                  NS_LITERAL_STRING("CheckboxStateChange"));
      } else {
        FireEventForAccessibility(aPresContext,
                                  NS_LITERAL_STRING("RadioStateChange"));
      }
#endif
    }
  }

  if (NS_SUCCEEDED(rv) && 
      !(aFlags & NS_EVENT_FLAG_CAPTURE) &&
      !(aFlags & NS_EVENT_FLAG_SYSTEM_EVENT)) {
    if (nsEventStatus_eIgnore == *aEventStatus) {
      switch (aEvent->message) {

        case NS_FOCUS_CONTENT:
        {
          // Check to see if focus has bubbled up from a form control's
          // child textfield or button.  If that's the case, don't focus
          // this parent file control -- leave focus on the child.
          nsIFormControlFrame* formControlFrame = GetFormControlFrame(PR_FALSE);
          if (formControlFrame && !(aFlags & NS_EVENT_FLAG_BUBBLE))
            formControlFrame->SetFocus(PR_TRUE, PR_TRUE);
        }                                                                         
        break; // NS_FOCUS_CONTENT

        case NS_KEY_PRESS:
        case NS_KEY_UP:
        {
          // For backwards compat, trigger checks/radios/buttons with
          // space or enter (bug 25300)
          nsKeyEvent * keyEvent = (nsKeyEvent *)aEvent;

          if ((aEvent->message == NS_KEY_PRESS &&
               keyEvent->keyCode == NS_VK_RETURN) ||
              (aEvent->message == NS_KEY_UP &&
               keyEvent->keyCode == NS_VK_SPACE)) {
            switch(mType) {
              case NS_FORM_INPUT_CHECKBOX:
              case NS_FORM_INPUT_RADIO:
              {
                // Checkbox and Radio try to submit on Enter press
                if (keyEvent->keyCode != NS_VK_SPACE) {
                  MaybeSubmitForm(aPresContext);

                  break;  // If we are submitting, do not send click event
                }
                // else fall through and treat Space like click...
              }
              case NS_FORM_INPUT_BUTTON:
              case NS_FORM_INPUT_RESET:
              case NS_FORM_INPUT_SUBMIT:
              case NS_FORM_INPUT_IMAGE: // Bug 34418
              {
                nsEventStatus status = nsEventStatus_eIgnore;
                nsMouseEvent event(NS_MOUSE_LEFT_CLICK);
                rv = HandleDOMEvent(aPresContext, &event, nsnull,
                                    NS_EVENT_FLAG_INIT, &status);
              } // case
            } // switch
          }
          if (aEvent->message == NS_KEY_PRESS && mType == NS_FORM_INPUT_RADIO &&
              !keyEvent->isAlt && !keyEvent->isControl && !keyEvent->isMeta) {
            PRBool isMovingBack = PR_FALSE;
            switch (keyEvent->keyCode) {
              case NS_VK_UP: 
              case NS_VK_LEFT:
                isMovingBack = PR_TRUE;
              case NS_VK_DOWN:
              case NS_VK_RIGHT:
              // Arrow key pressed, focus+select prev/next radio button
              nsCOMPtr<nsIRadioGroupContainer> container = GetRadioGroupContainer();
              if (container) {
                nsAutoString name;
                if (GetNameIfExists(name)) {
                  container->GetNextRadioButton(name, isMovingBack, this,
                                                getter_AddRefs(selectedRadioButton));
                  nsCOMPtr<nsIContent> radioContent =
                    do_QueryInterface(selectedRadioButton);
                  if (radioContent) {
                    rv = selectedRadioButton->Focus();
                    if (NS_SUCCEEDED(rv)) {
                      nsEventStatus status = nsEventStatus_eIgnore;
                      nsMouseEvent event(NS_MOUSE_LEFT_CLICK);
                      rv = radioContent->HandleDOMEvent(aPresContext, &event,
                                                        nsnull, NS_EVENT_FLAG_INIT, 
                                                        &status);
                    }
                  }
                }
              }
            }
          }

          /*
           * If this is input type=text, and the user hit enter, fire onChange
           * and submit the form (if we are in one)
           *
           * Bug 99920, bug 109463 and bug 147850:
           * (a) if there is a submit control in the form, click the first
           *     submit control in the form.
           * (b) if there is just one text control in the form, submit by
           *     sending a submit event directly to the form
           * (c) if there is more than one text input and no submit buttons, do
           *     not submit, period.
           */

          if (aEvent->message == NS_KEY_PRESS &&
              (keyEvent->keyCode == NS_VK_RETURN ||
               keyEvent->keyCode == NS_VK_ENTER) &&
              (mType == NS_FORM_INPUT_TEXT ||
               mType == NS_FORM_INPUT_PASSWORD ||
               mType == NS_FORM_INPUT_FILE)) {

            nsIFrame* primaryFrame = GetPrimaryFrame(PR_FALSE);
            if (primaryFrame) {
              nsITextControlFrame* textFrame = nsnull;
              CallQueryInterface(primaryFrame, &textFrame);
              
              // Fire onChange (if necessary)
              if (textFrame) {
                textFrame->CheckFireOnChange();
              }
            }

            rv = MaybeSubmitForm(aPresContext);
            NS_ENSURE_SUCCESS(rv, rv);
          }

        } break; // NS_KEY_PRESS || NS_KEY_UP

        // cancel all of these events for buttons
        case NS_MOUSE_MIDDLE_BUTTON_DOWN:
        case NS_MOUSE_MIDDLE_BUTTON_UP:
        case NS_MOUSE_MIDDLE_DOUBLECLICK:
        case NS_MOUSE_RIGHT_DOUBLECLICK:
        case NS_MOUSE_RIGHT_BUTTON_DOWN:
        case NS_MOUSE_RIGHT_BUTTON_UP:
        {
          if (mType == NS_FORM_INPUT_BUTTON || 
              mType == NS_FORM_INPUT_RESET || 
              mType == NS_FORM_INPUT_SUBMIT ) {
            nsCOMPtr<nsIDOMNSEvent> nsevent;

            if (aDOMEvent) {
              nsevent = do_QueryInterface(*aDOMEvent);
            }

            if (nsevent) {
              nsevent->PreventBubble();
            } else {
              rv = NS_ERROR_FAILURE;
            }
          }

          break;
        }
      default:
        break;
      }

      if (clickOrExtActivate) {
        if (mForm && (oldType == NS_FORM_INPUT_SUBMIT ||
                      oldType == NS_FORM_INPUT_IMAGE)) {
          if (mType != NS_FORM_INPUT_SUBMIT && mType != NS_FORM_INPUT_IMAGE) {
            // If the type has changed to a non-submit type, then we want to
            // flush the stored submission if there is one (as if the submit()
            // was allowed to succeed)
            mForm->FlushPendingSubmission();
          }
        }
        switch(mType) {
        case NS_FORM_INPUT_RESET:
        case NS_FORM_INPUT_SUBMIT:
        case NS_FORM_INPUT_IMAGE:
          if (mForm) {
            nsFormEvent event((mType == NS_FORM_INPUT_RESET) ?
                              NS_FORM_RESET : NS_FORM_SUBMIT);
            event.originator      = this;
            nsEventStatus status  = nsEventStatus_eIgnore;

            nsIPresShell *presShell = aPresContext->GetPresShell();

            // If |nsIPresShell::Destroy| has been called due to
            // handling the event (base class HandleDOMEvent, above),
            // the pres context will return a null pres shell.  See
            // bug 125624.
            if (presShell) {
              nsCOMPtr<nsIContent> form(do_QueryInterface(mForm));
              presShell->HandleDOMEventWithTarget(form, &event, &status);
            }
          }
          break;

        default:
          break;
        } //switch 
      } //click or outer activate event
    } else {
      if (clickOrExtActivate &&
          (oldType == NS_FORM_INPUT_SUBMIT || oldType == NS_FORM_INPUT_IMAGE) &&
          mForm) {
        // tell the form to flush a possible pending submission.
        // the reason is that the script returned false (the event was
        // not ignored) so if there is a stored submission, it needs to
        // be submitted immediately.
        mForm->FlushPendingSubmission();
      }
    } //if
  } // if

  return rv;
}


void
nsHTMLInputElement::SetDocument(nsIDocument* aDocument, PRBool aDeep,
                                PRBool aCompileEventHandlers)
{
  PRBool documentChanging = (aDocument != mDocument);

  // SetDocument() sets the form and that takes care of form's WillRemove
  // so we just have to take care of the case where we're removing from the
  // document and we don't have a form
  if (documentChanging && !mForm && mType == NS_FORM_INPUT_RADIO) {
    WillRemoveFromRadioGroup();
  }

  nsGenericHTMLFormElement::SetDocument(aDocument, aDeep,
                                        aCompileEventHandlers);

  if (mType == NS_FORM_INPUT_IMAGE &&
      documentChanging && aDocument && GetParent()) {
    // Our base URI may have changed; claim that our URI changed, and the
    // nsImageLoadingContent will decide whether a new image load is warranted.
    nsAutoString uri;
    nsresult result = GetAttr(kNameSpaceID_None, nsHTMLAtoms::src, uri);
    if (result == NS_CONTENT_ATTR_HAS_VALUE) {
      ImageURIChanged(uri);
    }
  }
  
  // If this is radio button which is in a form,
  // and the parser is still creating the element.
  if (mForm || mType != NS_FORM_INPUT_RADIO ||
      GET_BOOLBIT(mBitField, BF_PARSER_CREATING)) {
    return;
  }
  
  // Add radio to document if we don't have a form already (if we do it's
  // already been added into that group)
  if (aDocument && !mForm && mType == NS_FORM_INPUT_RADIO) {
    AddedToRadioGroup();
  }
}

void
nsHTMLInputElement::SetParent(nsIContent* aParent)
{
  nsGenericHTMLFormElement::SetParent(aParent);
  if (mType == NS_FORM_INPUT_IMAGE && aParent && mDocument) {
    // Our base URI may have changed; claim that our URI changed, and the
    // nsImageLoadingContent will decide whether a new image load is warranted.
    nsAutoString uri;
    nsresult result = GetAttr(kNameSpaceID_None, nsHTMLAtoms::src, uri);
    if (result == NS_CONTENT_ATTR_HAS_VALUE) {
      ImageURIChanged(uri);
    }
  }
}


// nsIHTMLContent

static const nsHTMLValue::EnumTable kInputTypeTable[] = {
  { "button", NS_FORM_INPUT_BUTTON },
  { "checkbox", NS_FORM_INPUT_CHECKBOX },
  { "file", NS_FORM_INPUT_FILE },
  { "hidden", NS_FORM_INPUT_HIDDEN },
  { "reset", NS_FORM_INPUT_RESET },
  { "image", NS_FORM_INPUT_IMAGE },
  { "password", NS_FORM_INPUT_PASSWORD },
  { "radio", NS_FORM_INPUT_RADIO },
  { "submit", NS_FORM_INPUT_SUBMIT },
  { "text", NS_FORM_INPUT_TEXT },
  { 0 }
};

PRBool
nsHTMLInputElement::ParseAttribute(nsIAtom* aAttribute,
                                   const nsAString& aValue,
                                   nsAttrValue& aResult)
{
  if (aAttribute == nsHTMLAtoms::type) {
    // XXX ARG!! This is major evilness. ParseAttribute
    // shouldn't set members. Override SetAttr instead
    if (!aResult.ParseEnumValue(aValue, kInputTypeTable)) {
      mType = NS_FORM_INPUT_TEXT;
      return PR_FALSE;
    }

    mType = aResult.GetEnumValue();
    if (mType == NS_FORM_INPUT_FILE) {
      // If the type is being changed to file, set the element value
      // to the empty string. This is for security.
      // Call SetValueInternal so that this doesn't accidentally get caught
      // in the security checks in SetValue.
      SetValueInternal(EmptyString(), nsnull);
    }

    return PR_TRUE;
  }
  if (aAttribute == nsHTMLAtoms::width) {
    return aResult.ParseSpecialIntValue(aValue, PR_TRUE, PR_FALSE);
  }
  if (aAttribute == nsHTMLAtoms::height) {
    return aResult.ParseSpecialIntValue(aValue, PR_TRUE, PR_FALSE);
  }
  if (aAttribute == nsHTMLAtoms::maxlength) {
    return aResult.ParseIntWithBounds(aValue, 0);
  }
  if (aAttribute == nsHTMLAtoms::size) {
    return aResult.ParseIntWithBounds(aValue, 0);
  }
  if (aAttribute == nsHTMLAtoms::border) {
    return aResult.ParseIntWithBounds(aValue, 0);
  }
  if (aAttribute == nsHTMLAtoms::align) {
    return ParseAlignValue(aValue, aResult);
  }
  if (ParseImageAttribute(aAttribute, aValue, aResult)) {
    // We have to call |ParseImageAttribute| unconditionally since we
    // don't know if we're going to have a type="image" attribute yet,
    // (or could have it set dynamically in the future).  See bug
    // 214077.
    return PR_TRUE;
  }

  return nsGenericHTMLElement::ParseAttribute(aAttribute, aValue, aResult);
}

NS_IMETHODIMP
nsHTMLInputElement::AttributeToString(nsIAtom* aAttribute,
                                      const nsHTMLValue& aValue,
                                      nsAString& aResult) const
{
  if (aAttribute == nsHTMLAtoms::type) {
    if (eHTMLUnit_Enumerated == aValue.GetUnit()) {
      // The DOM spec says that input types should be capitalized but
      // AFAIK all existing browsers return the type in lower case,
      // http://bugzilla.mozilla.org/show_bug.cgi?id=32369 is the bug
      // about this problem.  -- jst@netscape.com
      // Update.  The DOM spec will be changed to have input types be
      // all-lowercase.  See
      // http://bugzilla.mozilla.org/show_bug.cgi?id=113174#c12
      // -- bzbarsky@mit.edu

      aValue.EnumValueToString(kInputTypeTable, aResult);

      return NS_CONTENT_ATTR_HAS_VALUE;
    }
  }
  else if (aAttribute == nsHTMLAtoms::align) {
    if (eHTMLUnit_Enumerated == aValue.GetUnit()) {
      AlignValueToString(aValue, aResult);
      return NS_CONTENT_ATTR_HAS_VALUE;
    }
  }

  return nsGenericHTMLFormElement::AttributeToString(aAttribute, aValue,
                                                     aResult);
}


NS_IMETHODIMP
nsHTMLInputElement::GetType(nsAString& aValue)
{
  const nsHTMLValue::EnumTable *table = kInputTypeTable;

  while (table->tag) {
    if (mType == table->value) {
      CopyUTF8toUTF16(table->tag, aValue);

      return NS_OK;
    }

    ++table;
  }

  NS_ERROR("Shouldn't get here!");

  aValue.Truncate();

  return NS_OK;
}

NS_IMETHODIMP
nsHTMLInputElement::SetType(const nsAString& aValue)
{
  return SetAttrHelper(nsHTMLAtoms::type, aValue);
}

static void
MapAttributesIntoRule(const nsMappedAttributes* aAttributes,
                      nsRuleData* aData)
{
  const nsAttrValue* value = aAttributes->GetAttr(nsHTMLAtoms::type);
  if (value && value->Type() == nsAttrValue::eEnum &&
      value->GetEnumValue() == NS_FORM_INPUT_IMAGE) {
    nsGenericHTMLFormElement::MapImageBorderAttributeInto(aAttributes, aData);
    nsGenericHTMLFormElement::MapImageMarginAttributeInto(aAttributes, aData);
    nsGenericHTMLFormElement::MapImageSizeAttributesInto(aAttributes, aData);
    // Images treat align as "float"
    nsGenericHTMLFormElement::MapImageAlignAttributeInto(aAttributes, aData);
  } else {
    // Everything else treats align as "text-align"
    nsGenericHTMLFormElement::MapDivAlignAttributeInto(aAttributes, aData);
  }

  nsGenericHTMLFormElement::MapCommonAttributesInto(aAttributes, aData);
}

NS_IMETHODIMP
nsHTMLInputElement::GetAttributeChangeHint(const nsIAtom* aAttribute,
                                           PRInt32 aModType,
                                           nsChangeHint& aHint) const
{
  nsresult rv =
    nsGenericHTMLFormElement::GetAttributeChangeHint(aAttribute, aModType,
                                                     aHint);
  if (aAttribute == nsHTMLAtoms::type) {
    NS_UpdateHint(aHint, NS_STYLE_HINT_FRAMECHANGE);
  } else if (aAttribute == nsHTMLAtoms::value) {
    NS_UpdateHint(aHint, NS_STYLE_HINT_REFLOW);
  } else if (aAttribute == nsHTMLAtoms::size &&
             (mType == NS_FORM_INPUT_TEXT ||
              mType == NS_FORM_INPUT_PASSWORD)) {
    NS_UpdateHint(aHint, NS_STYLE_HINT_REFLOW);
  }
  return rv;
}

NS_IMETHODIMP_(PRBool)
nsHTMLInputElement::IsAttributeMapped(const nsIAtom* aAttribute) const
{
  static const MappedAttributeEntry attributes[] = {
    { &nsHTMLAtoms::align },
    { &nsHTMLAtoms::type },
    { nsnull },
  };

  static const MappedAttributeEntry* const map[] = {
    attributes,
    sCommonAttributeMap,
    sImageMarginSizeAttributeMap,
    sImageBorderAttributeMap,
  };

  return FindAttributeDependence(aAttribute, map, NS_ARRAY_LENGTH(map));
}

NS_IMETHODIMP
nsHTMLInputElement::GetAttributeMappingFunction(nsMapRuleToAttributesFunc& aMapRuleFunc) const
{
  aMapRuleFunc = &MapAttributesIntoRule;
  return NS_OK;
}


// Controllers Methods

NS_IMETHODIMP
nsHTMLInputElement::GetControllers(nsIControllers** aResult)
{
  NS_ENSURE_ARG_POINTER(aResult);

  //XXX: what about type "file"?
  if (mType == NS_FORM_INPUT_TEXT || mType == NS_FORM_INPUT_PASSWORD)
  {
    if (!mControllers)
    {
      nsresult rv;
      mControllers = do_CreateInstance(kXULControllersCID, &rv);
      NS_ENSURE_SUCCESS(rv, rv);

      nsCOMPtr<nsIController>
        controller(do_CreateInstance("@mozilla.org/editor/editorcontroller;1",
                                     &rv));
      NS_ENSURE_SUCCESS(rv, rv);

      mControllers->AppendController(controller);
    }
  }

  *aResult = mControllers;
  NS_IF_ADDREF(*aResult);

  return NS_OK;
}

NS_IMETHODIMP
nsHTMLInputElement::GetTextLength(PRInt32* aTextLength)
{
  nsAutoString val;

  nsresult rv = GetValue(val);

  *aTextLength = val.Length();

  return rv;
}

NS_IMETHODIMP
nsHTMLInputElement::SetSelectionRange(PRInt32 aSelectionStart,
                                      PRInt32 aSelectionEnd)
{
  nsresult rv = NS_ERROR_FAILURE;
  nsIFormControlFrame* formControlFrame = GetFormControlFrame(PR_TRUE);

  if (formControlFrame) {
    nsITextControlFrame* textControlFrame = nsnull;
    CallQueryInterface(formControlFrame, &textControlFrame);

    if (textControlFrame)
      rv = textControlFrame->SetSelectionRange(aSelectionStart, aSelectionEnd);
  }

  return rv;
}

NS_IMETHODIMP
nsHTMLInputElement::GetSelectionStart(PRInt32* aSelectionStart)
{
  NS_ENSURE_ARG_POINTER(aSelectionStart);
  
  PRInt32 selEnd;
  return GetSelectionRange(aSelectionStart, &selEnd);
}

NS_IMETHODIMP
nsHTMLInputElement::SetSelectionStart(PRInt32 aSelectionStart)
{
  nsresult rv = NS_ERROR_FAILURE;
  nsIFormControlFrame* formControlFrame = GetFormControlFrame(PR_TRUE);

  if (formControlFrame) {
    nsITextControlFrame* textControlFrame = nsnull;
    CallQueryInterface(formControlFrame, &textControlFrame);

    if (textControlFrame)
      rv = textControlFrame->SetSelectionStart(aSelectionStart);
  }

  return rv;
}

NS_IMETHODIMP
nsHTMLInputElement::GetSelectionEnd(PRInt32* aSelectionEnd)
{
  NS_ENSURE_ARG_POINTER(aSelectionEnd);
  
  PRInt32 selStart;
  return GetSelectionRange(&selStart, aSelectionEnd);
}


NS_IMETHODIMP
nsHTMLInputElement::SetSelectionEnd(PRInt32 aSelectionEnd)
{
  nsresult rv = NS_ERROR_FAILURE;
  nsIFormControlFrame* formControlFrame = GetFormControlFrame(PR_TRUE);

  if (formControlFrame) {
    nsITextControlFrame* textControlFrame = nsnull;
    CallQueryInterface(formControlFrame, &textControlFrame);

    if (textControlFrame)
      rv = textControlFrame->SetSelectionEnd(aSelectionEnd);
  }

  return rv;
}

nsresult
nsHTMLInputElement::GetSelectionRange(PRInt32* aSelectionStart,
                                      PRInt32* aSelectionEnd)
{
  nsresult rv = NS_ERROR_FAILURE;
  nsIFormControlFrame* formControlFrame = GetFormControlFrame(PR_TRUE);

  if (formControlFrame) {
    nsITextControlFrame* textControlFrame = nsnull;
    CallQueryInterface(formControlFrame, &textControlFrame);

    if (textControlFrame)
      rv = textControlFrame->GetSelectionRange(aSelectionStart, aSelectionEnd);
  }

  return rv;
}

NS_IMETHODIMP
nsHTMLInputElement::GetPhonetic(nsAString& aPhonetic)
{
  aPhonetic.Truncate(0);
  nsIFormControlFrame* formControlFrame = GetFormControlFrame(PR_TRUE);

  if (formControlFrame) {
    nsCOMPtr<nsIPhonetic>
      phonetic(do_QueryInterface(formControlFrame));

    if (phonetic)
      phonetic->GetPhonetic(aPhonetic);
  }

  return NS_OK;
}

#ifdef ACCESSIBILITY
nsresult
nsHTMLInputElement::FireEventForAccessibility(nsIPresContext* aPresContext,
                                              const nsAString& aEventType)
{
  nsCOMPtr<nsIDOMEvent> event;
  nsCOMPtr<nsIEventListenerManager> manager;
  GetListenerManager(getter_AddRefs(manager));
  if (manager &&
      NS_SUCCEEDED(manager->CreateEvent(aPresContext, nsnull, NS_LITERAL_STRING("Events"), getter_AddRefs(event)))) {
    event->InitEvent(aEventType, PR_TRUE, PR_TRUE);

    PRBool noDefault;
    nsISupports *target = NS_STATIC_CAST(nsIDOMHTMLInputElement*, this);
    aPresContext->EventStateManager()->DispatchNewEvent(target, event,
                                                        &noDefault);
  }

  return NS_OK;
}
#endif

nsresult
nsHTMLInputElement::Reset()
{
  nsresult rv = NS_OK;

  nsIFormControlFrame* formControlFrame = GetFormControlFrame(PR_FALSE);

  switch (mType) {
    case NS_FORM_INPUT_CHECKBOX:
    case NS_FORM_INPUT_RADIO:
    {
      PRBool resetVal;
      GetDefaultChecked(&resetVal);
      rv = DoSetChecked(resetVal);
      SetCheckedChanged(PR_FALSE);
      break;
    }
    case NS_FORM_INPUT_PASSWORD:
    case NS_FORM_INPUT_TEXT:
    {
      // If the frame is there, we have to set the value so that it will show
      // up.
      if (formControlFrame) {
        nsAutoString resetVal;
        GetDefaultValue(resetVal);
        rv = SetValue(resetVal);
      }
      SetValueChanged(PR_FALSE);
      break;
    }
    case NS_FORM_INPUT_FILE:
    {
      // Resetting it to blank should not perform security check
      rv = SetValueInternal(EmptyString(), nsnull);
      break;
    }
    // Value is the same as defaultValue for hidden inputs
    case NS_FORM_INPUT_HIDDEN:
    default:
      break;
  }

  // Notify frame that it has been reset
  if (formControlFrame) {
    formControlFrame->OnContentReset();
  }
  return rv;
}

NS_IMETHODIMP
nsHTMLInputElement::SubmitNamesValues(nsIFormSubmission* aFormSubmission,
                                      nsIContent* aSubmitElement)
{
  nsresult rv = NS_OK;

  //
  // Disabled elements don't submit
  //
  PRBool disabled;
  rv = GetDisabled(&disabled);
  if (NS_FAILED(rv) || disabled) {
    return rv;
  }

  //
  // For type=reset, and type=button, we just never submit, period.
  //
  if (mType == NS_FORM_INPUT_RESET || mType == NS_FORM_INPUT_BUTTON) {
    return rv;
  }

  //
  // For type=image and type=button, we only submit if we were the button
  // pressed
  //
  if ((mType == NS_FORM_INPUT_SUBMIT || mType == NS_FORM_INPUT_IMAGE)
      && aSubmitElement != this) {
    return rv;
  }

  //
  // For type=radio and type=checkbox, we only submit if checked=true
  //
  if (mType == NS_FORM_INPUT_RADIO || mType == NS_FORM_INPUT_CHECKBOX) {
    PRBool checked;
    rv = GetChecked(&checked);
    if (NS_FAILED(rv) || !checked) {
      return rv;
    }
  }

  //
  // Get the name
  //
  nsAutoString name;
  rv = GetAttr(kNameSpaceID_None, nsHTMLAtoms::name, name);
  if (NS_FAILED(rv)) {
    return rv;
  }
  PRBool nameThere = (rv != NS_CONTENT_ATTR_NOT_THERE);

  //
  // Submit .x, .y for input type=image
  //
  if (mType == NS_FORM_INPUT_IMAGE) {
    // Go to the frame to find out where it was clicked.  This is the only
    // case where I can actually see using the frame, because you're talking
    // about a value--mouse click--that is rightfully the domain of the frame.
    //
    // If the frame isn't there or isn't an ImageControlFrame, then we're not
    // submitting these values no matter *how* nicely you ask.
    PRInt32 clickedX;
    PRInt32 clickedY;
    nsIFormControlFrame* formControlFrame = GetFormControlFrame(PR_TRUE);

    nsIImageControlFrame* imageControlFrame = nsnull;
    if (formControlFrame) {
      CallQueryInterface(formControlFrame, &imageControlFrame);
    }
    
    nsAutoString xVal;
    nsAutoString yVal;
    
    if (imageControlFrame) {
      imageControlFrame->GetClickedX(&clickedX);
      imageControlFrame->GetClickedY(&clickedY);

      // Convert the values to strings for submission
      xVal.AppendInt(clickedX);
      yVal.AppendInt(clickedY);
    }
    
    if (!name.IsEmpty()) {
      aFormSubmission->AddNameValuePair(this,
                                        name + NS_LITERAL_STRING(".x"), xVal);
      aFormSubmission->AddNameValuePair(this,
                                        name + NS_LITERAL_STRING(".y"), yVal);
    } else {
      // If the Image Element has no name, simply return x and y
      // to Nav and IE compatability.
      aFormSubmission->AddNameValuePair(this, NS_LITERAL_STRING("x"), xVal);
      aFormSubmission->AddNameValuePair(this, NS_LITERAL_STRING("y"), yVal);
    }
  }

  //
  // Submit name=value
  //

  // If name not there, don't submit
  if (!nameThere) {
    return rv;
  }

  // Get the value
  nsAutoString value;
  rv = GetValue(value);
  if (NS_FAILED(rv)) {
    return rv;
  }

  //
  // Submit file if it's input type=file and this encoding method accepts files
  //
  if (mType == NS_FORM_INPUT_FILE) {
    //
    // Open the file
    //
    nsCOMPtr<nsIFile> file;
 
    if (StringBeginsWith(value, NS_LITERAL_STRING("file:"),
                         nsCaseInsensitiveStringComparator())) {
      // Converts the URL string into the corresponding nsIFile if possible.
      // A local file will be created if the URL string begins with file://.
      rv = NS_GetFileFromURLSpec(NS_ConvertUCS2toUTF8(value),
                                 getter_AddRefs(file));
    }

    if (!file) {
      // this is no "file://", try as local file
      nsCOMPtr<nsILocalFile> localFile;
      rv = NS_NewLocalFile(value, PR_FALSE, getter_AddRefs(localFile));
      file = localFile;
    }

    if (NS_SUCCEEDED(rv)) {

      //
      // Get the leaf path name (to be submitted as the value)
      //
      nsAutoString filename;
      rv = file->GetLeafName(filename);

      if (NS_SUCCEEDED(rv) && !filename.IsEmpty()) {
        PRBool acceptsFiles = aFormSubmission->AcceptsFiles();

        if (acceptsFiles) {
          //
          // Get content type
          //
          nsCOMPtr<nsIMIMEService> MIMEService =
            do_GetService(NS_MIMESERVICE_CONTRACTID, &rv);
          NS_ENSURE_SUCCESS(rv, rv);

          nsCAutoString contentType;
          rv = MIMEService->GetTypeFromFile(file, contentType);
          if (NS_FAILED(rv)) {
            contentType.AssignLiteral("application/octet-stream");
          }

          //
          // Get input stream
          //
          nsCOMPtr<nsIInputStream> fileStream;
          rv = NS_NewLocalFileInputStream(getter_AddRefs(fileStream),
                                          file, -1, -1,
                                          nsIFileInputStream::CLOSE_ON_EOF |
                                          nsIFileInputStream::REOPEN_ON_REWIND);
          if (fileStream) {
            //
            // Create buffered stream (for efficiency)
            //
            nsCOMPtr<nsIInputStream> bufferedStream;
            rv = NS_NewBufferedInputStream(getter_AddRefs(bufferedStream),
                                           fileStream, 8192);
            NS_ENSURE_SUCCESS(rv, rv);
            if (bufferedStream) {
              //
              // Submit
              //
              aFormSubmission->AddNameFilePair(this, name, filename,
                                               bufferedStream, contentType,
                                               PR_FALSE);
              return rv;
            }
          }
        }

        //
        // If we don't submit as a file, at least submit the truncated filename.
        //
        aFormSubmission->AddNameFilePair(this, name, filename,
            nsnull, NS_LITERAL_CSTRING("application/octet-stream"),
            PR_FALSE);
        return rv;
      } else {
        // Ignore error returns from GetLeafName.  See bug 199053
        rv = NS_OK;
      }
    }
    
    //
    // If we can't even make a truncated filename, submit empty string
    // rather than sending everything
    //
    aFormSubmission->AddNameFilePair(this, name, value,
        nsnull, NS_LITERAL_CSTRING("application/octet-stream"),
        PR_FALSE);
    return rv;
  }

  // Submit
  // (for type=image, only submit if value is non-null)
  if (mType != NS_FORM_INPUT_IMAGE || !value.IsEmpty()) {
    rv = aFormSubmission->AddNameValuePair(this, name, value);
  }

  return rv;
}


NS_IMETHODIMP
nsHTMLInputElement::SaveState()
{
  nsresult rv = NS_OK;

  nsCOMPtr<nsIPresState> state;
  switch (mType) {
    case NS_FORM_INPUT_CHECKBOX:
    case NS_FORM_INPUT_RADIO:
      {
        PRBool checked = PR_FALSE;
        GetChecked(&checked);
        PRBool defaultChecked = PR_FALSE;
        GetDefaultChecked(&defaultChecked);
        // Only save if checked != defaultChecked (bug 62713)
        // (always save if it's a radio button so that the checked
        // state of all radio buttons is restored)
        if (mType == NS_FORM_INPUT_RADIO || checked != defaultChecked) {
          rv = GetPrimaryPresState(this, getter_AddRefs(state));
          if (state) {
            if (checked) {
              rv = state->SetStateProperty(NS_LITERAL_STRING("checked"),
                                           NS_LITERAL_STRING("t"));
            } else {
              rv = state->SetStateProperty(NS_LITERAL_STRING("checked"),
                                           NS_LITERAL_STRING("f"));
            }
            NS_ASSERTION(NS_SUCCEEDED(rv), "checked save failed!");
          }
        }
        break;
      }

    // Never save passwords in session history
    case NS_FORM_INPUT_PASSWORD:
      break;
    case NS_FORM_INPUT_TEXT:
    case NS_FORM_INPUT_FILE:
    case NS_FORM_INPUT_HIDDEN:
      {
        if (GET_BOOLBIT(mBitField, BF_VALUE_CHANGED)) {
          rv = GetPrimaryPresState(this, getter_AddRefs(state));
          if (state) {
            nsAutoString value;
            GetValue(value);
            rv = nsLinebreakConverter::ConvertStringLineBreaks(
                     value,
                     nsLinebreakConverter::eLinebreakPlatform,
                     nsLinebreakConverter::eLinebreakContent);
            NS_ASSERTION(NS_SUCCEEDED(rv), "Converting linebreaks failed!");
            rv = state->SetStateProperty(NS_LITERAL_STRING("v"), value);
            NS_ASSERTION(NS_SUCCEEDED(rv), "value save failed!");
          }
        }
        break;
      }
  }
  
  if (GET_BOOLBIT(mBitField, BF_DISABLED_CHANGED)) {
    rv |= GetPrimaryPresState(this, getter_AddRefs(state));
    if (state) {
      PRBool disabled;
      GetDisabled(&disabled);
      if (disabled) {
        rv |= state->SetStateProperty(NS_LITERAL_STRING("disabled"),
                                     NS_LITERAL_STRING("t"));
      } else {
        rv |= state->SetStateProperty(NS_LITERAL_STRING("disabled"),
                                     NS_LITERAL_STRING("f"));
      }
      NS_ASSERTION(NS_SUCCEEDED(rv), "disabled save failed!");
    }
  }

  return rv;
}

void
nsHTMLInputElement::DoneCreatingElement()
{
  SET_BOOLBIT(mBitField, BF_PARSER_CREATING, PR_FALSE);

  //
  // Restore state for checkbox, radio, text and file
  //
  PRBool restored = PR_FALSE;
  switch (mType) {
    case NS_FORM_INPUT_CHECKBOX:
    case NS_FORM_INPUT_RADIO:
    case NS_FORM_INPUT_TEXT:
    case NS_FORM_INPUT_FILE:
    case NS_FORM_INPUT_HIDDEN:
      restored = RestoreFormControlState(this, this);
      break;
  }

  //
  // If restore does not occur, we initialize .checked using the CHECKED
  // property.
  //
  if (!restored && GET_BOOLBIT(mBitField, BF_SHOULD_INIT_CHECKED)) {
    PRBool resetVal;
    GetDefaultChecked(&resetVal);
    DoSetChecked(resetVal, PR_FALSE);
    SetCheckedChanged(PR_FALSE);
  }

  SET_BOOLBIT(mBitField, BF_SHOULD_INIT_CHECKED, PR_FALSE);
  
  //
  // If the radio button is not in a form, we can add it to
  // radio group in document here, otherwise we will miss it.
  //
  if (!mForm && mType == NS_FORM_INPUT_RADIO)
    AddedToRadioGroup(PR_FALSE);
}

NS_IMETHODIMP
nsHTMLInputElement::RestoreState(nsIPresState* aState)
{
  nsresult rv = NS_OK;

  switch (mType) {
    case NS_FORM_INPUT_CHECKBOX:
    case NS_FORM_INPUT_RADIO:
      {
        nsAutoString checked;
        rv = aState->GetStateProperty(NS_LITERAL_STRING("checked"), checked);
        if (rv == NS_STATE_PROPERTY_EXISTS) {
          DoSetChecked(checked.EqualsLiteral("t"), PR_FALSE);
        }
        break;
      }

    case NS_FORM_INPUT_TEXT:
    case NS_FORM_INPUT_FILE:
    case NS_FORM_INPUT_HIDDEN:
      {
        nsAutoString value;
        rv = aState->GetStateProperty(NS_LITERAL_STRING("v"), value);
        NS_ASSERTION(NS_SUCCEEDED(rv), "value restore failed!");
        if (rv == NS_STATE_PROPERTY_EXISTS) {
          SetValueInternal(value, nsnull);
        }
        break;
      }
  }
  
  nsAutoString disabled;
  nsresult rv2 = aState->GetStateProperty(NS_LITERAL_STRING("disabled"), disabled);
  NS_ASSERTION(NS_SUCCEEDED(rv2), "disabled restore failed!");
  if (rv2 == NS_STATE_PROPERTY_EXISTS) {
    SetDisabled(disabled.EqualsLiteral("t"));
  }

  if (NS_FAILED(rv|rv2)) {
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

PRBool
nsHTMLInputElement::AllowDrop()
{
  // Allow drop on anything other than file inputs.

  return mType != NS_FORM_INPUT_FILE;
}

/*
 * Radio group stuff
 */

NS_IMETHODIMP
nsHTMLInputElement::AddedToRadioGroup(PRBool aNotify)
{
  if (aNotify)
    aNotify = GET_BOOLBIT(mBitField, BF_PARSER_CREATING) != 0;

  //
  //  If the input element is not in a form and
  //  not in a document, we just need to return.
  //
  if (!mForm && !(mDocument && GetParent())) {
    return NS_OK;
  }

  //
  // If the input element is checked, and we add it to the group, it will
  // deselect whatever is currently selected in that group
  //
  PRBool checked;
  GetChecked(&checked);
  if (checked) {
    //
    // If it is checked, call "RadioSetChecked" to perform the selection/
    // deselection ritual.  This has the side effect of repainting the
    // radio button, but as adding a checked radio button into the group
    // should not be that common an occurrence, I think we can live with
    // that.
    //
    RadioSetChecked(aNotify);
  }

  //
  // For integrity purposes, we have to ensure that "checkedChanged" is
  // the same for this new element as for all the others in the group
  //
  PRBool checkedChanged = PR_FALSE;
  nsCOMPtr<nsIRadioVisitor> visitor;
  nsresult rv = NS_GetRadioGetCheckedChangedVisitor(&checkedChanged, this,
                                           getter_AddRefs(visitor));
  NS_ENSURE_SUCCESS(rv, rv);
  
  VisitGroup(visitor);
  SetCheckedChangedInternal(checkedChanged);
  
  //
  // Add the radio to the radio group container.
  //
  nsCOMPtr<nsIRadioGroupContainer> container = GetRadioGroupContainer();
  if (container) {
    nsAutoString name;
    if (GetNameIfExists(name)) {
      container->AddToRadioGroup(name, NS_STATIC_CAST(nsIFormControl*, this));
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsHTMLInputElement::WillRemoveFromRadioGroup()
{
  //
  // If the input element is not in a form and
  // not in a document, we just need to return.
  //
  if (!mForm && !(mDocument && GetParent())) {
    return NS_OK;
  }

  //
  // If this button was checked, we need to notify the group that there is no
  // longer a selected radio button
  //
  PRBool checked = PR_FALSE;
  GetChecked(&checked);

  nsAutoString name;
  PRBool gotName = PR_FALSE;
  if (checked) {
    if (!gotName) {
      if (!GetNameIfExists(name)) {
        // If the name doesn't exist, nothing is going to happen anyway
        return NS_OK;
      }
      gotName = PR_TRUE;
    }

    nsCOMPtr<nsIRadioGroupContainer> container = GetRadioGroupContainer();
    if (container) {
      container->SetCurrentRadioButton(name, nsnull);
    }
  }
  
  //
  // Remove this radio from its group in the container
  //
  nsCOMPtr<nsIRadioGroupContainer> container = GetRadioGroupContainer();
  if (container) {
    if (!gotName) {
      if (!GetNameIfExists(name)) {
        // If the name doesn't exist, nothing is going to happen anyway
        return NS_OK;
      }
      gotName = PR_TRUE;
    }
    container->RemoveFromRadioGroup(name,
                                    NS_STATIC_CAST(nsIFormControl*, this));
  }

  return NS_OK;
}

PRBool
nsHTMLInputElement::IsFocusable(PRInt32 *aTabIndex)
{
  if (!nsGenericHTMLElement::IsFocusable(aTabIndex)) {
    return PR_FALSE;
  }

  if (mType == NS_FORM_INPUT_TEXT || mType == NS_FORM_INPUT_PASSWORD) {
    return PR_TRUE;
  }

  if (mType == NS_FORM_INPUT_HIDDEN || mType == NS_FORM_INPUT_FILE) {
    // Sub controls of file input are tabbable, not the file input itself.
    if (aTabIndex) {
      *aTabIndex = -1;
    }
    return PR_FALSE;
  }

  if (!aTabIndex) {
    // The other controls are all focusable
    return PR_TRUE;
  }

  // We need to set tabindex to -1 if we're not tabbable
  if (mType != NS_FORM_INPUT_TEXT && mType != NS_FORM_INPUT_PASSWORD &&
      !(sTabFocusModel & eTabFocus_formElementsMask)) {
    *aTabIndex = -1;
  }

  if (mType != NS_FORM_INPUT_RADIO) {
    return PR_TRUE;
  }

  PRBool checked;
  GetChecked(&checked);
  if (checked) {
    // Selected radio buttons are tabbable
    return PR_TRUE;
  }

  // Current radio button is not selected.
  // But make it tabbable if nothing in group is selected.
  nsCOMPtr<nsIRadioGroupContainer> container = GetRadioGroupContainer();
  nsAutoString name;
  if (!container || !GetNameIfExists(name)) {
    return PR_TRUE;
  }

  nsCOMPtr<nsIDOMHTMLInputElement> currentRadio;
  container->GetCurrentRadioButton(name, getter_AddRefs(currentRadio));
  if (currentRadio) {
    *aTabIndex = -1;
  }
  return PR_TRUE;
}

nsresult
nsHTMLInputElement::VisitGroup(nsIRadioVisitor* aVisitor)
{
  nsresult rv = NS_OK;
  nsCOMPtr<nsIRadioGroupContainer> container = GetRadioGroupContainer();
  if (container) {
    nsAutoString name;
    if (GetNameIfExists(name)) {
      rv = container->WalkRadioGroup(name, aVisitor);
    } else {
      PRBool stop;
      aVisitor->Visit(this, &stop);
    }
  } else {
    PRBool stop;
    aVisitor->Visit(this, &stop);
  }
  return rv;
}


//
// Visitor classes
//
//
// CLASS nsRadioVisitor
//
// (this is the superclass of the others)
//
class nsRadioVisitor : public nsIRadioVisitor {
public:
  nsRadioVisitor() { }
  virtual ~nsRadioVisitor() { };

  NS_DECL_ISUPPORTS

  NS_IMETHOD Visit(nsIFormControl* aRadio, PRBool* aStop) = 0;
};

NS_IMPL_ADDREF(nsRadioVisitor)
NS_IMPL_RELEASE(nsRadioVisitor)

NS_INTERFACE_MAP_BEGIN(nsRadioVisitor)
  NS_INTERFACE_MAP_ENTRY(nsIRadioVisitor)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END


//
// CLASS nsRadioSetCheckedChangedVisitor
//
class nsRadioSetCheckedChangedVisitor : public nsRadioVisitor {
public:
  nsRadioSetCheckedChangedVisitor(PRBool aCheckedChanged) :
    nsRadioVisitor(), mCheckedChanged(aCheckedChanged)
    { }

  virtual ~nsRadioSetCheckedChangedVisitor() { }

  NS_IMETHOD Visit(nsIFormControl* aRadio, PRBool* aStop)
  {
    nsCOMPtr<nsIRadioControlElement> radio(do_QueryInterface(aRadio));
    NS_ASSERTION(radio, "Visit() passed a null button (or non-radio)!");
    radio->SetCheckedChangedInternal(mCheckedChanged);
    return NS_OK;
  }

protected:
  PRPackedBool mCheckedChanged;
};

//
// CLASS nsRadioGetCheckedChangedVisitor
//
class nsRadioGetCheckedChangedVisitor : public nsRadioVisitor {
public:
  nsRadioGetCheckedChangedVisitor(PRBool* aCheckedChanged,
                                  nsIFormControl* aExcludeElement) :
    nsRadioVisitor(),
    mCheckedChanged(aCheckedChanged),
    mExcludeElement(aExcludeElement)
    { }

  virtual ~nsRadioGetCheckedChangedVisitor() { }

  NS_IMETHOD Visit(nsIFormControl* aRadio, PRBool* aStop)
  {
    if (aRadio == mExcludeElement) {
      return NS_OK;
    }
    nsCOMPtr<nsIRadioControlElement> radio(do_QueryInterface(aRadio));
    NS_ASSERTION(radio, "Visit() passed a null button (or non-radio)!");
    radio->GetCheckedChanged(mCheckedChanged);
    *aStop = PR_TRUE;
    return NS_OK;
  }

protected:
  PRBool* mCheckedChanged;
  nsIFormControl* mExcludeElement;
};

nsresult
NS_GetRadioSetCheckedChangedVisitor(PRBool aCheckedChanged,
                                    nsIRadioVisitor** aVisitor)
{
  //
  // These are static so that we don't have to keep creating new visitors for
  // such an ordinary process all the time.  There are only two possibilities
  // for this visitor: set to true, and set to false.
  //
  static nsIRadioVisitor* visitorTrue = nsnull;
  static nsIRadioVisitor* visitorFalse = nsnull;

  //
  // Get the visitor that sets them to true
  //
  if (aCheckedChanged) {
    if (!visitorTrue) {
      visitorTrue = new nsRadioSetCheckedChangedVisitor(PR_TRUE);
      if (!visitorTrue) {
        return NS_ERROR_OUT_OF_MEMORY;
      }
      NS_ADDREF(visitorTrue);
    }
    *aVisitor = visitorTrue;
  }

  //
  // Get the visitor that sets them to false
  //
  if (!aCheckedChanged) {
    if (!visitorFalse) {
      visitorFalse = new nsRadioSetCheckedChangedVisitor(PR_FALSE);
      if (!visitorFalse) {
        return NS_ERROR_OUT_OF_MEMORY;
      }
      NS_ADDREF(visitorFalse);
    }
    *aVisitor = visitorFalse;
  }

  NS_ADDREF(*aVisitor);
  return NS_OK;
}

nsresult
NS_GetRadioGetCheckedChangedVisitor(PRBool* aCheckedChanged,
                                    nsIFormControl* aExcludeElement,
                                    nsIRadioVisitor** aVisitor)
{
  *aVisitor = new nsRadioGetCheckedChangedVisitor(aCheckedChanged,
                                                  aExcludeElement);
  if (!*aVisitor) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  NS_ADDREF(*aVisitor);

  return NS_OK;
}
