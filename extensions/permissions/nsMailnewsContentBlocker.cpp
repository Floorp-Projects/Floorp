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
 * The Original Code is content blocker code.
 *
 * The Initial Developer of the Original Code is
 * Michiel van Leeuwen <mvl@exedo.nl>.
 * Portions created by the Initial Developer are Copyright (C) 2004
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

#include "nsMailnewsContentBlocker.h"
#include "nsIURI.h"
#include "nsIServiceManager.h"
#include "nsIScriptGlobalObject.h"
#include "nsIDOMWindow.h"
#include "nsIDocShellTreeItem.h"
#include "nsIPrefService.h"
#include "nsIPrefBranch.h"
#include "nsIPrefBranchInternal.h"
#include "nsIDocShell.h"
#include "nsContentPolicyUtils.h"
#include "nsString.h"
#include "nsCRT.h"

/*
 * Block all remote stuff from loading in mailnews. Not only images, but
 * also documents, javascripts, iframes etc
 * remote is http, https, ftp.
 */

static const char kBlockRemotePrefName[] = "permissions.mailnews.block_remote";
static const PRBool kBlockRemotePrefDefault = PR_FALSE;


NS_IMPL_ISUPPORTS3(nsMailnewsContentBlocker, 
                   nsIContentPolicy,
                   nsIObserver,
                   nsSupportsWeakReference)

nsMailnewsContentBlocker::nsMailnewsContentBlocker()
  : mBlockRemotePref(kBlockRemotePrefDefault)
{
}

nsresult
nsMailnewsContentBlocker::Init()
{
  nsCOMPtr<nsIPrefBranchInternal> prefBranch = do_GetService(NS_PREFSERVICE_CONTRACTID);
  if (prefBranch) {
    prefBranch->AddObserver(kBlockRemotePrefName, this, PR_TRUE);

    PrefChanged(prefBranch, nsnull);
  }

  return NS_OK;
}

void
nsMailnewsContentBlocker::PrefChanged(nsIPrefBranch *aPrefBranch,
                                      const char    *aPref)
{
  PRInt32 val;

#define PREF_CHANGED(_P) (!aPref || !strcmp(aPref, _P))

  if (PREF_CHANGED(kBlockRemotePrefName) &&
      NS_SUCCEEDED(aPrefBranch->GetBoolPref(kBlockRemotePrefName, &val)))
    mBlockRemotePref = val;
}

/**
 * Helper function to get the root DocShell given a context
 *
 * @param aContext The context (can be null)
 * @return the root DocShell containing aContext, if found
 */
static inline already_AddRefed<nsIDocShell>
GetRootDocShell(nsISupports *context)
{
  nsIDocShell *docshell = NS_CP_GetDocShellFromContext(context);
  if (!docshell)
    return nsnull;

  nsresult rv;
  nsCOMPtr<nsIDocShellTreeItem> docshellTreeItem(do_QueryInterface(docshell, &rv));
  if (NS_FAILED(rv))
    return nsnull;

  nsCOMPtr<nsIDocShellTreeItem> rootItem;
  // we want the app docshell, so don't use GetSameTypeRootTreeItem
  rv = docshellTreeItem->GetRootTreeItem(getter_AddRefs(rootItem));
  if (NS_FAILED(rv))
    return nsnull;

  nsIDocShell *result;
  CallQueryInterface(rootItem, &result);
  return result;
}

// nsIContentPolicy Implementation
NS_IMETHODIMP
nsMailnewsContentBlocker::ShouldLoad(PRUint32          aContentType,
                                     nsIURI           *aContentLocation,
                                     nsIURI           *aRequestingLocation,
                                     nsISupports      *aRequestingContext,
                                     const nsACString &aMimeGuess,
                                     nsISupports      *aExtra,
                                     PRInt16          *aDecision)
{
  *aDecision = nsIContentPolicy::ACCEPT;
  nsresult rv;

  // we can't do anything without this
  if (!aContentLocation)
    return NS_OK;

  // Go find out if we are dealing with mailnews. Anything else
  // isn't our concern.
  nsCOMPtr<nsIDocShell> docshell = GetRootDocShell(aRequestingContext);
  if (!docshell)
    return NS_OK;

  PRUint32 appType;
  rv = docshell->GetAppType(&appType);
  // We only want to deal with mailnews
  if (NS_FAILED(rv) || appType != nsIDocShell::APP_TYPE_MAIL)
    return NS_OK;

  PRBool isFtp;
  rv = aContentLocation->SchemeIs("ftp", &isFtp);
  NS_ENSURE_SUCCESS(rv,rv);
  if (isFtp) {
    // never allow ftp for mail messages, 
    // because we don't want to send the users email address
    // as the anonymous password
    *aDecision = nsIContentPolicy::REJECT_REQUEST;
    return NS_OK;
  }


  // we only want to check http, https
  // for chrome:// and resources and others, no need to check.
  nsCAutoString scheme;
  aContentLocation->GetScheme(scheme);
  if (!scheme.LowerCaseEqualsLiteral("http") &&
      !scheme.LowerCaseEqualsLiteral("https"))
    return NS_OK;

  // This is mailnews, a protocol we want, now lets do the real work!

  *aDecision = mBlockRemotePref ? nsIContentPolicy::REJECT_REQUEST :
                                  nsIContentPolicy::ACCEPT;

  return NS_OK;
}

NS_IMETHODIMP
nsMailnewsContentBlocker::ShouldProcess(PRUint32          aContentType,
                                        nsIURI           *aContentLocation,
                                        nsIURI           *aRequestingLocation,
                                        nsISupports      *aRequestingContext,
                                        const nsACString &aMimeGuess,
                                        nsISupports      *aExtra,
                                        PRInt16          *aDecision)
{
  *aDecision = nsIContentPolicy::ACCEPT;
  return NS_OK;
}

NS_IMETHODIMP
nsMailnewsContentBlocker::Observe(nsISupports     *aSubject,
                                  const char      *aTopic,
                                  const PRUnichar *aData)
{
  nsCOMPtr<nsIPrefBranch> prefBranch = do_QueryInterface(aSubject);
  NS_ASSERTION(!strcmp(NS_PREFBRANCH_PREFCHANGE_TOPIC_ID, aTopic),
               "unexpected topic - we only deal with pref changes!");

  if (prefBranch)
    PrefChanged(prefBranch, NS_LossyConvertUTF16toASCII(aData).get());
  return NS_OK;
}
