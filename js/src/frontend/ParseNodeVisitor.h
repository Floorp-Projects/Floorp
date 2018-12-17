/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef frontend_ParseNodeVisitor_h
#define frontend_ParseNodeVisitor_h

#include "mozilla/Assertions.h"
#include "mozilla/Attributes.h"

#include "jsfriendapi.h"

#include "frontend/ParseNode.h"

namespace js {
namespace frontend {

/**
 * Utility class for walking a JS AST.
 *
 * Simple usage:
 *
 *     class HowTrueVisitor : public ParseNodeVisitor<HowTrueVisitor> {
 *     public:
 *       bool visitTrue(ParseNode*& pn) {
 *         std::cout << "How true.\n";
 *         return true;
 *       }
 *       bool visitClass(ParseNode*& pn) {
 *         // The base-class implementation of each visit method
 *         // simply visits the node's children. So the subclass
 *         // gets to decide whether to descend into a subtree
 *         // and can do things either before or after:
 *         std::cout << "How classy.\n";
 *         return ParseNodeVisitor::visitClass(pn);
 *       }
 *     };
 *
 *     HowTrueVisitor v;
 *     v.visit(programRootNode);  // walks the entire tree
 *
 * Note that the Curiously Recurring Template Pattern is used for performance,
 * as it eliminates the need for virtual method calls. Some rough testing shows
 * about a 12% speedup in the FoldConstants.cpp pass.
 * https://en.wikipedia.org/wiki/Curiously_recurring_template_pattern
 */
template <typename Super>
class ParseNodeVisitor {
 public:
  JSContext* cx;

  explicit ParseNodeVisitor(JSContext* cx) : cx(cx) {}

  MOZ_MUST_USE bool visit(ParseNode*& pn) {
    if (!CheckRecursionLimit(cx)) {
      return false;
    }

    switch (pn->getKind()) {
#define VISIT_CASE(KIND, _arity) \
  case ParseNodeKind::KIND:      \
    return static_cast<Super*>(this)->visit##KIND(pn);
      FOR_EACH_PARSE_NODE_KIND(VISIT_CASE)
#undef VISIT_CASE
      default:
        MOZ_CRASH("invalid node kind");
    }
  }

#define VISIT_METHOD(KIND, ARITY)                 \
  MOZ_MUST_USE bool visit##KIND(ParseNode*& pn) { \
    return visit_##ARITY##_children(pn);          \
  }
  FOR_EACH_PARSE_NODE_KIND(VISIT_METHOD)
#undef VISIT_METHOD

 private:
  MOZ_MUST_USE bool visit_PN_NULLARY_children(ParseNode*& pn) { return true; }

  MOZ_MUST_USE bool visit_PN_UNARY_children(ParseNode*& pn) {
    UnaryNode& node = pn->as<UnaryNode>();
    if (node.kid()) {
      if (!visit(*node.unsafeKidReference())) {
        return false;
      }
    }
    return true;
  }

  MOZ_MUST_USE bool visit_PN_BINARY_children(ParseNode*& pn) {
    BinaryNode& node = pn->as<BinaryNode>();
    if (node.left()) {
      if (!visit(*node.unsafeLeftReference())) {
        return false;
      }
    }
    if (node.right()) {
      if (!visit(*node.unsafeRightReference())) {
        return false;
      }
    }
    return true;
  }

  MOZ_MUST_USE bool visit_PN_TERNARY_children(ParseNode*& pn) {
    TernaryNode& node = pn->as<TernaryNode>();
    if (node.kid1()) {
      if (!visit(*node.unsafeKid1Reference())) {
        return false;
      }
    }
    if (node.kid2()) {
      if (!visit(*node.unsafeKid2Reference())) {
        return false;
      }
    }
    if (node.kid3()) {
      if (!visit(*node.unsafeKid3Reference())) {
        return false;
      }
    }
    return true;
  }

  MOZ_MUST_USE bool visit_PN_CODE_children(ParseNode*& pn) {
    CodeNode& node = pn->as<CodeNode>();

    // Note: body is null for lazily-parsed functions.
    if (node.body()) {
      if (!visit(*node.unsafeBodyReference())) {
        return false;
      }
    }
    return true;
  }

  MOZ_MUST_USE bool visit_PN_LIST_children(ParseNode*& pn) {
    ListNode& node = pn->as<ListNode>();
    ParseNode** listp = node.unsafeHeadReference();
    for (; *listp; listp = &(*listp)->pn_next) {
      // Don't use PN*& because we want to check if it changed, so we can use
      // ReplaceNode
      ParseNode* pn = *listp;
      if (!visit(pn)) {
        return false;
      }
      if (pn != *listp) {
        ReplaceNode(listp, pn);
      }
    }
    node.unsafeReplaceTail(listp);
    return true;
  }

  MOZ_MUST_USE bool visit_PN_NAME_children(ParseNode*& pn) {
    NameNode& node = pn->as<NameNode>();
    if (node.initializer()) {
      if (!visit(*node.unsafeInitializerReference())) {
        return false;
      }
    }
    return true;
  }

  MOZ_MUST_USE bool visit_PN_FIELD_children(ParseNode*& pn) {
    ClassField& node = pn->as<ClassField>();
    if (!visit(*node.unsafeNameReference())) {
      return false;
    }
    if (node.hasInitializer()) {
      if (!visit(*node.unsafeInitializerReference())) {
        return false;
      }
    }
    return true;
  }

  MOZ_MUST_USE bool visit_PN_NUMBER_children(ParseNode*& pn) { return true; }

#ifdef ENABLE_BIGINT
  MOZ_MUST_USE bool visit_PN_BIGINT_children(ParseNode*& pn) { return true; }
#endif

  MOZ_MUST_USE bool visit_PN_REGEXP_children(ParseNode*& pn) { return true; }

  MOZ_MUST_USE bool visit_PN_LOOP_children(ParseNode*& pn) { return true; }

  MOZ_MUST_USE bool visit_PN_SCOPE_children(ParseNode*& pn) {
    LexicalScopeNode& node = pn->as<LexicalScopeNode>();
    return visit(*node.unsafeScopeBodyReference());
  }
};

}  // namespace frontend
}  // namespace js

#endif  // frontend_ParseNodeVisitor_h
