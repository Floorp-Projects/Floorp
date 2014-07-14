/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_HTMLMetaElement_h
#define mozilla_dom_HTMLMetaElement_h

#include "mozilla/Attributes.h"
#include "nsGenericHTMLElement.h"
#include "nsIDOMHTMLMetaElement.h"

namespace mozilla {
namespace dom {

class HTMLMetaElement MOZ_FINAL : public nsGenericHTMLElement,
                                  public nsIDOMHTMLMetaElement
{
public:
  HTMLMetaElement(already_AddRefed<mozilla::dom::NodeInfo>& aNodeInfo);

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  // nsIDOMHTMLMetaElement
  NS_DECL_NSIDOMHTMLMETAELEMENT

  virtual nsresult BindToTree(nsIDocument* aDocument, nsIContent* aParent,
                              nsIContent* aBindingParent,
                              bool aCompileEventHandlers) MOZ_OVERRIDE;
  virtual void UnbindFromTree(bool aDeep = true,
                              bool aNullParent = true) MOZ_OVERRIDE;

  virtual nsresult AfterSetAttr(int32_t aNameSpaceID, nsIAtom* aName,
                                const nsAttrValue* aValue, bool aNotify) MOZ_OVERRIDE;

  void CreateAndDispatchEvent(nsIDocument* aDoc, const nsAString& aEventName);

  virtual nsresult Clone(mozilla::dom::NodeInfo *aNodeInfo, nsINode **aResult) const MOZ_OVERRIDE;

  // XPCOM GetName is fine.
  void SetName(const nsAString& aName, ErrorResult& aRv)
  {
    SetHTMLAttr(nsGkAtoms::name, aName, aRv);
  }
  // XPCOM GetHttpEquiv is fine.
  void SetHttpEquiv(const nsAString& aHttpEquiv, ErrorResult& aRv)
  {
    SetHTMLAttr(nsGkAtoms::httpEquiv, aHttpEquiv, aRv);
  }
  // XPCOM GetContent is fine.
  void SetContent(const nsAString& aContent, ErrorResult& aRv)
  {
    SetHTMLAttr(nsGkAtoms::content, aContent, aRv);
  }
  // XPCOM GetScheme is fine.
  void SetScheme(const nsAString& aScheme, ErrorResult& aRv)
  {
    SetHTMLAttr(nsGkAtoms::scheme, aScheme, aRv);
  }

  virtual JSObject* WrapNode(JSContext* aCx) MOZ_OVERRIDE;

protected:
  virtual ~HTMLMetaElement();

  virtual void GetItemValueText(nsAString& text) MOZ_OVERRIDE;
  virtual void SetItemValueText(const nsAString& text) MOZ_OVERRIDE;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_HTMLMetaElement_h
