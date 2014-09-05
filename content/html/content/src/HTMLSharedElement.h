/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_HTMLSharedElement_h
#define mozilla_dom_HTMLSharedElement_h

#include "nsIDOMHTMLBaseElement.h"
#include "nsIDOMHTMLDirectoryElement.h"
#include "nsIDOMHTMLQuoteElement.h"
#include "nsIDOMHTMLHeadElement.h"
#include "nsIDOMHTMLHtmlElement.h"
#include "nsGenericHTMLElement.h"

#include "nsGkAtoms.h"

#include "mozilla/Attributes.h"
#include "mozilla/Assertions.h"

namespace mozilla {
namespace dom {

class HTMLSharedElement MOZ_FINAL : public nsGenericHTMLElement,
                                    public nsIDOMHTMLBaseElement,
                                    public nsIDOMHTMLDirectoryElement,
                                    public nsIDOMHTMLQuoteElement,
                                    public nsIDOMHTMLHeadElement,
                                    public nsIDOMHTMLHtmlElement
{
public:
  explicit HTMLSharedElement(already_AddRefed<mozilla::dom::NodeInfo>& aNodeInfo)
    : nsGenericHTMLElement(aNodeInfo)
  {
    if (mNodeInfo->Equals(nsGkAtoms::head) ||
        mNodeInfo->Equals(nsGkAtoms::html)) {
      SetHasWeirdParserInsertionMode();
    }
  }

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  // nsIDOMHTMLBaseElement
  NS_DECL_NSIDOMHTMLBASEELEMENT

  // nsIDOMHTMLQuoteElement
  NS_DECL_NSIDOMHTMLQUOTEELEMENT

  // nsIDOMHTMLHeadElement
  NS_DECL_NSIDOMHTMLHEADELEMENT

  // nsIDOMHTMLHtmlElement
  NS_DECL_NSIDOMHTMLHTMLELEMENT

  // nsIContent
  virtual bool ParseAttribute(int32_t aNamespaceID,
                                nsIAtom* aAttribute,
                                const nsAString& aValue,
                                nsAttrValue& aResult) MOZ_OVERRIDE;
  nsresult SetAttr(int32_t aNameSpaceID, nsIAtom* aName,
                   const nsAString& aValue, bool aNotify)
  {
    return SetAttr(aNameSpaceID, aName, nullptr, aValue, aNotify);
  }
  virtual nsresult SetAttr(int32_t aNameSpaceID, nsIAtom* aName,
                           nsIAtom* aPrefix, const nsAString& aValue,
                           bool aNotify) MOZ_OVERRIDE;

  virtual nsresult UnsetAttr(int32_t aNameSpaceID, nsIAtom* aName,
                             bool aNotify) MOZ_OVERRIDE;

  virtual nsresult BindToTree(nsIDocument* aDocument, nsIContent* aParent,
                              nsIContent* aBindingParent,
                              bool aCompileEventHandlers) MOZ_OVERRIDE;

  virtual void UnbindFromTree(bool aDeep = true,
                              bool aNullParent = true) MOZ_OVERRIDE;

  virtual nsMapRuleToAttributesFunc GetAttributeMappingFunction() const MOZ_OVERRIDE;
  NS_IMETHOD_(bool) IsAttributeMapped(const nsIAtom* aAttribute) const MOZ_OVERRIDE;

  virtual nsresult Clone(mozilla::dom::NodeInfo *aNodeInfo, nsINode **aResult) const MOZ_OVERRIDE;

  // WebIDL API
  // HTMLParamElement
  void GetName(DOMString& aValue)
  {
    MOZ_ASSERT(mNodeInfo->Equals(nsGkAtoms::param));
    GetHTMLAttr(nsGkAtoms::name, aValue);
  }
  void SetName(const nsAString& aValue, ErrorResult& aResult)
  {
    MOZ_ASSERT(mNodeInfo->Equals(nsGkAtoms::param));
    SetHTMLAttr(nsGkAtoms::name, aValue, aResult);
  }
  void GetValue(DOMString& aValue)
  {
    MOZ_ASSERT(mNodeInfo->Equals(nsGkAtoms::param));
    GetHTMLAttr(nsGkAtoms::value, aValue);
  }
  void SetValue(const nsAString& aValue, ErrorResult& aResult)
  {
    MOZ_ASSERT(mNodeInfo->Equals(nsGkAtoms::param));
    SetHTMLAttr(nsGkAtoms::value, aValue, aResult);
  }
  void GetType(DOMString& aValue)
  {
    MOZ_ASSERT(mNodeInfo->Equals(nsGkAtoms::param));
    GetHTMLAttr(nsGkAtoms::type, aValue);
  }
  void SetType(const nsAString& aValue, ErrorResult& aResult)
  {
    MOZ_ASSERT(mNodeInfo->Equals(nsGkAtoms::param));
    SetHTMLAttr(nsGkAtoms::type, aValue, aResult);
  }
  void GetValueType(DOMString& aValue)
  {
    MOZ_ASSERT(mNodeInfo->Equals(nsGkAtoms::param));
    GetHTMLAttr(nsGkAtoms::valuetype, aValue);
  }
  void SetValueType(const nsAString& aValue, ErrorResult& aResult)
  {
    MOZ_ASSERT(mNodeInfo->Equals(nsGkAtoms::param));
    SetHTMLAttr(nsGkAtoms::valuetype, aValue, aResult);
  }

  // HTMLBaseElement
  void GetTarget(DOMString& aValue)
  {
    MOZ_ASSERT(mNodeInfo->Equals(nsGkAtoms::base));
    GetHTMLAttr(nsGkAtoms::target, aValue);
  }
  void SetTarget(const nsAString& aValue, ErrorResult& aResult)
  {
    MOZ_ASSERT(mNodeInfo->Equals(nsGkAtoms::base));
    SetHTMLAttr(nsGkAtoms::target, aValue, aResult);
  }
  // The XPCOM GetHref is fine for us
  void SetHref(const nsAString& aValue, ErrorResult& aResult)
  {
    MOZ_ASSERT(mNodeInfo->Equals(nsGkAtoms::base));
    SetHTMLAttr(nsGkAtoms::href, aValue, aResult);
  }

  // HTMLDirectoryElement
  bool Compact() const
  {
    MOZ_ASSERT(mNodeInfo->Equals(nsGkAtoms::dir));
    return GetBoolAttr(nsGkAtoms::compact);
  }
  void SetCompact(bool aCompact, ErrorResult& aResult)
  {
    MOZ_ASSERT(mNodeInfo->Equals(nsGkAtoms::dir));
    SetHTMLBoolAttr(nsGkAtoms::compact, aCompact, aResult);
  }

  // HTMLQuoteElement
  // The XPCOM GetCite works fine for us
  void SetCite(const nsAString& aValue, ErrorResult& aResult)
  {
    MOZ_ASSERT(mNodeInfo->Equals(nsGkAtoms::q) ||
               mNodeInfo->Equals(nsGkAtoms::blockquote));
    SetHTMLAttr(nsGkAtoms::cite, aValue, aResult);
  }

  // HTMLHtmlElement
  void GetVersion(DOMString& aValue)
  {
    MOZ_ASSERT(mNodeInfo->Equals(nsGkAtoms::html));
    GetHTMLAttr(nsGkAtoms::version, aValue);
  }
  void SetVersion(const nsAString& aValue, ErrorResult& aResult)
  {
    MOZ_ASSERT(mNodeInfo->Equals(nsGkAtoms::html));
    SetHTMLAttr(nsGkAtoms::version, aValue, aResult);
  }

protected:
  virtual ~HTMLSharedElement();

  virtual JSObject* WrapNode(JSContext* aCx) MOZ_OVERRIDE;
};

} // namespace mozilla
} // namespace dom

#endif // mozilla_dom_HTMLSharedElement_h
