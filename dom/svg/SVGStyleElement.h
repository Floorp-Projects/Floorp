/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_SVGStyleElement_h
#define mozilla_dom_SVGStyleElement_h

#include "mozilla/Attributes.h"
#include "nsSVGElement.h"
#include "nsStyleLinkElement.h"
#include "nsStubMutationObserver.h"

nsresult NS_NewSVGStyleElement(nsIContent **aResult,
                               already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo);

typedef nsSVGElement SVGStyleElementBase;

namespace mozilla {
namespace dom {

class SVGStyleElement final : public SVGStyleElementBase,
                              public nsStyleLinkElement,
                              public nsStubMutationObserver
{
protected:
  friend nsresult (::NS_NewSVGStyleElement(nsIContent **aResult,
                                           already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo));
  explicit SVGStyleElement(already_AddRefed<mozilla::dom::NodeInfo>& aNodeInfo);
  ~SVGStyleElement();

  virtual JSObject* WrapNode(JSContext *aCx, JS::Handle<JSObject*> aGivenProto) override;

public:
  NS_DECL_ISUPPORTS_INHERITED

  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(SVGStyleElement,
                                           SVGStyleElementBase)

  // nsIContent
  virtual nsresult BindToTree(nsIDocument* aDocument, nsIContent* aParent,
                              nsIContent* aBindingParent,
                              bool aCompileEventHandlers) override;
  virtual void UnbindFromTree(bool aDeep = true,
                              bool aNullParent = true) override;
  nsresult SetAttr(int32_t aNameSpaceID, nsIAtom* aName,
                   const nsAString& aValue, bool aNotify)
  {
    return SetAttr(aNameSpaceID, aName, nullptr, aValue, aNotify);
  }
  virtual nsresult SetAttr(int32_t aNameSpaceID, nsIAtom* aName,
                           nsIAtom* aPrefix, const nsAString& aValue,
                           bool aNotify) override;
  virtual nsresult UnsetAttr(int32_t aNameSpaceID, nsIAtom* aAttribute,
                             bool aNotify) override;
  virtual bool ParseAttribute(int32_t aNamespaceID,
                              nsIAtom* aAttribute,
                              const nsAString& aValue,
                              nsAttrValue& aResult) override;

  virtual nsresult Clone(mozilla::dom::NodeInfo *aNodeInfo, nsINode **aResult,
                         bool aPreallocateChildren) const override;

  // nsIMutationObserver
  NS_DECL_NSIMUTATIONOBSERVER_CHARACTERDATACHANGED
  NS_DECL_NSIMUTATIONOBSERVER_CONTENTAPPENDED
  NS_DECL_NSIMUTATIONOBSERVER_CONTENTINSERTED
  NS_DECL_NSIMUTATIONOBSERVER_CONTENTREMOVED

  // WebIDL
  void GetXmlspace(nsAString & aXmlspace);
  void SetXmlspace(const nsAString & aXmlspace, ErrorResult& rv);
  void GetMedia(nsAString & aMedia);
  void SetMedia(const nsAString& aMedia, ErrorResult& rv);
  bool Scoped() const;
  void SetScoped(bool aScoped, ErrorResult& rv);
  void GetType(nsAString & aType);
  void SetType(const nsAString& aType, ErrorResult& rv);
  void GetTitle(nsAString & aTitle);
  void SetTitle(const nsAString& aTitle, ErrorResult& rv);

protected:
  // Dummy init method to make the NS_IMPL_NS_NEW_SVG_ELEMENT and
  // NS_IMPL_ELEMENT_CLONE_WITH_INIT usable with this class. This should be
  // completely optimized away.
  inline nsresult Init()
  {
    return NS_OK;
  }

  // nsStyleLinkElement overrides
  already_AddRefed<nsIURI> GetStyleSheetURL(bool* aIsInline) override;

  void GetStyleSheetInfo(nsAString& aTitle,
                         nsAString& aType,
                         nsAString& aMedia,
                         bool* aIsScoped,
                         bool* aIsAlternate) override;
  virtual CORSMode GetCORSMode() const override;

  /**
   * Common method to call from the various mutation observer methods.
   * aContent is a content node that's either the one that changed or its
   * parent; we should only respond to the change if aContent is non-anonymous.
   */
  void ContentChanged(nsIContent* aContent);
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_SVGStyleElement_h
