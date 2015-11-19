/* -*-  Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_devtools_DominatorTree__
#define mozilla_devtools_DominatorTree__

#include "mozilla/dom/BindingDeclarations.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/RefCounted.h"
#include "js/UbiNodeDominatorTree.h"
#include "nsWrapperCache.h"

namespace mozilla {
namespace devtools {

class DominatorTree final : public nsISupports
                          , public nsWrapperCache
{
protected:
  nsCOMPtr<nsISupports> mParent;

  virtual ~DominatorTree() { }

private:
  JS::ubi::DominatorTree mDominatorTree;

public:
  explicit DominatorTree(JS::ubi::DominatorTree&& aDominatorTree, nsISupports* aParent)
    : mParent(aParent)
    , mDominatorTree(Move(aDominatorTree))
  {
    MOZ_ASSERT(aParent);
  };

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS;
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(DominatorTree);

  nsISupports* GetParentObject() const { return mParent; }

  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aGivenProto) override;

  uint64_t Root() const { return mDominatorTree.root().identifier(); }
};

} // namespace devtools
} // namespace mozilla

#endif // mozilla_devtools_DominatorTree__
