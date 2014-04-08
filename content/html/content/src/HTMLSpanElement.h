/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_HTMLSpanElement_h
#define mozilla_dom_HTMLSpanElement_h

#include "mozilla/Attributes.h"
#include "nsIDOMHTMLElement.h"
#include "nsGenericHTMLElement.h"
#include "nsGkAtoms.h"
#include "nsStyleConsts.h"
#include "nsIAtom.h"
#include "nsRuleData.h"

namespace mozilla {
namespace dom {

class HTMLSpanElement MOZ_FINAL : public nsGenericHTMLElement
{
public:
  HTMLSpanElement(already_AddRefed<nsINodeInfo>& aNodeInfo)
    : nsGenericHTMLElement(aNodeInfo)
  {
  }
  virtual ~HTMLSpanElement();

  virtual nsresult Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const MOZ_OVERRIDE;

protected:
  virtual JSObject* WrapNode(JSContext *aCx) MOZ_OVERRIDE;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_HTMLSpanElement_h
