/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_a11y_HTMLImageMapAccessible_h__
#define mozilla_a11y_HTMLImageMapAccessible_h__

#include "HTMLLinkAccessible.h"
#include "ImageAccessible.h"

namespace mozilla {
namespace a11y {

/**
 * Used for HTML image maps.
 */
class HTMLImageMapAccessible final : public ImageAccessible {
 public:
  HTMLImageMapAccessible(nsIContent* aContent, DocAccessible* aDoc);

  // nsISupports and cycle collector
  NS_INLINE_DECL_REFCOUNTING_INHERITED(HTMLImageMapAccessible, ImageAccessible)

  // LocalAccessible
  virtual a11y::role NativeRole() const override;

  /**
   * Update area children of the image map.
   */
  void UpdateChildAreas(bool aDoFireEvents = true);

  /**
   * Return accessible of child node.
   */
  LocalAccessible* GetChildAccessibleFor(const nsINode* aNode) const;

 protected:
  virtual ~HTMLImageMapAccessible() {}
};

/**
 * Accessible for image map areas - must be child of image.
 */
class HTMLAreaAccessible final : public HTMLLinkAccessible {
 public:
  HTMLAreaAccessible(nsIContent* aContent, DocAccessible* aDoc);

  // LocalAccessible
  virtual void Description(nsString& aDescription) const override;
  virtual LocalAccessible* LocalChildAtPoint(
      int32_t aX, int32_t aY, EWhichChildAtPoint aWhichChild) override;
  virtual nsRect RelativeBounds(nsIFrame** aBoundingFrame) const override;
  virtual nsRect ParentRelativeBounds() override;

  // HyperLinkAccessible
  virtual uint32_t StartOffset() override;
  virtual uint32_t EndOffset() override;

  virtual bool IsAcceptableChild(nsIContent* aEl) const override {
    return false;
  }

  // LocalAccessible
  virtual role NativeRole() const override;

 protected:
  // LocalAccessible
  virtual ENameValueFlag NativeName(nsString& aName) const override;
};

////////////////////////////////////////////////////////////////////////////////
// LocalAccessible downcasting method

inline HTMLImageMapAccessible* LocalAccessible::AsImageMap() {
  return IsImageMap() ? static_cast<HTMLImageMapAccessible*>(this) : nullptr;
}

}  // namespace a11y
}  // namespace mozilla

#endif
