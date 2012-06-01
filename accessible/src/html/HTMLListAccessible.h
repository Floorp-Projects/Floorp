/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_a11y_HTMLListAccessible_h__
#define mozilla_a11y_HTMLListAccessible_h__

#include "HyperTextAccessibleWrap.h"
#include "nsBaseWidgetAccessible.h"

namespace mozilla {
namespace a11y {

class HTMLListBulletAccessible;

/**
 * Used for HTML list (like HTML ul).
 */
class HTMLListAccessible : public HyperTextAccessibleWrap
{
public:
  HTMLListAccessible(nsIContent* aContent, DocAccessible* aDoc) :
    HyperTextAccessibleWrap(aContent, aDoc) { }
  virtual ~HTMLListAccessible() { }

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  // Accessible
  virtual a11y::role NativeRole();
  virtual PRUint64 NativeState();
};


/**
 * Used for HTML list item (e.g. HTML li).
 */
class HTMLLIAccessible : public HyperTextAccessibleWrap
{
public:
  HTMLLIAccessible(nsIContent* aContent, DocAccessible* aDoc);
  virtual ~HTMLLIAccessible() { }

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  // nsAccessNode
  virtual void Shutdown();

  // nsIAccessible
  NS_IMETHOD GetBounds(PRInt32* aX, PRInt32* aY,
                       PRInt32* aWidth, PRInt32* aHeight);

  // Accessible
  virtual a11y::role NativeRole();
  virtual PRUint64 NativeState();

  // nsHTMLLIAccessible
  void UpdateBullet(bool aHasBullet);

protected:
  // Accessible
  virtual void CacheChildren();

private:
  nsRefPtr<HTMLListBulletAccessible> mBullet;
};


/**
 * Used for bullet of HTML list item element (for example, HTML li).
 */
class HTMLListBulletAccessible : public nsLeafAccessible
{
public:
  HTMLListBulletAccessible(nsIContent* aContent, DocAccessible* aDoc) :
    nsLeafAccessible(aContent, aDoc) { }
  virtual ~HTMLListBulletAccessible() { }

  // nsAccessNode
  virtual nsIFrame* GetFrame() const;
  virtual bool IsPrimaryForNode() const;

  // Accessible
  virtual ENameValueFlag Name(nsString& aName);
  virtual a11y::role NativeRole();
  virtual PRUint64 NativeState();
  virtual void AppendTextTo(nsAString& aText, PRUint32 aStartOffset = 0,
                            PRUint32 aLength = PR_UINT32_MAX);

  // HTMLListBulletAccessible

  /**
   * Return true if the bullet is inside of list item element boundaries.
   */
  bool IsInside() const;
};

} // namespace a11y
} // namespace mozilla


inline mozilla::a11y::HTMLLIAccessible*
Accessible::AsHTMLListItem()
{
  return mFlags & eHTMLListItemAccessible ?
    static_cast<mozilla::a11y::HTMLLIAccessible*>(this) : nsnull;
}

#endif
