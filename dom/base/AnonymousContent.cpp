/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AnonymousContent.h"
#include "mozilla/dom/Document.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/ShadowRoot.h"
#include "mozilla/dom/AnonymousContentBinding.h"
#include "nsCycleCollectionParticipant.h"

namespace mozilla::dom {

// Ref counting and cycle collection
NS_IMPL_CYCLE_COLLECTION(AnonymousContent, mHost, mRoot)

already_AddRefed<AnonymousContent> AnonymousContent::Create(Document& aDoc) {
  RefPtr<Element> host = aDoc.CreateHTMLElement(nsGkAtoms::div);
  if (!host) {
    return nullptr;
  }
  host->SetAttr(kNameSpaceID_None, nsGkAtoms::role, u"presentation"_ns, false);
  host->SetAttr(kNameSpaceID_None, nsGkAtoms::_class,
                u"anonymous-content-host"_ns, false);
  RefPtr<ShadowRoot> root = host->AttachShadowWithoutNameChecks(
      ShadowRootMode::Closed, Element::DelegatesFocus::No);
  root->SetIsUAWidget();
  return do_AddRef(new AnonymousContent(host.forget(), root.forget()));
}

AnonymousContent::AnonymousContent(already_AddRefed<Element> aHost,
                                   already_AddRefed<ShadowRoot> aRoot)
    : mHost(aHost), mRoot(aRoot) {
  MOZ_ASSERT(mHost);
  MOZ_ASSERT(mRoot);
}

AnonymousContent::~AnonymousContent() = default;

bool AnonymousContent::WrapObject(JSContext* aCx,
                                  JS::Handle<JSObject*> aGivenProto,
                                  JS::MutableHandle<JSObject*> aReflector) {
  return AnonymousContent_Binding::Wrap(aCx, this, aGivenProto, aReflector);
}
}  // namespace mozilla::dom
