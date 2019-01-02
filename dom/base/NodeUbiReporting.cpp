/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "NodeUbiReporting.h"
#include "js/UbiNodeUtils.h"
#include "nsWindowSizes.h"

using JS::ubi::EdgeRange;
using JS::ubi::SimpleEdgeRange;

const char16_t JS::ubi::Concrete<mozilla::dom::Document>::concreteTypeName[] =
    u"Document";
const char16_t JS::ubi::Concrete<nsIContent>::concreteTypeName[] =
    u"nsIContent";
const char16_t JS::ubi::Concrete<mozilla::dom::Attr>::concreteTypeName[] =
    u"Attr";

void JS::ubi::Concrete<nsINode>::construct(void* storage, nsINode* ptr) {
  // nsINode is abstract, and all of its inherited instances have
  // an overridden function with instructions to construct ubi::Nodes.
  // We actually want to call that function and construct from those instances.
  ptr->ConstructUbiNode(storage);
}

js::UniquePtr<EdgeRange> JS::ubi::Concrete<nsINode>::edges(
    JSContext* cx, bool wantNames) const {
  AutoSuppressGCAnalysis suppress;
  auto range = js::MakeUnique<SimpleEdgeRange>();
  if (!range) {
    return nullptr;
  }
  if (get().GetParent()) {
    char16_t* edgeName = nullptr;
    if (wantNames) {
      edgeName = NS_xstrdup(u"Parent Node");
    }
    if (!range->addEdge(JS::ubi::Edge(edgeName, get().GetParent()))) {
      return nullptr;
    }
  }
  for (auto curr = get().GetFirstChild(); curr; curr = curr->GetNextSibling()) {
    char16_t* edgeName = nullptr;
    if (wantNames) {
      edgeName = NS_xstrdup(u"Child Node");
    }
    if (!range->addEdge(JS::ubi::Edge(edgeName, curr))) {
      return nullptr;
    }
  }
  return js::UniquePtr<EdgeRange>(range.release());
}

JS::ubi::Node::Size JS::ubi::Concrete<nsINode>::size(
    mozilla::MallocSizeOf mallocSizeOf) const {
  AutoSuppressGCAnalysis suppress;
  mozilla::SizeOfState sz(mallocSizeOf);
  nsWindowSizes wn(sz);
  size_t n = 0;
  get().AddSizeOfIncludingThis(wn, &n);
  return n;
}

const char16_t* JS::ubi::Concrete<nsINode>::descriptiveTypeName() const {
  return get().NodeName().get();
}

JS::ubi::Node::Size JS::ubi::Concrete<mozilla::dom::Document>::size(
    mozilla::MallocSizeOf mallocSizeOf) const {
  AutoSuppressGCAnalysis suppress;
  mozilla::SizeOfState sz(mallocSizeOf);
  nsWindowSizes wn(sz);
  getDoc().DocAddSizeOfIncludingThis(wn);
  return wn.getTotalSize();
}
