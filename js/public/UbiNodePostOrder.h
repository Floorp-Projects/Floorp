/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef js_UbiNodePostOrder_h
#define js_UbiNodePostOrder_h

#include "mozilla/DebugOnly.h"
#include "mozilla/Move.h"

#include "jsalloc.h"

#include "js/UbiNode.h"
#include "js/Utility.h"
#include "js/Vector.h"

namespace JS {
namespace ubi {

// A post-order depth-first traversal of `ubi::Node` graphs.
//
// NB: This traversal visits each node reachable from the start set exactly
// once, and does not visit edges at all. Therefore, this traversal would be a
// very poor choice for recording multiple paths to the same node, for example.
// If your analysis needs to consider edges, use `JS::ubi::BreadthFirst`
// instead.
//
// No GC may occur while an instance of `PostOrder` is live.
//
// The `Visitor` type provided to `PostOrder::traverse` must have the following
// member:
//
//   bool operator()(Node& node)
//
//     The visitor method. This method is called once for each `node` reachable
//     from the start set in post-order.
//
//     The visitor function should return true on success, or false if an error
//     occurs. A false return value terminates the traversal immediately, and
//     causes `PostOrder::traverse` to return false.
struct PostOrder {
  private:
    struct OriginAndEdges {
        Node       origin;
        EdgeVector edges;

        OriginAndEdges(const Node& node, EdgeVector&& edges)
          : origin(node)
          , edges(mozilla::Move(edges))
        { }

        OriginAndEdges(const OriginAndEdges& rhs) = delete;
        OriginAndEdges& operator=(const OriginAndEdges& rhs) = delete;

        OriginAndEdges(OriginAndEdges&& rhs)
          : origin(rhs.origin)
          , edges(mozilla::Move(rhs.edges))
        {
            MOZ_ASSERT(&rhs != this, "self-move disallowed");
        }

        OriginAndEdges& operator=(OriginAndEdges&& rhs) {
            this->~OriginAndEdges();
            new (this) OriginAndEdges(mozilla::Move(rhs));
            return *this;
        }
    };

    using Stack = js::Vector<OriginAndEdges, 256, js::SystemAllocPolicy>;
    using Set = js::HashSet<Node, js::DefaultHasher<Node>, js::SystemAllocPolicy>;

    JSRuntime*               rt;
    Set                      seen;
    Stack                    stack;
    mozilla::DebugOnly<bool> traversed;

  private:
    bool fillEdgesFromRange(EdgeVector& edges, UniquePtr<EdgeRange>& range) {
        MOZ_ASSERT(range);
        for ( ; !range->empty(); range->popFront()) {
            if (!edges.append(mozilla::Move(range->front())))
                return false;
        }
        return true;
    }

    bool pushForTraversing(const Node& node) {
        EdgeVector edges;
        auto range = node.edges(rt, /* wantNames */ false);
        return range &&
            fillEdgesFromRange(edges, range) &&
            stack.append(OriginAndEdges(node, mozilla::Move(edges)));
    }


  public:
    // Construct a post-order traversal object.
    //
    // The traversal asserts that no GC happens in its runtime during its
    // lifetime via the `AutoCheckCannotGC&` parameter. We do nothing with it,
    // other than require it to exist with a lifetime that encloses our own.
    PostOrder(JSRuntime* rt, AutoCheckCannotGC&)
      : rt(rt)
      , seen()
      , stack()
      , traversed(false)
    { }

    // Initialize this traversal object. Return false on OOM.
    bool init() { return seen.init(); }

    // Add `node` as a starting point for the traversal. You may add
    // as many starting points as you like. Returns false on OOM.
    bool addStart(const Node& node) {
        if (!seen.put(node))
            return false;
        return pushForTraversing(node);
    }

    // Traverse the graph in post-order, starting with the set of nodes passed
    // to `addStart` and applying `visitor::operator()` for each node in
    // the graph, as described above.
    //
    // This should be called only once per instance of this class.
    //
    // Return false on OOM or error return from `visitor::operator()`.
    template<typename Visitor>
    bool traverse(Visitor visitor) {
        MOZ_ASSERT(!traversed, "Can only traverse() once!");
        traversed = true;

        while (!stack.empty()) {
            auto& origin = stack.back().origin;
            auto& edges = stack.back().edges;

            if (edges.empty()) {
                if (!visitor(origin))
                    return false;
                stack.popBack();
                continue;
            }

            Edge edge = mozilla::Move(edges.back());
            edges.popBack();

            auto ptr = seen.lookupForAdd(edge.referent);
            // We've already seen this node, don't follow its edges.
            if (ptr)
                continue;

            // Mark the referent as seen and follow its edges.
            if (!seen.add(ptr, edge.referent) || !pushForTraversing(edge.referent))
                return false;
        }

        return true;
    }
};

} // namespace ubi
} // namespace JS

#endif // js_UbiNodePostOrder_h
