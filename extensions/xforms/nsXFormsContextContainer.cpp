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
 * The Original Code is Mozilla XForms support.
 *
 * The Initial Developer of the Original Code is
 * Novell, Inc.
 * Portions created by the Initial Developer are Copyright (C) 2004
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Allan Beaufour <abeaufour@novell.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#include "nsIXTFXMLVisualWrapper.h"

#include "nsCOMPtr.h"
#include "nsString.h"

#include "nsIDOM3Node.h"
#include "nsIDOMDocument.h"
#include "nsIDOMElement.h"
#include "nsIDOMEventTarget.h"
#include "nsIDOMSerializer.h"
#include "nsIDOMXPathResult.h"

#include "nsIXFormsControl.h"
#include "nsIXFormsContextControl.h"
#include "nsXFormsStubElement.h"
#include "nsXFormsUtils.h"

#ifdef DEBUG
//#define DEBUG_XF_CONTEXTCONTAINER
#endif

/**
 * Implementation of \<contextcontainer\>.
 * 
 * \<contextcontainer\> is a pseudo-element that is wrapped around each row in
 * an "unrolled" \<repeat\> or \<itemset\>. @see nsXFormsRepeatElement and
 * nsXFormsItemSetElement.
 *
 * @todo Should this class inherit from nsIXFormsControl? (XXX)
 *
 * @todo Support ::repeat-item and ::repeat-index pseudo-elements. (XXX)
 *       @see http://www.w3.org/TR/xforms/sliceF.html#id2645142
 *       @see http://bugzilla.mozilla.org/show_bug.cgi?id=271724
 */
class nsXFormsContextContainer : public nsIXFormsControl,
                                  public nsXFormsXMLVisualStub,
                                  public nsIXFormsContextControl
{
protected:
  /** The DOM element for the node */
  nsCOMPtr<nsIDOMElement> mElement;

  /** The HTML representation for the node */
  nsCOMPtr<nsIDOMElement> mHTMLElement;

  /** The context node for the node */
  nsCOMPtr<nsIDOMElement> mContextNode;

public:
  NS_DECL_ISUPPORTS_INHERITED

  // nsIXTFXMLVisual overrides
  NS_IMETHOD OnCreated(nsIXTFXMLVisualWrapper *aWrapper);
  
  // nsIXTFVisual overrides
  NS_IMETHOD GetVisualContent(nsIDOMElement **aElement);
  NS_IMETHOD GetInsertionPoint(nsIDOMElement **aElement);

  // nsIXTFElement overrides
  NS_IMETHOD OnDestroyed();

  // nsIXFormsControl
  NS_DECL_NSIXFORMSCONTROL

  // nsIXFormsContextControl
  NS_DECL_NSIXFORMSCONTEXTCONTROL
  
};

NS_IMPL_ISUPPORTS_INHERITED2(nsXFormsContextContainer,
                             nsXFormsXMLVisualStub,
                             nsIXFormsControl,
                             nsIXFormsContextControl)


// nsIXTFXMLVisual
NS_IMETHODIMP
nsXFormsContextContainer::OnCreated(nsIXTFXMLVisualWrapper *aWrapper)
{
#ifdef DEBUG_XF_CONTEXTCONTAINER
  printf("nsXFormsContextContainer::OnCreated(aWrapper=%p)\n", (void*) aWrapper);
#endif

  nsresult rv;

  // Get node and document
  nsCOMPtr<nsIDOMElement> node;
  rv = aWrapper->GetElementNode(getter_AddRefs(node));
  NS_ENSURE_SUCCESS(rv, rv);
  mElement = node;
  NS_ASSERTION(mElement, "Wrapper is not an nsIDOMElement, we'll crash soon");
  
  nsCOMPtr<nsIDOMDocument> domDoc;
  rv = node->GetOwnerDocument(getter_AddRefs(domDoc));
  NS_ENSURE_SUCCESS(rv, rv);
  
  PRBool isBlock;

  // If we're a "contextcontainer" element, then we create a div child.
  // If not, we're a "contextcontainer-inline" element and create a span child.
  nsAutoString localName;
  mElement->GetLocalName(localName);
  isBlock = localName.EqualsLiteral("contextcontainer");

  // Create UI element
  rv = domDoc->CreateElementNS(NS_LITERAL_STRING("http://www.w3.org/1999/xhtml"),
                               isBlock ? NS_LITERAL_STRING("div") :
                               NS_LITERAL_STRING("span"),
                               getter_AddRefs(mHTMLElement));
  NS_ENSURE_SUCCESS(rv, rv);
  return NS_OK;
}

NS_IMETHODIMP
nsXFormsContextContainer::GetVisualContent(nsIDOMElement **aElement)
{
#ifdef DEBUG_XF_CONTEXTCONTAINER
  printf("nsXFormsContextContainer::GetVisualContent()\n");
#endif

  NS_IF_ADDREF(*aElement = mHTMLElement);
  return NS_OK;
}

NS_IMETHODIMP
nsXFormsContextContainer::GetInsertionPoint(nsIDOMElement **aElement)
{
#ifdef DEBUG_XF_CONTEXTCONTAINER
  printf("nsXFormsContextContainer::GetInsertionPoint()\n");
#endif

  NS_IF_ADDREF(*aElement = mHTMLElement);
  
  return NS_OK;
}

// nsIXTFElement
NS_IMETHODIMP
nsXFormsContextContainer::OnDestroyed()
{
  mHTMLElement = nsnull;
  mElement = nsnull;
  mContextNode = nsnull;
  
  return NS_OK;
}

// nsIXFormsContextControl
NS_IMETHODIMP
nsXFormsContextContainer::SetContextNode(nsIDOMElement *aContextNode)
{
  mContextNode = aContextNode;
  return NS_OK;
}

NS_IMETHODIMP
nsXFormsContextContainer::GetContext(nsAString& aModelID,
                                      nsIDOMElement **aContextNode,
                                      PRInt32 *aContextPosition,
                                      PRInt32 *aContextSize)
{
  NS_IF_ADDREF(*aContextNode = mContextNode);
  nsAutoString val;
  mElement->GetAttribute(NS_LITERAL_STRING("contextsize"), val);
  PRInt32 errCode;
  *aContextSize = val.ToInteger(&errCode);
  if (errCode) {
    *aContextSize = 1;
  }  
  mElement->GetAttribute(NS_LITERAL_STRING("contextposition"), val);
  *aContextPosition = val.ToInteger(&errCode);
  if (errCode) {
    *aContextPosition = 1;
  }
  mElement->GetAttribute(NS_LITERAL_STRING("model"), aModelID);
  return NS_OK;
}

// nsXFormsControl
nsresult
nsXFormsContextContainer::Refresh()
{
  return NS_OK;
}

// Factory
NS_HIDDEN_(nsresult)
NS_NewXFormsContextContainer(nsIXTFElement **aResult)
{
  *aResult = new nsXFormsContextContainer();
  if (!*aResult)
    return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(*aResult);
  return NS_OK;
}
