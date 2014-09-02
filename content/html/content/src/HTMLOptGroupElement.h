/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_HTMLOptGroupElement_h
#define mozilla_dom_HTMLOptGroupElement_h

#include "mozilla/Attributes.h"
#include "nsIDOMHTMLOptGroupElement.h"
#include "nsGenericHTMLElement.h"

namespace mozilla {
class EventChainPreVisitor;
namespace dom {

class HTMLOptGroupElement MOZ_FINAL : public nsGenericHTMLElement,
                                      public nsIDOMHTMLOptGroupElement
{
public:
  explicit HTMLOptGroupElement(already_AddRefed<mozilla::dom::NodeInfo>& aNodeInfo);

  NS_IMPL_FROMCONTENT_HTML_WITH_TAG(HTMLOptGroupElement, optgroup)

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  // nsIDOMHTMLOptGroupElement
  NS_DECL_NSIDOMHTMLOPTGROUPELEMENT

  // nsINode
  virtual nsresult InsertChildAt(nsIContent* aKid, uint32_t aIndex,
                                 bool aNotify) MOZ_OVERRIDE;
  virtual void RemoveChildAt(uint32_t aIndex, bool aNotify) MOZ_OVERRIDE;

  // nsIContent
  virtual nsresult PreHandleEvent(EventChainPreVisitor& aVisitor) MOZ_OVERRIDE;

  virtual EventStates IntrinsicState() const MOZ_OVERRIDE;
 
  virtual nsresult Clone(mozilla::dom::NodeInfo* aNodeInfo, nsINode** aResult) const MOZ_OVERRIDE;

  virtual nsresult AfterSetAttr(int32_t aNameSpaceID, nsIAtom* aName,
                                const nsAttrValue* aValue, bool aNotify) MOZ_OVERRIDE;

  virtual nsIDOMNode* AsDOMNode() MOZ_OVERRIDE { return this; }

  virtual bool IsDisabled() const MOZ_OVERRIDE {
    return HasAttr(kNameSpaceID_None, nsGkAtoms::disabled);
  }

  bool Disabled() const
  {
    return GetBoolAttr(nsGkAtoms::disabled);
  }
  void SetDisabled(bool aValue, ErrorResult& aError)
  {
     SetHTMLBoolAttr(nsGkAtoms::disabled, aValue, aError);
  }

  // The XPCOM GetLabel is OK for us
  void SetLabel(const nsAString& aLabel, ErrorResult& aError)
  {
    SetHTMLAttr(nsGkAtoms::label, aLabel, aError);
  }

protected:
  virtual ~HTMLOptGroupElement();

  virtual JSObject* WrapNode(JSContext* aCx) MOZ_OVERRIDE;

protected:

  /**
   * Get the select content element that contains this option
   * @param aSelectElement the select element [OUT]
   */
  nsIContent* GetSelect();
};

} // namespace dom
} // namespace mozilla

#endif /* mozilla_dom_HTMLOptGroupElement_h */
