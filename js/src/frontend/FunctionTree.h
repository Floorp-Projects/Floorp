/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef frontend_FunctionTree_h
#define frontend_FunctionTree_h

#include "mozilla/Attributes.h"
#include "jsfriendapi.h"

#include "js/Vector.h"

namespace js {
namespace frontend {

class FunctionBox;
class ParserBase;

// A tree of function nodes pointing to a FunctionBox and all its
// nested inner functions.
class FunctionTree {
  FunctionBox* funbox_;

  Vector<FunctionTree> children_;

 public:
  FunctionTree(JSContext* cx, FunctionBox* funbox)
      : funbox_(funbox), children_(cx) {}

  // Note: If we're using vector type, the pointer returned here
  // is only valid if the tree is only added to in DFS order
  //
  // Open to suggestions about how to do that better.
  FunctionTree* add(JSContext* cx, FunctionBox* funbox) {
    if (!children_.emplaceBack(cx, funbox)) {
      return nullptr;
    }
    return &children_.back();
  }

  FunctionBox* funbox() { return funbox_; }

  using FunctionTreeVisitorFunction = bool (*)(ParserBase*, FunctionTree*);
  bool visitRecursively(JSContext* cx, ParserBase* parser,
                        FunctionTreeVisitorFunction func) {
    if (!CheckRecursionLimit(cx)) {
      return false;
    }

    for (auto& child : children_) {
      if (!child.visitRecursively(cx, parser, func)) {
        return false;
      }
    }

    return func(parser, this);
  }

  void dump(JSContext* cx) { dump(cx, *this, 1); }

 private:
  static void dump(JSContext* cx, FunctionTree& node, int indent);
};

// Owner of a function tree
//
// Note: Function trees point to function boxes, which only have the lifetime of
//       the BytecodeCompiler, so exercise caution when holding onto a
//       holder.
class FunctionTreeHolder {
 private:
  FunctionTree treeRoot_;
  FunctionTree* currentParent_;

 public:
  explicit FunctionTreeHolder(JSContext* cx)
      : treeRoot_(cx, nullptr), currentParent_(&treeRoot_) {}

  FunctionTree* getFunctionTree() { return &treeRoot_; }
  FunctionTree* getCurrentParent() { return currentParent_; }
  void setCurrentParent(FunctionTree* parent) { currentParent_ = parent; }
};

// A class used to maintain our function tree as ParseContexts are
// pushed and popped.
class MOZ_RAII AutoPushTree {
  FunctionTreeHolder& holder_;
  FunctionTree* oldParent_ = nullptr;

 public:
  explicit AutoPushTree(FunctionTreeHolder& holder);
  ~AutoPushTree();

  bool init(JSContext* cx, FunctionBox* box);
};

}  // namespace frontend
}  // namespace js

#endif
