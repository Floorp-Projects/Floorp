/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_HTMLStyleElement_h
#define mozilla_dom_HTMLStyleElement_h

#include "mozilla/Attributes.h"
#include "nsIDOMHTMLStyleElement.h"
#include "nsGenericHTMLElement.h"
#include "nsStyleLinkElement.h"
#include "nsStubMutationObserver.h"

class nsIDocument;

namespace mozilla {
namespace dom {

class HTMLStyleElement MOZ_FINAL : public nsGenericHTMLElement,
                                   public nsIDOMHTMLStyleElement,
                                   public nsStyleLinkElement,
                                   public nsStubMutationObserver
{
public:
  HTMLStyleElement(already_AddRefed<mozilla::dom::NodeInfo>& aNodeInfo);
  virtual ~HTMLStyleElement();

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  // CC
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(HTMLStyleElement,
                                           nsGenericHTMLElement)

  NS_IMETHOD GetInnerHTML(nsAString& aInnerHTML) MOZ_OVERRIDE;
  using nsGenericHTMLElement::SetInnerHTML;
  virtual void SetInnerHTML(const nsAString& aInnerHTML,
                            mozilla::ErrorResult& aError) MOZ_OVERRIDE;

  // nsIDOMHTMLStyleElement
  NS_DECL_NSIDOMHTMLSTYLEELEMENT

  virtual nsresult BindToTree(nsIDocument* aDocument, nsIContent* aParent,
                              nsIContent* aBindingParent,
                              bool aCompileEventHandlers) MOZ_OVERRIDE;
  virtual void UnbindFromTree(bool aDeep = true,
                              bool aNullParent = true) MOZ_OVERRIDE;
  nsresult SetAttr(int32_t aNameSpaceID, nsIAtom* aName,
                   const nsAString& aValue, bool aNotify)
  {
    return SetAttr(aNameSpaceID, aName, nullptr, aValue, aNotify);
  }
  virtual nsresult SetAttr(int32_t aNameSpaceID, nsIAtom* aName,
                           nsIAtom* aPrefix, const nsAString& aValue,
                           bool aNotify) MOZ_OVERRIDE;
  virtual nsresult UnsetAttr(int32_t aNameSpaceID, nsIAtom* aAttribute,
                             bool aNotify) MOZ_OVERRIDE;

  virtual nsresult Clone(mozilla::dom::NodeInfo *aNodeInfo, nsINode **aResult) const MOZ_OVERRIDE;

  // nsIMutationObserver
  NS_DECL_NSIMUTATIONOBSERVER_CHARACTERDATACHANGED
  NS_DECL_NSIMUTATIONOBSERVER_CONTENTAPPENDED
  NS_DECL_NSIMUTATIONOBSERVER_CONTENTINSERTED
  NS_DECL_NSIMUTATIONOBSERVER_CONTENTREMOVED

  bool Disabled();
  void SetDisabled(bool aDisabled);
  void SetMedia(const nsAString& aMedia, ErrorResult& aError)
  {
    SetHTMLAttr(nsGkAtoms::media, aMedia, aError);
  }
  void SetType(const nsAString& aType, ErrorResult& aError)
  {
    SetHTMLAttr(nsGkAtoms::type, aType, aError);
  }
  bool Scoped()
  {
    return GetBoolAttr(nsGkAtoms::scoped);
  }
  void SetScoped(bool aScoped, ErrorResult& aError)
  {
    SetHTMLBoolAttr(nsGkAtoms::scoped, aScoped, aError);
  }

  virtual JSObject* WrapNode(JSContext *aCx) MOZ_OVERRIDE;

protected:
  already_AddRefed<nsIURI> GetStyleSheetURL(bool* aIsInline) MOZ_OVERRIDE;
  void GetStyleSheetInfo(nsAString& aTitle,
                         nsAString& aType,
                         nsAString& aMedia,
                         bool* aIsScoped,
                         bool* aIsAlternate) MOZ_OVERRIDE;
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

