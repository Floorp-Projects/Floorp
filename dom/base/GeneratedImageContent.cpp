/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/GeneratedImageContent.h"

#include "nsContentCreatorFunctions.h"
#include "nsGkAtoms.h"
#include "nsIDocument.h"
#include "nsNodeInfoManager.h"
#include "mozilla/dom/HTMLElementBinding.h"
#include "mozilla/dom/NameSpaceConstants.h"

namespace mozilla {
namespace dom {

NS_IMPL_ELEMENT_CLONE(GeneratedImageContent);

already_AddRefed<GeneratedImageContent>
GeneratedImageContent::Create(nsIDocument& aDocument, uint32_t aContentIndex)
{
  RefPtr<dom::NodeInfo> nodeInfo =
    aDocument.NodeInfoManager()->
      GetNodeInfo(nsGkAtoms::mozgeneratedcontentimage,
                  nullptr,
                  kNameSpaceID_XHTML,
                  nsINode::ELEMENT_NODE);

  // Work around not being able to bind a non-const lvalue reference
  // to an rvalue of non-reference type by just creating an rvalue
  // reference.  And we can't change the constructor signature,
  // because then the macro-generated Clone() method fails to compile.
  already_AddRefed<dom::NodeInfo>&& rvalue = nodeInfo.forget();

  auto image =
    MakeRefPtr<GeneratedImageContent>(rvalue);
  image->mIndex = aContentIndex;
  return image.forget();
}

JSObject*
GeneratedImageContent::WrapNode(JSContext* aCx,
                                JS::Handle<JSObject*> aGivenProto)
{
  return dom::HTMLElement_Binding::Wrap(aCx, this, aGivenProto);
}

} // namespace dom
} // namespace mozilla

