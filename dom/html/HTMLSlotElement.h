/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_HTMLSlotElement_h
#define mozilla_dom_HTMLSlotElement_h

#include "nsGenericHTMLElement.h"
#include "nsTArray.h"

namespace mozilla {
namespace dom {

struct AssignedNodesOptions;

class HTMLSlotElement final : public nsGenericHTMLElement
{
public:
  explicit HTMLSlotElement(already_AddRefed<mozilla::dom::NodeInfo>& aNodeInfo);
  NS_IMPL_FROMNODE_HTML_WITH_TAG(HTMLSlotElement, slot)

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(HTMLSlotElement, nsGenericHTMLElement)
  virtual nsresult Clone(mozilla::dom::NodeInfo* aNodeInfo, nsINode** aResult,
                         bool aPreallocateChildren) const override;

  // nsIContent
  virtual nsresult BindToTree(nsIDocument* aDocument, nsIContent* aParent,
                              nsIContent* aBindingParent,
                              bool aCompileEventHandlers) override;
  virtual void UnbindFromTree(bool aDeep = true,
                              bool aNullParent = true) override;

  virtual nsresult BeforeSetAttr(int32_t aNameSpaceID, nsAtom* aName,
                                 const nsAttrValueOrString* aValue,
                                 bool aNotify) override;
  virtual nsresult AfterSetAttr(int32_t aNameSpaceID, nsAtom* aName,
                                const nsAttrValue* aValue,
                                const nsAttrValue* aOldValue,
                                nsIPrincipal* aSubjectPrincipal,
                                bool aNotify) override;

  // WebIDL
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

  // Helper methods
  const nsTArray<RefPtr<nsINode>>& AssignedNodes() const;
  void InsertAssignedNode(uint32_t aIndex, nsINode* aNode);
  void AppendAssignedNode(nsINode* aNode);
  void RemoveAssignedNode(nsINode* aNode);
  void ClearAssignedNodes();

  void EnqueueSlotChangeEvent();
  void RemovedFromSignalSlotList()
  {
    MOZ_ASSERT(mInSignalSlotList);
    mInSignalSlotList = false;
  }

  void FireSlotChangeEvent();

protected:
  virtual ~HTMLSlotElement();
  JSObject* WrapNode(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) final;

  nsTArray<RefPtr<nsINode>> mAssignedNodes;

  // Whether we're in the signal slot list of our unit of related similar-origin
  // browsing contexts.
  //
  // https://dom.spec.whatwg.org/#signal-slot-list
  bool mInSignalSlotList = false;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_HTMLSlotElement_h
