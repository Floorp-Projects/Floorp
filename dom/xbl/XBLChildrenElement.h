/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 sw=2 et tw=79: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsXBLChildrenElement_h___
#define nsXBLChildrenElement_h___

#include "nsIDOMElement.h"
#include "nsINodeList.h"
#include "nsBindingManager.h"
#include "mozilla/dom/nsXMLElement.h"

class nsAnonymousContentList;

namespace mozilla {
namespace dom {

class ExplicitChildIterator;

class XBLChildrenElement : public nsXMLElement
{
public:
  friend class mozilla::dom::ExplicitChildIterator;
  friend class nsAnonymousContentList;
  XBLChildrenElement(already_AddRefed<nsINodeInfo>& aNodeInfo)
    : nsXMLElement(aNodeInfo)
  {
  }
  XBLChildrenElement(already_AddRefed<nsINodeInfo>&& aNodeInfo)
    : nsXMLElement(aNodeInfo)
  {
  }
  ~XBLChildrenElement();

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  // nsINode interface methods
  virtual nsresult Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const;

  virtual nsXPCClassInfo* GetClassInfo() { return nullptr; }

  virtual nsIDOMNode* AsDOMNode() { return this; }

  // nsIContent interface methods
  virtual nsIAtom *GetIDAttributeName() const;
  virtual nsIAtom* DoGetID() const;
  virtual nsresult UnsetAttr(int32_t aNameSpaceID, nsIAtom* aAttribute,
                             bool aNotify);
  virtual bool ParseAttribute(int32_t aNamespaceID,
                              nsIAtom* aAttribute,
                              const nsAString& aValue,
                              nsAttrValue& aResult);

  void AppendInsertedChild(nsIContent* aChild)
  {
    mInsertedChildren.AppendElement(aChild);
    aChild->SetXBLInsertionParent(GetParent());
  }

  void InsertInsertedChildAt(nsIContent* aChild, uint32_t aIndex)
  {
    mInsertedChildren.InsertElementAt(aIndex, aChild);
    aChild->SetXBLInsertionParent(GetParent());
  }

  void RemoveInsertedChild(nsIContent* aChild)
  {
    // Can't use this assertion as we cheat for dynamic insertions and
    // only insert in the innermost insertion point.
    //NS_ASSERTION(mInsertedChildren.Contains(aChild),
    //             "Removing child that's not there");
    mInsertedChildren.RemoveElement(aChild);
  }

  void ClearInsertedChildren()
  {
    for (uint32_t c = 0; c < mInsertedChildren.Length(); ++c) {
      mInsertedChildren[c]->SetXBLInsertionParent(nullptr);
    }
    mInsertedChildren.Clear();
  }

  void MaybeSetupDefaultContent()
  {
    if (!HasInsertedChildren()) {
      for (nsIContent* child = static_cast<nsINode*>(this)->GetFirstChild();
           child;
           child = child->GetNextSibling()) {
        child->SetXBLInsertionParent(GetParent());
      }
    }
  }

  void MaybeRemoveDefaultContent()
  {
    if (!HasInsertedChildren()) {
      for (nsIContent* child = static_cast<nsINode*>(this)->GetFirstChild();
           child;
           child = child->GetNextSibling()) {
        child->SetXBLInsertionParent(nullptr);
      }
    }
  }

  uint32_t InsertedChildrenLength()
  {
    return mInsertedChildren.Length();
  }

  bool HasInsertedChildren()
  {
    return !mInsertedChildren.IsEmpty();
  }

  enum {
    NoIndex = uint32_t(-1)
  };
  uint32_t IndexOfInsertedChild(nsIContent* aChild)
  {
    return mInsertedChildren.IndexOf(aChild);
  }

  bool Includes(nsIContent* aChild)
  {
    NS_ASSERTION(!mIncludes.IsEmpty(),
                 "Shouldn't check for includes on default insertion point");
    return mIncludes.Contains(aChild->Tag());
  }

  bool IsDefaultInsertion()
  {
    return mIncludes.IsEmpty();
  }

  nsTArray<nsIContent*> mInsertedChildren;

private:
  nsTArray<nsCOMPtr<nsIAtom> > mIncludes;
};

} // namespace dom
} // namespace mozilla

class nsAnonymousContentList : public nsINodeList
{
public:
  nsAnonymousContentList(nsIContent* aParent)
    : mParent(aParent)
  {
    MOZ_COUNT_CTOR(nsAnonymousContentList);
    SetIsDOMBinding();
  }

  virtual ~nsAnonymousContentList()
  {
    MOZ_COUNT_DTOR(nsAnonymousContentList);
  }

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(nsAnonymousContentList)
  // nsIDOMNodeList interface
  NS_DECL_NSIDOMNODELIST

  // nsINodeList interface
  virtual int32_t IndexOf(nsIContent* aContent);
  virtual nsINode* GetParentObject() { return mParent; }
  virtual nsIContent* Item(uint32_t aIndex);

  virtual JSObject* WrapObject(JSContext *cx) MOZ_OVERRIDE;

  bool IsListFor(nsIContent* aContent) {
    return mParent == aContent;
  }

private:
  nsCOMPtr<nsIContent> mParent;
};

#endif // nsXBLChildrenElement_h___
