/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __NS_SVGSTYLABLEELEMENT_H__
#define __NS_SVGSTYLABLEELEMENT_H__

#include "nsAutoPtr.h"
#include "nsIDOMSVGStylable.h"
#include "nsSVGClass.h"
#include "nsSVGElement.h"

typedef nsSVGElement nsSVGStylableElementBase;

class nsSVGStylableElement : public nsSVGStylableElementBase,
                             public nsIDOMSVGStylable
{
protected:
  nsSVGStylableElement(already_AddRefed<nsINodeInfo> aNodeInfo);

public:
  // interfaces:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIDOMSVGSTYLABLE

  // nsIContent
  virtual const nsAttrValue* DoGetClasses() const;

  nsIDOMCSSStyleDeclaration* GetStyle(nsresult* retval)
  {
    return nsSVGStylableElementBase::GetStyle(retval);
  }
  virtual bool ParseAttribute(PRInt32 aNamespaceID, nsIAtom* aAttribute,
                              const nsAString& aValue, nsAttrValue& aResult);
  virtual nsresult UnsetAttr(PRInt32 aNameSpaceID, nsIAtom* aAttribute,
                             bool aNotify);

  virtual nsISMILAttr* GetAnimatedAttr(PRInt32 aNamespaceID, nsIAtom* aName);

  void DidAnimateClass();

protected:
  nsSVGClass mClassAttribute;
  nsAutoPtr<nsAttrValue> mClassAnimAttr;
};


#endif // __NS_SVGSTYLABLEELEMENT_H__
