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
 * Olli Pettay.
 * Portions created by the Initial Developer are Copyright (C) 2004
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Olli Pettay <Olli.Pettay@helsinki.fi> (original author)
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

#include "nsXFormsStubElement.h"
#include "nsXFormsAtoms.h"
#include "nsXFormsUtils.h"
#include "nsIXFormsSwitchElement.h"
#include "nsIXFormsCaseElement.h"
#include "nsIXTFXMLVisualWrapper.h"
#include "nsCOMPtr.h"
#include "nsIDOMElement.h"
#include "nsIDOMDocument.h"
#include "nsString.h"

#define SELECTED_CASE_STYLE ""
#define DESELECTED_CASE_STYLE "visibility:hidden;width:0px;height:0px;"

/**
 * Implementation of the XForms case element.
 */

class nsXFormsCaseElement : public nsIXFormsCaseElement,
                            public nsXFormsXMLVisualStub
{
public:
  nsXFormsCaseElement();

  NS_DECL_ISUPPORTS_INHERITED

  NS_IMETHOD OnCreated(nsIXTFXMLVisualWrapper *aWrapper);
  NS_IMETHOD OnDestroyed();
  NS_IMETHOD BeginAddingChildren();
  NS_IMETHOD DoneAddingChildren();

  NS_IMETHOD GetVisualContent(nsIDOMElement **aContent);
  NS_IMETHOD GetInsertionPoint(nsIDOMElement **aPoint);

  NS_IMETHOD AttributeSet(nsIAtom *aName, const nsAString &aNewValue);

  NS_DECL_NSIXFORMSCASEELEMENT

private:
  PRBool mDoneAddingChildren;
  nsIDOMElement* mElement;
  nsCOMPtr<nsIDOMElement> mVisual;
};

nsXFormsCaseElement::nsXFormsCaseElement() : mDoneAddingChildren(PR_TRUE)
{
}

NS_IMPL_ISUPPORTS_INHERITED1(nsXFormsCaseElement,
                             nsXFormsXMLVisualStub,
                             nsIXFormsCaseElement)

NS_IMETHODIMP
nsXFormsCaseElement::OnCreated(nsIXTFXMLVisualWrapper *aWrapper)
{
  aWrapper->SetNotificationMask(nsIXTFElement::NOTIFY_ATTRIBUTE_SET |
                                nsIXTFElement::NOTIFY_BEGIN_ADDING_CHILDREN |
                                nsIXTFElement::NOTIFY_DONE_ADDING_CHILDREN);

  nsCOMPtr<nsIDOMElement> node;
  aWrapper->GetElementNode(getter_AddRefs(node));
  mElement = node;
  NS_ASSERTION(mElement, "Wrapper is not an nsIDOMElement, we'll crash soon");

  nsCOMPtr<nsIDOMDocument> domDoc;
  mElement->GetOwnerDocument(getter_AddRefs(domDoc));

  nsresult rv = domDoc->CreateElementNS(NS_LITERAL_STRING(NS_NAMESPACE_XHTML),
                                        NS_LITERAL_STRING("div"),
                                        getter_AddRefs(mVisual));
  NS_ENSURE_SUCCESS(rv, rv);
  mVisual->SetAttribute(NS_LITERAL_STRING("style"),
                        NS_LITERAL_STRING(DESELECTED_CASE_STYLE));
  return NS_OK;
}

NS_IMETHODIMP
nsXFormsCaseElement::OnDestroyed()
{
  mElement = nsnull;
  mVisual = nsnull;
  return NS_OK;
}

NS_IMETHODIMP
nsXFormsCaseElement::GetVisualContent(nsIDOMElement **aContent)
{
  NS_ADDREF(*aContent = mVisual);
  return NS_OK;
}

NS_IMETHODIMP
nsXFormsCaseElement::GetInsertionPoint(nsIDOMElement **aPoint)
{
  NS_ADDREF(*aPoint = mVisual);
  return NS_OK;
}

NS_IMETHODIMP
nsXFormsCaseElement::BeginAddingChildren()
{
  mDoneAddingChildren = PR_FALSE;
  return NS_OK;
}

NS_IMETHODIMP
nsXFormsCaseElement::DoneAddingChildren()
{
  mDoneAddingChildren = PR_TRUE;
  return NS_OK;
}

NS_IMETHODIMP
nsXFormsCaseElement::AttributeSet(nsIAtom *aName, const nsAString &aNewValue)
{
  if (mDoneAddingChildren && mElement && aName == nsXFormsAtoms::selected) {
    nsCOMPtr<nsIDOMNode> parent;
    mElement->GetParentNode(getter_AddRefs(parent));
    nsCOMPtr<nsIXFormsSwitchElement> switchEl(do_QueryInterface(parent));
    if (switchEl)
      switchEl->SetSelected(mElement, aNewValue.EqualsLiteral("true"));
  }

  return NS_OK;
}

NS_IMETHODIMP
nsXFormsCaseElement::SetSelected(PRBool aEnable)
{
  if (mVisual) {
    mVisual->SetAttribute(NS_LITERAL_STRING("style"),
                          aEnable ? NS_LITERAL_STRING(SELECTED_CASE_STYLE)
                                  : NS_LITERAL_STRING(DESELECTED_CASE_STYLE));
  }
  return NS_OK;
}

NS_HIDDEN_(nsresult)
NS_NewXFormsCaseElement(nsIXTFElement **aResult)
{
  *aResult = new nsXFormsCaseElement();
  if (!*aResult)
    return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(*aResult);
  return NS_OK;
}
