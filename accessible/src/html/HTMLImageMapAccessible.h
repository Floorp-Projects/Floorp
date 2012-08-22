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
class HTMLImageMapAccessible : public ImageAccessibleWrap
{
public:
  HTMLImageMapAccessible(nsIContent* aContent, DocAccessible* aDoc);
  virtual ~HTMLImageMapAccessible() { }

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

protected:

  // Accessible
  virtual void CacheChildren();
};

/**
 * Accessible for image map areas - must be child of image.
 */
class HTMLAreaAccessible : public HTMLLinkAccessible
{
public:

  HTMLAreaAccessible(nsIContent* aContent, DocAccessible* aDoc);

  // nsAccessNode
  virtual bool IsPrimaryForNode() const;

  // Accessible
  virtual void Description(nsString& aDescription);
  virtual nsresult GetNameInternal(nsAString& aName);
  virtual Accessible* ChildAtPoint(int32_t aX, int32_t aY,
                                   EWhichChildAtPoint aWhichChild);
  virtual void GetBoundsRect(nsRect& aBounds, nsIFrame** aBoundingFrame);

  // HyperLinkAccessible
  virtual uint32_t StartOffset();
  virtual uint32_t EndOffset();

protected:

  // Accessible
  virtual void CacheChildren();
};

} // namespace a11y
} // namespace mozilla

////////////////////////////////////////////////////////////////////////////////
// Accessible downcasting method

inline mozilla::a11y::HTMLImageMapAccessible*
Accessible::AsImageMap()
{
  return IsImageMapAccessible() ?
    static_cast<mozilla::a11y::HTMLImageMapAccessible*>(this) : nullptr;
}

#endif
