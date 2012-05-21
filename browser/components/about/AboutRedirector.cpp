/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// See also: docshell/base/nsAboutRedirector.cpp

#include "AboutRedirector.h"
#include "nsNetUtil.h"
#include "nsIScriptSecurityManager.h"

namespace mozilla {
namespace browser {

NS_IMPL_ISUPPORTS1(AboutRedirector, nsIAboutModule)

struct RedirEntry {
  const char* id;
  const char* url;
  PRUint32 flags;  // See nsIAboutModule.  The URI_SAFE_FOR_UNTRUSTED_CONTENT
                   // flag does double duty here -- if it's not set, we don't
                   // drop chrome privileges.
};

/*
  Entries which do not have URI_SAFE_FOR_UNTRUSTED_CONTENT will run with chrome
  privileges. This is potentially dangerous. Please use
  URI_SAFE_FOR_UNTRUSTED_CONTENT in the third argument to each map item below
  unless your about: page really needs chrome privileges. Security review is
  required before adding new map entries without
  URI_SAFE_FOR_UNTRUSTED_CONTENT.  Also note, however, that adding
  URI_SAFE_FOR_UNTRUSTED_CONTENT will allow random web sites to link to that
  URI.  Perhaps we should separate the two concepts out...
 */
static RedirEntry kRedirMap[] = {
#ifdef MOZ_SAFE_BROWSING
  { "blocked", "chrome://browser/content/safebrowsing/blockedSite.xhtml",
    nsIAboutModule::URI_SAFE_FOR_UNTRUSTED_CONTENT |
    nsIAboutModule::ALLOW_SCRIPT |
    nsIAboutModule::HIDE_FROM_ABOUTABOUT },
#endif
  { "certerror", "chrome://browser/content/certerror/aboutCertError.xhtml",
    nsIAboutModule::URI_SAFE_FOR_UNTRUSTED_CONTENT |
    nsIAboutModule::ALLOW_SCRIPT |
    nsIAboutModule::HIDE_FROM_ABOUTABOUT },
  { "feeds", "chrome://browser/content/feeds/subscribe.xhtml",
    nsIAboutModule::URI_SAFE_FOR_UNTRUSTED_CONTENT |
    nsIAboutModule::ALLOW_SCRIPT |
    nsIAboutModule::HIDE_FROM_ABOUTABOUT },
  { "privatebrowsing", "chrome://browser/content/aboutPrivateBrowsing.xhtml",
    nsIAboutModule::ALLOW_SCRIPT },
  { "rights",
#ifdef MOZ_OFFICIAL_BRANDING
    "chrome://global/content/aboutRights.xhtml",
#else
    "chrome://global/content/aboutRights-unbranded.xhtml",
#endif
    nsIAboutModule::URI_SAFE_FOR_UNTRUSTED_CONTENT |
    nsIAboutModule::ALLOW_SCRIPT },
  { "robots", "chrome://browser/content/aboutRobots.xhtml",
    nsIAboutModule::URI_SAFE_FOR_UNTRUSTED_CONTENT |
    nsIAboutModule::ALLOW_SCRIPT },
  { "sessionrestore", "chrome://browser/content/aboutSessionRestore.xhtml",
    nsIAboutModule::ALLOW_SCRIPT },
#ifdef MOZ_SERVICES_SYNC
  { "sync-progress", "chrome://browser/content/sync/progress.xhtml",
    nsIAboutModule::ALLOW_SCRIPT },
  { "sync-tabs", "chrome://browser/content/sync/aboutSyncTabs.xul",
    nsIAboutModule::ALLOW_SCRIPT },
#endif
  { "home", "chrome://browser/content/abouthome/aboutHome.xhtml",
    nsIAboutModule::URI_SAFE_FOR_UNTRUSTED_CONTENT |
    nsIAboutModule::ALLOW_SCRIPT },
  { "newtab", "chrome://browser/content/newtab/newTab.xul",
    nsIAboutModule::ALLOW_SCRIPT },
  { "permissions", "chrome://browser/content/preferences/aboutPermissions.xul",
    nsIAboutModule::ALLOW_SCRIPT },
  { "preferences", "chrome://browser/content/preferences/in-content/preferences.xul",
    nsIAboutModule::ALLOW_SCRIPT },
};
static const int kRedirTotal = NS_ARRAY_LENGTH(kRedirMap);

static nsCAutoString
GetAboutModuleName(nsIURI *aURI)
{
  nsCAutoString path;
  aURI->GetPath(path);

  PRInt32 f = path.FindChar('#');
  if (f >= 0)
    path.SetLength(f);

  f = path.FindChar('?');
  if (f >= 0)
    path.SetLength(f);

  ToLowerCase(path);
  return path;
}

NS_IMETHODIMP
AboutRedirector::NewChannel(nsIURI *aURI, nsIChannel **result) 
{
  NS_ENSURE_ARG_POINTER(aURI);
  NS_ASSERTION(result, "must not be null");

  nsCAutoString path = GetAboutModuleName(aURI);

  nsresult rv;
  nsCOMPtr<nsIIOService> ioService = do_GetIOService(&rv);
  NS_ENSURE_SUCCESS(rv, rv);

  for (int i = 0; i < kRedirTotal; i++) {
    if (!strcmp(path.get(), kRedirMap[i].id)) {
      nsCOMPtr<nsIChannel> tempChannel;
      rv = ioService->NewChannel(nsDependentCString(kRedirMap[i].url),
                                 nsnull, nsnull, getter_AddRefs(tempChannel));
      NS_ENSURE_SUCCESS(rv, rv);

      tempChannel->SetOriginalURI(aURI);

      // Keep the page from getting unnecessary privileges unless it needs them
      if (kRedirMap[i].flags & nsIAboutModule::URI_SAFE_FOR_UNTRUSTED_CONTENT) {
        nsCOMPtr<nsIScriptSecurityManager> securityManager =
          do_GetService(NS_SCRIPTSECURITYMANAGER_CONTRACTID, &rv);
        NS_ENSURE_SUCCESS(rv, rv);

        nsCOMPtr<nsIPrincipal> principal;
        rv = securityManager->GetCodebasePrincipal(aURI, getter_AddRefs(principal));
        NS_ENSURE_SUCCESS(rv, rv);

        rv = tempChannel->SetOwner(principal);
        NS_ENSURE_SUCCESS(rv, rv);
      }

      NS_ADDREF(*result = tempChannel);
      return rv;
    }
  }

  return NS_ERROR_ILLEGAL_VALUE;
}

NS_IMETHODIMP
AboutRedirector::GetURIFlags(nsIURI *aURI, PRUint32 *result)
{
  NS_ENSURE_ARG_POINTER(aURI);

  nsCAutoString name = GetAboutModuleName(aURI);

  for (int i = 0; i < kRedirTotal; i++) {
    if (name.Equals(kRedirMap[i].id)) {
      *result = kRedirMap[i].flags;
      return NS_OK;
    }
  }

  return NS_ERROR_ILLEGAL_VALUE;
}

nsresult
AboutRedirector::Create(nsISupports *aOuter, REFNSIID aIID, void **result)
{
  AboutRedirector* about = new AboutRedirector();
  if (about == nsnull)
    return NS_ERROR_OUT_OF_MEMORY;
  NS_ADDREF(about);
  nsresult rv = about->QueryInterface(aIID, result);
  NS_RELEASE(about);
  return rv;
}

} // namespace browser
} // namespace mozilla
