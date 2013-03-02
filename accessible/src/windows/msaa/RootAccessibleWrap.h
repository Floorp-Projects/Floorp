/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_a11y_RootAccessibleWrap_h__
#define mozilla_a11y_RootAccessibleWrap_h__

#include "RootAccessible.h"

namespace mozilla {
namespace a11y {

class RootAccessibleWrap : public RootAccessible
{
public:
  RootAccessibleWrap(nsIDocument* aDocument, nsIContent* aRootContent,
                     nsIPresShell* aPresShell);
  virtual ~RootAccessibleWrap();

  // RootAccessible
  virtual void DocumentActivated(DocAccessible* aDocument);
};

} // namespace a11y
} // namespace mozilla

#endif
