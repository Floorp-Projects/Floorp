/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _nsBaseWidgetAccessible_H_
#define _nsBaseWidgetAccessible_H_

#include "AccessibleWrap.h"
#include "HyperTextAccessibleWrap.h"
#include "nsIContent.h"

/**
  * This file contains a number of classes that are used as base
  *  classes for the different accessibility implementations of
  *  the HTML and XUL widget sets.  --jgaunt
  */

/** 
  * Leaf version of DOM Accessible -- has no children
  */
class nsLeafAccessible : public AccessibleWrap
{
public:

  nsLeafAccessible(nsIContent* aContent, DocAccessible* aDoc);

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  // Accessible
  virtual Accessible* ChildAtPoint(PRInt32 aX, PRInt32 aY,
                                   EWhichChildAtPoint aWhichChild);

protected:

  // Accessible
  virtual void CacheChildren();
};

/**
 * Used for text or image accessible nodes contained by link accessibles or
 * accessibles for nodes with registered click event handler. It knows how to
 * report the state of the host link (traveled or not) and can activate (click)
 * the host accessible programmatically.
 */
class nsLinkableAccessible : public AccessibleWrap
{
public:
  enum { eAction_Jump = 0 };

  nsLinkableAccessible(nsIContent* aContent, DocAccessible* aDoc);

  NS_DECL_ISUPPORTS_INHERITED

  // nsIAccessible
  NS_IMETHOD GetActionName(PRUint8 aIndex, nsAString& aName);
  NS_IMETHOD DoAction(PRUint8 index);
  NS_IMETHOD TakeFocus();

  // nsAccessNode
  virtual void Shutdown();

  // Accessible
  virtual void Value(nsString& aValue);
  virtual PRUint64 NativeLinkState() const;

  // ActionAccessible
  virtual PRUint8 ActionCount();
  virtual KeyBinding AccessKey() const;

  // HyperLinkAccessible
  virtual already_AddRefed<nsIURI> AnchorURIAt(PRUint32 aAnchorIndex);

protected:
  // Accessible
  virtual void BindToParent(Accessible* aParent, PRUint32 aIndexInParent);
  virtual void UnbindFromParent();

  /**
   * Parent accessible that provides an action for this linkable accessible.
   */
  Accessible* mActionAcc;
  bool mIsLink;
  bool mIsOnclick;
};

/**
 * A simple accessible that gets its enumerated role passed into constructor.
 */ 
class nsEnumRoleAccessible : public AccessibleWrap
{
public:
  nsEnumRoleAccessible(nsIContent* aContent, DocAccessible* aDoc,
                       mozilla::a11y::role aRole);
  virtual ~nsEnumRoleAccessible() { }

  NS_DECL_ISUPPORTS_INHERITED

  // Accessible
  virtual mozilla::a11y::role NativeRole();

protected:
  mozilla::a11y::role mRole;
};

#endif  
