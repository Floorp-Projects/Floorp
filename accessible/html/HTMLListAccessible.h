/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_a11y_HTMLListAccessible_h__
#define mozilla_a11y_HTMLListAccessible_h__

#include "BaseAccessibles.h"
#include "HyperTextAccessibleWrap.h"

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
    HyperTextAccessibleWrap(aContent, aDoc) { mGenericTypes |= eList; }

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  // Accessible
  virtual a11y::role NativeRole() override;
  virtual uint64_t NativeState() override;

protected:
  virtual ~HTMLListAccessible() { }
};


/**
 * Used for HTML list item (e.g. HTML li).
 */
class HTMLLIAccessible : public HyperTextAccessibleWrap
{
public:
  HTMLLIAccessible(nsIContent* aContent, DocAccessible* aDoc);

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  // Accessible
  virtual void Shutdown() override;
  virtual nsIntRect Bounds() const override;
  virtual a11y::role NativeRole() override;
  virtual uint64_t NativeState() override;

  virtual bool InsertChildAt(uint32_t aIndex, Accessible* aChild) override;

  // HTMLLIAccessible
  HTMLListBulletAccessible* Bullet() const { return mBullet; }
  void UpdateBullet(bool aHasBullet);

protected:
  virtual ~HTMLLIAccessible() { }

private:
  RefPtr<HTMLListBulletAccessible> mBullet;
};


/**
 * Used for bullet of HTML list item element (for example, HTML li).
 */
class HTMLListBulletAccessible : public LeafAccessible
{
public:
  HTMLListBulletAccessible(nsIContent* aContent, DocAccessible* aDoc);
  virtual ~HTMLListBulletAccessible() { }

  // Accessible
  virtual nsIFrame* GetFrame() const override;
  virtual ENameValueFlag Name(nsString& aName) override;
  virtual a11y::role NativeRole() override;
  virtual uint64_t NativeState() override;
  virtual void AppendTextTo(nsAString& aText, uint32_t aStartOffset = 0,
                            uint32_t aLength = UINT32_MAX) override;

  // HTMLListBulletAccessible

  /**
   * Return true if the bullet is inside of list item element boundaries.
   */
  bool IsInside() const;
};


inline HTMLLIAccessible*
Accessible::AsHTMLListItem()
{
  return IsHTMLListItem() ? static_cast<HTMLLIAccessible*>(this) : nullptr;
}

} // namespace a11y
} // namespace mozilla

#endif
