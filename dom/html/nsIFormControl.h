/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef nsIFormControl_h___
#define nsIFormControl_h___

#include "mozilla/EventForwards.h"
#include "mozilla/StaticPrefs_dom.h"
#include "nsISupports.h"

namespace mozilla {
class PresState;
namespace dom {
class Element;
class HTMLFieldSetElement;
class HTMLFormSubmission;
class DialogFormSubmission;
class HTMLFormElement;
}  // namespace dom
}  // namespace mozilla

enum FormControlsTypes {
  NS_FORM_FIELDSET = 1,
  NS_FORM_OUTPUT,
  NS_FORM_SELECT,
  NS_FORM_TEXTAREA,
  NS_FORM_OBJECT,
  eFormControlsWithoutSubTypesMax,
  // After this, all types will have sub-types which introduce new enum lists.
  // eFormControlsWithoutSubTypesMax let us know if the previous types values
  // are not overlapping with sub-types/masks.

  // Elements with different types, the value is used as a mask.
  // When changing the order, adding or removing elements, be sure to update
  // the static_assert checks accordingly.
  NS_FORM_BUTTON_ELEMENT = 0x40,  // 0b01000000
  NS_FORM_INPUT_ELEMENT = 0x80    // 0b10000000
};

enum ButtonElementTypes : uint8_t {
  NS_FORM_BUTTON_BUTTON = NS_FORM_BUTTON_ELEMENT + 1,
  NS_FORM_BUTTON_RESET,
  NS_FORM_BUTTON_SUBMIT,
  eButtonElementTypesMax
};

enum InputElementTypes : uint8_t {
  NS_FORM_INPUT_BUTTON = NS_FORM_INPUT_ELEMENT + 1,
  NS_FORM_INPUT_CHECKBOX,
  NS_FORM_INPUT_COLOR,
  NS_FORM_INPUT_DATE,
  NS_FORM_INPUT_EMAIL,
  NS_FORM_INPUT_FILE,
  NS_FORM_INPUT_HIDDEN,
  NS_FORM_INPUT_RESET,
  NS_FORM_INPUT_IMAGE,
  NS_FORM_INPUT_MONTH,
  NS_FORM_INPUT_NUMBER,
  NS_FORM_INPUT_PASSWORD,
  NS_FORM_INPUT_RADIO,
  NS_FORM_INPUT_SEARCH,
  NS_FORM_INPUT_SUBMIT,
  NS_FORM_INPUT_TEL,
  NS_FORM_INPUT_TEXT,
  NS_FORM_INPUT_TIME,
  NS_FORM_INPUT_URL,
  NS_FORM_INPUT_RANGE,
  NS_FORM_INPUT_WEEK,
  NS_FORM_INPUT_DATETIME_LOCAL,
  eInputElementTypesMax
};

static_assert(static_cast<uint32_t>(eFormControlsWithoutSubTypesMax) <
                  static_cast<uint32_t>(NS_FORM_BUTTON_ELEMENT),
              "Too many FormControlsTypes without sub-types");
static_assert(static_cast<uint32_t>(eButtonElementTypesMax) <
                  static_cast<uint32_t>(NS_FORM_INPUT_ELEMENT),
              "Too many ButtonElementTypes");
static_assert(static_cast<uint32_t>(eInputElementTypesMax) < 1 << 8,
              "Too many form control types");

#define NS_IFORMCONTROL_IID                          \
  {                                                  \
    0x4b89980c, 0x4dcd, 0x428f, {                    \
      0xb7, 0xad, 0x43, 0x5b, 0x93, 0x29, 0x79, 0xec \
    }                                                \
  }

/**
 * Interface which all form controls (e.g. buttons, checkboxes, text,
 * radio buttons, select, etc) implement in addition to their dom specific
 * interface.
 */
class nsIFormControl : public nsISupports {
 public:
  nsIFormControl(uint8_t aType) : mType(aType) {}

  NS_DECLARE_STATIC_IID_ACCESSOR(NS_IFORMCONTROL_IID)

  /**
   * Get the fieldset for this form control.
   * @return the fieldset
   */
  virtual mozilla::dom::HTMLFieldSetElement* GetFieldSet() = 0;

  /**
   * Get the form for this form control.
   * @return the form
   */
  virtual mozilla::dom::HTMLFormElement* GetFormElement() = 0;

  /**
   * Set the form for this form control.
   * @param aForm the form.  This must not be null.
   *
   * @note that when setting the form the control is not added to the
   * form.  It adds itself when it gets bound to the tree thereafter,
   * so that it can be properly sorted with the other controls in the
   * form.
   */
  virtual void SetForm(mozilla::dom::HTMLFormElement* aForm) = 0;

  /**
   * Tell the control to forget about its form.
   *
   * @param aRemoveFromForm set false if you do not want this element removed
   *        from the form.  (Used by nsFormControlList::Clear())
   * @param aUnbindOrDelete set true if the element is being deleted or unbound
   *        from tree.
   */
  virtual void ClearForm(bool aRemoveFromForm, bool aUnbindOrDelete) = 0;

  /**
   * Get the type of this control as an int (see NS_FORM_* above)
   * @return the type of this control
   */
  uint32_t ControlType() const { return mType; }

  /**
   * Reset this form control (as it should be when the user clicks the Reset
   * button)
   */
  NS_IMETHOD Reset() = 0;

  /**
   * Tells the form control to submit its names and values to the form
   * submission object
   * @param aFormSubmission the form submission to notify of names/values/files
   *                       to submit
   */
  NS_IMETHOD
  SubmitNamesValues(mozilla::dom::HTMLFormSubmission* aFormSubmission) = 0;

  /**
   * Save to presentation state.  The form control will determine whether it
   * has anything to save and if so, create an entry in the layout history for
   * its pres context.
   */
  NS_IMETHOD SaveState() = 0;

  /**
   * Restore from presentation state.  You pass in the presentation state for
   * this form control (generated with GenerateStateKey() + "-C") and the form
   * control will grab its state from there.
   *
   * @param aState the pres state to use to restore the control
   * @return true if the form control was a checkbox and its
   *         checked state was restored, false otherwise.
   */
  virtual bool RestoreState(mozilla::PresState* aState) = 0;

  virtual bool AllowDrop() = 0;

  /**
   * Returns whether this is a control which submits the form when activated by
   * the user.
   * @return whether this is a submit control.
   */
  inline bool IsSubmitControl() const;

  /**
   * Returns whether this is a text control.
   * @param  aExcludePassword  to have NS_FORM_INPUT_PASSWORD returning false.
   * @return whether this is a text control.
   */
  inline bool IsTextControl(bool aExcludePassword) const;

  /**
   * Returns whether this is a single line text control.
   * @param  aExcludePassword  to have NS_FORM_INPUT_PASSWORD returning false.
   * @return whether this is a single line text control.
   */
  inline bool IsSingleLineTextControl(bool aExcludePassword) const;

  /**
   * Returns whether this is a submittable form control.
   * @return whether this is a submittable form control.
   */
  inline bool IsSubmittableControl() const;

  /**
   * Returns whether this form control can have draggable children.
   * @return whether this form control can have draggable children.
   */
  inline bool AllowDraggableChildren() const;

  virtual bool IsDisabledForEvents(mozilla::WidgetEvent* aEvent) {
    return false;
  }

  // Returns a number for this form control that is unique within its
  // owner document.  This is used by nsContentUtils::GenerateStateKey
  // to identify form controls that are inserted into the document by
  // the parser.  -1 is returned for form controls with no state or
  // which were inserted into the document by some other means than
  // the parser from the network.
  virtual int32_t GetParserInsertedControlNumberForStateKey() const {
    return -1;
  };

 protected:
  /**
   * Returns whether mType corresponds to a single line text control type.
   * @param aExcludePassword to have NS_FORM_INPUT_PASSWORD ignored.
   * @param aType the type to be tested.
   * @return whether mType corresponds to a single line text control type.
   */
  inline static bool IsSingleLineTextControl(bool aExcludePassword,
                                             uint32_t aType);

  /**
   * Returns whether this is a auto-focusable form control.
   * @return whether this is a auto-focusable form control.
   */
  inline bool IsAutofocusable() const;

  uint8_t mType;
};

bool nsIFormControl::IsSubmitControl() const {
  uint32_t type = ControlType();
  return type == NS_FORM_INPUT_SUBMIT || type == NS_FORM_INPUT_IMAGE ||
         type == NS_FORM_BUTTON_SUBMIT;
}

bool nsIFormControl::IsTextControl(bool aExcludePassword) const {
  uint32_t type = ControlType();
  return type == NS_FORM_TEXTAREA ||
         IsSingleLineTextControl(aExcludePassword, type);
}

bool nsIFormControl::IsSingleLineTextControl(bool aExcludePassword) const {
  return IsSingleLineTextControl(aExcludePassword, ControlType());
}

/*static*/
bool nsIFormControl::IsSingleLineTextControl(bool aExcludePassword,
                                             uint32_t aType) {
  return aType == NS_FORM_INPUT_TEXT || aType == NS_FORM_INPUT_EMAIL ||
         aType == NS_FORM_INPUT_SEARCH || aType == NS_FORM_INPUT_TEL ||
         aType == NS_FORM_INPUT_URL || aType == NS_FORM_INPUT_NUMBER ||
         // TODO: those are temporary until bug 773205 is fixed.
         aType == NS_FORM_INPUT_MONTH || aType == NS_FORM_INPUT_WEEK ||
         (aType == NS_FORM_INPUT_DATETIME_LOCAL &&
          !mozilla::StaticPrefs::dom_forms_datetime_local_widget()) ||
         (!aExcludePassword && aType == NS_FORM_INPUT_PASSWORD);
}

bool nsIFormControl::IsSubmittableControl() const {
  uint32_t type = ControlType();
  return type == NS_FORM_OBJECT || type == NS_FORM_TEXTAREA ||
         type == NS_FORM_SELECT || type & NS_FORM_BUTTON_ELEMENT ||
         type & NS_FORM_INPUT_ELEMENT;
}

bool nsIFormControl::AllowDraggableChildren() const {
  uint32_t type = ControlType();
  return type == NS_FORM_OBJECT || type == NS_FORM_FIELDSET ||
         type == NS_FORM_OUTPUT;
}

bool nsIFormControl::IsAutofocusable() const {
  uint32_t type = ControlType();
  return type & NS_FORM_INPUT_ELEMENT || type & NS_FORM_BUTTON_ELEMENT ||
         type == NS_FORM_TEXTAREA || type == NS_FORM_SELECT;
}

NS_DEFINE_STATIC_IID_ACCESSOR(nsIFormControl, NS_IFORMCONTROL_IID)

#endif /* nsIFormControl_h___ */
