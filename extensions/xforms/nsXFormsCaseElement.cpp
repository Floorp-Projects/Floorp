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
#include "nsIXFormsCaseElement.h"
#include "nsIXFormsCaseUIElement.h"
#include "nsIXFormsSwitchElement.h"
#include "nsCOMPtr.h"
#include "nsIDOMElement.h"
#include "nsString.h"
#include "nsIXTFBindableElementWrapper.h"

/**
 * Implementation of the XForms case element.
 */

class nsXFormsCaseElement : public nsXFormsBindableStub,
                            public nsIXFormsCaseElement
{
public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIXFORMSCASEELEMENT

  NS_IMETHOD OnCreated(nsIXTFBindableElementWrapper *aWrapper);
  NS_IMETHOD OnDestroyed();
  NS_IMETHOD BeginAddingChildren();
  NS_IMETHOD DoneAddingChildren();
  NS_IMETHOD AttributeSet(nsIAtom *aName, const nsAString &aNewValue);

  nsXFormsCaseElement()
  : mElement(nsnull), mDoneAddingChildren(PR_TRUE), mSelected(PR_FALSE)
  {
  }

protected:
  nsIDOMElement* mElement;
  PRPackedBool   mDoneAddingChildren;
  PRPackedBool   mSelected;
};

NS_IMPL_ISUPPORTS_INHERITED1(nsXFormsCaseElement,
                             nsXFormsBindableStub,
                             nsIXFormsCaseElement)

NS_IMETHODIMP
nsXFormsCaseElement::OnCreated(nsIXTFBindableElementWrapper *aWrapper)
{
  aWrapper->SetNotificationMask(nsIXTFElement::NOTIFY_ATTRIBUTE_SET |
                                nsIXTFElement::NOTIFY_BEGIN_ADDING_CHILDREN |
                                nsIXTFElement::NOTIFY_DONE_ADDING_CHILDREN);

  nsCOMPtr<nsIDOMElement> node;
  aWrapper->GetElementNode(getter_AddRefs(node));

  // It's ok to keep a pointer to mElement.  mElement will have an
  // owning reference to this object, so as long as we null out mElement in
  // OnDestroyed, it will always be valid.

  mElement = node;

  return NS_OK;
}

NS_IMETHODIMP
nsXFormsCaseElement::OnDestroyed()
{
  mElement = nsnull;
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
  mSelected = aEnable;

  nsCOMPtr<nsIXFormsCaseUIElement> caseUI(do_QueryInterface(mElement));
  if (caseUI) {
    caseUI->CaseSelected(mSelected);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsXFormsCaseElement::WidgetAttached()
{
  nsCOMPtr<nsIXFormsCaseUIElement> caseUI(do_QueryInterface(mElement));
  if (caseUI) {
    caseUI->CaseSelected(mSelected);
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
