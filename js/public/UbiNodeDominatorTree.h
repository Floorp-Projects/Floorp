/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef js_UbiNodeDominatorTree_h
#define js_UbiNodeDominatorTree_h

#include "mozilla/DebugOnly.h"
#include "mozilla/Maybe.h"
#include "mozilla/Move.h"
#include "mozilla/UniquePtr.h"

#include "jsalloc.h"

#include "js/UbiNode.h"
#include "js/UbiNodePostOrder.h"
#include "js/Utility.h"
#include "js/Vector.h"

namespace JS {
namespace ubi {

/**
 * In a directed graph with a root node `R`, a node `A` is said to "dominate" a
 * node `B` iff every path from `R` to `B` contains `A`. A node `A` is said to
 * be the "immediate dominator" of a node `B` iff it dominates `B`, is not `B`
 * itself, and does not dominate any other nodes which also dominate `B` in
 * turn.
 *
 * If we take every node from a graph `G` and create a new graph `T` with edges
 * to each node from its immediate dominator, then `T` is a tree (each node has
 * only one immediate dominator, or none if it is the root). This tree is called
 * a "dominator tree".
 *
 * This class represents a dominator tree constructed from a `JS::ubi::Node`
 * heap graph. The domination relationship and dominator trees are useful tools
 * for analyzing heap graphs because they tell you:
 *
 *   - Exactly what could be reclaimed by the GC if some node `A` became
 *     unreachable: those nodes which are dominated by `A`,
 *
 *   - The "retained size" of a node in the heap graph, in contrast to its
 *     "shallow size". The "shallow size" is the space taken by a node itself,
 *     not counting anything it references. The "retained size" of a node is its
 *     shallow size plus the size of all the things that would be collected if
 *     the original node wasn't (directly or indirectly) referencing them. In
 *     other words, the retained size is the shallow size of a node plus the
 *     shallow sizes of every other node it dominates. For example, the root
 *     node in a binary tree might have a small shallow size that does not take
 *     up much space itself, but it dominates the rest of the binary tree and
 *     its retained size is therefore significant (assuming no external
 *     references into the tree).
 *
 * The simple, engineered algorithm presented in "A Simple, Fast Dominance
 * Algorithm" by Cooper el al[0] is used to find dominators and construct the
 * dominator tree. This algorithm runs in O(n^2) time, but is faster in practice
 * than alternative algorithms with better theoretical running times, such as
 * Lengauer-Tarjan which runs in O(e * log(n)). The big caveat to that statement
 * is that Cooper et al found it is faster in practice *on control flow graphs*
 * and I'm not convinced that this property also holds on *heap* graphs. That
 * said, the implementation of this algorithm is *much* simpler than
 * Lengauer-Tarjan and has been found to be fast enough at least for the time
 * being.
 *
 * [0]: http://www.cs.rice.edu/~keith/EMBED/dom.pdf
 */
class JS_PUBLIC_API(DominatorTree)
{
  private:
    // Type aliases.
    using NodeSet = js::HashSet<Node, js::DefaultHasher<Node>, js::SystemAllocPolicy>;
    using NodeSetPtr = mozilla::UniquePtr<NodeSet, JS::DeletePolicy<NodeSet>>;
    using PredecessorSets = js::HashMap<Node, NodeSetPtr, js::DefaultHasher<Node>,
                                        js::SystemAllocPolicy>;
    using NodeToIndexMap = js::HashMap<Node, uint32_t, js::DefaultHasher<Node>,
                                       js::SystemAllocPolicy>;

  private:
    // Data members.
    mozilla::Vector<Node> postOrder;
    NodeToIndexMap nodeToPostOrderIndex;
    mozilla::Vector<uint32_t> doms;

  private:
    // We use `UNDEFINED` as a sentinel value in the `doms` vector to signal
    // that we haven't found any dominators for the node at the corresponding
    // index in `postOrder` yet.
    static const uint32_t UNDEFINED = UINT32_MAX;

    DominatorTree(mozilla::Vector<Node>&& postOrder, NodeToIndexMap&& nodeToPostOrderIndex,
                  mozilla::Vector<uint32_t>&& doms)
        : postOrder(mozilla::Move(postOrder))
        , nodeToPostOrderIndex(mozilla::Move(nodeToPostOrderIndex))
        , doms(mozilla::Move(doms))
    { }

    static uint32_t intersect(mozilla::Vector<uint32_t>& doms, uint32_t finger1, uint32_t finger2) {
        while (finger1 != finger2) {
            if (finger1 < finger2)
                finger1 = doms[finger1];
            else if (finger2 < finger1)
                finger2 = doms[finger2];
        }
        return finger1;
    }

    // Do the post order traversal of the heap graph and populate our
    // predecessor sets.
    static bool doTraversal(JSRuntime* rt, AutoCheckCannotGC& noGC, const Node& root,
                            mozilla::Vector<Node>& postOrder, PredecessorSets& predecessorSets) {
        uint32_t nodeCount = 0;
        auto onNode = [&](const Node& node) {
            nodeCount++;
            if (MOZ_UNLIKELY(nodeCount == UINT32_MAX))
                return false;
            return postOrder.append(node);
        };

        auto onEdge = [&](const Node& origin, const Edge& edge) {
            auto p = predecessorSets.lookupForAdd(edge.referent);
            if (!p) {
                auto set = rt->make_unique<NodeSet>();
                if (!set ||
                    !set->init() ||
                    !predecessorSets.add(p, edge.referent, mozilla::Move(set)))
                {
                    return false;
                }
            }
            MOZ_ASSERT(p && p->value());
            return p->value()->put(origin);
        };

        PostOrder traversal(rt, noGC);
        return traversal.init() &&
               traversal.addStart(root) &&
               traversal.traverse(onNode, onEdge);
    }

    // Populates the given `map` with an entry for each node to its index in
    // `postOrder`.
    static bool mapNodesToTheirIndices(mozilla::Vector<Node>& postOrder, NodeToIndexMap& map) {
        MOZ_ASSERT(!map.initialized());
        MOZ_ASSERT(postOrder.length() < UINT32_MAX);
        uint32_t length = postOrder.length();
        if (!map.init(length))
            return false;
        for (uint32_t i = 0; i < length; i++)
            map.putNewInfallible(postOrder[i], i);
        return true;
    }

    // Convert the Node -> NodeSet predecessorSets to a index -> Vector<index>
    // form.
    static bool convertPredecessorSetsToVectors(
        const Node& root,
        mozilla::Vector<Node>& postOrder,
        PredecessorSets& predecessorSets,
        NodeToIndexMap& nodeToPostOrderIndex,
        mozilla::Vector<mozilla::Vector<uint32_t>>& predecessorVectors)
    {
        MOZ_ASSERT(postOrder.length() < UINT32_MAX);
        uint32_t length = postOrder.length();

        MOZ_ASSERT(predecessorVectors.length() == 0);
        if (!predecessorVectors.growBy(length))
            return false;

        for (uint32_t i = 0; i < length - 1; i++) {
            auto& node = postOrder[i];
            MOZ_ASSERT(node != root,
                       "Only the last node should be root, since this was a post order traversal.");

            auto ptr = predecessorSets.lookup(node);
            MOZ_ASSERT(ptr,
                       "Because this isn't the root, it had better have predecessors, or else how "
                       "did we even find it.");

            auto& predecessors = ptr->value();
            if (!predecessorVectors[i].reserve(predecessors->count()))
                return false;
            for (auto range = predecessors->all(); !range.empty(); range.popFront()) {
                auto ptr = nodeToPostOrderIndex.lookup(range.front());
                MOZ_ASSERT(ptr);
                predecessorVectors[i].infallibleAppend(ptr->value());
            }
        }
        predecessorSets.finish();
        return true;
    }

    // Initialize `doms` such that the immediate dominator of the `root` is the
    // `root` itself and all others are `UNDEFINED`.
    static bool initializeDominators(mozilla::Vector<uint32_t>& doms, uint32_t length) {
        MOZ_ASSERT(doms.length() == 0);
        if (!doms.growByUninitialized(length))
            return false;
        doms[length - 1] = length - 1;
        for (uint32_t i = 0; i < length - 1; i++)
            doms[i] = UNDEFINED;
        return true;
    }

    void assertSanity() const {
        MOZ_ASSERT(postOrder.length() == doms.length());
        MOZ_ASSERT(postOrder.length() == nodeToPostOrderIndex.count());
    }

  public:
    // DominatorTree is not copy-able.
    DominatorTree(const DominatorTree&) = delete;
    DominatorTree& operator=(const DominatorTree&) = delete;

    // DominatorTree is move-able.
    DominatorTree(DominatorTree&& rhs)
      : postOrder(mozilla::Move(rhs.postOrder))
      , nodeToPostOrderIndex(mozilla::Move(rhs.nodeToPostOrderIndex))
      , doms(mozilla::Move(rhs.doms))
    {
        MOZ_ASSERT(this != &rhs, "self-move is not allowed");
    }
    DominatorTree& operator=(DominatorTree&& rhs) {
        this->~DominatorTree();
        new (this) DominatorTree(mozilla::Move(rhs));
        return *this;
    }

    /**
     * Construct a `DominatorTree` of the heap graph visible from `root`. The
     * `root` is also used as the root of the resulting dominator tree.
     *
     * The resulting `DominatorTree` instance must not outlive the
     * `JS::ubi::Node` graph it was constructed from.
     *
     *   - For `JS::ubi::Node` graphs backed by the live heap graph, this means
     *     that the `DominatorTree`'s lifetime _must_ be contained within the
     *     scope of the provided `AutoCheckCannotGC` reference because a GC will
     *     invalidate the nodes.
     *
     *   - For `JS::ubi::Node` graphs backed by some other offline structure
     *     provided by the embedder, the resulting `DominatorTree`'s lifetime is
     *     bounded by that offline structure's lifetime.
     *
     * In practice, this means that within SpiderMonkey we must treat
     * `DominatorTree` as if it were backed by the live heap graph and trust
     * that embedders with knowledge of the graph's implementation will do the
     * Right Thing.
     *
     * Returns `mozilla::Nothing()` on OOM failure. It is the caller's
     * responsibility to handle and report the OOM.
     */
    static mozilla::Maybe<DominatorTree>
    Create(JSRuntime* rt, AutoCheckCannotGC& noGC, const Node& root) {
        mozilla::Vector<Node> postOrder;
        PredecessorSets predecessorSets;
        if (!predecessorSets.init() || !doTraversal(rt, noGC, root, postOrder, predecessorSets))
            return mozilla::Nothing();

        MOZ_ASSERT(postOrder.length() < UINT32_MAX);
        uint32_t length = postOrder.length();
        MOZ_ASSERT(postOrder[length - 1] == root);

        // From here on out we wish to avoid hash table lookups, and we use
        // indices into `postOrder` instead of actual nodes wherever
        // possible. This greatly improves the performance of this
        // implementation, but we have to pay a little bit of upfront cost to
        // convert our data structures to play along first.

        NodeToIndexMap nodeToPostOrderIndex;
        if (!mapNodesToTheirIndices(postOrder, nodeToPostOrderIndex))
            return mozilla::Nothing();

        mozilla::Vector<mozilla::Vector<uint32_t>> predecessorVectors;
        if (!convertPredecessorSetsToVectors(root, postOrder, predecessorSets, nodeToPostOrderIndex,
                                             predecessorVectors))
            return mozilla::Nothing();

        mozilla::Vector<uint32_t> doms;
        if (!initializeDominators(doms, length))
            return mozilla::Nothing();

        bool changed = true;
        while (changed) {
            changed = false;

            // Iterate over the non-root nodes in reverse post order.
            for (uint32_t indexPlusOne = length - 1; indexPlusOne > 0; indexPlusOne--) {
                MOZ_ASSERT(postOrder[indexPlusOne - 1] != root);

                // Take the intersection of every predecessor's dominator set;
                // that is the current best guess at the immediate dominator for
                // this node.

                uint32_t newIDomIdx = UNDEFINED;

                auto& predecessors = predecessorVectors[indexPlusOne - 1];
                auto range = predecessors.all();
                for ( ; !range.empty(); range.popFront()) {
                    auto idx = range.front();
                    if (doms[idx] != UNDEFINED) {
                        newIDomIdx = idx;
                        break;
                    }
                }

                MOZ_ASSERT(newIDomIdx != UNDEFINED,
                           "Because the root is initialized to dominate itself and is the first "
                           "node in every path, there must exist a predecessor to this node that "
                           "also has a dominator.");

                for ( ; !range.empty(); range.popFront()) {
                    auto idx = range.front();
                    if (doms[idx] != UNDEFINED)
                        newIDomIdx = intersect(doms, newIDomIdx, idx);
                }

                // If the immediate dominator changed, we will have to do
                // another pass of the outer while loop to continue the forward
                // dataflow.
                if (newIDomIdx != doms[indexPlusOne - 1]) {
                    doms[indexPlusOne - 1] = newIDomIdx;
                    changed = true;
                }
            }
        }

        return mozilla::Some(DominatorTree(mozilla::Move(postOrder),
                                           mozilla::Move(nodeToPostOrderIndex),
                                           mozilla::Move(doms)));
    }

    /**
     * Return the immediate dominator of the given `node`. If `node` was not
     * reachable from the `root` that this dominator tree was constructed from,
     * then return the null `JS::ubi::Node`.
     */
    Node getImmediateDominator(const Node& node) const {
        assertSanity();
        auto ptr = nodeToPostOrderIndex.lookup(node);
        if (!ptr)
            return Node();

        auto idx = ptr->value();
        MOZ_ASSERT(idx < postOrder.length());
        return postOrder[doms[idx]];
    }
};

} // namespace ubi
} // namespace JS

#endif // js_UbiNodeDominatorTree_h
