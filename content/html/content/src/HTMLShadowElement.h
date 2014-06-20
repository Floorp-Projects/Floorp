/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_HTMLShadowElement_h__
#define mozilla_dom_HTMLShadowElement_h__

#include "nsGenericHTMLElement.h"

namespace mozilla {
namespace dom {

class HTMLShadowElement MOZ_FINAL : public nsGenericHTMLElement,
                                    public nsStubMutationObserver
{
public:
  HTMLShadowElement(already_AddRefed<mozilla::dom::NodeInfo>& aNodeInfo);
  virtual ~HTMLShadowElement();

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  NS_DECL_NSIMUTATIONOBSERVER_CONTENTAPPENDED
  NS_DECL_NSIMUTATIONOBSERVER_CONTENTINSERTED
  NS_DECL_NSIMUTATIONOBSERVER_CONTENTREMOVED

  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(HTMLShadowElement,
                                           nsGenericHTMLElement)

  virtual nsresult Clone(mozilla::dom::NodeInfo *aNodeInfo, nsINode **aResult) const MOZ_OVERRIDE;

  virtual nsresult BindToTree(nsIDocument* aDocument, nsIContent* aParent,
                              nsIContent* aBindingParent,
                              bool aCompileEventHandlers);

  virtual void UnbindFromTree(bool aDeep = true,
                              bool aNullParent = true);

  bool IsInsertionPoint() { return mIsInsertionPoint; }

  /**
   * Sets the ShadowRoot that will be rendered in place of
   * this shadow insertion point.
   */
  void SetProjectedShadow(ShadowRoot* aProjectedShadow);

  /**
   * Distributes a single explicit child of the projected ShadowRoot
   * to relevant insertion points.
   */
  void DistributeSingleNode(nsIContent* aContent);

  /**
   * Removes a single explicit child of the projected ShadowRoot
   * from relevant insertion points.
   */
  void RemoveDistributedNode(nsIContent* aContent);

  /**
   * Distributes all the explicit children of the projected ShadowRoot
   * to the shadow insertion point in the younger ShadowRoot and
   * the content insertion point of the parent node's ShadowRoot.
   */
  void DistributeAllNodes();

  // WebIDL methods.
  ShadowRoot* GetOlderShadowRoot() { return mProjectedShadow; }

protected:
  virtual JSObject* WrapNode(JSContext *aCx) MOZ_OVERRIDE;

  // The ShadowRoot that will be rendered in place of this shadow insertion point.
  nsRefPtr<ShadowRoot> mProjectedShadow;

  bool mIsInsertionPoint;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_HTMLShadowElement_h__

