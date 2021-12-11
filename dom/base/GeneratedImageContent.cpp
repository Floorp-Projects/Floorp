/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/GeneratedImageContent.h"

#include "nsContentCreatorFunctions.h"
#include "nsGkAtoms.h"
#include "mozilla/dom/Document.h"
#include "nsNodeInfoManager.h"
#include "mozilla/dom/HTMLElementBinding.h"
#include "mozilla/dom/NameSpaceConstants.h"

namespace mozilla::dom {

NS_IMPL_ELEMENT_CLONE(GeneratedImageContent);

already_AddRefed<GeneratedImageContent> GeneratedImageContent::Create(
    Document& aDocument, uint32_t aContentIndex) {
  RefPtr<dom::NodeInfo> nodeInfo = aDocument.NodeInfoManager()->GetNodeInfo(
      nsGkAtoms::mozgeneratedcontentimage, nullptr, kNameSpaceID_XHTML,
      nsINode::ELEMENT_NODE);

  auto* nim = nodeInfo->NodeInfoManager();
  RefPtr<GeneratedImageContent> image =
      new (nim) GeneratedImageContent(nodeInfo.forget());
  image->mIndex = aContentIndex;
  return image.forget();
}

already_AddRefed<GeneratedImageContent>
GeneratedImageContent::CreateForListStyleImage(Document& aDocument) {
  return Create(aDocument, uint32_t(-1));
}

JSObject* GeneratedImageContent::WrapNode(JSContext* aCx,
                                          JS::Handle<JSObject*> aGivenProto) {
  return dom::HTMLElement_Binding::Wrap(aCx, this, aGivenProto);
}

}  // namespace mozilla::dom
