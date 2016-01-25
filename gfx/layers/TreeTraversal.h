/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_layers_TreeTraversal_h
#define mozilla_layers_TreeTraversal_h

#include <queue>

namespace mozilla {
namespace layers {


/*
 * Returned by |aPostAction| and |aPreAction| in ForEachNode, indicates
 * the behavior to follow either action:
 *
 * TraversalFlag::Skip - the node's children are not traversed. If this
 * flag is returned by |aPreAction|, |aPostAction| is skipped for the
 * current node, as well.
 * TraversalFlag::Continue - traversal continues normally.
 * TraversalFlag::Abort - traversal stops immediately.
 */
enum class TraversalFlag { Skip, Continue, Abort };

/*
 * Do a depth-first traversal of the tree rooted at |aRoot|, performing
 * |aPreAction| before traversal of children and |aPostAction| after.
 *
 * Returns true if traversal aborted, false if continued normally. If
 * TraversalFlag::Skip is returned in |aPreAction|, then |aPostAction|
 * is not performed.
 */
template <typename Node, typename PreAction, typename PostAction>
static auto ForEachNode(Node* aRoot, const PreAction& aPreAction, const PostAction& aPostAction) ->
typename EnableIf<IsSame<decltype(aPreAction(aRoot)), TraversalFlag>::value &&
                  IsSame<decltype(aPostAction(aRoot)),TraversalFlag>::value, bool>::Type
{
  if (!aRoot) {
    return false;
  }

  TraversalFlag result = aPreAction(aRoot);

  if (result == TraversalFlag::Abort) {
    return true;
  }

  if (result == TraversalFlag::Continue) {
    for (Node* child = aRoot->GetLastChild();
         child;
         child = child->GetPrevSibling()) {
      bool abort = ForEachNode(child, aPreAction, aPostAction);
      if (abort) {
        return true;
      }
    }

    result = aPostAction(aRoot);

    if (result == TraversalFlag::Abort) {
      return true;
    }
  }

  return false;
}

/*
 * Do a depth-first traversal of the tree rooted at |aRoot|, performing
 * |aPreAction| before traversal of children and |aPostAction| after.
 */
template <typename Node, typename PreAction, typename PostAction>
static auto ForEachNode(Node* aRoot, const PreAction& aPreAction, const PostAction& aPostAction) ->
typename EnableIf<IsSame<decltype(aPreAction(aRoot)), void>::value &&
                  IsSame<decltype(aPostAction(aRoot)),void>::value, void>::Type
{
  if (!aRoot) {
    return;
  }

  aPreAction(aRoot);

  for (Node* child = aRoot->GetLastChild();
       child;
       child = child->GetPrevSibling()) {
    ForEachNode(child, aPreAction, aPostAction);
  }

  aPostAction(aRoot);
}

/*
 * ForEachNode pre-order traversal, using TraversalFlag.
 */
template <typename Node, typename PreAction>
auto ForEachNode(Node* aRoot, const PreAction& aPreAction) ->
typename EnableIf<IsSame<decltype(aPreAction(aRoot)), TraversalFlag>::value, bool>::Type
{
  return ForEachNode(aRoot, aPreAction, [](Node* aNode){ return TraversalFlag::Continue; });
}

/*
 * ForEachNode pre-order, not using TraversalFlag.
 */
template <typename Node, typename PreAction>
auto ForEachNode(Node* aRoot, const PreAction& aPreAction) ->
typename EnableIf<IsSame<decltype(aPreAction(aRoot)), void>::value, void>::Type
{
  ForEachNode(aRoot, aPreAction, [](Node* aNode){});
}

/*
 * ForEachNode post-order traversal, using TraversalFlag.
 */
template <typename Node, typename PostAction>
auto ForEachNodePostOrder(Node* aRoot, const PostAction& aPostAction) ->
typename EnableIf<IsSame<decltype(aPostAction(aRoot)), TraversalFlag>::value, bool>::Type
{
  return ForEachNode(aRoot, [](Node* aNode){ return TraversalFlag::Continue; }, aPostAction);
}

/*
 * ForEachNode post-order, not using TraversalFlag.
 */
template <typename Node, typename PostAction>
auto ForEachNodePostOrder(Node* aRoot, const PostAction& aPostAction) ->
typename EnableIf<IsSame<decltype(aPostAction(aRoot)), void>::value, void>::Type
{
  ForEachNode(aRoot, [](Node* aNode){}, aPostAction);
}

/*
 * Do a breadth-first search of the tree rooted at |aRoot|, and return the
 * first visited node that satisfies |aCondition|, or nullptr if no such node
 * was found.
 *
 * |Node| should have methods GetLastChild() and GetPrevSibling().
 */
template <typename Node, typename Condition>
Node* BreadthFirstSearch(Node* aRoot, const Condition& aCondition)
{
  if (!aRoot) {
    return nullptr;
  }

  std::queue<Node*> queue;
  queue.push(aRoot);
  while (!queue.empty()) {
    Node* node = queue.front();
    queue.pop();

    if (aCondition(node)) {
      return node;
    }

    for (Node* child = node->GetLastChild();
         child;
         child = child->GetPrevSibling()) {
      queue.push(child);
    }
  }

  return nullptr;
}

/*
 * Do a pre-order, depth-first search of the tree rooted at |aRoot|, and
 * return the first visited node that satisfies |aCondition|, or nullptr
 * if no such node was found.
 *
 * |Node| should have methods GetLastChild() and GetPrevSibling().
 */
template <typename Node, typename Condition>
Node* DepthFirstSearch(Node* aRoot, const Condition& aCondition)
{
  Node* result = nullptr;

  ForEachNode(aRoot,
      [&aCondition, &result](Node* aNode)
      {
        if (aCondition(aNode)) {
          result = aNode;
          return TraversalFlag::Abort;
        }

        return TraversalFlag::Continue;
      });

  return result;
}

/*
 * Perform a post-order, depth-first search starting at aRoot.
 */
template <typename Node, typename Condition>
Node* DepthFirstSearchPostOrder(Node* aRoot, const Condition& aCondition)
{
  Node* result = nullptr;

  ForEachNodePostOrder(aRoot,
      [&aCondition, &result](Node* aNode)
      {
        if (aCondition(aNode)) {
          result = aNode;
          return TraversalFlag::Abort;
        }

        return TraversalFlag::Continue;
      });

  return result;
}

}
}

#endif // mozilla_layers_TreeTraversal_h
