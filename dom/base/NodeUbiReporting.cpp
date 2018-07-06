/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "NodeUbiReporting.h"
#include "js/UbiNodeUtils.h"
#include "nsWindowSizes.h"

using JS::ubi::SimpleEdgeRange;
using JS::ubi::EdgeRange;

const char16_t JS::ubi::Concrete<nsIDocument>::concreteTypeName[] = u"nsIDocument";
const char16_t JS::ubi::Concrete<nsIContent>::concreteTypeName[] = u"nsIContent";
const char16_t JS::ubi::Concrete<Attr>::concreteTypeName[] = u"Attr";

void
JS::ubi::Concrete<nsINode>::construct(void* storage, nsINode* ptr)
{
  // nsINode is abstract, and all of its inherited instances have
  // an overridden function with instructions to construct ubi::Nodes.
  // We actually want to call that function and construct from those instances.
  ptr->ConstructUbiNode(storage);
}

js::UniquePtr<EdgeRange>
JS::ubi::Concrete<nsINode>::edges(JSContext* cx, bool wantNames) const
{
  auto range = js::MakeUnique<SimpleEdgeRange>();
  if (!range) {
    return nullptr;
  }
  if (get().GetParent()) {
    char16_t* edgeName = nullptr;
    if (wantNames) {
      edgeName = NS_strdup(u"Parent Node");
    }
    if (!range->addEdge(JS::ubi::Edge(edgeName, get().GetParent()))) {
      return nullptr;
    }
  }
  for (auto curr = get().GetFirstChild(); curr; curr = curr->GetNextSibling()) {
    char16_t* edgeName = nullptr;
    if (wantNames) {
      edgeName = NS_strdup(u"Child Node");
    }
    if (!range->addEdge(JS::ubi::Edge(edgeName, curr))) {
      return nullptr;
    }
  }
  return range;
}

JS::ubi::Node::Size
JS::ubi::Concrete<nsINode>::size(mozilla::MallocSizeOf mallocSizeOf) const
{
  mozilla::SizeOfState sz(mallocSizeOf);
  nsWindowSizes wn(sz);
  size_t n = 0;
  get().AddSizeOfIncludingThis(wn, &n);
  return n;
}

JS::ubi::Node::Size
JS::ubi::Concrete<nsIDocument>::size(mozilla::MallocSizeOf mallocSizeOf) const
{
  mozilla::SizeOfState sz(mallocSizeOf);
  nsWindowSizes wn(sz);
  getDoc().DocAddSizeOfIncludingThis(wn);
  return wn.getTotalSize();
}
