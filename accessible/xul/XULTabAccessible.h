/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_a11y_XULTabAccessible_h__
#define mozilla_a11y_XULTabAccessible_h__

// NOTE: alphabetically ordered
#include "XULMenuAccessible.h"
#include "XULSelectControlAccessible.h"

namespace mozilla {
namespace a11y {

/**
 * An individual tab, xul:tab element.
 */
class XULTabAccessible : public AccessibleWrap
{
public:
  enum { eAction_Switch = 0 };

  XULTabAccessible(nsIContent* aContent, DocAccessible* aDoc);

  // Accessible
  virtual a11y::role NativeRole() MOZ_OVERRIDE;
  virtual uint64_t NativeState() MOZ_OVERRIDE;
  virtual uint64_t NativeInteractiveState() const MOZ_OVERRIDE;
  virtual Relation RelationByType(RelationType aType) MOZ_OVERRIDE;

  // ActionAccessible
  virtual uint8_t ActionCount() MOZ_OVERRIDE;
  virtual void ActionNameAt(uint8_t aIndex, nsAString& aName) MOZ_OVERRIDE;
  virtual bool DoAction(uint8_t aIndex) MOZ_OVERRIDE;
};


/**
 * A container of tab objects, xul:tabs element.
 */
class XULTabsAccessible : public XULSelectControlAccessible
{
public:
  XULTabsAccessible(nsIContent* aContent, DocAccessible* aDoc);

  // Accessible
  virtual void Value(nsString& aValue);
  virtual a11y::role NativeRole() MOZ_OVERRIDE;

  // ActionAccessible
  virtual uint8_t ActionCount() MOZ_OVERRIDE;

protected:
  // Accessible
  virtual ENameValueFlag NativeName(nsString& aName) MOZ_OVERRIDE;
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
  virtual a11y::role NativeRole() MOZ_OVERRIDE;
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
  virtual a11y::role NativeRole() MOZ_OVERRIDE;
  virtual Relation RelationByType(RelationType aType) MOZ_OVERRIDE;
};

} // namespace a11y
} // namespace mozilla

#endif

