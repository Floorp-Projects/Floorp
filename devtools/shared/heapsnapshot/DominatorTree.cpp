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

dom::Nullable<uint64_t>
DominatorTree::GetRetainedSize(uint64_t aNodeId, ErrorResult& aRv)
{
  JS::ubi::Node::Id id(aNodeId);
  auto node = mHeapSnapshot->getNodeById(id);
  if (node.isNothing())
    return dom::Nullable<uint64_t>();

  auto ccrt = CycleCollectedJSRuntime::Get();
  MOZ_ASSERT(ccrt);
  auto rt = ccrt->Runtime();
  MOZ_ASSERT(rt);
  auto mallocSizeOf = JS::dbg::GetDebuggerMallocSizeOf(rt);
  MOZ_ASSERT(mallocSizeOf);

  JS::ubi::Node::Size size = 0;
  if (!mDominatorTree.getRetainedSize(*node, mallocSizeOf, size)) {
    aRv.Throw(NS_ERROR_OUT_OF_MEMORY);
    return dom::Nullable<uint64_t>();
  }

  MOZ_ASSERT(size != 0,
             "The node should not have been unknown since we got it from the heap snapshot.");
  return dom::Nullable<uint64_t>(size);
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
