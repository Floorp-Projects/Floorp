/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsContentBlocker.h"
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
#include "mozilla/ArrayUtils.h"
#include "nsContentUtils.h"

// Possible behavior pref values
// Those map to the nsIPermissionManager values where possible
#define BEHAVIOR_ACCEPT nsIPermissionManager::ALLOW_ACTION
#define BEHAVIOR_REJECT nsIPermissionManager::DENY_ACTION
#define BEHAVIOR_NOFOREIGN 3

// From nsIContentPolicy
static const char *kTypeString[] = {
                                    "other",
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
                                    "media",
                                    "websocket",
                                    "csp_report",
                                    "xslt",
                                    "beacon",
                                    "fetch",
                                    "image",
                                    "manifest",
                                    "", // TYPE_INTERNAL_SCRIPT
                                    "", // TYPE_INTERNAL_WORKER
                                    "", // TYPE_INTERNAL_SHARED_WORKER
                                    "", // TYPE_INTERNAL_EMBED
                                    "", // TYPE_INTERNAL_OBJECT
                                    "", // TYPE_INTERNAL_FRAME
                                    "", // TYPE_INTERNAL_IFRAME
                                    "", // TYPE_INTERNAL_AUDIO
                                    "", // TYPE_INTERNAL_VIDEO
                                    "", // TYPE_INTERNAL_TRACK
                                    "", // TYPE_INTERNAL_XMLHTTPREQUEST
                                    "", // TYPE_INTERNAL_EVENTSOURCE
                                    "", // TYPE_INTERNAL_SERVICE_WORKER
                                    "", // TYPE_INTERNAL_SCRIPT_PRELOAD
                                    "", // TYPE_INTERNAL_IMAGE
                                    "", // TYPE_INTERNAL_IMAGE_PRELOAD
                                    "", // TYPE_INTERNAL_STYLESHEET
                                    "", // TYPE_INTERNAL_STYLESHEET_PRELOAD
                                    "", // TYPE_INTERNAL_IMAGE_FAVICON
                                    "", // TYPE_INTERNAL_WORKERS_IMPORT_SCRIPTS
};

#define NUMBER_OF_TYPES MOZ_ARRAY_LENGTH(kTypeString)
uint8_t nsContentBlocker::mBehaviorPref[NUMBER_OF_TYPES];

NS_IMPL_ISUPPORTS(nsContentBlocker,
                  nsIContentPolicy,
                  nsIObserver,
                  nsISupportsWeakReference)

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
  int32_t oldPref;
  rv = oldPrefBranch->GetIntPref("network.image.imageBehavior", &oldPref);
  if (NS_SUCCEEDED(rv) && oldPref) {
    int32_t newPref;
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

  rv = mPrefBranchInternal->AddObserver("", this, true);
  PrefChanged(prefBranch, nullptr);

  return rv;
}

#undef  LIMIT
#define LIMIT(x, low, high, default) ((x) >= (low) && (x) <= (high) ? (x) : (default))

void
nsContentBlocker::PrefChanged(nsIPrefBranch *aPrefBranch,
                              const char    *aPref)
{
  int32_t val;

#define PREF_CHANGED(_P) (!aPref || !strcmp(aPref, _P))

  for(uint32_t i = 0; i < NUMBER_OF_TYPES; ++i) {
    if (*kTypeString[i] &&
        PREF_CHANGED(kTypeString[i]) &&
        NS_SUCCEEDED(aPrefBranch->GetIntPref(kTypeString[i], &val)))
      mBehaviorPref[i] = LIMIT(val, 1, 3, 1);
  }

}

// nsIContentPolicy Implementation
NS_IMETHODIMP
nsContentBlocker::ShouldLoad(uint32_t          aContentType,
                             nsIURI           *aContentLocation,
                             nsIURI           *aRequestingLocation,
                             nsISupports      *aRequestingContext,
                             const nsACString &aMimeGuess,
                             nsISupports      *aExtra,
                             nsIPrincipal     *aRequestPrincipal,
                             int16_t          *aDecision)
{
  MOZ_ASSERT(aContentType == nsContentUtils::InternalContentPolicyTypeToExternal(aContentType),
             "We should only see external content policy types here.");

  *aDecision = nsIContentPolicy::ACCEPT;
  nsresult rv;

  // Ony support NUMBER_OF_TYPES content types. that all there is at the
  // moment, but you never know...
  if (aContentType > NUMBER_OF_TYPES)
    return NS_OK;

  // we can't do anything without this
  if (!aContentLocation)
    return NS_OK;

  // The final type of an object tag may mutate before it reaches
  // shouldProcess, so we cannot make any sane blocking decisions here
  if (aContentType == nsIContentPolicy::TYPE_OBJECT)
    return NS_OK;

  // we only want to check http, https, ftp
  // for chrome:// and resources and others, no need to check.
  nsAutoCString scheme;
  aContentLocation->GetScheme(scheme);
  if (!scheme.LowerCaseEqualsLiteral("ftp") &&
      !scheme.LowerCaseEqualsLiteral("http") &&
      !scheme.LowerCaseEqualsLiteral("https"))
    return NS_OK;

  bool shouldLoad, fromPrefs;
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

  return NS_OK;
}

NS_IMETHODIMP
nsContentBlocker::ShouldProcess(uint32_t          aContentType,
                                nsIURI           *aContentLocation,
                                nsIURI           *aRequestingLocation,
                                nsISupports      *aRequestingContext,
                                const nsACString &aMimeGuess,
                                nsISupports      *aExtra,
                                nsIPrincipal     *aRequestPrincipal,
                                int16_t          *aDecision)
{
  MOZ_ASSERT(aContentType == nsContentUtils::InternalContentPolicyTypeToExternal(aContentType),
             "We should only see external content policy types here.");

  // For loads where aRequestingContext is chrome, we should just
  // accept.  Those are most likely toplevel loads in windows, and
  // chrome generally knows what it's doing anyway.
  nsCOMPtr<nsIDocShellTreeItem> item =
    do_QueryInterface(NS_CP_GetDocShellFromContext(aRequestingContext));

  if (item && item->ItemType() == nsIDocShellTreeItem::typeChrome) {
    *aDecision = nsIContentPolicy::ACCEPT;
    return NS_OK;
  }

  // For objects, we only check policy in shouldProcess, as the final type isn't
  // determined until the channel is open -- We don't want to block images in
  // object tags because plugins are disallowed.
  // NOTE that this bypasses the aContentLocation checks in ShouldLoad - this is
  // intentional, as aContentLocation may be null for plugins that load by type
  // (e.g. java)
  if (aContentType == nsIContentPolicy::TYPE_OBJECT) {
    *aDecision = nsIContentPolicy::ACCEPT;

    bool shouldLoad, fromPrefs;
    nsresult rv = TestPermission(aContentLocation, aRequestingLocation,
                                 aContentType, &shouldLoad, &fromPrefs);
    NS_ENSURE_SUCCESS(rv, rv);
    if (!shouldLoad) {
      if (fromPrefs) {
        *aDecision = nsIContentPolicy::REJECT_TYPE;
      } else {
        *aDecision = nsIContentPolicy::REJECT_SERVER;
      }
    }
    return NS_OK;
  }

  // This isn't a load from chrome or an object tag - Just do a ShouldLoad()
  // check -- we want the same answer here
  return ShouldLoad(aContentType, aContentLocation, aRequestingLocation,
                    aRequestingContext, aMimeGuess, aExtra, aRequestPrincipal,
                    aDecision);
}

nsresult
nsContentBlocker::TestPermission(nsIURI *aCurrentURI,
                                 nsIURI *aFirstURI,
                                 int32_t aContentType,
                                 bool *aPermission,
                                 bool *aFromPrefs)
{
  *aFromPrefs = false;
  nsresult rv;

  if (!*kTypeString[aContentType - 1]) {
    // Disallow internal content policy types, they should not be used here.
    *aPermission = false;
    return NS_OK;
  }

  // This default will also get used if there is an unknown value in the
  // permission list, or if the permission manager returns unknown values.
  *aPermission = true;

  // check the permission list first; if we find an entry, it overrides
  // default prefs.
  // Don't forget the aContentType ranges from 1..8, while the
  // array is indexed 0..7
  // All permissions tested by this method are preload permissions, so don't
  // bother actually checking with the permission manager unless we have a
  // preload permission.
  uint32_t permission = nsIPermissionManager::UNKNOWN_ACTION;
  if (mPermissionManager->GetHasPreloadPermissions()) {
    rv = mPermissionManager->TestPermission(aCurrentURI,
                                            kTypeString[aContentType - 1],
                                            &permission);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // If there is nothing on the list, use the default.
  if (!permission) {
    permission = mBehaviorPref[aContentType - 1];
    *aFromPrefs = true;
  }

  // Use the fact that the nsIPermissionManager values map to
  // the BEHAVIOR_* values above.
  switch (permission) {
  case BEHAVIOR_ACCEPT:
    *aPermission = true;
    break;
  case BEHAVIOR_REJECT:
    *aPermission = false;
    break;

  case BEHAVIOR_NOFOREIGN:
    // Third party checking

    // Need a requesting uri for third party checks to work.
    if (!aFirstURI)
      return NS_OK;

    bool trustedSource = false;
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

    nsAutoCString currentHost;
    rv = aCurrentURI->GetAsciiHost(currentHost);
    NS_ENSURE_SUCCESS(rv, rv);

    // Search for two dots, starting at the end.
    // If there are no two dots found, ++dot will turn to zero,
    // that will return the entire string.
    int32_t dot = currentHost.RFindChar('.');
    dot = currentHost.RFindChar('.', dot-1);
    ++dot;

    // Get the domain, ie the last part of the host (www.domain.com -> domain.com)
    // This will break on co.uk
    const nsACString& tail =
      Substring(currentHost, dot, currentHost.Length() - dot);

    nsAutoCString firstHost;
    rv = aFirstURI->GetAsciiHost(firstHost);
    NS_ENSURE_SUCCESS(rv, rv);

    // If the tail is longer then the whole firstHost, it will never match
    if (firstHost.Length() < tail.Length()) {
      *aPermission = false;
      return NS_OK;
    }

    // Get the last part of the firstUri with the same length as |tail|
    const nsACString& firstTail =
      Substring(firstHost, firstHost.Length() - tail.Length(), tail.Length());

    // Check that both tails are the same, and that just before the tail in
    // |firstUri| there is a dot. That means both url are in the same domain
    if ((firstHost.Length() > tail.Length() &&
         firstHost.CharAt(firstHost.Length() - tail.Length() - 1) != '.') ||
        !tail.Equals(firstTail)) {
      *aPermission = false;
    }
    break;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsContentBlocker::Observe(nsISupports     *aSubject,
                          const char      *aTopic,
                          const char16_t *aData)
{
  NS_ASSERTION(!strcmp(NS_PREFBRANCH_PREFCHANGE_TOPIC_ID, aTopic),
               "unexpected topic - we only deal with pref changes!");

  if (mPrefBranchInternal)
    PrefChanged(mPrefBranchInternal, NS_LossyConvertUTF16toASCII(aData).get());
  return NS_OK;
}
