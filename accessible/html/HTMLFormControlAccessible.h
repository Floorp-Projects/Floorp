/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_A11Y_HTMLFormControlAccessible_H_
#define MOZILLA_A11Y_HTMLFormControlAccessible_H_

#include "FormControlAccessible.h"
#include "HyperTextAccessibleWrap.h"

namespace mozilla {
namespace a11y {

/**
 * Accessible for HTML progress element.
 */
typedef ProgressMeterAccessible<1> HTMLProgressMeterAccessible;

/**
 * Accessible for HTML input@type="checkbox".
 */
class HTMLCheckboxAccessible : public LeafAccessible
{

public:
  enum { eAction_Click = 0 };

  HTMLCheckboxAccessible(nsIContent* aContent, DocAccessible* aDoc) :
    LeafAccessible(aContent, aDoc)
  {
    // Ignore "CheckboxStateChange" DOM event in lieu of document observer
    // state change notification.
    mStateFlags |= eIgnoreDOMUIEvent;
  }

  // Accessible
  virtual mozilla::a11y::role NativeRole() override;
  virtual uint64_t NativeState() override;

  // ActionAccessible
  virtual uint8_t ActionCount() override;
  virtual void ActionNameAt(uint8_t aIndex, nsAString& aName) override;
  virtual bool DoAction(uint8_t aIndex) override;

  // Widgets
  virtual bool IsWidget() const override;
};


/**
 * Accessible for HTML input@type="radio" element.
 */
class HTMLRadioButtonAccessible : public RadioButtonAccessible
{

public:
  HTMLRadioButtonAccessible(nsIContent* aContent, DocAccessible* aDoc) :
    RadioButtonAccessible(aContent, aDoc)
  {
    // Ignore "RadioStateChange" DOM event in lieu of document observer
    // state change notification.
    mStateFlags |= eIgnoreDOMUIEvent;
  }

  // Accessible
  virtual uint64_t NativeState() override;
  virtual void GetPositionAndSizeInternal(int32_t *aPosInSet,
                                          int32_t *aSetSize) override;
};


/**
 * Accessible for HTML input@type="button", @type="submit", @type="image"
 * and HTML button elements.
 */
class HTMLButtonAccessible : public HyperTextAccessibleWrap
{

public:
  enum { eAction_Click = 0 };

  HTMLButtonAccessible(nsIContent* aContent, DocAccessible* aDoc);

  // Accessible
  virtual mozilla::a11y::role NativeRole() override;
  virtual uint64_t State() override;
  virtual uint64_t NativeState() override;

  // ActionAccessible
  virtual uint8_t ActionCount() override;
  virtual void ActionNameAt(uint8_t aIndex, nsAString& aName) override;
  virtual bool DoAction(uint8_t aIndex) override;

  // Widgets
  virtual bool IsWidget() const override;

protected:
  // Accessible
  virtual ENameValueFlag NativeName(nsString& aName) override;
};


/**
 * Accessible for HTML input@type="text", input@type="password", textarea and
 * other HTML text controls.
 */
class HTMLTextFieldAccessible final : public HyperTextAccessibleWrap
{

public:
  enum { eAction_Click = 0 };

  HTMLTextFieldAccessible(nsIContent* aContent, DocAccessible* aDoc);

  NS_DECL_ISUPPORTS_INHERITED

  // HyperTextAccessible
  virtual already_AddRefed<nsIEditor> GetEditor() const override;

  // Accessible
  virtual void Value(nsString& aValue) override;
  virtual void ApplyARIAState(uint64_t* aState) const override;
  virtual mozilla::a11y::role NativeRole() override;
  virtual uint64_t NativeState() override;
  virtual already_AddRefed<nsIPersistentProperties> NativeAttributes() override;

  // ActionAccessible
  virtual uint8_t ActionCount() override;
  virtual void ActionNameAt(uint8_t aIndex, nsAString& aName) override;
  virtual bool DoAction(uint8_t aIndex) override;

  // Widgets
  virtual bool IsWidget() const override;
  virtual Accessible* ContainerWidget() const override;

protected:
  virtual ~HTMLTextFieldAccessible() {}

  // Accessible
  virtual ENameValueFlag NativeName(nsString& aName) override;

  /**
   * Return a XUL widget element this input is part of.
   */
  nsIContent* XULWidgetElm() const { return mContent->GetBindingParent(); }
};


/**
 * Accessible for input@type="file" element.
 */
class HTMLFileInputAccessible : public HyperTextAccessibleWrap
{
public:
  HTMLFileInputAccessible(nsIContent* aContent, DocAccessible* aDoc);

  // Accessible
  virtual mozilla::a11y::role NativeRole() override;
  virtual nsresult HandleAccEvent(AccEvent* aAccEvent) override;
};


/**
 * Used for HTML input@type="number".
 */
class HTMLSpinnerAccessible : public AccessibleWrap
{
public:
  HTMLSpinnerAccessible(nsIContent* aContent, DocAccessible* aDoc) :
    AccessibleWrap(aContent, aDoc)
  {
    mStateFlags |= eHasNumericValue;
}

  // Accessible
  virtual mozilla::a11y::role NativeRole() override;
  virtual void Value(nsString& aValue) override;

  virtual double MaxValue() const override;
  virtual double MinValue() const override;
  virtual double CurValue() const override;
  virtual double Step() const override;
  virtual bool SetCurValue(double aValue) override;
};


/**
  * Used for input@type="range" element.
  */
class HTMLRangeAccessible : public LeafAccessible
{
public:
  HTMLRangeAccessible(nsIContent* aContent, DocAccessible* aDoc) :
    LeafAccessible(aContent, aDoc)
  {
    mStateFlags |= eHasNumericValue;
  }

  // Accessible
  virtual void Value(nsString& aValue) override;
  virtual mozilla::a11y::role NativeRole() override;

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
class HTMLGroupboxAccessible : public HyperTextAccessibleWrap
{
public:
  HTMLGroupboxAccessible(nsIContent* aContent, DocAccessible* aDoc);

  // Accessible
  virtual mozilla::a11y::role NativeRole() override;
  virtual Relation RelationByType(RelationType aType) override;

protected:
  // Accessible
  virtual ENameValueFlag NativeName(nsString& aName) override;

  // HTMLGroupboxAccessible
  nsIContent* GetLegend() const;
};


/**
 * Accessible for HTML legend element.
 */
class HTMLLegendAccessible : public HyperTextAccessibleWrap
{
public:
  HTMLLegendAccessible(nsIContent* aContent, DocAccessible* aDoc);

  // Accessible
  virtual Relation RelationByType(RelationType aType) override;
};

/**
 * Accessible for HTML5 figure element.
 */
class HTMLFigureAccessible : public HyperTextAccessibleWrap
{
public:
  HTMLFigureAccessible(nsIContent* aContent, DocAccessible* aDoc);

  // Accessible
  virtual Relation RelationByType(RelationType aType) override;

protected:
  // Accessible
  virtual ENameValueFlag NativeName(nsString& aName) override;

  // HTMLLegendAccessible
  nsIContent* Caption() const;
};


/**
 * Accessible for HTML5 figcaption element.
 */
class HTMLFigcaptionAccessible : public HyperTextAccessibleWrap
{
public:
  HTMLFigcaptionAccessible(nsIContent* aContent, DocAccessible* aDoc);

  // Accessible
  virtual Relation RelationByType(RelationType aType) override;
};

} // namespace a11y
} // namespace mozilla

#endif
