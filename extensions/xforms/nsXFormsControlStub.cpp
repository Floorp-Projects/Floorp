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

#include "nsIModelElementPrivate.h"
#include "nsXFormsAtoms.h"
#include "nsXFormsControlStub.h"
#include "nsXFormsMDGEngine.h"

#include "nsIDOMEvent.h"
#include "nsIDOMXPathResult.h"
#include "nsIXTFXMLVisualWrapper.h"

NS_IMETHODIMP
nsXFormsControlStub::GetBoundNode(nsIDOMNode **aBoundNode)
{
  NS_IF_ADDREF(*aBoundNode = mBoundNode);
  return NS_OK;  
}

NS_IMETHODIMP
nsXFormsControlStub::GetDependencies(nsIArray **aDependencies)
{
  NS_IF_ADDREF(*aDependencies = mDependencies);
  return NS_OK;  
}

NS_IMETHODIMP
nsXFormsControlStub::GetElement(nsIDOMElement **aElement)
{
  NS_IF_ADDREF(*aElement = mElement);
  return NS_OK;  
}

/**
 * @note Refresh() is always called after a Bind(), so if a control decides to
 * do all the work in Refresh() this function implements a NOP Bind().
 */
NS_IMETHODIMP
nsXFormsControlStub::Bind()
{
  return NS_OK;
}

nsresult
nsXFormsControlStub::ProcessNodeBinding(const nsString          &aBindingAttr,
                                        PRUint16                 aResultType,
                                        nsIDOMXPathResult      **aResult,
                                        nsIModelElementPrivate **aModel)
{
  nsresult rv;

  mMDG = nsnull;
  if (!mDependencies) {  
    rv = NS_NewArray(getter_AddRefs(mDependencies));
  } else {
    rv = mDependencies->Clear();
  }
  NS_ENSURE_SUCCESS(rv, rv);
  
  nsCOMPtr<nsIModelElementPrivate> model;
  rv = nsXFormsUtils::EvaluateNodeBinding(mElement,
                                          kElementFlags,
                                          aBindingAttr,
                                          EmptyString(),
                                          aResultType,
                                          getter_AddRefs(model),
                                          aResult,
                                          mDependencies);

  NS_ENSURE_SUCCESS(rv, rv);

  if (model) {
    model->AddFormControl(this);
    model->GetMDG(&mMDG);
    if (aModel) {
      NS_ADDREF(*aModel = model);
    }
  }

  return NS_OK;
}

PRBool
nsXFormsControlStub::GetReadOnlyState()
{
  PRBool res = PR_FALSE;
  if (mBoundNode && mMDG) {
    const nsXFormsNodeState* ns = mMDG->GetNodeState(mBoundNode);
    if (ns) {
      res = ns->IsReadonly();
    }
  }
  return res;
}

void  
nsXFormsControlStub::ToggleProperty(const nsAString &aOn,
                                    const nsAString &aOff)
{
  mElement->SetAttribute(aOn, NS_LITERAL_STRING("1"));
  mElement->RemoveAttribute(aOff);
}

NS_IMETHODIMP
nsXFormsControlStub::HandleDefault(nsIDOMEvent *aEvent,
                                   PRBool      *aHandled)
{
  NS_ENSURE_ARG(aHandled);

  if (aEvent) {
    nsAutoString type;
    aEvent->GetType(type);

    *aHandled = PR_TRUE;
    /// @todo Change to be less cut-n-paste-stylish. Everything can be extraced
    /// from the sXFormsEventsEntries, only problem is the dash in
    /// read-only/read-write... (XXX)    
    if (type.EqualsASCII(sXFormsEventsEntries[eEvent_Valid].name)) {
      ToggleProperty(NS_LITERAL_STRING("valid"),
                     NS_LITERAL_STRING("invalid"));
    } else if (type.EqualsASCII(sXFormsEventsEntries[eEvent_Invalid].name)) {
      ToggleProperty(NS_LITERAL_STRING("invalid"),
                     NS_LITERAL_STRING("valid"));
    } else if (type.EqualsASCII(sXFormsEventsEntries[eEvent_Enabled].name)) {
      ToggleProperty(NS_LITERAL_STRING("enabled"),
                     NS_LITERAL_STRING("disabled"));
    } else if (type.EqualsASCII(sXFormsEventsEntries[eEvent_Disabled].name)) {
      ToggleProperty(NS_LITERAL_STRING("disabled"),
                     NS_LITERAL_STRING("enabled"));
    } else if (type.EqualsASCII(sXFormsEventsEntries[eEvent_Required].name)) {
      ToggleProperty(NS_LITERAL_STRING("required"),
                     NS_LITERAL_STRING("optional"));
    } else if (type.EqualsASCII(sXFormsEventsEntries[eEvent_Optional].name)) {
      ToggleProperty(NS_LITERAL_STRING("optional"),
                     NS_LITERAL_STRING("required"));
    } else if (type.EqualsASCII(sXFormsEventsEntries[eEvent_Readonly].name)) {
      ToggleProperty(NS_LITERAL_STRING("read-only"),
                     NS_LITERAL_STRING("read-write"));
    } else if (type.EqualsASCII(sXFormsEventsEntries[eEvent_Readwrite].name)) {
      ToggleProperty(NS_LITERAL_STRING("read-write"),
                     NS_LITERAL_STRING("read-only"));
    } else {
      *aHandled = PR_FALSE;
    }
  }
  
  return NS_OK;
}

NS_IMETHODIMP
nsXFormsControlStub::OnCreated(nsIXTFXMLVisualWrapper *aWrapper)
{
  aWrapper->SetNotificationMask(kStandardNotificationMask);

  aWrapper->GetElementNode(getter_AddRefs(mElement));
  NS_ASSERTION(mElement, "Wrapper is not an nsIDOMElement, we'll crash soon");

  return NS_OK;
}

NS_IMETHODIMP
nsXFormsControlStub::OnDestroyed()
{
  mMDG = nsnull;
  mElement = nsnull;

  return NS_OK;
}

NS_IMETHODIMP
nsXFormsControlStub::DocumentChanged(nsIDOMDocument *aNewDocument)
{
  // We need to re-evaluate our instance data binding when our document
  // changes, since our context can change
  if (aNewDocument) {
    Bind();
    Refresh();
  }

  return NS_OK;
}

NS_IMETHODIMP
nsXFormsControlStub::ParentChanged(nsIDOMElement *aNewParent)
{
  // We need to re-evaluate our instance data binding when our parent changes,
  // since xmlns declarations or our context could have changed.
  if (aNewParent) {
    Bind();
    Refresh();
  }

  return NS_OK;
}

NS_IMETHODIMP
nsXFormsControlStub::WillSetAttribute(nsIAtom *aName, const nsAString &aValue)
{
  if (aName == nsXFormsAtoms::model ||
      aName == nsXFormsAtoms::bind ||
      aName == nsXFormsAtoms::ref) {
    nsCOMPtr<nsIModelElementPrivate> model = nsXFormsUtils::GetModel(mElement);
    if (model)
      model->RemoveFormControl(this);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsXFormsControlStub::AttributeSet(nsIAtom *aName, const nsAString &aValue)
{
  if (aName == nsXFormsAtoms::model ||
      aName == nsXFormsAtoms::bind ||
      aName == nsXFormsAtoms::ref) {
    Bind();
    Refresh();
  }

  return NS_OK;
}
