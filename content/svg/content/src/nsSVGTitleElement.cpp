/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsSVGElement.h"
#include "nsIDOMSVGTitleElement.h"
#include "nsStubMutationObserver.h"

typedef nsSVGElement nsSVGTitleElementBase;

class nsSVGTitleElement : public nsSVGTitleElementBase,
                          public nsIDOMSVGTitleElement,
                          public nsStubMutationObserver
{
protected:
  friend nsresult NS_NewSVGTitleElement(nsIContent **aResult,
                                        already_AddRefed<nsINodeInfo> aNodeInfo);
  nsSVGTitleElement(already_AddRefed<nsINodeInfo> aNodeInfo);
  nsresult Init();

public:
  // interfaces:

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIDOMSVGTITLEELEMENT

  // xxx I wish we could use virtual inheritance
  NS_FORWARD_NSIDOMNODE_TO_NSINODE
  NS_FORWARD_NSIDOMELEMENT_TO_GENERIC
  NS_FORWARD_NSIDOMSVGELEMENT(nsSVGTitleElementBase::)

  // nsIMutationObserver
  NS_DECL_NSIMUTATIONOBSERVER_CHARACTERDATACHANGED
  NS_DECL_NSIMUTATIONOBSERVER_CONTENTAPPENDED
  NS_DECL_NSIMUTATIONOBSERVER_CONTENTINSERTED
  NS_DECL_NSIMUTATIONOBSERVER_CONTENTREMOVED

  virtual nsresult Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const;

  virtual nsresult BindToTree(nsIDocument *aDocument, nsIContent *aParent,
                              nsIContent *aBindingParent,
                              bool aCompileEventHandlers);

  virtual void UnbindFromTree(bool aDeep = true,
                              bool aNullParent = true);

  virtual void DoneAddingChildren(bool aHaveNotified);

  virtual nsXPCClassInfo* GetClassInfo();

  virtual nsIDOMNode* AsDOMNode() { return this; }
private:
  void SendTitleChangeEvent(bool aBound);
};

NS_IMPL_NS_NEW_SVG_ELEMENT(Title)


//----------------------------------------------------------------------
// nsISupports methods

NS_IMPL_ADDREF_INHERITED(nsSVGTitleElement, nsSVGTitleElementBase)
NS_IMPL_RELEASE_INHERITED(nsSVGTitleElement, nsSVGTitleElementBase)

DOMCI_NODE_DATA(SVGTitleElement, nsSVGTitleElement)

NS_INTERFACE_TABLE_HEAD(nsSVGTitleElement)
  NS_NODE_INTERFACE_TABLE5(nsSVGTitleElement, nsIDOMNode, nsIDOMElement,
                           nsIDOMSVGElement, nsIDOMSVGTitleElement,
                           nsIMutationObserver)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(SVGTitleElement)
NS_INTERFACE_MAP_END_INHERITING(nsSVGTitleElementBase)


//----------------------------------------------------------------------
// Implementation

nsSVGTitleElement::nsSVGTitleElement(already_AddRefed<nsINodeInfo> aNodeInfo)
  : nsSVGTitleElementBase(aNodeInfo)
{
  AddMutationObserver(this);
}

nsresult
nsSVGTitleElement::Init()
{
  return nsSVGTitleElementBase::Init();
}

void
nsSVGTitleElement::CharacterDataChanged(nsIDocument *aDocument,
                                        nsIContent *aContent,
                                        CharacterDataChangeInfo *aInfo)
{
  SendTitleChangeEvent(false);
}

void
nsSVGTitleElement::ContentAppended(nsIDocument *aDocument,
                                   nsIContent *aContainer,
                                   nsIContent *aFirstNewContent,
                                   int32_t aNewIndexInContainer)
{
  SendTitleChangeEvent(false);
}

void
nsSVGTitleElement::ContentInserted(nsIDocument *aDocument,
                                   nsIContent *aContainer,
                                   nsIContent *aChild,
                                   int32_t aIndexInContainer)
{
  SendTitleChangeEvent(false);
}

void
nsSVGTitleElement::ContentRemoved(nsIDocument *aDocument,
                                  nsIContent *aContainer,
                                  nsIContent *aChild,
                                  int32_t aIndexInContainer,
                                  nsIContent *aPreviousSibling)
{
  SendTitleChangeEvent(false);
}

nsresult
nsSVGTitleElement::BindToTree(nsIDocument *aDocument,
                               nsIContent *aParent,
                               nsIContent *aBindingParent,
                               bool aCompileEventHandlers)
{
  // Let this fall through.
  nsresult rv = nsSVGTitleElementBase::BindToTree(aDocument, aParent,
                                                  aBindingParent,
                                                  aCompileEventHandlers);
  NS_ENSURE_SUCCESS(rv, rv);

  SendTitleChangeEvent(true);

  return NS_OK;
}

void
nsSVGTitleElement::UnbindFromTree(bool aDeep, bool aNullParent)
{
  SendTitleChangeEvent(false);

  // Let this fall through.
  nsSVGTitleElementBase::UnbindFromTree(aDeep, aNullParent);
}

void
nsSVGTitleElement::DoneAddingChildren(bool aHaveNotified)
{
  if (!aHaveNotified) {
    SendTitleChangeEvent(false);
  }
}

void
nsSVGTitleElement::SendTitleChangeEvent(bool aBound)
{
  nsIDocument* doc = GetCurrentDoc();
  if (doc) {
    doc->NotifyPossibleTitleChange(aBound);
  }
}

//----------------------------------------------------------------------
// nsIDOMNode methods

NS_IMPL_ELEMENT_CLONE_WITH_INIT(nsSVGTitleElement)
