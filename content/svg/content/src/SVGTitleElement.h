/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_SVGTitleElement_h
#define mozilla_dom_SVGTitleElement_h

#include "nsSVGElement.h"
#include "nsStubMutationObserver.h"

typedef nsSVGElement SVGTitleElementBase;

nsresult NS_NewSVGTitleElement(nsIContent **aResult,
                               already_AddRefed<nsINodeInfo> aNodeInfo);
namespace mozilla {
namespace dom {

class SVGTitleElement MOZ_FINAL : public SVGTitleElementBase,
                                  public nsStubMutationObserver
{
protected:
  friend nsresult (::NS_NewSVGTitleElement(nsIContent **aResult,
                                           already_AddRefed<nsINodeInfo> aNodeInfo));
  SVGTitleElement(already_AddRefed<nsINodeInfo> aNodeInfo);

  virtual JSObject* WrapNode(JSContext *aCx, JSObject *aScope) MOZ_OVERRIDE;

public:
  // interfaces:

  NS_DECL_ISUPPORTS_INHERITED

  // nsIMutationObserver
  NS_DECL_NSIMUTATIONOBSERVER_CHARACTERDATACHANGED
  NS_DECL_NSIMUTATIONOBSERVER_CONTENTAPPENDED
  NS_DECL_NSIMUTATIONOBSERVER_CONTENTINSERTED
  NS_DECL_NSIMUTATIONOBSERVER_CONTENTREMOVED

  virtual nsresult Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const;

  virtual nsresult BindToTree(nsIDocument *aDocument, nsIContent *aParent,
                              nsIContent *aBindingParent,
                              bool aCompileEventHandlers);

  virtual void UnbindFromTree(bool aDeep = true,
                              bool aNullParent = true);

  virtual void DoneAddingChildren(bool aHaveNotified);
private:
  void SendTitleChangeEvent(bool aBound);
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_SVGTitleElement_h

