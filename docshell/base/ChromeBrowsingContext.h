/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef ChromeBrowsingContext_h
#define ChromeBrowsingContext_h

#include "mozilla/dom/BrowsingContext.h"
#include "mozilla/RefPtr.h"
#include "nsCycleCollectionParticipant.h"
#include "nsWrapperCache.h"

class nsIDocShell;

namespace mozilla {
namespace dom {

// ChromeBrowsingContext is a BrowsingContext living in the parent
// process, with whatever extra data that a BrowsingContext in the
// parent needs.
class ChromeBrowsingContext final
  : public BrowsingContext
{
public:
  static void CleanupContexts(uint64_t aProcessId);
  static already_AddRefed<ChromeBrowsingContext> Get(uint64_t aId);
  static ChromeBrowsingContext* Cast(BrowsingContext* aContext);
  static already_AddRefed<ChromeBrowsingContext> Create(
    uint64_t aBrowsingContextId,
    const nsAString& aName,
    uint64_t aProcessId);

  bool IsOwnedByProcess(uint64_t aProcessId) const
  {
    return mProcessId == aProcessId;
  }

protected:
  void Traverse(nsCycleCollectionTraversalCallback& cb) {}
  void Unlink() {}
  ChromeBrowsingContext(uint64_t aBrowsingContextId,
                        const nsAString& aName,
                        uint64_t aProcessId);

private:
  friend class BrowsingContext;

  explicit ChromeBrowsingContext(nsIDocShell* aDocShell);

  // Indicates which process owns the docshell.
  uint64_t mProcessId;
};

} // namespace dom
} // namespace mozilla
#endif
