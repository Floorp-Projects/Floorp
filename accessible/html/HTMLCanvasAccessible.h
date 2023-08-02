/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_a11y_HTMLCanvasAccessible_h__
#define mozilla_a11y_HTMLCanvasAccessible_h__

#include "HyperTextAccessible.h"

namespace mozilla {
namespace a11y {

/**
 * HTML canvas accessible (html:canvas).
 */
class HTMLCanvasAccessible : public HyperTextAccessible {
 public:
  HTMLCanvasAccessible(nsIContent* aContent, DocAccessible* aDoc);

  // nsISupports
  NS_INLINE_DECL_REFCOUNTING_INHERITED(HTMLCanvasAccessible,
                                       HyperTextAccessible)

  // LocalAccessible
  virtual a11y::role NativeRole() const override;

 protected:
  virtual ~HTMLCanvasAccessible() {}
};

}  // namespace a11y
}  // namespace mozilla

#endif
