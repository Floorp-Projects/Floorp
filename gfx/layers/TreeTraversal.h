/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_layers_TreeTraversal_h
#define mozilla_layers_TreeTraversal_h

namespace mozilla {
namespace layers {

#include <queue>
#include <stack>

/*
 * Do a breadth-first search of the tree rooted at |aRoot|, and return the
 * first visited node that satisfies |aCondition|, or nullptr if no such node
 * was found.
 *
 * |Node| should have methods GetLastChild() and GetPrevSibling().
 */
template <typename Node, typename Condition>
const Node* BreadthFirstSearch(const Node* aRoot, const Condition& aCondition)
{
  if (!aRoot) {
    return nullptr;
  }

  std::queue<const Node*> queue;
  queue.push(aRoot);
  while (!queue.empty()) {
    const Node* node = queue.front();
    queue.pop();

    if (aCondition(node)) {
      return node;
    }

    for (const Node* child = node->GetLastChild();
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
const Node* DepthFirstSearch(const Node* aRoot, const Condition& aCondition)
{
  if (!aRoot) {
    return nullptr;
  }

  std::stack<const Node*> stack;
  stack.push(aRoot);
  while (!stack.empty()) {
    const Node* node = stack.top();
    stack.pop();

    if (aCondition(node)) {
        return node;
    }

    for (const Node* child = node->GetLastChild();
         child;
         child = child->GetPrevSibling()) {
      stack.push(child);
    }
  }

  return nullptr;
}

}
}

#endif // mozilla_layers_TreeTraversal_h
