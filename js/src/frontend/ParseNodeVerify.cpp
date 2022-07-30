/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "frontend/ParseNodeVerify.h"

#include "frontend/ParseNodeVisitor.h"
#include "js/Stack.h"  // JS::NativeStackLimit

using namespace js;

#ifdef DEBUG

namespace js {
namespace frontend {

class ParseNodeVerifier : public ParseNodeVisitor<ParseNodeVerifier> {
  using Base = ParseNodeVisitor<ParseNodeVerifier>;

  const LifoAlloc& alloc_;

 public:
  ParseNodeVerifier(ErrorContext* ec, JS::NativeStackLimit stackLimit,
                    const LifoAlloc& alloc)
      : Base(ec, stackLimit), alloc_(alloc) {}

  [[nodiscard]] bool visit(ParseNode* pn) {
    // pn->size() asserts that pn->pn_kind is valid, so we don't redundantly
    // assert that here.
    JS_PARSE_NODE_ASSERT(alloc_.contains(pn),
                         "start of parse node is in alloc");
    JS_PARSE_NODE_ASSERT(alloc_.contains((unsigned char*)pn + pn->size()),
                         "end of parse node is in alloc");
    if (pn->is<ListNode>()) {
      pn->as<ListNode>().checkConsistency();
    }
    return Base::visit(pn);
  }
};

}  // namespace frontend
}  // namespace js

bool frontend::CheckParseTree(ErrorContext* ec, JS::NativeStackLimit stackLimit,
                              const LifoAlloc& alloc, ParseNode* pn) {
  ParseNodeVerifier verifier(ec, stackLimit, alloc);
  return verifier.visit(pn);
}

#endif  // DEBUG
