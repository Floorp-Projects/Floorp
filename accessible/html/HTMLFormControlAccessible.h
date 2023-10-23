/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_A11Y_HTMLFormControlAccessible_H_
#define MOZILLA_A11Y_HTMLFormControlAccessible_H_

#include "FormControlAccessible.h"
#include "HyperTextAccessible.h"
#include "mozilla/a11y/AccTypes.h"
#include "mozilla/dom/Element.h"
#include "AccAttributes.h"
#include "nsAccUtils.h"
#include "Relation.h"

namespace mozilla {
class EditorBase;
namespace a11y {

/**
 * Accessible for HTML input@type="radio" element.
 */
class HTMLRadioButtonAccessible : public RadioButtonAccessible {
 public:
  HTMLRadioButtonAccessible(nsIContent* aContent, DocAccessible* aDoc)
      : RadioButtonAccessible(aContent, aDoc) {
    // Ignore "RadioStateChange" DOM event in lieu of document observer
    // state change notification.
    mStateFlags |= eIgnoreDOMUIEvent;
    mType = eHTMLRadioButtonType;
  }

  // LocalAccessible
  virtual uint64_t NativeState() const override;
  virtual Relation RelationByType(RelationType aType) const override;

 protected:
  virtual void GetPositionAndSetSize(int32_t* aPosInSet,
                                     int32_t* aSetSize) override;

  virtual void DOMAttributeChanged(int32_t aNameSpaceID, nsAtom* aAttribute,
                                   int32_t aModType,
                                   const nsAttrValue* aOldValue,
                                   uint64_t aOldState) override;

 private:
  Relation ComputeGroupAttributes(int32_t* aPosInSet, int32_t* aSetSize) const;
};

/**
 * Accessible for HTML input@type="button", @type="submit", @type="image"
 * and HTML button elements.
 */
class HTMLButtonAccessible : public HyperTextAccessible {
 public:
  enum { eAction_Click = 0 };

  HTMLButtonAccessible(nsIContent* aContent, DocAccessible* aDoc);

  // LocalAccessible
  virtual mozilla::a11y::role NativeRole() const override;
  virtual uint64_t NativeState() const override;

  // ActionAccessible
  virtual bool HasPrimaryAction() const override;
  virtual void ActionNameAt(uint8_t aIndex, nsAString& aName) override;

  // Widgets
  virtual bool IsWidget() const override;

 protected:
  // LocalAccessible
  virtual ENameValueFlag NativeName(nsString& aName) const override;

  virtual void DOMAttributeChanged(int32_t aNameSpaceID, nsAtom* aAttribute,
                                   int32_t aModType,
                                   const nsAttrValue* aOldValue,
                                   uint64_t aOldState) override;
};

/**
 * Accessible for HTML input@type="text", input@type="password", textarea
 * and other HTML text controls.
 */
class HTMLTextFieldAccessible : public HyperTextAccessible {
 public:
  enum { eAction_Click = 0 };

  HTMLTextFieldAccessible(nsIContent* aContent, DocAccessible* aDoc);

  NS_INLINE_DECL_REFCOUNTING_INHERITED(HTMLTextFieldAccessible,
                                       HyperTextAccessible)

  // HyperTextAccessible
  MOZ_CAN_RUN_SCRIPT_BOUNDARY virtual already_AddRefed<EditorBase> GetEditor()
      const override;

  // LocalAccessible
  virtual void Value(nsString& aValue) const override;
  virtual void ApplyARIAState(uint64_t* aState) const override;
  virtual mozilla::a11y::role NativeRole() const override;
  virtual uint64_t NativeState() const override;
  virtual already_AddRefed<AccAttributes> NativeAttributes() override;
  virtual bool AttributeChangesState(nsAtom* aAttribute) override;

  // ActionAccessible
  virtual bool HasPrimaryAction() const override;
  virtual void ActionNameAt(uint8_t aIndex, nsAString& aName) override;
  virtual bool DoAction(uint8_t aIndex) const override;

  // Widgets
  virtual bool IsWidget() const override;
  virtual LocalAccessible* ContainerWidget() const override;

 protected:
  virtual ~HTMLTextFieldAccessible() {}

  // LocalAccessible
  virtual ENameValueFlag NativeName(nsString& aName) const override;

  virtual void DOMAttributeChanged(int32_t aNameSpaceID, nsAtom* aAttribute,
                                   int32_t aModType,
                                   const nsAttrValue* aOldValue,
                                   uint64_t aOldState) override;
};

/**
 * Accessible for input@type="file" element.
 */
class HTMLFileInputAccessible : public HyperTextAccessible {
 public:
  HTMLFileInputAccessible(nsIContent* aContent, DocAccessible* aDoc);

  // LocalAccessible
  virtual mozilla::a11y::role NativeRole() const override;
  virtual bool IsAcceptableChild(nsIContent* aEl) const override;
  virtual ENameValueFlag Name(nsString& aName) const override;
  virtual bool HasPrimaryAction() const override;
  virtual void ActionNameAt(uint8_t aIndex, nsAString& aName) override;
  virtual bool IsWidget() const override;
};

/**
 * Used for HTML input@type="number".
 */
class HTMLSpinnerAccessible final : public HTMLTextFieldAccessible {
 public:
  HTMLSpinnerAccessible(nsIContent* aContent, DocAccessible* aDoc)
      : HTMLTextFieldAccessible(aContent, aDoc) {
    mGenericTypes |= eNumericValue;
  }

  // LocalAccessible
  virtual mozilla::a11y::role NativeRole() const override;
  virtual void Value(nsString& aValue) const override;

  virtual double MaxValue() const override;
  virtual double MinValue() const override;
  virtual double CurValue() const override;
  virtual double Step() const override;
  virtual bool SetCurValue(double aValue) override;
};

/**
 * Used for input@type="range" element.
 */
class HTMLRangeAccessible : public LeafAccessible {
 public:
  HTMLRangeAccessible(nsIContent* aContent, DocAccessible* aDoc)
      : LeafAccessible(aContent, aDoc) {
    mGenericTypes |= eNumericValue;
  }

  // LocalAccessible
  virtual void Value(nsString& aValue) const override;
  virtual mozilla::a11y::role NativeRole() const override;

  // Value
  virtual double MaxValue() const override;
  virtual double MinValue() const override;
  virtual double CurValue() const override;
  virtual double Step() const override;
  virtual bool SetCurValue(double aValue) override;

  // Widgets
  virtual bool IsWidget() const override;
};

/**
 * Accessible for HTML fieldset element.
 */
class HTMLGroupboxAccessible : public HyperTextAccessible {
 public:
  HTMLGroupboxAccessible(nsIContent* aContent, DocAccessible* aDoc);

  // LocalAccessible
  virtual mozilla::a11y::role NativeRole() const override;
  virtual Relation RelationByType(RelationType aType) const override;

 protected:
  // LocalAccessible
  virtual ENameValueFlag NativeName(nsString& aName) const override;

  // HTMLGroupboxAccessible
  nsIContent* GetLegend() const;
};

/**
 * Accessible for HTML legend element.
 */
class HTMLLegendAccessible : public HyperTextAccessible {
 public:
  HTMLLegendAccessible(nsIContent* aContent, DocAccessible* aDoc);

  // LocalAccessible
  virtual Relation RelationByType(RelationType aType) const override;
};

/**
 * Accessible for HTML5 figure element.
 */
class HTMLFigureAccessible : public HyperTextAccessible {
 public:
  HTMLFigureAccessible(nsIContent* aContent, DocAccessible* aDoc);

  // LocalAccessible
  virtual Relation RelationByType(RelationType aType) const override;

 protected:
  // LocalAccessible
  virtual ENameValueFlag NativeName(nsString& aName) const override;

  // HTMLLegendAccessible
  nsIContent* Caption() const;
};

/**
 * Accessible for HTML5 figcaption element.
 */
class HTMLFigcaptionAccessible : public HyperTextAccessible {
 public:
  HTMLFigcaptionAccessible(nsIContent* aContent, DocAccessible* aDoc);

  // LocalAccessible
  virtual Relation RelationByType(RelationType aType) const override;
};

/**
 * Used for HTML form element.
 */
class HTMLFormAccessible : public HyperTextAccessible {
 public:
  HTMLFormAccessible(nsIContent* aContent, DocAccessible* aDoc)
      : HyperTextAccessible(aContent, aDoc) {}

  NS_INLINE_DECL_REFCOUNTING_INHERITED(HTMLFormAccessible, HyperTextAccessible)

  // LocalAccessible
  virtual a11y::role NativeRole() const override;

 protected:
  virtual void DOMAttributeChanged(int32_t aNameSpaceID, nsAtom* aAttribute,
                                   int32_t aModType,
                                   const nsAttrValue* aOldValue,
                                   uint64_t aOldState) override;

  virtual ~HTMLFormAccessible() = default;
};

/**
 * Accessible for HTML progress element.
 */

class HTMLProgressAccessible : public LeafAccessible {
 public:
  HTMLProgressAccessible(nsIContent* aContent, DocAccessible* aDoc)
      : LeafAccessible(aContent, aDoc) {
    // Ignore 'ValueChange' DOM event in lieu of @value attribute change
    // notifications.
    mStateFlags |= eIgnoreDOMUIEvent;
    mGenericTypes |= eNumericValue;
    mType = eProgressType;
  }

  // LocalAccessible
  virtual void Value(nsString& aValue) const override;
  virtual mozilla::a11y::role NativeRole() const override;
  virtual uint64_t NativeState() const override;

  // Value
  virtual double MaxValue() const override;
  virtual double MinValue() const override;
  virtual double CurValue() const override;
  virtual double Step() const override;
  virtual bool SetCurValue(double aValue) override;

  // Widgets
  virtual bool IsWidget() const override;

 protected:
  virtual ~HTMLProgressAccessible() {}

  virtual void DOMAttributeChanged(int32_t aNameSpaceID, nsAtom* aAttribute,
                                   int32_t aModType,
                                   const nsAttrValue* aOldValue,
                                   uint64_t aOldState) override;
};

/**
 * Accessible for HTML meter element.
 */

class HTMLMeterAccessible : public LeafAccessible {
 public:
  HTMLMeterAccessible(nsIContent* aContent, DocAccessible* aDoc)
      : LeafAccessible(aContent, aDoc) {
    // Ignore 'ValueChange' DOM event in lieu of @value attribute change
    // notifications.
    mStateFlags |= eIgnoreDOMUIEvent;
    mGenericTypes |= eNumericValue;
    mType = eProgressType;
  }

  // LocalAccessible
  virtual void Value(nsString& aValue) const override;
  virtual mozilla::a11y::role NativeRole() const override;

  // Value
  virtual double MaxValue() const override;
  virtual double MinValue() const override;
  virtual double CurValue() const override;
  virtual bool SetCurValue(double aValue) override;

  // Widgets
  virtual bool IsWidget() const override;

 protected:
  virtual ~HTMLMeterAccessible() {}

  virtual void DOMAttributeChanged(int32_t aNameSpaceID, nsAtom* aAttribute,
                                   int32_t aModType,
                                   const nsAttrValue* aOldValue,
                                   uint64_t aOldState) override;
};

/**
 * Accessible for HTML date/time inputs.
 */
template <a11y::role R>
class HTMLDateTimeAccessible : public HyperTextAccessible {
 public:
  HTMLDateTimeAccessible(nsIContent* aContent, DocAccessible* aDoc)
      : HyperTextAccessible(aContent, aDoc) {
    mType = eHTMLDateTimeFieldType;
  }

  NS_INLINE_DECL_REFCOUNTING_INHERITED(HTMLDateTimeAccessible,
                                       HyperTextAccessible)

  // LocalAccessible
  virtual mozilla::a11y::role NativeRole() const override { return R; }
  virtual already_AddRefed<AccAttributes> NativeAttributes() override {
    RefPtr<AccAttributes> attributes = HyperTextAccessible::NativeAttributes();
    // Unfortunately, an nsStaticAtom can't be passed as a
    // template argument, so fetch the type from the DOM.
    if (const nsAttrValue* attr =
            mContent->AsElement()->GetParsedAttr(nsGkAtoms::type)) {
      RefPtr<nsAtom> inputType = attr->GetAsAtom();
      if (inputType) {
        attributes->SetAttribute(nsGkAtoms::textInputType, inputType);
      }
    }
    return attributes.forget();
  }

  // Widgets
  virtual bool IsWidget() const override { return true; }

 protected:
  virtual ~HTMLDateTimeAccessible() {}
};

}  // namespace a11y
}  // namespace mozilla

#endif
