/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __NS_SVGSWITCHELEMENT_H__
#define __NS_SVGSWITCHELEMENT_H__

#include "DOMSVGTests.h"
#include "nsIDOMSVGSwitchElement.h"
#include "nsSVGGraphicElement.h"

typedef nsSVGGraphicElement nsSVGSwitchElementBase;

class nsSVGSwitchElement : public nsSVGSwitchElementBase,
                           public nsIDOMSVGSwitchElement,
                           public DOMSVGTests
{
  friend class nsSVGSwitchFrame;
protected:
  friend nsresult NS_NewSVGSwitchElement(nsIContent **aResult,
                                         already_AddRefed<nsINodeInfo> aNodeInfo);
  nsSVGSwitchElement(already_AddRefed<nsINodeInfo> aNodeInfo);

public:
  nsIContent * GetActiveChild() const
  { return mActiveChild; }
  void MaybeInvalidate();
    
  // interfaces:

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(nsSVGSwitchElement,
                                           nsSVGSwitchElementBase)
  NS_DECL_NSIDOMSVGSWITCHELEMENT

  // xxx I wish we could use virtual inheritance
  NS_FORWARD_NSIDOMNODE(nsSVGSwitchElementBase::)
  NS_FORWARD_NSIDOMELEMENT(nsSVGSwitchElementBase::)
  NS_FORWARD_NSIDOMSVGELEMENT(nsSVGSwitchElementBase::)

  // nsINode
  virtual nsresult InsertChildAt(nsIContent* aKid, PRUint32 aIndex,
                                 bool aNotify);
  virtual void RemoveChildAt(PRUint32 aIndex, bool aNotify);

  // nsIContent
  NS_IMETHOD_(bool) IsAttributeMapped(const nsIAtom* aAttribute) const;

  virtual nsresult Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const;

  virtual nsXPCClassInfo* GetClassInfo();

  virtual nsIDOMNode* AsDOMNode() { return this; }
private:
  void UpdateActiveChild()
  { mActiveChild = FindActiveChild(); }
  nsIContent* FindActiveChild() const;

  // only this child will be displayed
  nsCOMPtr<nsIContent> mActiveChild;
};

#endif // __NS_SVGSWITCHELEMENT_H__
