/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim:expandtab:shiftwidth=4:tabstop=4:
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

#include "nsApplicationAccessibleWrap.h"

#include "AccessibleApplication_i.c"

#include "nsServiceManagerUtils.h"

nsIXULAppInfo* nsApplicationAccessibleWrap::sAppInfo = nsnull;

// nsISupports
NS_IMPL_ISUPPORTS_INHERITED0(nsApplicationAccessibleWrap,
                             nsApplicationAccessible)

// IUnknown

STDMETHODIMP
nsApplicationAccessibleWrap::QueryInterface(REFIID iid, void** ppv)
{
  *ppv = NULL;

  if (IID_IAccessibleApplication == iid) {
    *ppv = static_cast<IAccessibleApplication*>(this);
    (reinterpret_cast<IUnknown*>(*ppv))->AddRef();
    return S_OK;
  }

  return nsAccessibleWrap::QueryInterface(iid, ppv);
}

// IAccessibleApplication

STDMETHODIMP
nsApplicationAccessibleWrap::get_appName(BSTR *aName)
{
__try {
  *aName = NULL;

  if (!sAppInfo)
    return E_FAIL;

  nsCAutoString cname;
  nsresult rv = sAppInfo->GetName(cname);
  if (NS_FAILED(rv))
    return GetHRESULT(rv);

  if (cname.IsEmpty())
    return S_FALSE;

  NS_ConvertUTF8toUTF16 name(cname);
  *aName = ::SysAllocStringLen(name.get(), name.Length());
  return *aName ? S_OK : E_OUTOFMEMORY;

} __except(FilterA11yExceptions(::GetExceptionCode(), GetExceptionInformation())) { }
  return E_FAIL;
}

STDMETHODIMP
nsApplicationAccessibleWrap::get_appVersion(BSTR *aVersion)
{
__try {
  *aVersion = NULL;

  if (!sAppInfo)
    return E_FAIL;

  nsCAutoString cversion;
  nsresult rv = sAppInfo->GetVersion(cversion);
  if (NS_FAILED(rv))
    return GetHRESULT(rv);

  if (cversion.IsEmpty())
    return S_FALSE;

  NS_ConvertUTF8toUTF16 version(cversion);
  *aVersion = ::SysAllocStringLen(version.get(), version.Length());
  return *aVersion ? S_OK : E_OUTOFMEMORY;

} __except(FilterA11yExceptions(::GetExceptionCode(), GetExceptionInformation())) { }
  return E_FAIL;
}

STDMETHODIMP
nsApplicationAccessibleWrap::get_toolkitName(BSTR *aName)
{
__try {
  *aName = ::SysAllocString(L"Gecko");
  return *aName ? S_OK : E_OUTOFMEMORY;

} __except(FilterA11yExceptions(::GetExceptionCode(), GetExceptionInformation())) { }
  return E_FAIL;
}

STDMETHODIMP
nsApplicationAccessibleWrap::get_toolkitVersion(BSTR *aVersion)
{
__try {
  *aVersion = NULL;

  if (!sAppInfo)
    return E_FAIL;

  nsCAutoString cversion;
  nsresult rv = sAppInfo->GetPlatformVersion(cversion);
  if (NS_FAILED(rv))
    return GetHRESULT(rv);

  if (cversion.IsEmpty())
    return S_FALSE;

  NS_ConvertUTF8toUTF16 version(cversion);
  *aVersion = ::SysAllocStringLen(version.get(), version.Length());
  return *aVersion ? S_OK : E_OUTOFMEMORY;

} __except(FilterA11yExceptions(::GetExceptionCode(), GetExceptionInformation())) { }
  return E_FAIL;
}

// nsApplicationAccessibleWrap

void
nsApplicationAccessibleWrap::PreCreate()
{
  nsresult rv = CallGetService("@mozilla.org/xre/app-info;1", &sAppInfo);
  NS_ASSERTION(NS_SUCCEEDED(rv), "No XUL application info service");
}

void
nsApplicationAccessibleWrap::Unload()
{
  NS_IF_RELEASE(sAppInfo);
}

