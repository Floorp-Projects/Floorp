/* clang-format off */
/* -*- Mode: Objective-C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* clang-format on */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_a11y_DocAccessibleWrap_h__
#define mozilla_a11y_DocAccessibleWrap_h__

#include "DocAccessible.h"
#include "nsTHashSet.h"

namespace mozilla {

class PresShell;

namespace a11y {

class DocAccessibleWrap : public DocAccessible {
 public:
  DocAccessibleWrap(dom::Document* aDocument, PresShell* aPresShell);

  virtual ~DocAccessibleWrap();

  virtual void Shutdown() override;

  virtual void AttributeChanged(dom::Element* aElement, int32_t aNameSpaceID,
                                nsAtom* aAttribute, int32_t aModType,
                                const nsAttrValue* aOldValue) override;

  void QueueNewLiveRegion(LocalAccessible* aAccessible);

  void ProcessNewLiveRegions();

 protected:
  virtual void DoInitialUpdate() override;

 private:
  nsTHashSet<void*> mNewLiveRegions;
};

}  // namespace a11y
}  // namespace mozilla

#endif
