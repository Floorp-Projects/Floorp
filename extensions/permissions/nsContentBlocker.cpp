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
#include "nsContentBlocker.h"
#include "nsIDocument.h"
#include "nsIContent.h"
#include "nsIURI.h"
#include "nsIServiceManager.h"
#include "nsIDocShellTreeItem.h"
#include "nsIPrefService.h"
#include "nsIPrefBranch.h"
#include "nsIDocShell.h"
#include "nsString.h"
#include "nsContentPolicyUtils.h"
#include "nsIObjectLoadingContent.h"

// Possible behavior pref values
// Those map to the nsIPermissionManager values where possible
#define BEHAVIOR_ACCEPT nsIPermissionManager::ALLOW_ACTION
#define BEHAVIOR_REJECT nsIPermissionManager::DENY_ACTION
#define BEHAVIOR_NOFOREIGN 3

// From nsIContentPolicy
static const char *kTypeString[] = {"other",
                                    "script",
                                    "image",
                                    "stylesheet",
                                    "object",
                                    "document",
                                    "subdocument",
                                    "refresh",
                                    "xbl",
                                    "ping",
                                    "xmlhttprequest",
                                    "objectsubrequest",
                                    "dtd",
                                    "font",
                                    "media"};

#define NUMBER_OF_TYPES NS_ARRAY_LENGTH(kTypeString)
PRUint8 nsContentBlocker::mBehaviorPref[NUMBER_OF_TYPES];

NS_IMPL_ISUPPORTS3(nsContentBlocker, 
                   nsIContentPolicy,
                   nsIObserver,
                   nsSupportsWeakReference)

nsContentBlocker::nsContentBlocker()
{
  memset(mBehaviorPref, BEHAVIOR_ACCEPT, NUMBER_OF_TYPES);
}

nsresult
nsContentBlocker::Init()
{
  nsresult rv;
  mPermissionManager = do_GetService(NS_PERMISSIONMANAGER_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIPrefService> prefService = do_GetService(NS_PREFSERVICE_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIPrefBranch> prefBranch;
  rv = prefService->GetBranch("permissions.default.", getter_AddRefs(prefBranch));
  NS_ENSURE_SUCCESS(rv, rv);

  // Migrate old image blocker pref
  nsCOMPtr<nsIPrefBranch> oldPrefBranch;
  oldPrefBranch = do_QueryInterface(prefService);
  PRInt32 oldPref;
  rv = oldPrefBranch->GetIntPref("network.image.imageBehavior", &oldPref);
  if (NS_SUCCEEDED(rv) && oldPref) {
    PRInt32 newPref;
    switch (oldPref) {
      default:
        newPref = BEHAVIOR_ACCEPT;
        break;
      case 1:
        newPref = BEHAVIOR_NOFOREIGN;
        break;
      case 2:
        newPref = BEHAVIOR_REJECT;
        break;
    }
    prefBranch->SetIntPref("image", newPref);
    oldPrefBranch->ClearUserPref("network.image.imageBehavior");
  }


  // The branch is not a copy of the prefservice, but a new object, because
  // it is a non-default branch. Adding obeservers to it will only work if
  // we make sure that the object doesn't die. So, keep a reference to it.
  mPrefBranchInternal = do_QueryInterface(prefBranch, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = mPrefBranchInternal->AddObserver("", this, PR_TRUE);
  PrefChanged(prefBranch, nsnull);

  return rv;
}

#undef  LIMIT
#define LIMIT(x, low, high, default) ((x) >= (low) && (x) <= (high) ? (x) : (default))

void
nsContentBlocker::PrefChanged(nsIPrefBranch *aPrefBranch,
                              const char    *aPref)
{
  PRInt32 val;

#define PREF_CHANGED(_P) (!aPref || !strcmp(aPref, _P))

  for(PRUint32 i = 0; i < NUMBER_OF_TYPES; ++i) {
    if (PREF_CHANGED(kTypeString[i]) &&
        NS_SUCCEEDED(aPrefBranch->GetIntPref(kTypeString[i], &val)))
      mBehaviorPref[i] = LIMIT(val, 1, 3, 1);
  }

}

// nsIContentPolicy Implementation
NS_IMETHODIMP 
nsContentBlocker::ShouldLoad(PRUint32          aContentType,
                             nsIURI           *aContentLocation,
                             nsIURI           *aRequestingLocation,
                             nsISupports      *aRequestingContext,
                             const nsACString &aMimeGuess,
                             nsISupports      *aExtra,
                             PRInt16          *aDecision)
{
  *aDecision = nsIContentPolicy::ACCEPT;
  nsresult rv;

  // Ony support NUMBER_OF_TYPES content types. that all there is at the
  // moment, but you never know...
  if (aContentType > NUMBER_OF_TYPES)
    return NS_OK;
  
  // we can't do anything without this
  if (!aContentLocation)
    return NS_OK;

  // we only want to check http, https, ftp
  // for chrome:// and resources and others, no need to check.
  nsCAutoString scheme;
  aContentLocation->GetScheme(scheme);
  if (!scheme.LowerCaseEqualsLiteral("ftp") &&
      !scheme.LowerCaseEqualsLiteral("http") &&
      !scheme.LowerCaseEqualsLiteral("https"))
    return NS_OK;

  PRBool shouldLoad, fromPrefs;
  rv = TestPermission(aContentLocation, aRequestingLocation, aContentType,
                      &shouldLoad, &fromPrefs);
  NS_ENSURE_SUCCESS(rv, rv);
  if (!shouldLoad) {
    if (fromPrefs) {
      *aDecision = nsIContentPolicy::REJECT_TYPE;
    } else {
      *aDecision = nsIContentPolicy::REJECT_SERVER;
    }
  }
  if (aContentType != nsIContentPolicy::TYPE_OBJECT || aMimeGuess.IsEmpty())
    return NS_OK;

  // For TYPE_OBJECT we should check what aMimeGuess might tell us
  // about what sort of object it is.
  nsCOMPtr<nsIObjectLoadingContent> objectLoader =
    do_QueryInterface(aRequestingContext);
  if (!objectLoader)
    return NS_OK;

  PRUint32 contentType;
  rv = objectLoader->GetContentTypeForMIMEType(aMimeGuess, &contentType);
  if (NS_FAILED(rv))
    return rv;
    
  switch (contentType) {
  case nsIObjectLoadingContent::TYPE_IMAGE:
    aContentType = nsIContentPolicy::TYPE_IMAGE;
    break;
  case nsIObjectLoadingContent::TYPE_DOCUMENT:
    aContentType = nsIContentPolicy::TYPE_SUBDOCUMENT;
    break;
  default:
    return NS_OK;
  }

  NS_ASSERTION(aContentType != nsIContentPolicy::TYPE_OBJECT,
	       "Shouldn't happen.  Infinite loops are bad!");

  // Found a type that tells us more about what we're loading.  Try
  // the permissions check again!
  return ShouldLoad(aContentType, aContentLocation, aRequestingLocation,
		    aRequestingContext, aMimeGuess, aExtra, aDecision);
}

NS_IMETHODIMP
nsContentBlocker::ShouldProcess(PRUint32          aContentType,
                                nsIURI           *aContentLocation,
                                nsIURI           *aRequestingLocation,
                                nsISupports      *aRequestingContext,
                                const nsACString &aMimeGuess,
                                nsISupports      *aExtra,
                                PRInt16          *aDecision)
{
  // For loads where aRequestingContext is chrome, we should just
  // accept.  Those are most likely toplevel loads in windows, and
  // chrome generally knows what it's doing anyway.
  nsCOMPtr<nsIDocShellTreeItem> item =
    do_QueryInterface(NS_CP_GetDocShellFromContext(aRequestingContext));

  if (item) {
    PRInt32 type;
    item->GetItemType(&type);
    if (type == nsIDocShellTreeItem::typeChrome) {
      *aDecision = nsIContentPolicy::ACCEPT;
      return NS_OK;
    }
  }

  // This isn't a load from chrome.  Just do a ShouldLoad() check --
  // we want the same answer here
  return ShouldLoad(aContentType, aContentLocation, aRequestingLocation,
                    aRequestingContext, aMimeGuess, aExtra, aDecision);
}

nsresult
nsContentBlocker::TestPermission(nsIURI *aCurrentURI,
                                 nsIURI *aFirstURI,
                                 PRInt32 aContentType,
                                 PRBool *aPermission,
                                 PRBool *aFromPrefs)
{
  *aFromPrefs = PR_FALSE;
  // This default will also get used if there is an unknown value in the
  // permission list, or if the permission manager returns unknown values.
  *aPermission = PR_TRUE;

  // check the permission list first; if we find an entry, it overrides
  // default prefs.
  // Don't forget the aContentType ranges from 1..8, while the
  // array is indexed 0..7
  PRUint32 permission;
  nsresult rv = mPermissionManager->TestPermission(aCurrentURI, 
                                                   kTypeString[aContentType - 1],
                                                   &permission);
  NS_ENSURE_SUCCESS(rv, rv);

  // If there is nothing on the list, use the default.
  if (!permission) {
    permission = mBehaviorPref[aContentType - 1];
    *aFromPrefs = PR_TRUE;
  }

  // Use the fact that the nsIPermissionManager values map to 
  // the BEHAVIOR_* values above.
  switch (permission) {
  case BEHAVIOR_ACCEPT:
    *aPermission = PR_TRUE;
    break;
  case BEHAVIOR_REJECT:
    *aPermission = PR_FALSE;
    break;

  case BEHAVIOR_NOFOREIGN:
    // Third party checking

    // Need a requesting uri for third party checks to work.
    if (!aFirstURI)
      return NS_OK;

    PRBool trustedSource = PR_FALSE;
    rv = aFirstURI->SchemeIs("chrome", &trustedSource);
    NS_ENSURE_SUCCESS(rv,rv);
    if (!trustedSource) {
      rv = aFirstURI->SchemeIs("resource", &trustedSource);
      NS_ENSURE_SUCCESS(rv,rv);
    }
    if (trustedSource)
      return NS_OK;

    // compare tails of names checking to see if they have a common domain
    // we do this by comparing the tails of both names where each tail 
    // includes at least one dot
    
    // A more generic method somewhere would be nice

    nsCAutoString currentHost;
    rv = aCurrentURI->GetAsciiHost(currentHost);
    NS_ENSURE_SUCCESS(rv, rv);

    // Search for two dots, starting at the end.
    // If there are no two dots found, ++dot will turn to zero,
    // that will return the entire string.
    PRInt32 dot = currentHost.RFindChar('.');
    dot = currentHost.RFindChar('.', dot-1);
    ++dot;

    // Get the domain, ie the last part of the host (www.domain.com -> domain.com)
    // This will break on co.uk
    const nsCSubstring &tail =
      Substring(currentHost, dot, currentHost.Length() - dot);

    nsCAutoString firstHost;
    rv = aFirstURI->GetAsciiHost(firstHost);
    NS_ENSURE_SUCCESS(rv, rv);

    // If the tail is longer then the whole firstHost, it will never match
    if (firstHost.Length() < tail.Length()) {
      *aPermission = PR_FALSE;
      return NS_OK;
    }
    
    // Get the last part of the firstUri with the same length as |tail|
    const nsCSubstring &firstTail = 
      Substring(firstHost, firstHost.Length() - tail.Length(), tail.Length());

    // Check that both tails are the same, and that just before the tail in
    // |firstUri| there is a dot. That means both url are in the same domain
    if ((firstHost.Length() > tail.Length() && 
         firstHost.CharAt(firstHost.Length() - tail.Length() - 1) != '.') || 
        !tail.Equals(firstTail)) {
      *aPermission = PR_FALSE;
    }
    break;
  }
  
  return NS_OK;
}

NS_IMETHODIMP
nsContentBlocker::Observe(nsISupports     *aSubject,
                          const char      *aTopic,
                          const PRUnichar *aData)
{
  NS_ASSERTION(!strcmp(NS_PREFBRANCH_PREFCHANGE_TOPIC_ID, aTopic),
               "unexpected topic - we only deal with pref changes!");

  if (mPrefBranchInternal)
    PrefChanged(mPrefBranchInternal, NS_LossyConvertUTF16toASCII(aData).get());
  return NS_OK;
}
