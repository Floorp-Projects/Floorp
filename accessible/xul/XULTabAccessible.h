/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_a11y_XULTabAccessible_h__
#define mozilla_a11y_XULTabAccessible_h__

// NOTE: alphabetically ordered
#include "HyperTextAccessibleWrap.h"
#include "XULMenuAccessible.h"
#include "XULSelectControlAccessible.h"

namespace mozilla {
namespace a11y {

/**
 * An individual tab, xul:tab element.
 */
class XULTabAccessible : public HyperTextAccessibleWrap
{
public:
  enum { eAction_Switch = 0 };

  XULTabAccessible(nsIContent* aContent, DocAccessible* aDoc);

  // Accessible
  virtual a11y::role NativeRole() const override;
  virtual uint64_t NativeState() const override;
  virtual uint64_t NativeInteractiveState() const override;
  virtual Relation RelationByType(RelationType aType) const override;

  // ActionAccessible
  virtual uint8_t ActionCount() const override;
  virtual void ActionNameAt(uint8_t aIndex, nsAString& aName) override;
  virtual bool DoAction(uint8_t aIndex) const override;
};


/**
 * A container of tab objects, xul:tabs element.
 */
class XULTabsAccessible : public XULSelectControlAccessible
{
public:
  XULTabsAccessible(nsIContent* aContent, DocAccessible* aDoc);

  // Accessible
  virtual void Value(nsString& aValue) const override;
  virtual a11y::role NativeRole() const override;

  // ActionAccessible
  virtual uint8_t ActionCount() const override;

protected:
  // Accessible
  virtual ENameValueFlag NativeName(nsString& aName) const override;
};


/**
 * A container of tab panels, xul:tabpanels element.
 */
class XULTabpanelsAccessible : public AccessibleWrap
{
public:
  XULTabpanelsAccessible(nsIContent* aContent, DocAccessible* aDoc) :
    AccessibleWrap(aContent, aDoc)
    { mType = eXULTabpanelsType; }

  // Accessible
  virtual a11y::role NativeRole() const override;
};


/**
 * A tabpanel object, child elements of xul:tabpanels element.
 *
 * XXX: we need to move the class logic into generic class since
 * for example we do not create instance of this class for XUL textbox used as
 * a tabpanel.
 */
class XULTabpanelAccessible : public AccessibleWrap
{
public:
  XULTabpanelAccessible(nsIContent* aContent, DocAccessible* aDoc);

  // Accessible
  virtual a11y::role NativeRole() const override;
  virtual Relation RelationByType(RelationType aType) const override;
};

} // namespace a11y
} // namespace mozilla

#endif

