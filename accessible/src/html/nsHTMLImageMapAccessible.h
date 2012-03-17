/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Aaron Leventhal <aaronl@netscape.com> (original author)
 *   Alexander Surkov <surkov.alexander@gmail.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef _nsHTMLAreaAccessible_H_
#define _nsHTMLAreaAccessible_H_

#include "nsHTMLLinkAccessible.h"
#include "nsHTMLImageAccessibleWrap.h"

#include "nsIDOMHTMLMapElement.h"

/**
 * Used for HTML image maps.
 */
class nsHTMLImageMapAccessible : public nsHTMLImageAccessibleWrap
{
public:
  nsHTMLImageMapAccessible(nsIContent* aContent, nsDocAccessible* aDoc);
  virtual ~nsHTMLImageMapAccessible() { }

  // nsISupports and cycle collector
  NS_DECL_ISUPPORTS_INHERITED

  // nsAccessible
  virtual mozilla::a11y::role NativeRole();

  // HyperLinkAccessible
  virtual PRUint32 AnchorCount();
  virtual nsAccessible* AnchorAt(PRUint32 aAnchorIndex);
  virtual already_AddRefed<nsIURI> AnchorURIAt(PRUint32 aAnchorIndex);

  /**
   * Update area children of the image map.
   */
  void UpdateChildAreas(bool aDoFireEvents = true);

protected:

  // nsAccessible
  virtual void CacheChildren();
};

////////////////////////////////////////////////////////////////////////////////
// nsAccessible downcasting method

inline nsHTMLImageMapAccessible*
nsAccessible::AsImageMap()
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

  nsHTMLAreaAccessible(nsIContent* aContent, nsDocAccessible* aDoc);

  // nsIAccessible

  NS_IMETHOD GetBounds(PRInt32 *x, PRInt32 *y, PRInt32 *width, PRInt32 *height);

  // nsAccessNode
  virtual bool IsPrimaryForNode() const;

  // nsAccessible
  virtual void Description(nsString& aDescription);
  virtual nsresult GetNameInternal(nsAString& aName);
  virtual PRUint64 NativeState();
  virtual nsAccessible* ChildAtPoint(PRInt32 aX, PRInt32 aY,
                                     EWhichChildAtPoint aWhichChild);

  // HyperLinkAccessible
  virtual PRUint32 StartOffset();
  virtual PRUint32 EndOffset();

protected:

  // nsAccessible
  virtual void CacheChildren();
};

#endif  
