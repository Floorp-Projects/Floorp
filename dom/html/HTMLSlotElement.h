/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_HTMLSlotElement_h
#define mozilla_dom_HTMLSlotElement_h

#include "nsGenericHTMLElement.h"
#include "nsTArray.h"
#include "mozilla/dom/HTMLSlotElementBinding.h"

namespace mozilla::dom {

class HTMLSlotElement final : public nsGenericHTMLElement {
 public:
  explicit HTMLSlotElement(
      already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo);
  NS_IMPL_FROMNODE_HTML_WITH_TAG(HTMLSlotElement, slot)

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(HTMLSlotElement,
                                           nsGenericHTMLElement)
  nsresult Clone(dom::NodeInfo*, nsINode** aResult) const override;

  // nsIContent
  nsresult BindToTree(BindContext&, nsINode& aParent) override;
  void UnbindFromTree(UnbindContext&) override;

  void BeforeSetAttr(int32_t aNameSpaceID, nsAtom* aName,
                     const nsAttrValue* aValue, bool aNotify) override;
  void AfterSetAttr(int32_t aNameSpaceID, nsAtom* aName,
                    const nsAttrValue* aValue, const nsAttrValue* aOldValue,
                    nsIPrincipal* aSubjectPrincipal, bool aNotify) override;

  // WebIDL
  void SetName(const nsAString& aName, ErrorResult& aRv) {
    SetHTMLAttr(nsGkAtoms::name, aName, aRv);
  }

  void GetName(nsAString& aName) { GetHTMLAttr(nsGkAtoms::name, aName); }

  void AssignedNodes(const AssignedNodesOptions& aOptions,
                     nsTArray<RefPtr<nsINode>>& aNodes);

  void AssignedElements(const AssignedNodesOptions& aOptions,
                        nsTArray<RefPtr<Element>>& aNodes);

  void Assign(const Sequence<OwningElementOrText>& aNodes);

  // Helper methods
  const nsTArray<RefPtr<nsINode>>& AssignedNodes() const;
  const nsTArray<nsINode*>& ManuallyAssignedNodes() const;
  void InsertAssignedNode(uint32_t aIndex, nsIContent&);
  void AppendAssignedNode(nsIContent&);
  void RemoveAssignedNode(nsIContent&);
  void ClearAssignedNodes();

  void EnqueueSlotChangeEvent();
  void RemovedFromSignalSlotList() {
    MOZ_ASSERT(mInSignalSlotList);
    mInSignalSlotList = false;
  }

  void FireSlotChangeEvent();

  void RemoveManuallyAssignedNode(nsIContent&);

 protected:
  virtual ~HTMLSlotElement();
  JSObject* WrapNode(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) final;

  nsTArray<RefPtr<nsINode>> mAssignedNodes;
  nsTArray<nsINode*> mManuallyAssignedNodes;

  // Whether we're in the signal slot list of our unit of related similar-origin
  // browsing contexts.
  //
  // https://dom.spec.whatwg.org/#signal-slot-list
  bool mInSignalSlotList = false;

  bool mInManualShadowRoot = false;
};

}  // namespace mozilla::dom

#endif  // mozilla_dom_HTMLSlotElement_h
