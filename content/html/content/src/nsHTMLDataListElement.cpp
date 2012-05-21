/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsIDOMHTMLDataListElement.h"
#include "nsGenericHTMLElement.h"
#include "nsIDOMEventTarget.h"
#include "nsGkAtoms.h"
#include "nsIDOMHTMLOptionElement.h"


class nsHTMLDataListElement : public nsGenericHTMLElement,
                              public nsIDOMHTMLDataListElement
{
public:
  nsHTMLDataListElement(already_AddRefed<nsINodeInfo> aNodeInfo);
  virtual ~nsHTMLDataListElement();

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  // nsIDOMNode
  NS_FORWARD_NSIDOMNODE(nsGenericHTMLElement::)

  // nsIDOMElement
  NS_FORWARD_NSIDOMELEMENT(nsGenericHTMLElement::)

  // nsIDOMHTMLElement
  NS_FORWARD_NSIDOMHTMLELEMENT(nsGenericHTMLElement::)

  // nsIDOMHTMLDataListElement
  NS_DECL_NSIDOMHTMLDATALISTELEMENT

  virtual nsresult Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const;

  // This function is used to generate the nsContentList (option elements).
  static bool MatchOptions(nsIContent* aContent, PRInt32 aNamespaceID,
                             nsIAtom* aAtom, void* aData);

  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(nsHTMLDataListElement,
                                           nsGenericHTMLElement)

  virtual nsXPCClassInfo* GetClassInfo();
  virtual nsIDOMNode* AsDOMNode() { return this; }
protected:

  // <option>'s list inside the datalist element.
  nsRefPtr<nsContentList> mOptions;
};


NS_IMPL_NS_NEW_HTML_ELEMENT(DataList)


nsHTMLDataListElement::nsHTMLDataListElement(already_AddRefed<nsINodeInfo> aNodeInfo)
  : nsGenericHTMLElement(aNodeInfo)
{
}

nsHTMLDataListElement::~nsHTMLDataListElement()
{
}


NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(nsHTMLDataListElement,
                                                nsGenericHTMLElement)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mOptions)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_CLASS(nsHTMLDataListElement)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(nsHTMLDataListElement,
                                                  nsGenericHTMLElement)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR_AMBIGUOUS(mOptions, nsIDOMNodeList)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_ADDREF_INHERITED(nsHTMLDataListElement, nsGenericElement)
NS_IMPL_RELEASE_INHERITED(nsHTMLDataListElement, nsGenericElement)

DOMCI_NODE_DATA(HTMLDataListElement, nsHTMLDataListElement)

NS_INTERFACE_TABLE_HEAD_CYCLE_COLLECTION_INHERITED(nsHTMLDataListElement)
  NS_HTML_CONTENT_INTERFACE_TABLE1(nsHTMLDataListElement, nsIDOMHTMLDataListElement)
  NS_HTML_CONTENT_INTERFACE_TABLE_TO_MAP_SEGUE(nsHTMLDataListElement,
                                               nsGenericHTMLElement)
NS_HTML_CONTENT_INTERFACE_TABLE_TAIL_CLASSINFO(HTMLDataListElement)


NS_IMPL_ELEMENT_CLONE(nsHTMLDataListElement)

bool
nsHTMLDataListElement::MatchOptions(nsIContent* aContent, PRInt32 aNamespaceID,
                                    nsIAtom* aAtom, void* aData)
{
  return aContent->NodeInfo()->Equals(nsGkAtoms::option, kNameSpaceID_XHTML) &&
         !aContent->HasAttr(kNameSpaceID_None, nsGkAtoms::disabled);
}

NS_IMETHODIMP
nsHTMLDataListElement::GetOptions(nsIDOMHTMLCollection** aOptions)
{
  if (!mOptions) {
    mOptions = new nsContentList(this, MatchOptions, nsnull, nsnull, true);
  }

  NS_ADDREF(*aOptions = mOptions);

  return NS_OK;
}

