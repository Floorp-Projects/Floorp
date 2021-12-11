/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsDocShellEnumerator_h___
#define nsDocShellEnumerator_h___

#include "nsTArray.h"

class nsDocShell;
class nsIDocShell;

class MOZ_STACK_CLASS nsDocShellEnumerator {
 public:
  enum class EnumerationDirection : uint8_t { Forwards, Backwards };

  nsDocShellEnumerator(EnumerationDirection aDirection, int32_t aDocShellType,
                       nsDocShell& aRootItem);

 public:
  nsresult BuildDocShellArray(nsTArray<RefPtr<nsIDocShell>>& aItemArray);

 private:
  nsresult BuildArrayRecursiveForwards(
      nsDocShell* aItem, nsTArray<RefPtr<nsIDocShell>>& aItemArray);
  nsresult BuildArrayRecursiveBackwards(
      nsDocShell* aItem, nsTArray<RefPtr<nsIDocShell>>& aItemArray);

 private:
  const RefPtr<nsDocShell> mRootItem;

  const int32_t mDocShellType;  // only want shells of this type

  const EnumerationDirection mDirection;
};

#endif  // nsDocShellEnumerator_h___
