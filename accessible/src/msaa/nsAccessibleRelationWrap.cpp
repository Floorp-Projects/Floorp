/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:expandtab:shiftwidth=2:tabstop=2:
 */
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2007
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Alexander Surkov <surkov.alexander@gmail.com> (original author)
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

#include "nsAccessibleRelationWrap.h"

#include "AccessibleRelation_i.c"
#include "nsAccessNodeWrap.h"

#include "nsArrayUtils.h"

nsAccessibleRelationWrap::
  nsAccessibleRelationWrap(PRUint32 aType, nsIAccessible *aTarget) :
  nsAccessibleRelation(aType, aTarget)
{
}

// ISupports

NS_IMPL_ISUPPORTS_INHERITED1(nsAccessibleRelationWrap, nsAccessibleRelation,
                             nsIWinAccessNode)

// nsIWinAccessNode

NS_IMETHODIMP
nsAccessibleRelationWrap::QueryNativeInterface(REFIID aIID, void** aInstancePtr)
{
  return QueryInterface(aIID, aInstancePtr);
}

// IUnknown

STDMETHODIMP
nsAccessibleRelationWrap::QueryInterface(REFIID iid, void** ppv)
{
  *ppv = NULL;

  if (IID_IAccessibleRelation == iid || IID_IUnknown == iid) {
    *ppv = static_cast<IAccessibleRelation*>(this);
    (reinterpret_cast<IUnknown*>(*ppv))->AddRef();
    return S_OK;
  }

  return E_NOINTERFACE;
}

// IAccessibleRelation

STDMETHODIMP
nsAccessibleRelationWrap::get_relationType(BSTR *aRelationType)
{
__try {
  *aRelationType = NULL;

  PRUint32 type = 0;
  nsresult rv = GetRelationType(&type);
  if (NS_FAILED(rv))
    return GetHRESULT(rv);

  switch (type) {
    case RELATION_CONTROLLED_BY:
      *aRelationType = ::SysAllocString(IA2_RELATION_CONTROLLED_BY);
      break;
    case RELATION_CONTROLLER_FOR:
      *aRelationType = ::SysAllocString(IA2_RELATION_CONTROLLER_FOR);
      break;
    case RELATION_DESCRIBED_BY:
      *aRelationType = ::SysAllocString(IA2_RELATION_DESCRIBED_BY);
      break;
    case RELATION_DESCRIPTION_FOR:
      *aRelationType = ::SysAllocString(IA2_RELATION_DESCRIPTION_FOR);
      break;
    case RELATION_EMBEDDED_BY:
      *aRelationType = ::SysAllocString(IA2_RELATION_EMBEDDED_BY);
      break;
    case RELATION_EMBEDS:
      *aRelationType = ::SysAllocString(IA2_RELATION_EMBEDS);
      break;
    case RELATION_FLOWS_FROM:
      *aRelationType = ::SysAllocString(IA2_RELATION_FLOWS_FROM);
      break;
    case RELATION_FLOWS_TO:
      *aRelationType = ::SysAllocString(IA2_RELATION_FLOWS_TO);
      break;
    case RELATION_LABEL_FOR:
      *aRelationType = ::SysAllocString(IA2_RELATION_LABEL_FOR);
      break;
    case RELATION_LABELLED_BY:
      *aRelationType = ::SysAllocString(IA2_RELATION_LABELED_BY);
      break;
    case RELATION_MEMBER_OF:
      *aRelationType = ::SysAllocString(IA2_RELATION_MEMBER_OF);
      break;
    case RELATION_NODE_CHILD_OF:
      *aRelationType = ::SysAllocString(IA2_RELATION_NODE_CHILD_OF);
      break;
    case RELATION_PARENT_WINDOW_OF:
      *aRelationType = ::SysAllocString(IA2_RELATION_PARENT_WINDOW_OF);
      break;
    case RELATION_POPUP_FOR:
      *aRelationType = ::SysAllocString(IA2_RELATION_POPUP_FOR);
      break;
    case RELATION_SUBWINDOW_OF:
      *aRelationType = ::SysAllocString(IA2_RELATION_SUBWINDOW_OF);
      break;
    default:
      return E_FAIL;
  }

  return *aRelationType ? S_OK : E_OUTOFMEMORY;

} __except(nsAccessNodeWrap::FilterA11yExceptions(::GetExceptionCode(), GetExceptionInformation())) { }
  return E_FAIL;
}

STDMETHODIMP
nsAccessibleRelationWrap::get_localizedRelationType(BSTR *aLocalizedRelationType)
{
__try {
  *aLocalizedRelationType = NULL;

} __except(nsAccessNodeWrap::FilterA11yExceptions(::GetExceptionCode(), GetExceptionInformation())) { }
  return E_NOTIMPL;
}

STDMETHODIMP
nsAccessibleRelationWrap::get_nTargets(long *aNTargets)
{
__try {
  *aNTargets = 0;

  PRUint32 count = 0;
  nsresult rv = GetTargetsCount(&count);
  if (NS_FAILED(rv))
    return GetHRESULT(rv);

  *aNTargets = count;
  return S_OK;

} __except(nsAccessNodeWrap::FilterA11yExceptions(::GetExceptionCode(), GetExceptionInformation())) { }
  return E_FAIL;
}

STDMETHODIMP
nsAccessibleRelationWrap::get_target(long aTargetIndex, IUnknown **aTarget)
{
__try {
  nsCOMPtr<nsIAccessible> accessible;
  nsresult rv = GetTarget(aTargetIndex, getter_AddRefs(accessible));
  if (NS_FAILED(rv))
    return GetHRESULT(rv);

  nsCOMPtr<nsIWinAccessNode> winAccessNode(do_QueryInterface(accessible));
  if (!winAccessNode)
    return E_FAIL;

  void *instancePtr = NULL;
  rv = winAccessNode->QueryNativeInterface(IID_IUnknown, &instancePtr);
  if (NS_FAILED(rv))
    return GetHRESULT(rv);

  *aTarget = static_cast<IUnknown*>(instancePtr);
  return S_OK;

} __except(nsAccessNodeWrap::FilterA11yExceptions(::GetExceptionCode(), GetExceptionInformation())) { }
  return E_FAIL;
}

STDMETHODIMP
nsAccessibleRelationWrap::get_targets(long aMaxTargets, IUnknown **aTarget,
                                      long *aNTargets)
{
__try {
  *aNTargets = 0;

  nsCOMPtr<nsIArray> targets;
  nsresult rv = GetTargets(getter_AddRefs(targets));
  if (NS_FAILED(rv))
    return GetHRESULT(rv);

  PRUint32 length = 0;
  rv = targets->GetLength(&length);
  if (NS_FAILED(rv))
    return GetHRESULT(rv);

  if (length == 0)
    return S_FALSE;

  PRUint32 count = length < PRUint32(aMaxTargets) ? length : aMaxTargets;

  PRUint32 index = 0;
  for (; index < count; index++) {
    nsCOMPtr<nsIWinAccessNode> winAccessNode =
      do_QueryElementAt(targets, index, &rv);
    if (NS_FAILED(rv))
      break;

    void *instancePtr = NULL;
    nsresult rv =  winAccessNode->QueryNativeInterface(IID_IUnknown,
                                                       &instancePtr);
    if (NS_FAILED(rv))
      break;

    aTarget[index] = static_cast<IUnknown*>(instancePtr);
  }

  if (NS_FAILED(rv)) {
    for (PRUint32 index2 = 0; index2 < index; index2++) {
      aTarget[index2]->Release();
      aTarget[index2] = NULL;
    }
    return GetHRESULT(rv);
  }

  *aNTargets = count;
  return S_OK;

} __except(nsAccessNodeWrap::FilterA11yExceptions(::GetExceptionCode(), GetExceptionInformation())) { }
  return E_FAIL;
}

