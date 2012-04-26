/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla SVG Project code.
 *
 * The Initial Developer of the Original Code is
 * Jonathan Watt.
 * Portions created by the Initial Developer are Copyright (C) 2004
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Jonathan Watt <jonathan.watt@strath.ac.uk> (original author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsSVGStylableElement.h"
#include "nsIDOMSVGTitleElement.h"

typedef nsSVGStylableElement nsSVGTitleElementBase;

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
  NS_FORWARD_NSIDOMNODE(nsSVGTitleElementBase::)
  NS_FORWARD_NSIDOMELEMENT(nsSVGTitleElementBase::)
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
                                   PRInt32 aNewIndexInContainer)
{
  SendTitleChangeEvent(false);
}

void
nsSVGTitleElement::ContentInserted(nsIDocument *aDocument,
                                   nsIContent *aContainer,
                                   nsIContent *aChild,
                                   PRInt32 aIndexInContainer)
{
  SendTitleChangeEvent(false);
}

void
nsSVGTitleElement::ContentRemoved(nsIDocument *aDocument,
                                  nsIContent *aContainer,
                                  nsIContent *aChild,
                                  PRInt32 aIndexInContainer,
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
