/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
#include "nsIDOMHTMLMapElement.h"
#include "nsIDOMEventReceiver.h"
#include "nsIHTMLContent.h"
#include "nsGenericHTMLElement.h"
#include "nsHTMLAtoms.h"
#include "nsHTMLIIDs.h"
#include "nsIStyleContext.h"
#include "nsStyleConsts.h"
#include "nsIPresContext.h"
#include "GenericElementCollection.h"
#include "nsIDocument.h"
#include "nsIHTMLDocument.h"
#include "nsCOMPtr.h"


class nsHTMLMapElement : public nsGenericHTMLContainerElement,
                         public nsIDOMHTMLMapElement
{
public:
  nsHTMLMapElement();
  virtual ~nsHTMLMapElement();

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  // nsIDOMNode
  NS_FORWARD_NSIDOMNODE_NO_CLONENODE(nsGenericHTMLContainerElement::)

  // nsIDOMElement
  NS_FORWARD_NSIDOMELEMENT(nsGenericHTMLContainerElement::)

  // nsIDOMHTMLElement
  NS_FORWARD_NSIDOMHTMLELEMENT(nsGenericHTMLContainerElement::)

  // nsIDOMHTMLMapElement
  NS_DECL_NSIDOMHTMLMAPELEMENT

  NS_IMETHOD GetMappedAttributeImpact(const nsIAtom* aAttribute, PRInt32 aModType,
                                      PRInt32& aHint) const;
  NS_IMETHOD SetDocument(nsIDocument* aDocument, PRBool aDeep,
                         PRBool aCompileEventHandlers);
#ifdef DEBUG
  NS_IMETHOD SizeOf(nsISizeOfHandler* aSizer, PRUint32* aResult) const;
#endif

protected:
  GenericElementCollection* mAreas;
};


nsresult
NS_NewHTMLMapElement(nsIHTMLContent** aInstancePtrResult,
                     nsINodeInfo *aNodeInfo)
{
  NS_ENSURE_ARG_POINTER(aInstancePtrResult);

  nsHTMLMapElement* it = new nsHTMLMapElement();

  if (!it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  nsresult rv = it->Init(aNodeInfo);

  if (NS_FAILED(rv)) {
    delete it;

    return rv;
  }

  *aInstancePtrResult = NS_STATIC_CAST(nsIHTMLContent *, it);
  NS_ADDREF(*aInstancePtrResult);

  return NS_OK;
}


nsHTMLMapElement::nsHTMLMapElement()
{
  mAreas = nsnull;
}

nsHTMLMapElement::~nsHTMLMapElement()
{
  if (mAreas) {
    mAreas->ParentDestroyed();
    NS_RELEASE(mAreas);
  }
}


NS_IMPL_ADDREF_INHERITED(nsHTMLMapElement, nsGenericElement) 
NS_IMPL_RELEASE_INHERITED(nsHTMLMapElement, nsGenericElement) 


// QueryInterface implementation for nsHTMLMapElement
NS_HTML_CONTENT_INTERFACE_MAP_BEGIN(nsHTMLMapElement,
                                    nsGenericHTMLContainerElement)
  NS_INTERFACE_MAP_ENTRY(nsIDOMHTMLMapElement)
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO(HTMLMapElement)
NS_HTML_CONTENT_INTERFACE_MAP_END


NS_IMETHODIMP 
nsHTMLMapElement::SetDocument(nsIDocument* aDocument, PRBool aDeep,
                              PRBool aCompileEventHandlers)
{
  nsCOMPtr<nsIHTMLDocument> htmlDoc(do_QueryInterface(mDocument));
  nsresult rv;

  if (htmlDoc) {
    htmlDoc->RemoveImageMap(this);
  }

  rv = nsGenericHTMLContainerElement::SetDocument(aDocument, aDeep,
                                                  aCompileEventHandlers);

  if (NS_SUCCEEDED(rv) && htmlDoc) {
    htmlDoc->AddImageMap(this);
  }
  
  return rv;
}

NS_IMETHODIMP
nsHTMLMapElement::CloneNode(PRBool aDeep, nsIDOMNode** aReturn)
{
  NS_ENSURE_ARG_POINTER(aReturn);
  *aReturn = nsnull;

  nsHTMLMapElement* it = new nsHTMLMapElement();

  if (!it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  nsCOMPtr<nsIDOMNode> kungFuDeathGrip(it);

  nsresult rv = it->Init(mNodeInfo);

  if (NS_FAILED(rv))
    return rv;

  CopyInnerTo(this, it, aDeep);

  *aReturn = NS_STATIC_CAST(nsIDOMNode *, it);

  NS_ADDREF(*aReturn);

  return NS_OK;
}

NS_IMETHODIMP
nsHTMLMapElement::GetAreas(nsIDOMHTMLCollection** aAreas)
{
  NS_ENSURE_ARG_POINTER(aAreas);

  if (!mAreas) {
    mAreas = new GenericElementCollection(NS_STATIC_CAST(nsIContent*, this),
                                          nsHTMLAtoms::area);

    if (!mAreas) {
      return NS_ERROR_OUT_OF_MEMORY;
    }

    NS_ADDREF(mAreas);
  }

  *aAreas = NS_STATIC_CAST(nsIDOMHTMLCollection*, mAreas);
  NS_ADDREF(*aAreas);

  return NS_OK;
}


NS_IMPL_STRING_ATTR(nsHTMLMapElement, Name, name)


NS_IMETHODIMP
nsHTMLMapElement::GetMappedAttributeImpact(const nsIAtom* aAttribute, PRInt32 aModType,
                                           PRInt32& aHint) const
{
  if (aAttribute == nsHTMLAtoms::name) {
    aHint = NS_STYLE_HINT_RECONSTRUCT_ALL;
  }
  else if (!GetCommonMappedAttributesImpact(aAttribute, aHint)) {
    aHint = NS_STYLE_HINT_CONTENT;
  }

  return NS_OK;
}

#ifdef DEBUG
NS_IMETHODIMP
nsHTMLMapElement::SizeOf(nsISizeOfHandler* aSizer, PRUint32* aResult) const
{
  *aResult = sizeof(*this) + BaseSizeOf(aSizer);

  return NS_OK;
}
#endif
