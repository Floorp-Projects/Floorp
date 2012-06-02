/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _nsHTMLAreaAccessible_H_
#define _nsHTMLAreaAccessible_H_

#include "ImageAccessibleWrap.h"
#include "nsHTMLLinkAccessible.h"

#include "nsIDOMHTMLMapElement.h"

/**
 * Used for HTML image maps.
 */
class nsHTMLImageMapAccessible : public mozilla::a11y::ImageAccessibleWrap
{
public:
  nsHTMLImageMapAccessible(nsIContent* aContent, DocAccessible* aDoc);
  virtual ~nsHTMLImageMapAccessible() { }

  // nsISupports and cycle collector
  NS_DECL_ISUPPORTS_INHERITED

  // Accessible
  virtual mozilla::a11y::role NativeRole();

  // HyperLinkAccessible
  virtual PRUint32 AnchorCount();
  virtual Accessible* AnchorAt(PRUint32 aAnchorIndex);
  virtual already_AddRefed<nsIURI> AnchorURIAt(PRUint32 aAnchorIndex);

  /**
   * Update area children of the image map.
   */
  void UpdateChildAreas(bool aDoFireEvents = true);

protected:

  // Accessible
  virtual void CacheChildren();
};

////////////////////////////////////////////////////////////////////////////////
// Accessible downcasting method

inline nsHTMLImageMapAccessible*
Accessible::AsImageMap()
{
  return IsImageMapAccessible() ?
    static_cast<nsHTMLImageMapAccessible*>(this) : nsnull;
}


/**
 * Accessible for image map areas - must be child of image.
 */
class nsHTMLAreaAccessible : public nsHTMLLinkAccessible
{
public:

  nsHTMLAreaAccessible(nsIContent* aContent, DocAccessible* aDoc);

  // nsAccessNode
  virtual bool IsPrimaryForNode() const;

  // Accessible
  virtual void Description(nsString& aDescription);
  virtual nsresult GetNameInternal(nsAString& aName);
  virtual Accessible* ChildAtPoint(PRInt32 aX, PRInt32 aY,
                                   EWhichChildAtPoint aWhichChild);
  virtual void GetBoundsRect(nsRect& aBounds, nsIFrame** aBoundingFrame);

  // HyperLinkAccessible
  virtual PRUint32 StartOffset();
  virtual PRUint32 EndOffset();

protected:

  // Accessible
  virtual void CacheChildren();
};

#endif
