/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_HTMLSourceElement_h
#define mozilla_dom_HTMLSourceElement_h

#include "mozilla/Attributes.h"
#include "nsIDOMHTMLSourceElement.h"
#include "nsGenericHTMLElement.h"
#include "mozilla/dom/HTMLMediaElement.h"

class nsMediaList;

namespace mozilla {
namespace dom {

class ResponsiveImageSelector;
class HTMLSourceElement MOZ_FINAL : public nsGenericHTMLElement,
                                    public nsIDOMHTMLSourceElement
{
public:
  explicit HTMLSourceElement(already_AddRefed<mozilla::dom::NodeInfo>& aNodeInfo);

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  NS_IMPL_FROMCONTENT_HTML_WITH_TAG(HTMLSourceElement, source)

  // nsIDOMHTMLSourceElement
  NS_DECL_NSIDOMHTMLSOURCEELEMENT

  virtual nsresult Clone(mozilla::dom::NodeInfo* aNodeInfo, nsINode** aResult) const MOZ_OVERRIDE;

  // Override BindToTree() so that we can trigger a load when we add a
  // child source element.
  virtual nsresult BindToTree(nsIDocument* aDocument, nsIContent* aParent,
                              nsIContent* aBindingParent,
                              bool aCompileEventHandlers) MOZ_OVERRIDE;
  virtual void UnbindFromTree(bool aDeep, bool aNullParent) MOZ_OVERRIDE;

  // If this element's media attr matches for its owner document.  Returns true
  // if no media attr was set.
  bool MatchesCurrentMedia();

  // WebIDL
  void GetSrc(nsString& aSrc)
  {
    GetURIAttr(nsGkAtoms::src, nullptr, aSrc);
  }
  void SetSrc(const nsAString& aSrc, mozilla::ErrorResult& rv)
  {
    SetHTMLAttr(nsGkAtoms::src, aSrc, rv);
  }

  void GetType(nsString& aType)
  {
    GetHTMLAttr(nsGkAtoms::type, aType);
  }
  void SetType(const nsAString& aType, ErrorResult& rv)
  {
    SetHTMLAttr(nsGkAtoms::type, aType, rv);
  }

  void GetSrcset(nsString& aSrcset)
  {
    GetHTMLAttr(nsGkAtoms::srcset, aSrcset);
  }
  void SetSrcset(const nsAString& aSrcset, mozilla::ErrorResult& rv)
  {
    SetHTMLAttr(nsGkAtoms::srcset, aSrcset, rv);
  }

  void GetSizes(nsString& aSizes)
  {
    GetHTMLAttr(nsGkAtoms::sizes, aSizes);
  }
  void SetSizes(const nsAString& aSizes, mozilla::ErrorResult& rv)
  {
    SetHTMLAttr(nsGkAtoms::sizes, aSizes, rv);
  }

  void GetMedia(nsString& aMedia)
  {
    GetHTMLAttr(nsGkAtoms::media, aMedia);
  }
  void SetMedia(const nsAString& aMedia, mozilla::ErrorResult& rv)
  {
    SetHTMLAttr(nsGkAtoms::media, aMedia, rv);
  }

protected:
  virtual ~HTMLSourceElement();

  virtual JSObject* WrapNode(JSContext* aCx) MOZ_OVERRIDE;

protected:
  virtual void GetItemValueText(nsAString& text) MOZ_OVERRIDE;
  virtual void SetItemValueText(const nsAString& text) MOZ_OVERRIDE;

  virtual nsresult AfterSetAttr(int32_t aNameSpaceID, nsIAtom* aName,
                                const nsAttrValue* aValue,
                                bool aNotify) MOZ_OVERRIDE;


private:
  nsRefPtr<nsMediaList> mMediaList;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_HTMLSourceElement_h
