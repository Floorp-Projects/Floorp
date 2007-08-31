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

#include "CAccessibleAction.h"

#include "AccessibleAction_i.c"

#include "nsIAccessible.h"

#include "nsCOMPtr.h"
#include "nsString.h"
#include "nsIDOMDOMStringList.h"

// IUnknown

STDMETHODIMP
CAccessibleAction::QueryInterface(REFIID iid, void** ppv)
{
  *ppv = NULL;

  if (IID_IAccessibleAction == iid) {
    *ppv = static_cast<IAccessibleAction*>(this);
    (reinterpret_cast<IUnknown*>(*ppv))->AddRef();
    return S_OK;
  }

  return E_NOINTERFACE;
}

// IAccessibleAction

STDMETHODIMP
CAccessibleAction::nActions(long *aNumActions)
{
  nsCOMPtr<nsIAccessible> acc(do_QueryInterface(this));
  if (!acc)
    return E_FAIL;

  PRUint8 count = 0;
  nsresult rv = acc->GetNumActions(&count);
  *aNumActions = count;

  if (NS_SUCCEEDED(rv))
    return NS_OK;
  return E_FAIL;
}

STDMETHODIMP
CAccessibleAction::doAction(long aActionIndex)
{
  nsCOMPtr<nsIAccessible> acc(do_QueryInterface(this));
  if (!acc)
    return E_FAIL;

  PRUint8 index = static_cast<PRUint8>(aActionIndex);
  if (NS_SUCCEEDED(acc->DoAction(index)))
    return S_OK;
  return E_FAIL;
}

STDMETHODIMP
CAccessibleAction::get_description(long aActionIndex, BSTR *aDescription)
{
  *aDescription = NULL;

  nsCOMPtr<nsIAccessible> acc(do_QueryInterface(this));
  if (!acc)
    return E_FAIL;

  nsAutoString description;
  PRUint8 index = static_cast<PRUint8>(aActionIndex);
  if (NS_FAILED(acc->GetActionDescription(index, description)))
    return E_FAIL;

  if (!description.IsVoid()) {
    INT result = ::SysReAllocStringLen(aDescription, description.get(),
                                       description.Length());
    if (!result)
      return E_OUTOFMEMORY;
  }

  return S_OK;
}

STDMETHODIMP
CAccessibleAction::get_keyBinding(long aActionIndex, long aNumMaxBinding,
                                 BSTR **aKeyBinding,
                                 long *aNumBinding)
{
  *aKeyBinding = NULL;
  aNumBinding = 0;

  nsCOMPtr<nsIAccessible> acc(do_QueryInterface(this));
  if (!acc)
    return E_FAIL;

  nsCOMPtr<nsIDOMDOMStringList> keys;
  PRUint8 index = static_cast<PRUint8>(aActionIndex);
  nsresult rv = acc->GetKeyBindings(index, getter_AddRefs(keys));
  if (NS_FAILED(rv))
    return E_FAIL;

  PRUint32 length = 0;
  keys->GetLength(&length);

  PRBool aUseNumMaxBinding = length > static_cast<PRUint32>(aNumMaxBinding);

  PRUint32 maxBinding = static_cast<PRUint32>(aNumMaxBinding);

  PRUint32 numBinding = length > maxBinding ? maxBinding : length;
  *aNumBinding = numBinding;

  *aKeyBinding = new BSTR[numBinding];
  if (!*aKeyBinding)
    return E_OUTOFMEMORY;

  for (PRUint32 i = 0; i < numBinding; i++) {
    nsAutoString key;
    keys->Item(i, key);
    INT result = ::SysReAllocStringLen(aKeyBinding[i], key.get(),
                                       key.Length());
    if (!result)
      return E_OUTOFMEMORY;
  }

  return S_OK;
}

STDMETHODIMP
CAccessibleAction::get_name(long aActionIndex, BSTR *aName)
{
  *aName = NULL;

  nsCOMPtr<nsIAccessible> acc(do_QueryInterface(this));
  if (!acc)
    return E_FAIL;

  nsAutoString name;
  PRUint8 index = static_cast<PRUint8>(aActionIndex);
  if (NS_FAILED(acc->GetActionName(index, name)))
    return E_FAIL;

  if (!name.IsVoid()) {
    INT result = ::SysReAllocStringLen(aName, name.get(), name.Length());
    if (!result)
      return E_OUTOFMEMORY;
  }

  return S_OK;
}

STDMETHODIMP
CAccessibleAction::get_localizedName(long aActionIndex, BSTR *aLocalizedName)
{
  return E_NOTIMPL;
}

