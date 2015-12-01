/* -*-  Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_devtools_DominatorTree__
#define mozilla_devtools_DominatorTree__

#include "mozilla/devtools/HeapSnapshot.h"
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
  RefPtr<HeapSnapshot> mHeapSnapshot;

public:
  explicit DominatorTree(JS::ubi::DominatorTree&& aDominatorTree, HeapSnapshot* aHeapSnapshot,
                         nsISupports* aParent)
    : mParent(aParent)
    , mDominatorTree(Move(aDominatorTree))
    , mHeapSnapshot(aHeapSnapshot)
  {
    MOZ_ASSERT(aParent);
    MOZ_ASSERT(aHeapSnapshot);
  };

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS;
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(DominatorTree);

  nsISupports* GetParentObject() const { return mParent; }

  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aGivenProto) override;

  // readonly attribute NodeId root
  uint64_t Root() const { return mDominatorTree.root().identifier(); }

  // [Throws] NodeSize getRetainedSize(NodeId node)
  dom::Nullable<uint64_t> GetRetainedSize(uint64_t aNodeId, ErrorResult& aRv);

  // [Throws] sequence<NodeId>? getImmediatelyDominated(NodeId node);
  void GetImmediatelyDominated(uint64_t aNodeId, dom::Nullable<nsTArray<uint64_t>>& aOutDominated,
                               ErrorResult& aRv);
};

} // namespace devtools
} // namespace mozilla

#endif // mozilla_devtools_DominatorTree__
