/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_A11Y_HTMLFormControlAccessible_H_
#define MOZILLA_A11Y_HTMLFormControlAccessible_H_

#include "FormControlAccessible.h"
#include "nsHyperTextAccessibleWrap.h"

namespace mozilla {
namespace a11y {

/**
 * Accessible for HTML progress element.
 */
typedef ProgressMeterAccessible<1> HTMLProgressMeterAccessible;

/**
 * Accessible for HTML input@type="checkbox".
 */
class HTMLCheckboxAccessible : public nsLeafAccessible
{

public:
  enum { eAction_Click = 0 };

  HTMLCheckboxAccessible(nsIContent* aContent, nsDocAccessible* aDoc);

  // nsIAccessible
  NS_IMETHOD GetActionName(PRUint8 aIndex, nsAString& aName);
  NS_IMETHOD DoAction(PRUint8 index);

  // nsAccessible
  virtual mozilla::a11y::role NativeRole();
  virtual PRUint64 NativeState();

  // ActionAccessible
  virtual PRUint8 ActionCount();

  // Widgets
  virtual bool IsWidget() const;
};


/**
 * Accessible for HTML input@type="radio" element.
 */
class HTMLRadioButtonAccessible : public RadioButtonAccessible
{

public:
  HTMLRadioButtonAccessible(nsIContent* aContent, nsDocAccessible* aDoc);

  // nsAccessible
  virtual PRUint64 NativeState();
  virtual void GetPositionAndSizeInternal(PRInt32 *aPosInSet,
                                          PRInt32 *aSetSize);
};


/**
 * Accessible for HTML input@type="button", @type="submit", @type="image"
 * and HTML button elements.
 */
class HTMLButtonAccessible : public nsHyperTextAccessibleWrap
{

public:
  enum { eAction_Click = 0 };

  HTMLButtonAccessible(nsIContent* aContent, nsDocAccessible* aDoc);

  // nsIAccessible
  NS_IMETHOD GetActionName(PRUint8 aIndex, nsAString& aName);
  NS_IMETHOD DoAction(PRUint8 index);

  // nsAccessible
  virtual nsresult GetNameInternal(nsAString& aName);
  virtual mozilla::a11y::role NativeRole();
  virtual PRUint64 State();
  virtual PRUint64 NativeState();

  // ActionAccessible
  virtual PRUint8 ActionCount();

  // Widgets
  virtual bool IsWidget() const;
};


/**
 * Accessible for HTML input@type="text" element.
 */
class HTMLTextFieldAccessible : public nsHyperTextAccessibleWrap
{

public:
  enum { eAction_Click = 0 };

  HTMLTextFieldAccessible(nsIContent* aContent, nsDocAccessible* aDoc);

  NS_DECL_ISUPPORTS_INHERITED

  // nsIAccessible
  NS_IMETHOD GetActionName(PRUint8 aIndex, nsAString& aName);
  NS_IMETHOD DoAction(PRUint8 index);

  // nsHyperTextAccessible
  virtual already_AddRefed<nsIEditor> GetEditor() const;

  // nsAccessible
  virtual void Value(nsString& aValue);
  virtual void ApplyARIAState(PRUint64* aState) const;
  virtual nsresult GetNameInternal(nsAString& aName);
  virtual mozilla::a11y::role NativeRole();
  virtual PRUint64 State();
  virtual PRUint64 NativeState();

  // ActionAccessible
  virtual PRUint8 ActionCount();

  // Widgets
  virtual bool IsWidget() const;
  virtual nsAccessible* ContainerWidget() const;
};


/**
 * Accessible for input@type="file" element.
 */
class HTMLFileInputAccessible : public nsHyperTextAccessibleWrap
{
public:
  HTMLFileInputAccessible(nsIContent* aContent, nsDocAccessible* aDoc);

  // nsAccessible
  virtual mozilla::a11y::role NativeRole();
  virtual nsresult HandleAccEvent(AccEvent* aAccEvent);
};

/**
 * Accessible for HTML fieldset element.
 */
class HTMLGroupboxAccessible : public nsHyperTextAccessibleWrap
{
public:
  HTMLGroupboxAccessible(nsIContent* aContent, nsDocAccessible* aDoc);

  // nsAccessible
  virtual nsresult GetNameInternal(nsAString& aName);
  virtual mozilla::a11y::role NativeRole();
  virtual Relation RelationByType(PRUint32 aType);

protected:
  nsIContent* GetLegend();
};


/**
 * Accessible for HTML legend element.
 */
class HTMLLegendAccessible : public nsHyperTextAccessibleWrap
{
public:
  HTMLLegendAccessible(nsIContent* aContent, nsDocAccessible* aDoc);

  // nsAccessible
  virtual mozilla::a11y::role NativeRole();
  virtual Relation RelationByType(PRUint32 aType);
};

/**
 * Accessible for HTML5 figure element.
 */
class HTMLFigureAccessible : public nsHyperTextAccessibleWrap
{
public:
  HTMLFigureAccessible(nsIContent* aContent, nsDocAccessible* aDoc);

  // nsAccessible
  virtual nsresult GetAttributesInternal(nsIPersistentProperties* aAttributes);
  virtual nsresult GetNameInternal(nsAString& aName);
  virtual mozilla::a11y::role NativeRole();
  virtual Relation RelationByType(PRUint32 aType);

protected:
  nsIContent* Caption() const;
};


/**
 * Accessible for HTML5 figcaption element.
 */
class HTMLFigcaptionAccessible : public nsHyperTextAccessibleWrap
{
public:
  HTMLFigcaptionAccessible(nsIContent* aContent, nsDocAccessible* aDoc);

  // nsAccessible
  virtual mozilla::a11y::role NativeRole();
  virtual Relation RelationByType(PRUint32 aType);
};

} // namespace a11y
} // namespace mozilla

#endif
