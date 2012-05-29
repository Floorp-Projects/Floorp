/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_A11Y_OUTERDOCACCESSIBLE_H_
#define MOZILLA_A11Y_OUTERDOCACCESSIBLE_H_

#include "AccessibleWrap.h"

namespace mozilla {
namespace a11y {

/**
 * Used for <browser>, <frame>, <iframe>, <page> or editor> elements.
 * 
 * In these variable names, "outer" relates to the OuterDocAccessible as
 * opposed to the DocAccessibleWrap which is "inner". The outer node is
 * a something like tags listed above, whereas the inner node corresponds to
 * the inner document root.
 */

class OuterDocAccessible : public AccessibleWrap
{
public:
  OuterDocAccessible(nsIContent* aContent, DocAccessible* aDoc);
  virtual ~OuterDocAccessible();

  NS_DECL_ISUPPORTS_INHERITED

  // nsIAccessible
  NS_IMETHOD GetActionName(PRUint8 aIndex, nsAString& aName);
  NS_IMETHOD GetActionDescription(PRUint8 aIndex, nsAString& aDescription);
  NS_IMETHOD DoAction(PRUint8 aIndex);

  // nsAccessNode
  virtual void Shutdown();

  // Accessible
  virtual mozilla::a11y::role NativeRole();
  virtual nsresult GetAttributesInternal(nsIPersistentProperties *aAttributes);
  virtual Accessible* ChildAtPoint(PRInt32 aX, PRInt32 aY,
                                   EWhichChildAtPoint aWhichChild);

  virtual void InvalidateChildren();
  virtual bool AppendChild(Accessible* aAccessible);
  virtual bool RemoveChild(Accessible* aAccessible);

  // ActionAccessible
  virtual PRUint8 ActionCount();

protected:
  // Accessible
  virtual void CacheChildren();
};

} // namespace a11y
} // namespace mozilla

#endif  
