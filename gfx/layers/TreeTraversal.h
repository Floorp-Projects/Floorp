/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_layers_TreeTraversal_h
#define mozilla_layers_TreeTraversal_h

#include <queue>
#include <stack>

namespace mozilla {
namespace layers {


/*
 * Returned by |aAction| in ForEachNode. If the action returns
 * TraversalFlag::Skip, the node's children are not added to the traverrsal
 * stack. Otherwise, a return value of TraversalFlag::Continue indicates that
 * ForEachNode should traverse each of the node's children.
 */
enum class TraversalFlag { Skip, Continue };

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
 * Do a depth-first search of the tree rooted at |aRoot|, and return the
 * first visited node that satisfies |aCondition|, or nullptr if no such node
 * was found.
 *
 * |Node| should have methods GetLastChild() and GetPrevSibling().
 */
template <typename Node, typename Condition>
Node* DepthFirstSearch(Node* aRoot, const Condition& aCondition)
{
  if (!aRoot) {
    return nullptr;
  }

  std::stack<Node*> stack;
  stack.push(aRoot);
  while (!stack.empty()) {
    Node* node = stack.top();
    stack.pop();

    if (aCondition(node)) {
        return node;
    }

    for (Node* child = node->GetLastChild();
         child;
         child = child->GetPrevSibling()) {
      stack.push(child);
    }
  }

  return nullptr;
}

/*
 * Do a depth-first traversal of the tree rooted at |aRoot|, performing
 * |aAction| for each node.  |aAction| can return a TraversalFlag to determine
 * whether or not to omit the children of a particular node.
 *
 * If |aAction| does not return a TraversalFlag, it must return nothing.  There
 * is no ForEachNode instance handling types other than void or TraversalFlag.
 */
template <typename Node, typename Action>
auto ForEachNode(Node* aRoot, const Action& aAction) ->
typename EnableIf<IsSame<decltype(aAction(aRoot)), TraversalFlag>::value, void>::Type
{
  if (!aRoot) {
    return;
  }

  std::stack<Node*> stack;
  stack.push(aRoot);

  while (!stack.empty()) {
    Node* node = stack.top();
    stack.pop();

    TraversalFlag result = aAction(node);

    if (result == TraversalFlag::Continue) {
      for (Node* child = node->GetLastChild();
           child;
           child = child->GetPrevSibling()) {
        stack.push(child);
      }
    }
  }
}

template <typename Node, typename Action>
auto ForEachNode(Node* aRoot, const Action& aAction) ->
typename EnableIf<IsSame<decltype(aAction(aRoot)), void>::value, void>::Type
{
  if (!aRoot) {
    return;
  }

  std::stack<Node*> stack;
  stack.push(aRoot);

  while (!stack.empty()) {
    Node* node = stack.top();
    stack.pop();

    aAction(node);

    for (Node* child = node->GetLastChild();
         child;
         child = child->GetPrevSibling()) {
      stack.push(child);
    }
  }
}

}
}

#endif // mozilla_layers_TreeTraversal_h
