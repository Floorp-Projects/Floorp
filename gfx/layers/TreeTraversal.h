/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
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
 * Iterator types to be specified in traversal function calls:
 *
 * ForwardIterator - for nodes using GetFirstChild() and GetNextSibling()
 * ReverseIterator - for nodes using GetLastChild() and GetPrevSibling()
 */
class ForwardIterator {
 public:
  template <typename Node>
  static Node* FirstChild(Node* n) {
    return n->GetFirstChild();
  }
  template <typename Node>
  static Node* NextSibling(Node* n) {
    return n->GetNextSibling();
  }
  template <typename Node>
  static Node FirstChild(Node n) {
    return n.GetFirstChild();
  }
  template <typename Node>
  static Node NextSibling(Node n) {
    return n.GetNextSibling();
  }
};
class ReverseIterator {
 public:
  template <typename Node>
  static Node* FirstChild(Node* n) {
    return n->GetLastChild();
  }
  template <typename Node>
  static Node* NextSibling(Node* n) {
    return n->GetPrevSibling();
  }
  template <typename Node>
  static Node FirstChild(Node n) {
    return n.GetLastChild();
  }
  template <typename Node>
  static Node NextSibling(Node n) {
    return n.GetPrevSibling();
  }
};

/*
 * Do a depth-first traversal of the tree rooted at |aRoot|, performing
 * |aPreAction| before traversal of children and |aPostAction| after.
 *
 * Returns true if traversal aborted, false if continued normally. If
 * TraversalFlag::Skip is returned in |aPreAction|, then |aPostAction|
 * is not performed.
 *
 * |Iterator| should have static methods named NextSibling() and FirstChild()
 * that accept an argument of type Node. For convenience, classes
 * |ForwardIterator| and |ReverseIterator| are provided which implement these
 * methods as GetNextSibling()/GetFirstChild() and
 * GetPrevSibling()/GetLastChild(), respectively.
 */
template <typename Iterator, typename Node, typename PreAction,
          typename PostAction>
static auto ForEachNode(Node aRoot, const PreAction& aPreAction,
                        const PostAction& aPostAction) ->
    typename EnableIf<
        IsSame<decltype(aPreAction(aRoot)), TraversalFlag>::value &&
            IsSame<decltype(aPostAction(aRoot)), TraversalFlag>::value,
        bool>::Type {
  if (!aRoot) {
    return false;
  }

  TraversalFlag result = aPreAction(aRoot);

  if (result == TraversalFlag::Abort) {
    return true;
  }

  if (result == TraversalFlag::Continue) {
    for (Node child = Iterator::FirstChild(aRoot); child;
         child = Iterator::NextSibling(child)) {
      bool abort = ForEachNode<Iterator>(child, aPreAction, aPostAction);
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
template <typename Iterator, typename Node, typename PreAction,
          typename PostAction>
static auto ForEachNode(Node aRoot, const PreAction& aPreAction,
                        const PostAction& aPostAction) ->
    typename EnableIf<IsSame<decltype(aPreAction(aRoot)), void>::value &&
                          IsSame<decltype(aPostAction(aRoot)), void>::value,
                      void>::Type {
  if (!aRoot) {
    return;
  }

  aPreAction(aRoot);

  for (Node child = Iterator::FirstChild(aRoot); child;
       child = Iterator::NextSibling(child)) {
    ForEachNode<Iterator>(child, aPreAction, aPostAction);
  }

  aPostAction(aRoot);
}

/*
 * ForEachNode pre-order traversal, using TraversalFlag.
 */
template <typename Iterator, typename Node, typename PreAction>
auto ForEachNode(Node aRoot, const PreAction& aPreAction) ->
    typename EnableIf<IsSame<decltype(aPreAction(aRoot)), TraversalFlag>::value,
                      bool>::Type {
  return ForEachNode<Iterator>(
      aRoot, aPreAction, [](Node aNode) { return TraversalFlag::Continue; });
}

/*
 * ForEachNode pre-order, not using TraversalFlag.
 */
template <typename Iterator, typename Node, typename PreAction>
auto ForEachNode(Node aRoot, const PreAction& aPreAction) ->
    typename EnableIf<IsSame<decltype(aPreAction(aRoot)), void>::value,
                      void>::Type {
  ForEachNode<Iterator>(aRoot, aPreAction, [](Node aNode) {});
}

/*
 * ForEachNode post-order traversal, using TraversalFlag.
 */
template <typename Iterator, typename Node, typename PostAction>
auto ForEachNodePostOrder(Node aRoot, const PostAction& aPostAction) ->
    typename EnableIf<
        IsSame<decltype(aPostAction(aRoot)), TraversalFlag>::value,
        bool>::Type {
  return ForEachNode<Iterator>(
      aRoot, [](Node aNode) { return TraversalFlag::Continue; }, aPostAction);
}

/*
 * ForEachNode post-order, not using TraversalFlag.
 */
template <typename Iterator, typename Node, typename PostAction>
auto ForEachNodePostOrder(Node aRoot, const PostAction& aPostAction) ->
    typename EnableIf<IsSame<decltype(aPostAction(aRoot)), void>::value,
                      void>::Type {
  ForEachNode<Iterator>(
      aRoot, [](Node aNode) {}, aPostAction);
}

/*
 * Do a breadth-first search of the tree rooted at |aRoot|, and return the
 * first visited node that satisfies |aCondition|, or nullptr if no such node
 * was found.
 *
 * |Iterator| and |Node| have all the same requirements seen in ForEachNode()'s
 * definition, but in addition to those, |Node| must be able to express a null
 * value, returned from Node()
 */
template <typename Iterator, typename Node, typename Condition>
Node BreadthFirstSearch(Node aRoot, const Condition& aCondition) {
  if (!aRoot) {
    return Node();
  }

  std::queue<Node> queue;
  queue.push(aRoot);
  while (!queue.empty()) {
    Node node = queue.front();
    queue.pop();

    if (aCondition(node)) {
      return node;
    }

    for (Node child = Iterator::FirstChild(node); child;
         child = Iterator::NextSibling(child)) {
      queue.push(child);
    }
  }

  return Node();
}

/*
 * Do a pre-order, depth-first search of the tree rooted at |aRoot|, and
 * return the first visited node that satisfies |aCondition|, or nullptr
 * if no such node was found.
 *
 * |Iterator| and |Node| have all the same requirements seen in ForEachNode()'s
 * definition, but in addition to those, |Node| must be able to express a null
 * value, returned from Node().
 */
template <typename Iterator, typename Node, typename Condition>
Node DepthFirstSearch(Node aRoot, const Condition& aCondition) {
  Node result = Node();

  ForEachNode<Iterator>(aRoot, [&aCondition, &result](Node aNode) {
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
 *
 * |Iterator| and |Node| have all the same requirements seen in ForEachNode()'s
 * definition, but in addition to those, |Node| must be able to express a null
 * value, returned from Node().
 */
template <typename Iterator, typename Node, typename Condition>
Node DepthFirstSearchPostOrder(Node aRoot, const Condition& aCondition) {
  Node result = Node();

  ForEachNodePostOrder<Iterator>(aRoot, [&aCondition, &result](Node aNode) {
    if (aCondition(aNode)) {
      result = aNode;
      return TraversalFlag::Abort;
    }

    return TraversalFlag::Continue;
  });

  return result;
}

}  // namespace layers
}  // namespace mozilla

#endif  // mozilla_layers_TreeTraversal_h
