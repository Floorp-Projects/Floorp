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
class FormData;
class HTMLFieldSetElement;
class HTMLFormElement;
}  // namespace dom
}  // namespace mozilla

// Elements with different types, the value is used as a mask.
// When changing the order, adding or removing elements, be sure to update
// the static_assert checks accordingly.
constexpr uint8_t kFormControlButtonElementMask = 0x40;  // 0b01000000
constexpr uint8_t kFormControlInputElementMask = 0x80;   // 0b10000000

enum class FormControlType : uint8_t {
  Fieldset = 1,
  Output,
  Select,
  Textarea,
  Object,

  LastWithoutSubtypes = Object,

  ButtonButton = kFormControlButtonElementMask + 1,
  ButtonReset,
  ButtonSubmit,
  LastButtonElement = ButtonSubmit,

  InputButton = kFormControlInputElementMask + 1,
  InputCheckbox,
  InputColor,
  InputDate,
  InputEmail,
  InputFile,
  InputHidden,
  InputReset,
  InputImage,
  InputMonth,
  InputNumber,
  InputPassword,
  InputRadio,
  InputSearch,
  InputSubmit,
  InputTel,
  InputText,
  InputTime,
  InputUrl,
  InputRange,
  InputWeek,
  InputDatetimeLocal,
  LastInputElement = InputDatetimeLocal,
};

static_assert(uint8_t(FormControlType::LastWithoutSubtypes) <
                  kFormControlButtonElementMask,
              "Too many FormControlsTypes without sub-types");
static_assert(uint8_t(FormControlType::LastButtonElement) <
                  kFormControlInputElementMask,
              "Too many ButtonElementTypes");
static_assert(uint32_t(FormControlType::LastInputElement) < (1 << 8),
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
  nsIFormControl(FormControlType aType) : mType(aType) {}

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
  virtual mozilla::dom::HTMLFormElement* GetForm() const = 0;

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
  FormControlType ControlType() const { return mType; }

  /**
   * Reset this form control (as it should be when the user clicks the Reset
   * button)
   */
  NS_IMETHOD Reset() = 0;

  /**
   * Tells the form control to submit its names and values to the form data
   * object
   *
   * @param aFormData the form data to notify of names/values/files to submit
   */
  NS_IMETHOD
  SubmitNamesValues(mozilla::dom::FormData* aFormData) = 0;

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
                                             FormControlType);

  inline static bool IsButtonElement(FormControlType aType) {
    return uint8_t(aType) & kFormControlButtonElementMask;
  }

  inline static bool IsInputElement(FormControlType aType) {
    return uint8_t(aType) & kFormControlInputElementMask;
  }

  /**
   * Returns whether this is a auto-focusable form control.
   * @return whether this is a auto-focusable form control.
   */
  inline bool IsAutofocusable() const;

  FormControlType mType;
};

bool nsIFormControl::IsSubmitControl() const {
  FormControlType type = ControlType();
  return type == FormControlType::InputSubmit ||
         type == FormControlType::InputImage ||
         type == FormControlType::ButtonSubmit;
}

bool nsIFormControl::IsTextControl(bool aExcludePassword) const {
  FormControlType type = ControlType();
  return type == FormControlType::Textarea ||
         IsSingleLineTextControl(aExcludePassword, type);
}

bool nsIFormControl::IsSingleLineTextControl(bool aExcludePassword) const {
  return IsSingleLineTextControl(aExcludePassword, ControlType());
}

/*static*/
bool nsIFormControl::IsSingleLineTextControl(bool aExcludePassword,
                                             FormControlType aType) {
  switch (aType) {
    case FormControlType::InputText:
    case FormControlType::InputEmail:
    case FormControlType::InputSearch:
    case FormControlType::InputTel:
    case FormControlType::InputUrl:
    case FormControlType::InputNumber:
    // TODO: those are temporary until bug 773205 is fixed.
    case FormControlType::InputMonth:
    case FormControlType::InputWeek:
      return true;
    case FormControlType::InputDatetimeLocal:
      return !mozilla::StaticPrefs::dom_forms_datetime_local_widget();
    case FormControlType::InputPassword:
      return !aExcludePassword;
    default:
      return false;
  }
}

bool nsIFormControl::IsSubmittableControl() const {
  auto type = ControlType();
  return type == FormControlType::Object || type == FormControlType::Textarea ||
         type == FormControlType::Select || IsButtonElement(type) ||
         IsInputElement(type);
}

bool nsIFormControl::AllowDraggableChildren() const {
  auto type = ControlType();
  return type == FormControlType::Object || type == FormControlType::Fieldset ||
         type == FormControlType::Output;
}

bool nsIFormControl::IsAutofocusable() const {
  auto type = ControlType();
  return IsInputElement(type) || IsButtonElement(type) ||
         type == FormControlType::Textarea || type == FormControlType::Select;
}

NS_DEFINE_STATIC_IID_ACCESSOR(nsIFormControl, NS_IFORMCONTROL_IID)

#endif /* nsIFormControl_h___ */
