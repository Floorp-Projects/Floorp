/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_HTMLMetaElement_h
#define mozilla_dom_HTMLMetaElement_h

#include "mozilla/Attributes.h"
#include "nsGenericHTMLElement.h"

namespace mozilla {
namespace dom {

class HTMLMetaElement final : public nsGenericHTMLElement
{
public:
  explicit HTMLMetaElement(already_AddRefed<mozilla::dom::NodeInfo>& aNodeInfo);

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

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

  void CreateAndDispatchEvent(nsIDocument* aDoc, const nsAString& aEventName);

  virtual nsresult Clone(mozilla::dom::NodeInfo *aNodeInfo, nsINode **aResult,
                         bool aPreallocateChildren) const override;

  void GetName(nsAString& aValue)
  {
    GetHTMLAttr(nsGkAtoms::name, aValue);
  }
  void SetName(const nsAString& aName, ErrorResult& aRv)
  {
    SetHTMLAttr(nsGkAtoms::name, aName, aRv);
  }
  void GetHttpEquiv(nsAString& aValue)
  {
    GetHTMLAttr(nsGkAtoms::httpEquiv, aValue);
  }
  void SetHttpEquiv(const nsAString& aHttpEquiv, ErrorResult& aRv)
  {
    SetHTMLAttr(nsGkAtoms::httpEquiv, aHttpEquiv, aRv);
  }
  nsresult GetContent(nsAString& aValue)
  {
    GetHTMLAttr(nsGkAtoms::content, aValue);
    return NS_OK;
  }
  void SetContent(const nsAString& aContent, ErrorResult& aRv)
  {
    SetHTMLAttr(nsGkAtoms::content, aContent, aRv);
  }
  void GetScheme(nsAString& aValue)
  {
    GetHTMLAttr(nsGkAtoms::scheme, aValue);
  }
  void SetScheme(const nsAString& aScheme, ErrorResult& aRv)
  {
    SetHTMLAttr(nsGkAtoms::scheme, aScheme, aRv);
  }

  virtual JSObject* WrapNode(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

protected:
  virtual ~HTMLMetaElement();

private:
  nsresult SetMetaReferrer(nsIDocument* aDocument);
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_HTMLMetaElement_h
