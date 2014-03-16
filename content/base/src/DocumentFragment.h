/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=2 et :
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_DocumentFragment_h__
#define mozilla_dom_DocumentFragment_h__

#include "mozilla/Attributes.h"
#include "mozilla/dom/FragmentOrElement.h"
#include "nsIDOMDocumentFragment.h"

class nsINodeInfo;
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
    NS_ABORT_IF_FALSE(mNodeInfo->NodeType() ==
                      nsIDOMNode::DOCUMENT_FRAGMENT_NODE &&
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

  DocumentFragment(already_AddRefed<nsINodeInfo>& aNodeInfo)
    : FragmentOrElement(aNodeInfo), mHost(nullptr)
  {
    Init();
  }

  DocumentFragment(nsNodeInfoManager* aNodeInfoManager)
    : FragmentOrElement(aNodeInfoManager->GetNodeInfo(
                                            nsGkAtoms::documentFragmentNodeName,
                                            nullptr, kNameSpaceID_None,
                                            nsIDOMNode::DOCUMENT_FRAGMENT_NODE)),
      mHost(nullptr)
  {
    Init();
  }

  virtual ~DocumentFragment()
  {
  }

  virtual JSObject* WrapNode(JSContext *aCx,
                             JS::Handle<JSObject*> aScope) MOZ_OVERRIDE;

  // nsIContent
  nsresult SetAttr(int32_t aNameSpaceID, nsIAtom* aName,
                   const nsAString& aValue, bool aNotify)
  {
    return SetAttr(aNameSpaceID, aName, nullptr, aValue, aNotify);
  }
  virtual nsresult SetAttr(int32_t aNameSpaceID, nsIAtom* aName,
                           nsIAtom* aPrefix, const nsAString& aValue,
                           bool aNotify) MOZ_OVERRIDE
  {
    return NS_OK;
  }
  virtual nsresult UnsetAttr(int32_t aNameSpaceID, nsIAtom* aAttribute, 
                             bool aNotify) MOZ_OVERRIDE
  {
    return NS_OK;
  }
  virtual const nsAttrName* GetAttrNameAt(uint32_t aIndex) const MOZ_OVERRIDE
  {
    return nullptr;
  }
  virtual uint32_t GetAttrCount() const MOZ_OVERRIDE
  {
    return 0;
  }

  virtual bool IsNodeOfType(uint32_t aFlags) const MOZ_OVERRIDE;

  virtual nsIDOMNode* AsDOMNode() MOZ_OVERRIDE { return this; }

  virtual nsIAtom* DoGetID() const MOZ_OVERRIDE;
  virtual nsIAtom *GetIDAttributeName() const MOZ_OVERRIDE;

  virtual nsresult BindToTree(nsIDocument* aDocument, nsIContent* aParent,
                              nsIContent* aBindingParent,
                              bool aCompileEventHandlers) MOZ_OVERRIDE
  {
    NS_ASSERTION(false, "Trying to bind a fragment to a tree");
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  virtual void UnbindFromTree(bool aDeep, bool aNullParent) MOZ_OVERRIDE
  {
    NS_ASSERTION(false, "Trying to unbind a fragment from a tree");
    return;
  }

  virtual Element* GetNameSpaceElement()
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
  virtual void List(FILE* out, int32_t aIndent) const MOZ_OVERRIDE;
  virtual void DumpContent(FILE* out, int32_t aIndent, bool aDumpAll) const MOZ_OVERRIDE;
#endif

protected:
  nsresult Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const MOZ_OVERRIDE;
  nsIContent* mHost; // Weak
};

} // namespace dom
} // namespace mozilla


#endif // mozilla_dom_DocumentFragment_h__
