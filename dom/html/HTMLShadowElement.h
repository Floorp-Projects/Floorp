/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_HTMLShadowElement_h__
#define mozilla_dom_HTMLShadowElement_h__

#include "nsGenericHTMLElement.h"

namespace mozilla {
namespace dom {

class HTMLShadowElement final : public nsGenericHTMLElement,
                                public nsStubMutationObserver
{
public:
  explicit HTMLShadowElement(already_AddRefed<mozilla::dom::NodeInfo>& aNodeInfo);

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  NS_DECL_NSIMUTATIONOBSERVER_CONTENTAPPENDED
  NS_DECL_NSIMUTATIONOBSERVER_CONTENTINSERTED
  NS_DECL_NSIMUTATIONOBSERVER_CONTENTREMOVED

  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(HTMLShadowElement,
                                           nsGenericHTMLElement)

  static HTMLShadowElement* FromContent(nsIContent* aContent)
  {
    if (aContent->IsHTMLShadowElement()) {
      return static_cast<HTMLShadowElement*>(aContent);
    }

    return nullptr;
  }

  virtual bool IsHTMLShadowElement() const override { return true; }

  virtual nsresult Clone(mozilla::dom::NodeInfo *aNodeInfo, nsINode **aResult) const override;

  virtual nsresult BindToTree(nsIDocument* aDocument, nsIContent* aParent,
                              nsIContent* aBindingParent,
                              bool aCompileEventHandlers) override;

  virtual void UnbindFromTree(bool aDeep = true,
                              bool aNullParent = true) override;

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
  virtual ~HTMLShadowElement();

  virtual JSObject* WrapNode(JSContext *aCx, JS::Handle<JSObject*> aGivenProto) override;

  // The ShadowRoot that will be rendered in place of this shadow insertion point.
  RefPtr<ShadowRoot> mProjectedShadow;

  bool mIsInsertionPoint;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_HTMLShadowElement_h__

