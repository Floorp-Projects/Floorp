/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef JoinSplitNodeDirection_h
#define JoinSplitNodeDirection_h

#include <iostream>

namespace mozilla {

// JoinNodesDirection is also affected to which one is new node at splitting
// a node because a couple of undo/redo.
enum class JoinNodesDirection {
  LeftNodeIntoRightNode,
  RightNodeIntoLeftNode,
};

static inline std::ostream& operator<<(std::ostream& aStream,
                                       JoinNodesDirection aJoinNodesDirection) {
  if (aJoinNodesDirection == JoinNodesDirection::LeftNodeIntoRightNode) {
    return aStream << "JoinNodesDirection::LeftNodeIntoRightNode";
  }
  if (aJoinNodesDirection == JoinNodesDirection::RightNodeIntoLeftNode) {
    return aStream << "JoinNodesDirection::RightNodeIntoLeftNode";
  }
  return aStream << "Invalid value";
}

// SplitNodeDirection is also affected to which one is removed at joining a
// node because a couple of undo/redo.
enum class SplitNodeDirection {
  LeftNodeIsNewOne,
  RightNodeIsNewOne,
};

static inline std::ostream& operator<<(std::ostream& aStream,
                                       SplitNodeDirection aSplitNodeDirection) {
  if (aSplitNodeDirection == SplitNodeDirection::LeftNodeIsNewOne) {
    return aStream << "SplitNodeDirection::LeftNodeIsNewOne";
  }
  if (aSplitNodeDirection == SplitNodeDirection::RightNodeIsNewOne) {
    return aStream << "SplitNodeDirection::RightNodeIsNewOne";
  }
  return aStream << "Invalid value";
}

}  // namespace mozilla

#endif  // JoinSplitNodeDirection_h
