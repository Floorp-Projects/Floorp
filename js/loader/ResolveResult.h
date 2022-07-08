/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef js_loader_ResolveResult_h
#define js_loader_ResolveResult_h

#include "mozilla/ResultVariant.h"
#include "mozilla/NotNull.h"
#include "nsIURI.h"

namespace JS::loader {

enum class ResolveError : uint8_t {
  Failure,
  FailureMayBeBare,
  BlockedByNullEntry,
  BlockedByAfterPrefix,
  BlockedByBacktrackingPrefix,
  InvalidBareSpecifier
};

struct ResolveErrorInfo {
  static const char* GetString(ResolveError aError) {
    switch (aError) {
      case ResolveError::Failure:
        return "ModuleResolveFailureNoWarn";
      case ResolveError::FailureMayBeBare:
        return "ModuleResolveFailureWarnRelative";
      case ResolveError::BlockedByNullEntry:
        return "ImportMapResolutionBlockedByNullEntry";
      case ResolveError::BlockedByAfterPrefix:
        return "ImportMapResolutionBlockedByAfterPrefix";
      case ResolveError::BlockedByBacktrackingPrefix:
        return "ImportMapResolutionBlockedByBacktrackingPrefix";
      case ResolveError::InvalidBareSpecifier:
        return "ImportMapResolveInvalidBareSpecifierWarnRelative";
      default:
        MOZ_CRASH("Unexpected ResolveError value");
    }
  }
};

/**
 * ResolveResult is used to store the result of 'resolving a module specifier',
 * which could be an URI on success or a ResolveError on failure.
 */
using ResolveResult =
    mozilla::Result<mozilla::NotNull<nsCOMPtr<nsIURI>>, ResolveError>;
}  // namespace JS::loader

#endif  // js_loader_ResolveResult_h
