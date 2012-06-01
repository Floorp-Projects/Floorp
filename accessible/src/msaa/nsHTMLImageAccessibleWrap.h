/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:expandtab:shiftwidth=2:tabstop=2:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _NSHTMLIMAGEACCESSIBLEWRAP_H
#define _NSHTMLIMAGEACCESSIBLEWRAP_H

#include "nsHTMLImageAccessible.h"
#include "ia2AccessibleImage.h"

class nsHTMLImageAccessibleWrap : public nsHTMLImageAccessible,
                                  public ia2AccessibleImage
{
public:
  nsHTMLImageAccessibleWrap(nsIContent* aContent, DocAccessible* aDoc) :
    nsHTMLImageAccessible(aContent, aDoc) {}

  // IUnknown
  DECL_IUNKNOWN_INHERITED

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED
};

#endif

