/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_DocumentFragment_h__
#define mozilla_dom_DocumentFragment_h__

#include "mozilla/Attributes.h"
#include "mozilla/dom/BorrowedAttrInfo.h"
#include "mozilla/dom/FragmentOrElement.h"
#include "nsStringFwd.h"

// XXX Avoid including this here by moving function bodies to the cpp file.
#include "mozilla/dom/Element.h"

class nsAtom;
class nsIContent;

namespace mozilla {
namespace dom {

class Document;
class Element;

class DocumentFragment : public FragmentOrElement {
 private:
  void Init() {
    MOZ_ASSERT(mNodeInfo->NodeType() == DOCUMENT_FRAGMENT_NODE &&
                   mNodeInfo->Equals(nsGkAtoms::documentFragmentNodeName,
                                     kNameSpaceID_None),
               "Bad NodeType in aNodeInfo");
  }

 public:
  using FragmentOrElement::GetFirstChild;
  using nsINode::QuerySelector;
  using nsINode::QuerySelectorAll;
  // Make sure bindings can see our superclass' protected GetElementById method.
  using nsINode::GetElementById;

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(DocumentFragment, FragmentOrElement)

  explicit DocumentFragment(already_AddRefed<dom::NodeInfo>&& aNodeInfo)
      : FragmentOrElement(std::move(aNodeInfo)), mHost(nullptr) {
    Init();
  }

  explicit DocumentFragment(nsNodeInfoManager* aNodeInfoManager)
      : FragmentOrElement(aNodeInfoManager->GetNodeInfo(
            nsGkAtoms::documentFragmentNodeName, nullptr, kNameSpaceID_None,
            DOCUMENT_FRAGMENT_NODE)),
        mHost(nullptr) {
    Init();
  }

  virtual JSObject* WrapNode(JSContext* aCx,
                             JS::Handle<JSObject*> aGivenProto) override;

  virtual bool IsNodeOfType(uint32_t aFlags) const override;

  nsresult BindToTree(BindContext&, nsINode& aParent) override {
    NS_ASSERTION(false, "Trying to bind a fragment to a tree");
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  virtual void UnbindFromTree(bool aNullParent) override {
    NS_ASSERTION(false, "Trying to unbind a fragment from a tree");
  }

  virtual Element* GetNameSpaceElement() override { return nullptr; }

  Element* GetHost() const { return mHost; }

  void SetHost(Element* aHost) { mHost = aHost; }

  void GetInnerHTML(nsAString& aInnerHTML) { GetMarkup(false, aInnerHTML); }
  void SetInnerHTML(const nsAString& aInnerHTML, ErrorResult& aError) {
    SetInnerHTMLInternal(aInnerHTML, aError);
  }

  static already_AddRefed<DocumentFragment> Constructor(
      const GlobalObject& aGlobal, ErrorResult& aRv);

#ifdef MOZ_DOM_LIST
  virtual void List(FILE* out, int32_t aIndent) const override;
  virtual void DumpContent(FILE* out, int32_t aIndent,
                           bool aDumpAll) const override;
#endif

 protected:
  virtual ~DocumentFragment() = default;

  nsresult Clone(dom::NodeInfo* aNodeInfo, nsINode** aResult) const override;
  RefPtr<Element> mHost;
};

}  // namespace dom
}  // namespace mozilla

inline mozilla::dom::DocumentFragment* nsINode::AsDocumentFragment() {
  MOZ_ASSERT(IsDocumentFragment());
  return static_cast<mozilla::dom::DocumentFragment*>(this);
}

inline const mozilla::dom::DocumentFragment* nsINode::AsDocumentFragment()
    const {
  MOZ_ASSERT(IsDocumentFragment());
  return static_cast<const mozilla::dom::DocumentFragment*>(this);
}

#endif  // mozilla_dom_DocumentFragment_h__
