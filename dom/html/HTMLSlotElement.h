/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_HTMLSlotElement_h
#define mozilla_dom_HTMLSlotElement_h

#include "nsIDOMHTMLElement.h"
#include "nsGenericHTMLElement.h"
#include "nsTArray.h"

namespace mozilla {
namespace dom {

struct AssignedNodesOptions;

class HTMLSlotElement final : public nsGenericHTMLElement
{
public:
  explicit HTMLSlotElement(already_AddRefed<mozilla::dom::NodeInfo>& aNodeInfo);
  NS_IMPL_FROMCONTENT_HTML_WITH_TAG(HTMLSlotElement, slot)

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(HTMLSlotElement, nsGenericHTMLElement)
  virtual nsresult Clone(mozilla::dom::NodeInfo* aNodeInfo, nsINode** aResult,
                         bool aPreallocateChildren) const override;

  void SetName(const nsAString& aName, ErrorResult& aRv)
  {
    SetHTMLAttr(nsGkAtoms::name, aName, aRv);
  }

  void GetName(nsAString& aName)
  {
    GetHTMLAttr(nsGkAtoms::name, aName);
  }

  void AssignedNodes(const AssignedNodesOptions& aOptions,
                     nsTArray<RefPtr<nsINode>>& aNodes);

protected:
  virtual ~HTMLSlotElement();
  virtual JSObject*
  WrapNode(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

  nsTArray<RefPtr<nsINode>> mAssignedNodes;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_HTMLSlotElement_h