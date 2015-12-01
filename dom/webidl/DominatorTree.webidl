/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 */

typedef unsigned long long NodeId;
typedef unsigned long long NodeSize;

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
 * This interface represents a dominator tree constructed from a HeapSnapshot's
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
 */
[ChromeOnly, Exposed=(Window,System,Worker)]
interface DominatorTree {
  /**
   * The `NodeId` for the root of the dominator tree. This is a "meta-root" in
   * that it has an edge to each GC root in the heap snapshot this dominator
   * tree was created from.
   */
  readonly attribute NodeId root;

  /**
   * Get the retained size of the node with the given id. If given an invalid
   * id, null is returned. Throws an error on OOM.
   */
  [Throws]
  NodeSize? getRetainedSize(NodeId node);

  /**
   * Get the set of ids of nodes immediately dominated by the node with the
   * given id. The resulting array is sorted by greatest to least retained
   * size. If given an invalid id, null is returned. Throws an error on OOM.
   */
  [Throws]
  sequence<NodeId>? getImmediatelyDominated(NodeId node);
};
