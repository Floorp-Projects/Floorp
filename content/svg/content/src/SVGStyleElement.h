/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
                               already_AddRefed<nsINodeInfo>&& aNodeInfo);

typedef nsSVGElement SVGStyleElementBase;

namespace mozilla {
namespace dom {

class SVGStyleElement MOZ_FINAL : public SVGStyleElementBase,
                                  public nsStyleLinkElement,
                                  public nsStubMutationObserver
{
protected:
  friend nsresult (::NS_NewSVGStyleElement(nsIContent **aResult,
                                           already_AddRefed<nsINodeInfo>&& aNodeInfo));
  SVGStyleElement(already_AddRefed<nsINodeInfo>& aNodeInfo);

  virtual JSObject* WrapNode(JSContext *aCx) MOZ_OVERRIDE;

public:
  NS_DECL_ISUPPORTS_INHERITED

  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(SVGStyleElement,
                                           SVGStyleElementBase)

  // nsIContent
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
  virtual bool ParseAttribute(int32_t aNamespaceID,
                              nsIAtom* aAttribute,
                              const nsAString& aValue,
                              nsAttrValue& aResult) MOZ_OVERRIDE;

  virtual nsresult Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const MOZ_OVERRIDE;

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
  already_AddRefed<nsIURI> GetStyleSheetURL(bool* aIsInline) MOZ_OVERRIDE;

  void GetStyleSheetInfo(nsAString& aTitle,
                         nsAString& aType,
                         nsAString& aMedia,
                         bool* aIsScoped,
                         bool* aIsAlternate) MOZ_OVERRIDE;
  virtual CORSMode GetCORSMode() const MOZ_OVERRIDE;

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
