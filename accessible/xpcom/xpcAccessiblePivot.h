/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _xpcAccessiblePivot_H_
#define _xpcAccessiblePivot_H_

#include "nsIAccessiblePivot.h"

#include "Accessible.h"
#include "nsCycleCollectionParticipant.h"
#include "mozilla/Attributes.h"
#include "xpcAccessible.h"

namespace mozilla::a11y {
/**
 * Class represents an accessible pivot.
 */
class xpcAccessiblePivot final : public nsIAccessiblePivot {
 public:
  explicit xpcAccessiblePivot(nsIAccessible* aRoot);

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS_AMBIGUOUS(xpcAccessiblePivot,
                                           nsIAccessiblePivot)

  NS_DECL_NSIACCESSIBLEPIVOT

 private:
  ~xpcAccessiblePivot();
  xpcAccessiblePivot() = delete;
  xpcAccessiblePivot(const xpcAccessiblePivot&) = delete;
  void operator=(const xpcAccessiblePivot&) = delete;

  Accessible* Root() { return mRoot ? mRoot->ToInternalGeneric() : nullptr; }

  /*
   * The root accessible.
   */
  RefPtr<nsIAccessible> mRoot;
};

}  // namespace mozilla::a11y

#endif
