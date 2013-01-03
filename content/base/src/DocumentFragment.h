/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=2 et :
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_DocumentFragment_h__
#define mozilla_dom_DocumentFragment_h__

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
public:
  using FragmentOrElement::GetFirstChild;

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  // interface nsIDOMNode
  NS_FORWARD_NSIDOMNODE_TO_NSINODE

  // interface nsIDOMDocumentFragment
  // NS_DECL_NSIDOCUMENTFRAGMENT  Empty

  DocumentFragment(already_AddRefed<nsINodeInfo> aNodeInfo);
  virtual ~DocumentFragment()
  {
  }

  virtual JSObject* WrapNode(JSContext *aCx, JSObject *aScope,
                             bool *aTriedToWrap);

  // nsIContent
  virtual already_AddRefed<nsINodeInfo>
    GetExistingAttrNameFromQName(const nsAString& aStr) const
  {
    return nullptr;
  }

  nsresult SetAttr(int32_t aNameSpaceID, nsIAtom* aName,
                   const nsAString& aValue, bool aNotify)
  {
    return SetAttr(aNameSpaceID, aName, nullptr, aValue, aNotify);
  }
  virtual nsresult SetAttr(int32_t aNameSpaceID, nsIAtom* aName,
                           nsIAtom* aPrefix, const nsAString& aValue,
                           bool aNotify)
  {
    return NS_OK;
  }
  virtual bool GetAttr(int32_t aNameSpaceID, nsIAtom* aName, 
                       nsAString& aResult) const
  {
    return false;
  }
  virtual bool HasAttr(int32_t aNameSpaceID, nsIAtom* aName) const
  {
    return false;
  }
  virtual nsresult UnsetAttr(int32_t aNameSpaceID, nsIAtom* aAttribute, 
                             bool aNotify)
  {
    return NS_OK;
  }
  virtual const nsAttrName* GetAttrNameAt(uint32_t aIndex) const
  {
    return nullptr;
  }
  virtual uint32_t GetAttrCount() const
  {
    return 0;
  }

  virtual bool IsNodeOfType(uint32_t aFlags) const;

  virtual nsXPCClassInfo* GetClassInfo();

  virtual nsIDOMNode* AsDOMNode() { return this; }

  virtual nsIAtom* DoGetID() const;
  virtual nsIAtom *GetIDAttributeName() const;

  virtual nsresult BindToTree(nsIDocument* aDocument, nsIContent* aParent,
                              nsIContent* aBindingParent,
                              bool aCompileEventHandlers)
  {
    NS_ASSERTION(false, "Trying to bind a fragment to a tree");
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  virtual void UnbindFromTree(bool aDeep, bool aNullParent)
  {
    NS_ASSERTION(false, "Trying to unbind a fragment from a tree");
    return;
  }

  virtual Element* GetNameSpaceElement()
  {
    return nullptr;
  }

#ifdef DEBUG
  virtual void List(FILE* out, int32_t aIndent) const;
  virtual void DumpContent(FILE* out, int32_t aIndent, bool aDumpAll) const;
#endif

protected:
  nsresult Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const;
};

} // namespace dom
} // namespace mozilla


#endif // mozilla_dom_DocumentFragment_h__
