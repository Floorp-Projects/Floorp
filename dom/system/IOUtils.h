/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_IOUtils__
#define mozilla_dom_IOUtils__

#include <prio.h>

namespace mozilla {

/**
 * Utility class to be used with |UniquePtr| to automatically close NSPR file
 * descriptors when they go out of scope.
 *
 * Example:
 *
 *   UniquePtr<PRFileDesc, PR_CloseDelete> fd = PR_Open(path, flags, mode);
 */
class PR_CloseDelete {
 public:
  constexpr PR_CloseDelete() = default;
  PR_CloseDelete(const PR_CloseDelete& aOther) = default;

  void operator()(PRFileDesc* aPtr) const { PR_Close(aPtr); }
};

}  // namespace mozilla

#endif
