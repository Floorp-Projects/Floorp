/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsXBLChildrenElement_h___
#define nsXBLChildrenElement_h___

#include "nsINodeList.h"
#include "nsBindingManager.h"
#include "mozilla/dom/nsXMLElement.h"

class nsAnonymousContentList;

namespace mozilla {
namespace dom {

class XBLChildrenElement : public nsXMLElement
{
public:
  explicit XBLChildrenElement(already_AddRefed<mozilla::dom::NodeInfo>& aNodeInfo)
    : nsXMLElement(aNodeInfo)
  {
  }
  explicit XBLChildrenElement(already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo)
    : nsXMLElement(aNodeInfo)
  {
  }

  // nsISupports
  NS_INLINE_DECL_REFCOUNTING_INHERITED(XBLChildrenElement, nsXMLElement)

  // nsINode interface methods
  virtual nsresult Clone(mozilla::dom::NodeInfo* aNodeInfo, nsINode** aResult,
                         bool aPreallocateChildren) const override;

  void AppendInsertedChild(nsIContent* aChild, bool aNotify)
  {
    // Appending an inserted child causes the inserted
    // children to be projected instead of default content.
    MaybeRemoveDefaultContent(aNotify);

    mInsertedChildren.AppendElement(aChild);
    aChild->SetXBLInsertionPoint(this);
  }

  void InsertInsertedChildAt(nsIContent* aChild, uint32_t aIndex)
  {
    // Inserting an inserted child causes the inserted
    // children to be projected instead of default content.
    MaybeRemoveDefaultContent(true);

    mInsertedChildren.InsertElementAt(aIndex, aChild);
    aChild->SetXBLInsertionPoint(this);
  }

  void RemoveInsertedChild(nsIContent* aChild)
  {
    // Can't use this assertion as we cheat for dynamic insertions and
    // only insert in the innermost insertion point.
    //NS_ASSERTION(mInsertedChildren.Contains(aChild),
    //             "Removing child that's not there");
    mInsertedChildren.RemoveElement(aChild);

    // After removing the inserted child, default content
    // may be projected into this insertion point.
    //
    // FIXME: Layout should be told about this before clearing
    // mInsertedChildren, this leaves stale styles and frames in the frame tree.
    MaybeSetupDefaultContent();
  }

  void ClearInsertedChildren()
  {
    for (auto* child : mInsertedChildren) {
      child->SetXBLInsertionPoint(nullptr);
    }
    mInsertedChildren.Clear();

    // After clearing inserted children, default content
    // will be projected into this insertion point.
    //
    // FIXME: Layout should be told about this before clearing
    // mInsertedChildren, this leaves stale styles and frames in the frame tree.
    MaybeSetupDefaultContent();
  }

  void MaybeSetupDefaultContent()
  {
    if (!HasInsertedChildren()) {
      for (nsIContent* child = static_cast<nsINode*>(this)->GetFirstChild();
           child;
           child = child->GetNextSibling()) {
        child->SetXBLInsertionPoint(this);
      }
    }
  }

  void MaybeRemoveDefaultContent(bool aNotify)
  {
    if (!HasInsertedChildren() && HasChildren()) {
      DoRemoveDefaultContent(aNotify);
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

  int32_t IndexOfInsertedChild(nsIContent* aChild)
  {
    return mInsertedChildren.IndexOf(aChild);
  }

  bool Includes(nsIContent* aChild)
  {
    NS_ASSERTION(!mIncludes.IsEmpty(),
                 "Shouldn't check for includes on default insertion point");
    return mIncludes.Contains(aChild->NodeInfo()->NameAtom());
  }

  bool IsDefaultInsertion()
  {
    return mIncludes.IsEmpty();
  }

  nsIContent* InsertedChild(uint32_t aIndex)
  {
    return mInsertedChildren[aIndex];
  }

protected:
  ~XBLChildrenElement();
  virtual nsresult BeforeSetAttr(int32_t aNamespaceID, nsAtom* aName,
                                 const nsAttrValueOrString* aValue,
                                 bool aNotify) override;

  void DoRemoveDefaultContent(bool aNotify);

private:
  nsTArray<nsIContent*> mInsertedChildren; // WEAK
  nsTArray<RefPtr<nsAtom> > mIncludes;
};

} // namespace dom
} // namespace mozilla

class nsAnonymousContentList final : public nsINodeList
{
public:
  explicit nsAnonymousContentList(nsIContent* aParent)
    : mParent(aParent)
  {
  }

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(nsAnonymousContentList)

  // nsINodeList interface
  virtual int32_t IndexOf(nsIContent* aContent) override;
  virtual nsINode* GetParentObject() override { return mParent; }
  virtual nsIContent* Item(uint32_t aIndex) override;
  uint32_t Length() override;

  virtual JSObject* WrapObject(JSContext *cx, JS::Handle<JSObject*> aGivenProto) override;

  bool IsListFor(nsIContent* aContent) {
    return mParent == aContent;
  }

private:
  virtual ~nsAnonymousContentList()
  {
  }

  nsCOMPtr<nsIContent> mParent;
};

#endif // nsXBLChildrenElement_h___
