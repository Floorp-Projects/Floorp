/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_a11y_HTMLImageMapAccessible_h__
#define mozilla_a11y_HTMLImageMapAccessible_h__

#include "HTMLLinkAccessible.h"
#include "ImageAccessibleWrap.h"
#include "nsIDOMHTMLMapElement.h"

namespace mozilla {
namespace a11y {

/**
 * Used for HTML image maps.
 */
class HTMLImageMapAccessible MOZ_FINAL : public ImageAccessibleWrap
{
public:
  HTMLImageMapAccessible(nsIContent* aContent, DocAccessible* aDoc);

  // nsISupports and cycle collector
  NS_DECL_ISUPPORTS_INHERITED

  // Accessible
  virtual a11y::role NativeRole();

  // HyperLinkAccessible
  virtual uint32_t AnchorCount();
  virtual Accessible* AnchorAt(uint32_t aAnchorIndex);
  virtual already_AddRefed<nsIURI> AnchorURIAt(uint32_t aAnchorIndex);

  /**
   * Update area children of the image map.
   */
  void UpdateChildAreas(bool aDoFireEvents = true);

  /**
   * Return accessible of child node.
   */
  Accessible* GetChildAccessibleFor(const nsINode* aNode) const;

protected:
  virtual ~HTMLImageMapAccessible() { }

  // Accessible
  virtual void CacheChildren();
};

/**
 * Accessible for image map areas - must be child of image.
 */
class HTMLAreaAccessible MOZ_FINAL : public HTMLLinkAccessible
{
public:

  HTMLAreaAccessible(nsIContent* aContent, DocAccessible* aDoc);

  // Accessible
  virtual void Description(nsString& aDescription);
  virtual Accessible* ChildAtPoint(int32_t aX, int32_t aY,
                                   EWhichChildAtPoint aWhichChild);
  virtual void GetBoundsRect(nsRect& aBounds, nsIFrame** aBoundingFrame);

  // HyperLinkAccessible
  virtual uint32_t StartOffset();
  virtual uint32_t EndOffset();

protected:
  // Accessible
  virtual ENameValueFlag NativeName(nsString& aName) MOZ_OVERRIDE;
  virtual void CacheChildren();
};


////////////////////////////////////////////////////////////////////////////////
// Accessible downcasting method

inline HTMLImageMapAccessible*
Accessible::AsImageMap()
{
  return IsImageMap() ? static_cast<HTMLImageMapAccessible*>(this) : nullptr;
}

} // namespace a11y
} // namespace mozilla

#endif
