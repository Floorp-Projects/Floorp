/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef frontend_FunctionTree_h
#define frontend_FunctionTree_h

#include "mozilla/Attributes.h"

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
  explicit FunctionTree(JSContext* cx) : funbox_(nullptr), children_(cx) {}

  // Note: If we're using vector type, the pointer returned here
  // is only valid if the tree is only added to in DFS order
  //
  // Open to suggestions about how to do that better.
  FunctionTree* add(JSContext* cx) {
    if (!children_.emplaceBack(cx)) {
      return nullptr;
    }
    return &children_.back();
  }

  void reset() {
    funbox_ = nullptr;
    children_.clear();
  }

  FunctionBox* funbox() { return funbox_; }
  void setFunctionBox(FunctionBox* node) { funbox_ = node; }

  typedef bool (*FunctionTreeVisitorFunction)(ParserBase*, FunctionTree*);
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
// The holder mode can be eager or deferred:
//
// - In Eager mode, deferred items happens right away and the tree is not
//   constructed.
// - In Deferred mode, deferred items happens only when publishDeferredItems
//   is called.
//
// Note: Function trees point to function boxes, which only have the lifetime of
//       the BytecodeCompiler, so exercise caution when holding onto a
//       holder.
class FunctionTreeHolder {
 public:
  enum Mode { Eager, Deferred };

 private:
  FunctionTree treeRoot_;
  FunctionTree* currentParent_;
  Mode mode_;

 public:
  explicit FunctionTreeHolder(JSContext* cx, Mode mode = Mode::Eager)
      : treeRoot_(cx), currentParent_(&treeRoot_), mode_(mode) {}

  FunctionTree* getFunctionTree() { return &treeRoot_; }
  FunctionTree* getCurrentParent() { return currentParent_; }
  void setCurrentParent(FunctionTree* parent) { currentParent_ = parent; }

  bool isEager() { return mode_ == Mode::Eager; }
  bool isDeferred() { return mode_ == Mode::Deferred; }

  // When a parse has failed, we need to reset the root of the
  // function tree as we don't want a reparse to have old entries.
  void resetFunctionTree() {
    treeRoot_.reset();
    currentParent_ = &treeRoot_;
  }
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
