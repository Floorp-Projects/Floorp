/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _nsHTMLLinkAccessible_H_
#define _nsHTMLLinkAccessible_H_

#include "HyperTextAccessibleWrap.h"

class nsHTMLLinkAccessible : public HyperTextAccessibleWrap
{
public:
  nsHTMLLinkAccessible(nsIContent* aContent, DocAccessible* aDoc);
 
  NS_DECL_ISUPPORTS_INHERITED

  // nsIAccessible
  NS_IMETHOD GetActionName(PRUint8 aIndex, nsAString& aName);
  NS_IMETHOD DoAction(PRUint8 aIndex);

  // Accessible
  virtual void Value(nsString& aValue);
  virtual mozilla::a11y::role NativeRole();
  virtual PRUint64 NativeState();
  virtual PRUint64 NativeLinkState() const;

  // ActionAccessible
  virtual PRUint8 ActionCount();

  // HyperLinkAccessible
  virtual bool IsLink();
  virtual already_AddRefed<nsIURI> AnchorURIAt(PRUint32 aAnchorIndex);

protected:
  enum { eAction_Jump = 0 };

  /**
   * Returns true if the link has href attribute.
   */
  bool IsLinked();
};

#endif  
