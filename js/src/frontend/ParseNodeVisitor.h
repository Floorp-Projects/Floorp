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
#define VISIT_CASE(KIND, _type) \
  case ParseNodeKind::KIND:     \
    return static_cast<Super*>(this)->visit##KIND(pn);
      FOR_EACH_PARSE_NODE_KIND(VISIT_CASE)
#undef VISIT_CASE
      default:
        MOZ_CRASH("invalid node kind");
    }
  }

  // using static_cast<Super*> here allows plain visit() to be overridden.
#define VISIT_METHOD(KIND, TYPE)                                 \
  MOZ_MUST_USE bool visit##KIND(ParseNode*& pn) {                \
    MOZ_ASSERT(pn->is<TYPE>(),                                   \
               "Node of kind " #KIND " was not of type " #TYPE); \
    return pn->as<TYPE>().accept(*static_cast<Super*>(this));    \
  }
  FOR_EACH_PARSE_NODE_KIND(VISIT_METHOD)
#undef VISIT_METHOD
};

}  // namespace frontend
}  // namespace js

#endif  // frontend_ParseNodeVisitor_h
