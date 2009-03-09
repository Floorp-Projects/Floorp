/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim:set ts=4 sw=4 sts=4 et cindent: */
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
 * The Original Code is The about:feeds Page.
 *
 * The Initial Developer of the Original Code is Google Inc.
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Ben Goodger <beng@google.com>
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

#include "nsAboutFeeds.h"
#include "nsNetUtil.h"
#include "nsIScriptSecurityManager.h"

NS_IMPL_ISUPPORTS1(nsAboutFeeds, nsIAboutModule)

#define FEEDS_PAGE_URI "chrome://browser/content/feeds/subscribe.xhtml"

NS_IMETHODIMP
nsAboutFeeds::NewChannel(nsIURI* uri, nsIChannel** result)
{

  nsresult rv;
  nsCOMPtr<nsIIOService> ios(do_GetIOService(&rv));
  if (NS_FAILED(rv))
    return rv;

  nsCOMPtr<nsIChannel> channel;
  rv = ios->NewChannel(NS_LITERAL_CSTRING(FEEDS_PAGE_URI),
                       nsnull, nsnull, getter_AddRefs(channel));
  if (NS_FAILED(rv))
    return rv;

  channel->SetOriginalURI(uri);

  nsCOMPtr<nsIScriptSecurityManager> ssm =
    do_GetService(NS_SCRIPTSECURITYMANAGER_CONTRACTID, &rv);
  if (NS_FAILED(rv))
    return rv;

  nsCOMPtr<nsIPrincipal> principal;
  rv = ssm->GetCodebasePrincipal(uri, getter_AddRefs(principal));
  if (NS_FAILED(rv))
    return rv;

  rv = channel->SetOwner(principal);
  if (NS_FAILED(rv))
    return rv;

  NS_ADDREF(*result = channel);
  return NS_OK;
}

NS_IMETHODIMP
nsAboutFeeds::GetURIFlags(nsIURI* uri, PRUint32* uriFlags)
{
  // Feeds page needs script, and is untrusted-content-safe
  *uriFlags = URI_SAFE_FOR_UNTRUSTED_CONTENT | ALLOW_SCRIPT;
  return NS_OK;
}

NS_METHOD
nsAboutFeeds::Create(nsISupports* outer, REFNSIID iid, void** result)
{
  nsAboutFeeds* aboutFeeds = new nsAboutFeeds();
  if (aboutFeeds == nsnull)
    return NS_ERROR_OUT_OF_MEMORY;
  NS_ADDREF(aboutFeeds);
  nsresult rv = aboutFeeds->QueryInterface(iid, result);
  NS_RELEASE(aboutFeeds);
  return rv;
}
