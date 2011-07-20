/* -*- Mode: c++; c-basic-offset: 4; indent-tabs-mode: nil; tab-width: 40 -*- */
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
 * The Original Code is worker threads.
 *
 * The Initial Developer of the Original Code is
 *   Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2008
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Ben Turner <bent.mozilla@gmail.com> (Original Author)
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

#include "nsDOMWorkerNavigator.h"

#include "nsIClassInfoImpl.h"
#include "nsStringGlue.h"

#include "nsDOMThreadService.h"
#include "nsDOMWorkerMacros.h"

#define XPC_MAP_CLASSNAME nsDOMWorkerNavigator
#define XPC_MAP_QUOTED_CLASSNAME "Navigator"

#define XPC_MAP_FLAGS                                      \
  nsIXPCScriptable::USE_JSSTUB_FOR_ADDPROPERTY           | \
  nsIXPCScriptable::USE_JSSTUB_FOR_DELPROPERTY           | \
  nsIXPCScriptable::USE_JSSTUB_FOR_SETPROPERTY           | \
  nsIXPCScriptable::DONT_ENUM_QUERY_INTERFACE            | \
  nsIXPCScriptable::CLASSINFO_INTERFACES_ONLY            | \
  nsIXPCScriptable::DONT_REFLECT_INTERFACE_NAMES

#include "xpc_map_end.h"

NS_IMPL_THREADSAFE_ISUPPORTS3(nsDOMWorkerNavigator, nsIWorkerNavigator,
                                                    nsIClassInfo,
                                                    nsIXPCScriptable)

NS_IMPL_CI_INTERFACE_GETTER1(nsDOMWorkerNavigator, nsIWorkerNavigator)

NS_IMPL_THREADSAFE_DOM_CI_GETINTERFACES(nsDOMWorkerNavigator)
NS_IMPL_THREADSAFE_DOM_CI_ALL_THE_REST(nsDOMWorkerNavigator)

NS_IMETHODIMP
nsDOMWorkerNavigator::GetHelperForLanguage(PRUint32 aLanguage,
                                           nsISupports** _retval)
{
  if (aLanguage == nsIProgrammingLanguage::JAVASCRIPT) {
    NS_ADDREF(*_retval = NS_ISUPPORTS_CAST(nsIWorkerNavigator*, this));
  }
  else {
    *_retval = nsnull;
  }
  return NS_OK;
}

NS_IMETHODIMP
nsDOMWorkerNavigator::GetAppName(nsAString& aAppName)
{
  nsDOMThreadService::get()->GetAppName(aAppName);
  return NS_OK;
}

NS_IMETHODIMP
nsDOMWorkerNavigator::GetAppVersion(nsAString& aAppVersion)
{
  nsDOMThreadService::get()->GetAppVersion(aAppVersion);
  return NS_OK;
}

NS_IMETHODIMP
nsDOMWorkerNavigator::GetPlatform(nsAString& aPlatform)
{
  nsDOMThreadService::get()->GetPlatform(aPlatform);
  return NS_OK;
}

NS_IMETHODIMP
nsDOMWorkerNavigator::GetUserAgent(nsAString& aUserAgent)
{
  nsDOMThreadService::get()->GetUserAgent(aUserAgent);
  return NS_OK;
}
