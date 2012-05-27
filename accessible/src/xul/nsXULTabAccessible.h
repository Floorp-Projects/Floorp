/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _nsXULTabAccessible_H_
#define _nsXULTabAccessible_H_

// NOTE: alphabetically ordered
#include "nsBaseWidgetAccessible.h"
#include "nsXULMenuAccessible.h"
#include "XULSelectControlAccessible.h"

/**
 * An individual tab, xul:tab element.
 */
class nsXULTabAccessible : public nsAccessibleWrap
{
public:
  enum { eAction_Switch = 0 };

  nsXULTabAccessible(nsIContent* aContent, DocAccessible* aDoc);

  // nsIAccessible
  NS_IMETHOD GetActionName(PRUint8 aIndex, nsAString& aName);
  NS_IMETHOD DoAction(PRUint8 index);

  // nsAccessible
  virtual mozilla::a11y::role NativeRole();
  virtual PRUint64 NativeState();
  virtual Relation RelationByType(PRUint32 aType);

  // ActionAccessible
  virtual PRUint8 ActionCount();
};


/**
 * A container of tab objects, xul:tabs element.
 */
class nsXULTabsAccessible : public XULSelectControlAccessible
{
public:
  nsXULTabsAccessible(nsIContent* aContent, DocAccessible* aDoc);

  // nsAccessible
  virtual void Value(nsString& aValue);
  virtual nsresult GetNameInternal(nsAString& aName);
  virtual mozilla::a11y::role NativeRole();

  // ActionAccessible
  virtual PRUint8 ActionCount();
};


/** 
 * A container of tab panels, xul:tabpanels element.
 */
class nsXULTabpanelsAccessible : public nsAccessibleWrap
{
public:
  nsXULTabpanelsAccessible(nsIContent* aContent, DocAccessible* aDoc);

  // nsAccessible
  virtual mozilla::a11y::role NativeRole();
};


/**
 * A tabpanel object, child elements of xul:tabpanels element. Note,the object
 * is created from nsAccessibilityService::GetAccessibleForDeckChildren()
 * method and we do not use nsIAccessibleProvider interface here because
 * all children of xul:tabpanels element acts as xul:tabpanel element.
 *
 * XXX: we need to move the class logic into generic class since
 * for example we do not create instance of this class for XUL textbox used as
 * a tabpanel.
 */
class nsXULTabpanelAccessible : public nsAccessibleWrap
{
public:
  nsXULTabpanelAccessible(nsIContent* aContent, DocAccessible* aDoc);

  // nsAccessible
  virtual mozilla::a11y::role NativeRole();
  virtual Relation RelationByType(PRUint32 aType);
};

#endif

