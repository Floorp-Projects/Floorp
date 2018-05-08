/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_HTMLStyleElement_h
#define mozilla_dom_HTMLStyleElement_h

#include "mozilla/Attributes.h"
#include "nsGenericHTMLElement.h"
#include "nsStyleLinkElement.h"
#include "nsStubMutationObserver.h"

class nsIDocument;

namespace mozilla {
namespace dom {

class HTMLStyleElement final : public nsGenericHTMLElement,
                               public nsStyleLinkElement,
                               public nsStubMutationObserver
{
public:
  explicit HTMLStyleElement(already_AddRefed<mozilla::dom::NodeInfo>& aNodeInfo);

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  // CC
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(HTMLStyleElement,
                                           nsGenericHTMLElement)

  void GetInnerHTML(nsAString& aInnerHTML, OOMReporter& aError) override;
  using nsGenericHTMLElement::SetInnerHTML;
  virtual void SetInnerHTML(const nsAString& aInnerHTML,
                            nsIPrincipal* aSubjectPrincipal,
                            mozilla::ErrorResult& aError) override;
  virtual void SetTextContentInternal(const nsAString& aTextContent,
                                      nsIPrincipal* aSubjectPrincipal,
                                      mozilla::ErrorResult& aError) override;

  virtual nsresult BindToTree(nsIDocument* aDocument, nsIContent* aParent,
                              nsIContent* aBindingParent,
                              bool aCompileEventHandlers) override;
  virtual void UnbindFromTree(bool aDeep = true,
                              bool aNullParent = true) override;
  virtual nsresult AfterSetAttr(int32_t aNameSpaceID, nsAtom* aName,
                                const nsAttrValue* aValue,
                                const nsAttrValue* aOldValue,
                                nsIPrincipal* aSubjectPrincipal,
                                bool aNotify) override;

  virtual nsresult Clone(mozilla::dom::NodeInfo *aNodeInfo, nsINode **aResult,
                         bool aPreallocateChildren) const override;

  // nsIMutationObserver
  NS_DECL_NSIMUTATIONOBSERVER_CHARACTERDATACHANGED
  NS_DECL_NSIMUTATIONOBSERVER_CONTENTAPPENDED
  NS_DECL_NSIMUTATIONOBSERVER_CONTENTINSERTED
  NS_DECL_NSIMUTATIONOBSERVER_CONTENTREMOVED

  bool Disabled();
  void SetDisabled(bool aDisabled);
  void GetMedia(nsAString& aValue)
  {
    GetHTMLAttr(nsGkAtoms::media, aValue);
  }
  void SetMedia(const nsAString& aMedia, ErrorResult& aError)
  {
    SetHTMLAttr(nsGkAtoms::media, aMedia, aError);
  }
  void GetType(nsAString& aValue)
  {
    GetHTMLAttr(nsGkAtoms::type, aValue);
  }
  void SetType(const nsAString& aType, ErrorResult& aError)
  {
    SetHTMLAttr(nsGkAtoms::type, aType, aError);
  }

  virtual JSObject* WrapNode(JSContext *aCx, JS::Handle<JSObject*> aGivenProto) override;

protected:
  virtual ~HTMLStyleElement();

  Maybe<SheetInfo> GetStyleSheetInfo() final;

  /**
   * Common method to call from the various mutation observer methods.
   * aContent is a content node that's either the one that changed or its
   * parent; we should only respond to the change if aContent is non-anonymous.
   */
  void ContentChanged(nsIContent* aContent);
};

} // namespace dom
} // namespace mozilla

#endif

