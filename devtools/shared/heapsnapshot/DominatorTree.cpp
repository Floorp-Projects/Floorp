/* -*-  Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/devtools/DominatorTree.h"
#include "js/Debug.h"
#include "mozilla/CycleCollectedJSRuntime.h"
#include "mozilla/dom/DominatorTreeBinding.h"

namespace mozilla {
namespace devtools {

static MallocSizeOf
getCurrentThreadDebuggerMallocSizeOf()
{
  auto ccrt = CycleCollectedJSRuntime::Get();
  MOZ_ASSERT(ccrt);
  auto rt = ccrt->Runtime();
  MOZ_ASSERT(rt);
  auto mallocSizeOf = JS::dbg::GetDebuggerMallocSizeOf(rt);
  MOZ_ASSERT(mallocSizeOf);
  return mallocSizeOf;
}

dom::Nullable<uint64_t>
DominatorTree::GetRetainedSize(uint64_t aNodeId, ErrorResult& aRv)
{
  JS::ubi::Node::Id id(aNodeId);
  auto node = mHeapSnapshot->getNodeById(id);
  if (node.isNothing())
    return dom::Nullable<uint64_t>();

  auto mallocSizeOf = getCurrentThreadDebuggerMallocSizeOf();
  JS::ubi::Node::Size size = 0;
  if (!mDominatorTree.getRetainedSize(*node, mallocSizeOf, size)) {
    aRv.Throw(NS_ERROR_OUT_OF_MEMORY);
    return dom::Nullable<uint64_t>();
  }

  MOZ_ASSERT(size != 0,
             "The node should not have been unknown since we got it from the heap snapshot.");
  return dom::Nullable<uint64_t>(size);
}

struct NodeAndRetainedSize
{
  JS::ubi::Node mNode;
  JS::ubi::Node::Size mSize;

  NodeAndRetainedSize(const JS::ubi::Node& aNode, JS::ubi::Node::Size aSize)
    : mNode(aNode)
    , mSize(aSize)
  { }

  struct Comparator
  {
    static bool
    Equals(const NodeAndRetainedSize& aLhs, const NodeAndRetainedSize& aRhs)
    {
      return aLhs.mSize == aRhs.mSize;
    }

    static bool
    LessThan(const NodeAndRetainedSize& aLhs, const NodeAndRetainedSize& aRhs)
    {
      // Use > because we want to sort from greatest to least retained size.
      return aLhs.mSize > aRhs.mSize;
    }
  };
};

void
DominatorTree::GetImmediatelyDominated(uint64_t aNodeId,
                                       dom::Nullable<nsTArray<uint64_t>>& aOutResult,
                                       ErrorResult& aRv)
{
  MOZ_ASSERT(aOutResult.IsNull());

  JS::ubi::Node::Id id(aNodeId);
  Maybe<JS::ubi::Node> node = mHeapSnapshot->getNodeById(id);
  if (node.isNothing())
    return;

  // Get all immediately dominated nodes and their retained sizes.
  MallocSizeOf mallocSizeOf = getCurrentThreadDebuggerMallocSizeOf();
  Maybe<JS::ubi::DominatorTree::DominatedSetRange> range = mDominatorTree.getDominatedSet(*node);
  MOZ_ASSERT(range.isSome(), "The node should be known, since we got it from the heap snapshot.");
  size_t length = range->length();
  nsTArray<NodeAndRetainedSize> dominatedNodes(length);
  for (const JS::ubi::Node& dominatedNode : *range) {
    JS::ubi::Node::Size retainedSize = 0;
    if (NS_WARN_IF(!mDominatorTree.getRetainedSize(dominatedNode, mallocSizeOf, retainedSize))) {
      aRv.Throw(NS_ERROR_OUT_OF_MEMORY);
      return;
    }
    MOZ_ASSERT(retainedSize != 0,
               "retainedSize should not be zero since we know the node is in the dominator tree.");

    dominatedNodes.AppendElement(NodeAndRetainedSize(dominatedNode, retainedSize));
  }

  // Sort them by retained size.
  NodeAndRetainedSize::Comparator comparator;
  dominatedNodes.Sort(comparator);

  // Fill the result with the nodes' ids.
  JS::ubi::Node root = mDominatorTree.root();
  aOutResult.SetValue(nsTArray<uint64_t>(length));
  for (const NodeAndRetainedSize& entry : dominatedNodes) {
    // The root dominates itself, but we don't want to expose that to JS.
    if (entry.mNode == root)
      continue;

    aOutResult.Value().AppendElement(entry.mNode.identifier());
  }
}


/*** Cycle Collection Boilerplate *****************************************************************/

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(DominatorTree, mParent, mHeapSnapshot)

NS_IMPL_CYCLE_COLLECTING_ADDREF(DominatorTree)
NS_IMPL_CYCLE_COLLECTING_RELEASE(DominatorTree)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(DominatorTree)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

/* virtual */ JSObject*
DominatorTree::WrapObject(JSContext* aCx, JS::HandleObject aGivenProto)
{
  return dom::DominatorTreeBinding::Wrap(aCx, this, aGivenProto);
}

} // namespace devtools
} // namespace mozilla
