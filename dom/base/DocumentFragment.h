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
#include "nsIDOMNode.h"
#include "nsStringFwd.h"

class nsAtom;
class nsIDocument;
class nsIContent;

namespace mozilla {
namespace dom {

class Element;

class DocumentFragment : public FragmentOrElement,
                         public nsIDOMNode
{
private:
  void Init()
  {
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

  explicit DocumentFragment(already_AddRefed<mozilla::dom::NodeInfo>& aNodeInfo)
    : FragmentOrElement(aNodeInfo), mHost(nullptr)
  {
    Init();
  }

  explicit DocumentFragment(nsNodeInfoManager* aNodeInfoManager)
    : FragmentOrElement(aNodeInfoManager->GetNodeInfo(
                                            nsGkAtoms::documentFragmentNodeName,
                                            nullptr, kNameSpaceID_None,
                                            DOCUMENT_FRAGMENT_NODE)),
      mHost(nullptr)
  {
    Init();
  }

  virtual JSObject* WrapNode(JSContext *aCx, JS::Handle<JSObject*> aGivenProto) override;

  virtual bool IsNodeOfType(uint32_t aFlags) const override;

  virtual nsIDOMNode* AsDOMNode() override { return this; }

  virtual nsresult BindToTree(nsIDocument* aDocument, nsIContent* aParent,
                              nsIContent* aBindingParent,
                              bool aCompileEventHandlers) override
  {
    NS_ASSERTION(false, "Trying to bind a fragment to a tree");
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  virtual void UnbindFromTree(bool aDeep, bool aNullParent) override
  {
    NS_ASSERTION(false, "Trying to unbind a fragment from a tree");
  }

  virtual Element* GetNameSpaceElement() override
  {
    return nullptr;
  }

  Element* GetHost() const { return mHost; }

  void SetHost(Element* aHost) { mHost = aHost; }

  static already_AddRefed<DocumentFragment>
  Constructor(const GlobalObject& aGlobal, ErrorResult& aRv);

#ifdef DEBUG
  virtual void List(FILE* out, int32_t aIndent) const override;
  virtual void DumpContent(FILE* out, int32_t aIndent, bool aDumpAll) const override;
#endif

protected:
  virtual ~DocumentFragment()
  {
  }

  nsresult Clone(mozilla::dom::NodeInfo *aNodeInfo, nsINode **aResult,
                 bool aPreallocateChildren) const override;
  RefPtr<Element> mHost;
};

} // namespace dom
} // namespace mozilla


#endif // mozilla_dom_DocumentFragment_h__
