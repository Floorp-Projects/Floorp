/* -*-  Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/devtools/DeserializedNode.h"
#include "mozilla/devtools/HeapSnapshot.h"
#include "nsCRTGlue.h"

namespace mozilla {
namespace devtools {

DeserializedEdge::DeserializedEdge(DeserializedEdge&& rhs)
{
  referent = rhs.referent;
  name = rhs.name;
}

DeserializedEdge& DeserializedEdge::operator=(DeserializedEdge&& rhs)
{
  MOZ_ASSERT(&rhs != this);
  this->~DeserializedEdge();
  new(this) DeserializedEdge(std::move(rhs));
  return *this;
}

JS::ubi::Node
DeserializedNode::getEdgeReferent(const DeserializedEdge& edge)
{
  auto ptr = owner->nodes.lookup(edge.referent);
  MOZ_ASSERT(ptr);

  // `HashSets` only provide const access to their values, because mutating a
  // value might change its hash, rendering it unfindable in the set.
  // Unfortunately, the `ubi::Node` constructor requires a non-const pointer to
  // its referent.  However, the only aspect of a `DeserializedNode` we hash on
  // is its id, which can't be changed via `ubi::Node`, so this cast can't cause
  // the trouble `HashSet` is concerned a non-const reference would cause.
  return JS::ubi::Node(const_cast<DeserializedNode*>(&*ptr));
}

JS::ubi::StackFrame
DeserializedStackFrame::getParentStackFrame() const
{
  MOZ_ASSERT(parent.isSome());
  auto ptr = owner->frames.lookup(parent.ref());
  MOZ_ASSERT(ptr);
  // See above comment in DeserializedNode::getEdgeReferent about why this
  // const_cast is needed and safe.
  return JS::ubi::StackFrame(const_cast<DeserializedStackFrame*>(&*ptr));
}

} // namespace devtools
} // namespace mozilla

namespace JS {
namespace ubi {

using mozilla::devtools::DeserializedEdge;

const char16_t Concrete<DeserializedNode>::concreteTypeName[] =
  u"mozilla::devtools::DeserializedNode";

const char16_t*
Concrete<DeserializedNode>::typeName() const
{
  return get().typeName;
}

Node::Size
Concrete<DeserializedNode>::size(mozilla::MallocSizeOf mallocSizeof) const
{
  return get().size;
}

class DeserializedEdgeRange : public EdgeRange
{
  DeserializedNode* node;
  Edge              currentEdge;
  size_t            i;

  void settle() {
    if (i >= node->edges.length()) {
      front_ = nullptr;
      return;
    }

    auto& edge = node->edges[i];
    auto referent = node->getEdgeReferent(edge);
    currentEdge = Edge(edge.name
                ? NS_strdup(edge.name) : nullptr, referent);
    front_ = &currentEdge;
  }

public:
  explicit DeserializedEdgeRange(DeserializedNode& node)
    : node(&node)
    , i(0)
  {
    settle();
  }

  void popFront() override
  {
    i++;
    settle();
  }
};

StackFrame
Concrete<DeserializedNode>::allocationStack() const
{
  MOZ_ASSERT(hasAllocationStack());
  auto id = get().allocationStack.ref();
  auto ptr = get().owner->frames.lookup(id);
  MOZ_ASSERT(ptr);
  // See above comment in DeserializedNode::getEdgeReferent about why this
  // const_cast is needed and safe.
  return JS::ubi::StackFrame(const_cast<DeserializedStackFrame*>(&*ptr));
}


js::UniquePtr<EdgeRange>
Concrete<DeserializedNode>::edges(JSContext* cx, bool) const
{
  js::UniquePtr<DeserializedEdgeRange> range(js_new<DeserializedEdgeRange>(get()));

  if (!range)
    return nullptr;

  return js::UniquePtr<EdgeRange>(range.release());
}

StackFrame
ConcreteStackFrame<DeserializedStackFrame>::parent() const
{
  return get().parent.isNothing() ? StackFrame() : get().getParentStackFrame();
}

bool
ConcreteStackFrame<DeserializedStackFrame>::constructSavedFrameStack(
  JSContext* cx,
  MutableHandleObject outSavedFrameStack) const
{
  StackFrame f(&get());
  return ConstructSavedFrameStackSlow(cx, f, outSavedFrameStack);
}

} // namespace ubi
} // namespace JS
