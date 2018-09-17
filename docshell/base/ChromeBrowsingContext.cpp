/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ChromeBrowsingContext.h"

namespace mozilla {
namespace dom {

ChromeBrowsingContext::ChromeBrowsingContext(uint64_t aBrowsingContextId,
                                             const nsAString& aName,
                                             uint64_t aProcessId)
  : BrowsingContext(aBrowsingContextId, aName)
  , mProcessId(aProcessId)
{
  // You are only ever allowed to create ChromeBrowsingContexts in the
  // parent process.
  MOZ_RELEASE_ASSERT(XRE_IsParentProcess());
}

ChromeBrowsingContext::ChromeBrowsingContext(nsIDocShell* aDocShell)
  : BrowsingContext(aDocShell)
  , mProcessId(0)
{
  // You are only ever allowed to create ChromeBrowsingContexts in the
  // parent process.
  MOZ_RELEASE_ASSERT(XRE_IsParentProcess());
}

// TODO(farre): ChromeBrowsingContext::CleanupContexts starts from the
// list of root BrowsingContexts. This isn't enough when separate
// BrowsingContext nodes of a BrowsingContext tree, not in a crashing
// child process, are from that process and thus needs to be
// cleaned. [Bug 1472108]
/* static */ void
ChromeBrowsingContext::CleanupContexts(uint64_t aProcessId)
{
  nsTArray<RefPtr<BrowsingContext>> roots;
  BrowsingContext::GetRootBrowsingContexts(roots);

  for (RefPtr<BrowsingContext> context : roots) {
    if (Cast(context)->IsOwnedByProcess(aProcessId)) {
      context->Detach();
    }
  }
}

/* static */ already_AddRefed<ChromeBrowsingContext>
ChromeBrowsingContext::Get(uint64_t aId)
{
  MOZ_RELEASE_ASSERT(XRE_IsParentProcess());
  return BrowsingContext::Get(aId).downcast<ChromeBrowsingContext>();
}

/* static */ ChromeBrowsingContext*
ChromeBrowsingContext::Cast(BrowsingContext* aContext)
{
  MOZ_RELEASE_ASSERT(XRE_IsParentProcess());
  return static_cast<ChromeBrowsingContext*>(aContext);
}

/* static */ already_AddRefed<ChromeBrowsingContext>
ChromeBrowsingContext::Create(
  uint64_t aBrowsingContextId,
  const nsAString& aName,
  uint64_t aProcessId)
{
  return do_AddRef(new ChromeBrowsingContext(aBrowsingContextId, aName, aProcessId));
}

} // namespace dom
} // namespace mozilla
