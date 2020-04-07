/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef XULFrameElement_h__
#define XULFrameElement_h__

#include "mozilla/Attributes.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/dom/Nullable.h"
#include "mozilla/dom/WindowProxyHolder.h"
#include "js/TypeDecls.h"
#include "nsCycleCollectionParticipant.h"
#include "nsWrapperCache.h"
#include "nsString.h"
#include "nsXULElement.h"
#include "nsFrameLoaderOwner.h"

class nsIWebNavigation;
class nsFrameLoader;

namespace mozilla {
namespace dom {

class BrowsingContext;

class XULFrameElement final : public nsXULElement, public nsFrameLoaderOwner {
 public:
  explicit XULFrameElement(already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo)
      : nsXULElement(std::move(aNodeInfo)) {}

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(XULFrameElement, nsXULElement)

  // XULFrameElement.webidl
  nsDocShell* GetDocShell();
  already_AddRefed<nsIWebNavigation> GetWebNavigation();
  Nullable<WindowProxyHolder> GetContentWindow();
  Document* GetContentDocument();

  void SwapFrameLoaders(mozilla::dom::HTMLIFrameElement& aOtherLoaderOwner,
                        mozilla::ErrorResult& rv);
  void SwapFrameLoaders(XULFrameElement& aOtherLoaderOwner,
                        mozilla::ErrorResult& rv);
  void SwapFrameLoaders(nsFrameLoaderOwner* aOtherLoaderOwner,
                        mozilla::ErrorResult& rv);

  // nsIContent
  virtual nsresult BindToTree(BindContext&, nsINode& aParent) override;
  virtual void UnbindFromTree(bool aNullParent) override;
  virtual void DestroyContent() override;

  virtual nsresult AfterSetAttr(int32_t aNamespaceID, nsAtom* aName,
                                const nsAttrValue* aValue,
                                const nsAttrValue* aOldValue,
                                nsIPrincipal* aSubjectPrincipal,
                                bool aNotify) override;

  NS_IMPL_FROMNODE_HELPER(XULFrameElement,
                          IsAnyOfXULElements(nsGkAtoms::iframe,
                                             nsGkAtoms::browser,
                                             nsGkAtoms::editor))

 protected:
  virtual ~XULFrameElement() = default;

  JSObject* WrapNode(JSContext* aCx,
                     JS::Handle<JSObject*> aGivenProto) override;

  void LoadSrc();
};

}  // namespace dom
}  // namespace mozilla

#endif  // XULFrameElement_h
