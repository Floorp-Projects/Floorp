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
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Allan Beaufour <abeaufour@novell.com>
 *  Olli Pettay <Olli.Pettay@helsinki.fi>
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

#include "nsString.h"
#include "nsIDOMElement.h"
#include "nsXFormsUtils.h"
#include "nsXFormsAccessors.h"
#include "nsDOMString.h"
#include "nsIEventStateManager.h"
#include "nsIContent.h"

NS_IMPL_ISUPPORTS2(nsXFormsAccessors, nsIXFormsAccessors, nsIClassInfo)

void
nsXFormsAccessors::Destroy()
{
  mElement = nsnull;
  mDelegate = nsnull;
}

nsresult
nsXFormsAccessors::GetState(PRInt32 aState, PRBool *aStateVal)
{
  NS_ENSURE_ARG_POINTER(aStateVal);
  nsCOMPtr<nsIContent> content(do_QueryInterface(mElement));
  *aStateVal = (content && (content->IntrinsicState() & aState));

  return NS_OK;
}

NS_IMETHODIMP
nsXFormsAccessors::GetValue(nsAString &aValue)
{
  if (mDelegate) {
    mDelegate->GetValue(aValue);
  } else {
    SetDOMStringToNull(aValue);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsXFormsAccessors::SetValue(const nsAString & aValue)
{
  return mDelegate ? mDelegate->SetValue(aValue) : NS_OK;
}

NS_IMETHODIMP
nsXFormsAccessors::HasBoundNode(PRBool *aHasBoundNode)
{
  NS_ENSURE_ARG_POINTER(aHasBoundNode);
  *aHasBoundNode = PR_FALSE;
  return mDelegate ? mDelegate->GetHasBoundNode(aHasBoundNode) : NS_OK;
}

NS_IMETHODIMP
nsXFormsAccessors::IsReadonly(PRBool *aStateVal)
{
  return GetState(NS_EVENT_STATE_MOZ_READONLY, aStateVal);
}

NS_IMETHODIMP
nsXFormsAccessors::IsRelevant(PRBool *aStateVal)
{
  return GetState(NS_EVENT_STATE_ENABLED, aStateVal);
}

NS_IMETHODIMP
nsXFormsAccessors::IsRequired(PRBool *aStateVal)
{
  return GetState(NS_EVENT_STATE_REQUIRED, aStateVal);
}

NS_IMETHODIMP
nsXFormsAccessors::IsValid(PRBool *aStateVal)
{
  return GetState(NS_EVENT_STATE_VALID, aStateVal);
}

// nsIClassInfo implementation

static const nsIID sScriptingIIDs[] = {
  NS_IXFORMSACCESSORS_IID
};

NS_IMETHODIMP
nsXFormsAccessors::GetInterfaces(PRUint32 *aCount, nsIID * **aArray)
{
  return nsXFormsUtils::CloneScriptingInterfaces(sScriptingIIDs,
                                                 NS_ARRAY_LENGTH(sScriptingIIDs),
                                                 aCount, aArray);
}

NS_IMETHODIMP
nsXFormsAccessors::GetHelperForLanguage(PRUint32 language,
                                        nsISupports **_retval)
{
  *_retval = nsnull;
  return NS_OK;
}

NS_IMETHODIMP
nsXFormsAccessors::GetContractID(char * *aContractID)
{
  *aContractID = nsnull;
  return NS_OK;
}

NS_IMETHODIMP
nsXFormsAccessors::GetClassDescription(char * *aClassDescription)
{
  *aClassDescription = nsnull;
  return NS_OK;
}

NS_IMETHODIMP
nsXFormsAccessors::GetClassID(nsCID * *aClassID)
{
  *aClassID = nsnull;
  return NS_OK;
}

NS_IMETHODIMP
nsXFormsAccessors::GetImplementationLanguage(PRUint32 *aImplementationLanguage)
{
  *aImplementationLanguage = nsIProgrammingLanguage::CPLUSPLUS;
  return NS_OK;
}

NS_IMETHODIMP
nsXFormsAccessors::GetFlags(PRUint32 *aFlags)
{
  *aFlags = nsIClassInfo::DOM_OBJECT;
  return NS_OK;
}

NS_IMETHODIMP
nsXFormsAccessors::GetClassIDNoAlloc(nsCID *aClassIDNoAlloc)
{
  return NS_ERROR_NOT_AVAILABLE;
}
