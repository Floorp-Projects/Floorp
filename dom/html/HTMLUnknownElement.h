/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef mozilla_dom_HTMLUnknownElement_h
#define mozilla_dom_HTMLUnknownElement_h

#include "mozilla/Attributes.h"
#include "nsGenericHTMLElement.h"

namespace mozilla {
namespace dom {

class HTMLUnknownElement final : public nsGenericHTMLElement
{
public:
  explicit HTMLUnknownElement(already_AddRefed<mozilla::dom::NodeInfo>& aNodeInfo)
    : nsGenericHTMLElement(aNodeInfo)
  {
    if (NodeInfo()->Equals(nsGkAtoms::bdi)) {
      SetHasDirAuto();
    }
  }

  virtual nsresult Clone(mozilla::dom::NodeInfo *aNodeInfo, nsINode **aResult) const override;

protected:
  virtual JSObject* WrapNode(JSContext *aCx, JS::Handle<JSObject*> aGivenProto) override;
};

} // namespace dom
} // namespace mozilla

#endif /* mozilla_dom_HTMLUnknownElement_h */
