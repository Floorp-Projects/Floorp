/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_HTMLContentElement_h__
#define mozilla_dom_HTMLContentElement_h__

#include "nsINodeList.h"
#include "nsGenericHTMLElement.h"

class nsCSSSelectorList;

namespace mozilla {
namespace dom {

class DistributedContentList;

class HTMLContentElement MOZ_FINAL : public nsGenericHTMLElement
{
public:
  HTMLContentElement(already_AddRefed<nsINodeInfo>& aNodeInfo);
  virtual ~HTMLContentElement();

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(HTMLContentElement,
                                           nsGenericHTMLElement)

  virtual nsresult Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const;

  virtual nsIDOMNode* AsDOMNode() { return this; }

  virtual nsresult BindToTree(nsIDocument* aDocument, nsIContent* aParent,
                              nsIContent* aBindingParent,
                              bool aCompileEventHandlers);

  virtual void UnbindFromTree(bool aDeep = true,
                              bool aNullParent = true);

  /**
   * Returns whether if the selector of this insertion point
   * matches the provided content.
   */
  bool Match(nsIContent* aContent);
  bool IsInsertionPoint() const { return mIsInsertionPoint; }
  nsCOMArray<nsIContent>& MatchedNodes() { return mMatchedNodes; }
  void AppendMatchedNode(nsIContent* aContent);
  void RemoveMatchedNode(nsIContent* aContent);
  void InsertMatchedNode(uint32_t aIndex, nsIContent* aContent);
  void ClearMatchedNodes();

  virtual nsresult SetAttr(int32_t aNameSpaceID, nsIAtom* aName,
                           nsIAtom* aPrefix, const nsAString& aValue,
                           bool aNotify);

  virtual nsresult UnsetAttr(int32_t aNameSpaceID, nsIAtom* aAttribute,
                             bool aNotify);

  // WebIDL methods.
  already_AddRefed<DistributedContentList> GetDistributedNodes();
  void GetSelect(nsAString& aSelect)
  {
    Element::GetAttr(kNameSpaceID_None, nsGkAtoms::select, aSelect);
  }
  void SetSelect(const nsAString& aSelect)
  {
    Element::SetAttr(kNameSpaceID_None, nsGkAtoms::select, aSelect, true);
  }

protected:
  virtual JSObject* WrapNode(JSContext *aCx) MOZ_OVERRIDE;

  /**
   * Updates the destination insertion points of the fallback
   * content of this insertion point. If there are nodes matched
   * to this insertion point, then destination insertion points
   * of fallback are cleared, otherwise, this insertion point
   * is a destination insertion point.
   */
  void UpdateFallbackDistribution();

  /**
   * An array of nodes from the ShadowRoot host that match the
   * content insertion selector.
   */
  nsCOMArray<nsIContent> mMatchedNodes;

  nsAutoPtr<nsCSSSelectorList> mSelectorList;
  bool mValidSelector;
  bool mIsInsertionPoint;
};

class DistributedContentList : public nsINodeList
{
public:
  DistributedContentList(HTMLContentElement* aHostElement);
  virtual ~DistributedContentList();

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS(DistributedContentList)

  // nsIDOMNodeList
  NS_DECL_NSIDOMNODELIST

  // nsINodeList
  virtual nsIContent* Item(uint32_t aIndex);
  virtual int32_t IndexOf(nsIContent* aContent);
  virtual nsINode* GetParentObject() { return mParent; }
  virtual uint32_t Length() const;
  virtual JSObject* WrapObject(JSContext* aCx) MOZ_OVERRIDE;
protected:
  nsRefPtr<HTMLContentElement> mParent;
  nsCOMArray<nsIContent> mDistributedNodes;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_HTMLContentElement_h__

