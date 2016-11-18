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
#include "nsIDOMDocumentFragment.h"

class nsIAtom;
class nsAString;
class nsIDocument;
class nsIContent;

namespace mozilla {
namespace dom {

class Element;

class DocumentFragment : public FragmentOrElement,
                         public nsIDOMDocumentFragment
{
private:
  void Init()
  {
    MOZ_ASSERT(mNodeInfo->NodeType() == nsIDOMNode::DOCUMENT_FRAGMENT_NODE &&
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

  // interface nsIDOMNode
  NS_FORWARD_NSIDOMNODE_TO_NSINODE

  // interface nsIDOMDocumentFragment
  NS_DECL_NSIDOMDOCUMENTFRAGMENT

  explicit DocumentFragment(already_AddRefed<mozilla::dom::NodeInfo>& aNodeInfo)
    : FragmentOrElement(aNodeInfo), mHost(nullptr)
  {
    Init();
  }

  explicit DocumentFragment(nsNodeInfoManager* aNodeInfoManager)
    : FragmentOrElement(aNodeInfoManager->GetNodeInfo(
                                            nsGkAtoms::documentFragmentNodeName,
                                            nullptr, kNameSpaceID_None,
                                            nsIDOMNode::DOCUMENT_FRAGMENT_NODE)),
      mHost(nullptr)
  {
    Init();
  }

  virtual JSObject* WrapNode(JSContext *aCx, JS::Handle<JSObject*> aGivenProto) override;

  // nsIContent
  nsresult SetAttr(int32_t aNameSpaceID, nsIAtom* aName,
                   const nsAString& aValue, bool aNotify)
  {
    return SetAttr(aNameSpaceID, aName, nullptr, aValue, aNotify);
  }
  virtual nsresult SetAttr(int32_t aNameSpaceID, nsIAtom* aName,
                           nsIAtom* aPrefix, const nsAString& aValue,
                           bool aNotify) override
  {
    return NS_OK;
  }
  virtual nsresult UnsetAttr(int32_t aNameSpaceID, nsIAtom* aAttribute,
                             bool aNotify) override
  {
    return NS_OK;
  }
  virtual const nsAttrName* GetAttrNameAt(uint32_t aIndex) const override
  {
    return nullptr;
  }
  virtual BorrowedAttrInfo GetAttrInfoAt(uint32_t aIndex) const override
  {
    return BorrowedAttrInfo(nullptr, nullptr);
  }
  virtual uint32_t GetAttrCount() const override
  {
    return 0;
  }

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
    return;
  }

  virtual Element* GetNameSpaceElement() override
  {
    return nullptr;
  }

  nsIContent* GetHost() const
  {
    return mHost;
  }

  void SetHost(nsIContent* aHost)
  {
    mHost = aHost;
  }

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

  nsresult Clone(mozilla::dom::NodeInfo *aNodeInfo, nsINode **aResult) const override;
  nsIContent* mHost; // Weak
};

} // namespace dom
} // namespace mozilla


#endif // mozilla_dom_DocumentFragment_h__
