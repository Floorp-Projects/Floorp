/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_a11y_RootAccessibleWrap_h__
#define mozilla_a11y_RootAccessibleWrap_h__

#include "RootAccessible.h"

namespace mozilla {

class PresShell;

namespace a11y {

class DocRemoteAccessibleWrap;

class RootAccessibleWrap : public RootAccessible {
 public:
  RootAccessibleWrap(dom::Document* aDocument, PresShell* aPresShell);
  virtual ~RootAccessibleWrap();

  AccessibleWrap* GetContentAccessible();

  AccessibleWrap* FindAccessibleById(int32_t aID);

  // Recursively searches for the accessible ID within the document tree.
  AccessibleWrap* FindAccessibleById(DocAccessibleWrap* aDocument, int32_t aID);

  // Recursively searches for the accessible ID within the proxy document tree.
  AccessibleWrap* FindAccessibleById(DocRemoteAccessibleWrap* aDocument,
                                     int32_t aID);
};

}  // namespace a11y
}  // namespace mozilla

#endif
