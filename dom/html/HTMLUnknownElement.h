/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef mozilla_dom_HTMLUnknownElement_h
#define mozilla_dom_HTMLUnknownElement_h

#include "mozilla/Attributes.h"
#include "mozilla/EventStates.h"
#include "nsGenericHTMLElement.h"

namespace mozilla {
namespace dom {

#define NS_HTMLUNKNOWNELEMENT_IID \
{ 0xc09e665b, 0x3876, 0x40dd, \
  { 0x85, 0x28, 0x44, 0xc2, 0x3f, 0xd4, 0x58, 0xf2 } }

class HTMLUnknownElement final : public nsGenericHTMLElement
{
public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_HTMLUNKNOWNELEMENT_IID)

  NS_DECL_ISUPPORTS_INHERITED

  explicit HTMLUnknownElement(already_AddRefed<mozilla::dom::NodeInfo>& aNodeInfo)
    : nsGenericHTMLElement(aNodeInfo)
  {
    if (NodeInfo()->Equals(nsGkAtoms::bdi)) {
      AddStatesSilently(NS_EVENT_STATE_DIR_ATTR_LIKE_AUTO);
      SetHasDirAuto();
    }
  }

  virtual nsresult Clone(mozilla::dom::NodeInfo *aNodeInfo, nsINode **aResult,
                         bool aPreallocateChildren) const override;

protected:
  virtual ~HTMLUnknownElement() {}
  virtual JSObject* WrapNode(JSContext *aCx, JS::Handle<JSObject*> aGivenProto) override;
};

NS_DEFINE_STATIC_IID_ACCESSOR(HTMLUnknownElement, NS_HTMLUNKNOWNELEMENT_IID)

} // namespace dom
} // namespace mozilla

#endif /* mozilla_dom_HTMLUnknownElement_h */
