/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "HyperTextAccessible.h"

#ifndef _nsHTMLCanvasAccessible_H_
#define _nsHTMLCanvasAccessible_H_

/**
 * HTML canvas accessible (html:canvas).
 */
class nsHTMLCanvasAccessible : public HyperTextAccessible
{
public:
  nsHTMLCanvasAccessible(nsIContent* aContent, DocAccessible* aDoc);
  virtual ~nsHTMLCanvasAccessible() { }

  // Accessible
  virtual mozilla::a11y::role NativeRole();
};

#endif
