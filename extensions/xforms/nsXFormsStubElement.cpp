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
 * IBM Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2004
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Brian Ryner <bryner@brianryner.com>
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

#include "nsXFormsStubElement.h"
#include "nsIDOMElement.h"
#include "nsIDOMEventTarget.h"
#include "nsIDOM3Node.h"
#include "nsMemory.h"
#include "nsXFormsUtils.h"
#include "nsXFormsAtoms.h"
#include "nsIXTFElementWrapper.h"

// nsXFormsStubElement implementation

NS_IMPL_ISUPPORTS1(nsXFormsStubElement, nsIXTFElement)

NS_IMETHODIMP
nsXFormsStubElement::OnDestroyed()
{
  return NS_OK;
}

NS_IMETHODIMP
nsXFormsStubElement::GetIsAttributeHandler(PRBool *aIsAttributeHandler)
{
  *aIsAttributeHandler = PR_FALSE;
  return NS_OK;
}

NS_IMETHODIMP
nsXFormsStubElement::GetScriptingInterfaces(PRUint32 *aCount, nsIID ***aArray)
{
  NS_ENSURE_ARG_POINTER(aCount);
  NS_ENSURE_ARG_POINTER(aArray);
  *aCount = 0;
  *aArray = nsnull;
  return NS_OK;
}

NS_IMETHODIMP
nsXFormsStubElement::WillChangeDocument(nsIDOMDocument *aNewDocument)
{
  return NS_OK;
}

NS_IMETHODIMP
nsXFormsStubElement::DocumentChanged(nsIDOMDocument *aNewDocument)
{
  return NS_OK;
}

NS_IMETHODIMP
nsXFormsStubElement::WillChangeParent(nsIDOMElement *aNewParent)
{
  return NS_OK;
}

NS_IMETHODIMP
nsXFormsStubElement::ParentChanged(nsIDOMElement *aNewParent)
{
  return NS_OK;
}

NS_IMETHODIMP
nsXFormsStubElement::WillInsertChild(nsIDOMNode *aChild, PRUint32 aIndex)
{
  return NS_OK;
}

NS_IMETHODIMP
nsXFormsStubElement::ChildInserted(nsIDOMNode *aChild, PRUint32 aIndex)
{
  return NS_OK;
}

NS_IMETHODIMP
nsXFormsStubElement::WillAppendChild(nsIDOMNode *aChild)
{
  return NS_OK;
}

NS_IMETHODIMP
nsXFormsStubElement::ChildAppended(nsIDOMNode *aChild)
{
  return NS_OK;
}

NS_IMETHODIMP
nsXFormsStubElement::WillRemoveChild(PRUint32 aIndex)
{
  return NS_OK;
}

NS_IMETHODIMP
nsXFormsStubElement::ChildRemoved(PRUint32 aIndex)
{
  return NS_OK;
}

NS_IMETHODIMP
nsXFormsStubElement::WillSetAttribute(nsIAtom *aName,
                                      const nsAString &aNewValue)
{
  return NS_OK;
}

NS_IMETHODIMP
nsXFormsStubElement::AttributeSet(nsIAtom *aName, const nsAString &aNewValue)
{
  return NS_OK;
}

NS_IMETHODIMP
nsXFormsStubElement::WillRemoveAttribute(nsIAtom *aName)
{
  return NS_OK;
}

NS_IMETHODIMP
nsXFormsStubElement::AttributeRemoved(nsIAtom *aName)
{
  return NS_OK;
}

NS_IMETHODIMP
nsXFormsStubElement::BeginAddingChildren()
{
  return NS_OK;
}

NS_IMETHODIMP
nsXFormsStubElement::DoneAddingChildren()
{
  return NS_OK;
}

NS_IMETHODIMP
nsXFormsStubElement::HandleDefault(nsIDOMEvent *aEvent, PRBool *aHandled)
{
  *aHandled = PR_FALSE;
  return NS_OK;
}

NS_IMETHODIMP
nsXFormsStubElement::GetAccesskeyNode(nsIDOMAttr** aNode)
{
  return NS_OK;
}

NS_IMETHODIMP
nsXFormsStubElement::PerformAccesskey()
{
  return NS_OK;
}

NS_IMETHODIMP
nsXFormsStubElement::OnCreated(nsIXTFElementWrapper *aWrapper)
{
  return aWrapper->SetClassAttributeName(nsXFormsAtoms::clazz);
}

NS_IMETHODIMP
nsXFormsStubElement::CloneState(nsIDOMElement *aElement)
{
  return NS_OK;
}

nsresult
NS_NewXFormsStubElement(nsIXTFElement **aResult)
{
  *aResult = new nsXFormsStubElement();
  if (!*aResult)
    return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(*aResult);
  return NS_OK;
}

