/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _nsXULTextAccessible_H_
#define _nsXULTextAccessible_H_

#include "nsBaseWidgetAccessible.h"
#include "nsHyperTextAccessibleWrap.h"

/**
 * Used for XUL description and label elements.
 */
class nsXULTextAccessible : public nsHyperTextAccessibleWrap
{
public:
  nsXULTextAccessible(nsIContent* aContent, DocAccessible* aDoc);

  // nsAccessible
  virtual nsresult GetNameInternal(nsAString& aName);
  virtual mozilla::a11y::role NativeRole();
  virtual PRUint64 NativeState();
  virtual Relation RelationByType(PRUint32 aRelationType);
};

/**
 * Used for XUL tooltip element.
 */
class nsXULTooltipAccessible : public nsLeafAccessible
{

public:
  nsXULTooltipAccessible(nsIContent* aContent, DocAccessible* aDoc);

  // nsAccessible
  virtual mozilla::a11y::role NativeRole();
  virtual PRUint64 NativeState();
};

class nsXULLinkAccessible : public nsHyperTextAccessibleWrap
{

public:
  nsXULLinkAccessible(nsIContent* aContent, DocAccessible* aDoc);

  NS_DECL_ISUPPORTS_INHERITED

  // nsIAccessible
  NS_IMETHOD GetActionName(PRUint8 aIndex, nsAString& aName);
  NS_IMETHOD DoAction(PRUint8 aIndex);

  // nsAccessible
  virtual void Value(nsString& aValue);
  virtual nsresult GetNameInternal(nsAString& aName);
  virtual mozilla::a11y::role NativeRole();
  virtual PRUint64 NativeLinkState() const;

  // ActionAccessible
  virtual PRUint8 ActionCount();

  // HyperLinkAccessible
  virtual bool IsLink();
  virtual PRUint32 StartOffset();
  virtual PRUint32 EndOffset();
  virtual already_AddRefed<nsIURI> AnchorURIAt(PRUint32 aAnchorIndex);

protected:
  enum { eAction_Jump = 0 };

};

#endif  
