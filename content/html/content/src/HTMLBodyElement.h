/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef HTMLBodyElement_h___
#define HTMLBodyElement_h___

#include "nsGenericHTMLElement.h"
#include "nsIDOMHTMLBodyElement.h"
#include "nsIStyleRule.h"

namespace mozilla {
namespace dom {

class BeforeUnloadEventHandlerNonNull;
class HTMLBodyElement;

class BodyRule: public nsIStyleRule
{
public:
  BodyRule(HTMLBodyElement* aPart);
  virtual ~BodyRule();

  NS_DECL_ISUPPORTS

  // nsIStyleRule interface
  virtual void MapRuleInfoInto(nsRuleData* aRuleData);
#ifdef DEBUG
  virtual void List(FILE* out = stdout, int32_t aIndent = 0) const;
#endif

  HTMLBodyElement*  mPart;  // not ref-counted, cleared by content 
};

class HTMLBodyElement : public nsGenericHTMLElement,
                        public nsIDOMHTMLBodyElement
{
public:
  using Element::GetText;
  using Element::SetText;

  HTMLBodyElement(already_AddRefed<nsINodeInfo> aNodeInfo)
    : nsGenericHTMLElement(aNodeInfo),
      mContentStyleRule(nullptr)
  {
  }
  virtual ~HTMLBodyElement();

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  // nsIDOMNode
  NS_FORWARD_NSIDOMNODE_TO_NSINODE

  // nsIDOMElement
  NS_FORWARD_NSIDOMELEMENT_TO_GENERIC

  // nsIDOMHTMLElement
  NS_FORWARD_NSIDOMHTMLELEMENT_TO_GENERIC

  // nsIDOMHTMLBodyElement
  NS_DECL_NSIDOMHTMLBODYELEMENT

  // Event listener stuff; we need to declare only the ones we need to
  // forward to window that don't come from nsIDOMHTMLBodyElement.
#define EVENT(name_, id_, type_, struct_) /* nothing; handled by the shim */
#define FORWARDED_EVENT(name_, id_, type_, struct_)                     \
  NS_IMETHOD GetOn##name_(JSContext *cx, jsval *vp);                    \
  NS_IMETHOD SetOn##name_(JSContext *cx, const jsval &v);
#include "nsEventNameList.h"
#undef FORWARDED_EVENT
#undef EVENT

  virtual bool ParseAttribute(int32_t aNamespaceID,
                              nsIAtom* aAttribute,
                              const nsAString& aValue,
                              nsAttrValue& aResult);
  virtual void UnbindFromTree(bool aDeep = true,
                              bool aNullParent = true);
  virtual nsMapRuleToAttributesFunc GetAttributeMappingFunction() const;
  NS_IMETHOD WalkContentStyleRules(nsRuleWalker* aRuleWalker);
  NS_IMETHOD_(bool) IsAttributeMapped(const nsIAtom* aAttribute) const;
  virtual already_AddRefed<nsIEditor> GetAssociatedEditor();
  virtual nsresult Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const;
  virtual nsXPCClassInfo* GetClassInfo();
  virtual nsIDOMNode* AsDOMNode() { return this; }
private:
  nsresult GetColorHelper(nsIAtom* aAtom, nsAString& aColor);

protected:
  BodyRule* mContentStyleRule;
};

} // namespace dom
} // namespace mozilla

#endif /* HTMLBodyElement_h___ */
