/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_A11Y_HTMLFormControlAccessible_H_
#define MOZILLA_A11Y_HTMLFormControlAccessible_H_

#include "FormControlAccessible.h"
#include "HyperTextAccessibleWrap.h"

namespace mozilla {
class TextEditor;
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
  }

  // Accessible
  virtual uint64_t NativeState() const override;
  virtual void GetPositionAndSizeInternal(int32_t* aPosInSet,
                                          int32_t* aSetSize) override;
};

/**
 * Accessible for HTML input@type="button", @type="submit", @type="image"
 * and HTML button elements.
 */
class HTMLButtonAccessible : public HyperTextAccessibleWrap {
 public:
  enum { eAction_Click = 0 };

  HTMLButtonAccessible(nsIContent* aContent, DocAccessible* aDoc);

  // Accessible
  virtual mozilla::a11y::role NativeRole() const override;
  virtual uint64_t State() override;
  virtual uint64_t NativeState() const override;

  // ActionAccessible
  virtual uint8_t ActionCount() const override;
  virtual void ActionNameAt(uint8_t aIndex, nsAString& aName) override;
  virtual bool DoAction(uint8_t aIndex) const override;

  // Widgets
  virtual bool IsWidget() const override;

 protected:
  // Accessible
  virtual ENameValueFlag NativeName(nsString& aName) const override;
};

/**
 * Accessible for HTML input@type="text", input@type="password", textarea and
 * other HTML text controls.
 */
class HTMLTextFieldAccessible final : public HyperTextAccessibleWrap {
 public:
  enum { eAction_Click = 0 };

  HTMLTextFieldAccessible(nsIContent* aContent, DocAccessible* aDoc);

  NS_INLINE_DECL_REFCOUNTING_INHERITED(HTMLTextFieldAccessible,
                                       HyperTextAccessibleWrap)

  // HyperTextAccessible
  virtual already_AddRefed<TextEditor> GetEditor() const override;

  // Accessible
  virtual void Value(nsString& aValue) const override;
  virtual void ApplyARIAState(uint64_t* aState) const override;
  virtual mozilla::a11y::role NativeRole() const override;
  virtual uint64_t NativeState() const override;
  virtual already_AddRefed<nsIPersistentProperties> NativeAttributes() override;

  // ActionAccessible
  virtual uint8_t ActionCount() const override;
  virtual void ActionNameAt(uint8_t aIndex, nsAString& aName) override;
  virtual bool DoAction(uint8_t aIndex) const override;

  // Widgets
  virtual bool IsWidget() const override;
  virtual Accessible* ContainerWidget() const override;

 protected:
  virtual ~HTMLTextFieldAccessible() {}

  // Accessible
  virtual ENameValueFlag NativeName(nsString& aName) const override;

  /**
   * Return a widget element this input is part of, for example, XUL:textbox or
   * HTML:input@type="number".
   */
  nsIContent* BindingOrWidgetParent() const {
    nsIContent * el = mContent->GetBindingParent();
    if (el) {
      return el;
    }
    // XUL textboxes custom elements implementation.
    ErrorResult rv;
    return Elm()->Closest(NS_LITERAL_STRING("textbox"), rv);
  }
};

/**
 * Accessible for input@type="file" element.
 */
class HTMLFileInputAccessible : public HyperTextAccessibleWrap {
 public:
  HTMLFileInputAccessible(nsIContent* aContent, DocAccessible* aDoc);

  // Accessible
  virtual mozilla::a11y::role NativeRole() const override;
  virtual nsresult HandleAccEvent(AccEvent* aAccEvent) override;
};

/**
 * Used for HTML input@type="number".
 */
class HTMLSpinnerAccessible : public AccessibleWrap {
 public:
  HTMLSpinnerAccessible(nsIContent* aContent, DocAccessible* aDoc)
      : AccessibleWrap(aContent, aDoc) {
    mStateFlags |= eHasNumericValue;
  }

  // Accessible
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
    mStateFlags |= eHasNumericValue;
  }

  // Accessible
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
class HTMLGroupboxAccessible : public HyperTextAccessibleWrap {
 public:
  HTMLGroupboxAccessible(nsIContent* aContent, DocAccessible* aDoc);

  // Accessible
  virtual mozilla::a11y::role NativeRole() const override;
  virtual Relation RelationByType(RelationType aType) const override;

 protected:
  // Accessible
  virtual ENameValueFlag NativeName(nsString& aName) const override;

  // HTMLGroupboxAccessible
  nsIContent* GetLegend() const;
};

/**
 * Accessible for HTML legend element.
 */
class HTMLLegendAccessible : public HyperTextAccessibleWrap {
 public:
  HTMLLegendAccessible(nsIContent* aContent, DocAccessible* aDoc);

  // Accessible
  virtual Relation RelationByType(RelationType aType) const override;
};

/**
 * Accessible for HTML5 figure element.
 */
class HTMLFigureAccessible : public HyperTextAccessibleWrap {
 public:
  HTMLFigureAccessible(nsIContent* aContent, DocAccessible* aDoc);

  // Accessible
  virtual Relation RelationByType(RelationType aType) const override;

 protected:
  // Accessible
  virtual ENameValueFlag NativeName(nsString& aName) const override;

  // HTMLLegendAccessible
  nsIContent* Caption() const;
};

/**
 * Accessible for HTML5 figcaption element.
 */
class HTMLFigcaptionAccessible : public HyperTextAccessibleWrap {
 public:
  HTMLFigcaptionAccessible(nsIContent* aContent, DocAccessible* aDoc);

  // Accessible
  virtual Relation RelationByType(RelationType aType) const override;
};

/**
 * Used for HTML form element.
 */
class HTMLFormAccessible : public HyperTextAccessibleWrap {
 public:
  HTMLFormAccessible(nsIContent* aContent, DocAccessible* aDoc)
      : HyperTextAccessibleWrap(aContent, aDoc) {}

  NS_INLINE_DECL_REFCOUNTING_INHERITED(HTMLFormAccessible,
                                       HyperTextAccessibleWrap)

  // Accessible
  virtual nsAtom* LandmarkRole() const override;
  virtual a11y::role NativeRole() const override;

 protected:
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
    mStateFlags |= eHasNumericValue | eIgnoreDOMUIEvent;
    mType = eProgressType;
  }

  // Accessible
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
};

}  // namespace a11y
}  // namespace mozilla

#endif
