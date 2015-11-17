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
 * |aAction| for each node, returning a TraversalFlag to determine behavior for
 * that node's children. If a node is determined to be ineligible (by |aAction|
 * returning TraversalFlag::Skip), its children are skipped by the function.
 *
 * |Node| should have methods GetLastChild() and GetPrevSibling().
 * |aAction| must return a TraversalFlag. Bear in mind that the TraversalFlag
 * only dictates the behavior for children of a node, not the node itself. To
 * skip the node itself, the logic returning TraversalFlag::Skip should be
 * performed before the node is manipulated in any way.
 */
template <typename Node, typename Action>
void ForEachNode(Node* aRoot, const Action& aAction)
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

}
}

#endif // mozilla_layers_TreeTraversal_h
