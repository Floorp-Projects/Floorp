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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
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

#include "nsImgManager.h"
#include "nsIDocument.h"
#include "nsIContent.h"
#include "nsINodeInfo.h"
#include "nsIURI.h"
#include "nsIServiceManager.h"
#include "nsIScriptGlobalObject.h"
#include "nsIDOMWindow.h"
#include "nsIDOMDocument.h"
#include "nsIDocShellTreeItem.h"
#include "nsIPrefService.h"
#include "nsIPrefBranch.h"
#include "nsIPrefBranchInternal.h"
#include "nsIDocShell.h"
#include "nsString.h"

// Possible behavior pref values
#define IMAGE_ACCEPT 0
#define IMAGE_NOFOREIGN 1
#define IMAGE_DENY 2

static const char kImageBehaviorPrefName[] = "network.image.imageBehavior";
static const char kImageWarningPrefName[] = "network.image.warnAboutImages";
static const char kImageBlockImageInMailNewsPrefName[] = "mailnews.message_display.disable_remote_image";

static const PRUint8      kImageBehaviorPrefDefault = IMAGE_ACCEPT;
static const PRPackedBool kImageWarningPrefDefault = PR_FALSE;
static const PRPackedBool kImageBlockImageInMailNewsPrefDefault = PR_FALSE;

static inline already_AddRefed<nsIDocShell>
GetRootDocShell(nsIDOMWindow *aWindow)
{
  nsIDocShell *rootShell = nsnull;

  nsCOMPtr<nsIScriptGlobalObject> globalObj(do_QueryInterface(aWindow));
  if (globalObj) {
    nsCOMPtr<nsIDocShellTreeItem> docShellTreeItem =
      do_QueryInterface(globalObj->GetDocShell());

    if (docShellTreeItem) {
      nsCOMPtr<nsIDocShellTreeItem> rootItem;
      docShellTreeItem->GetRootTreeItem(getter_AddRefs(rootItem));

      CallQueryInterface(rootItem, &rootShell);
    }
  }

  return rootShell;
}

////////////////////////////////////////////////////////////////////////////////
// nsImgManager Implementation
////////////////////////////////////////////////////////////////////////////////

NS_IMPL_ISUPPORTS4(nsImgManager, 
                   nsIImgManager, 
                   nsIContentPolicy,
                   nsIObserver,
                   nsSupportsWeakReference)

nsImgManager::nsImgManager()
  : mBehaviorPref(kImageBehaviorPrefDefault)
  , mWarningPref(kImageWarningPrefDefault)
  , mBlockInMailNewsPref(kImageBlockImageInMailNewsPrefDefault)
{
}

nsImgManager::~nsImgManager()
{
}

nsresult nsImgManager::Init()
{
  // On error, just don't use the host based lookup anymore. We can do the
  // other things, like mailnews blocking
  mPermissionManager = do_GetService(NS_PERMISSIONMANAGER_CONTRACTID);

  nsCOMPtr<nsIPrefBranchInternal> prefBranch = do_GetService(NS_PREFSERVICE_CONTRACTID);
  if (prefBranch) {
    prefBranch->AddObserver(kImageBehaviorPrefName, this, PR_TRUE);

    // We don't do anything with it yet, but let it be. (bug 110112, 146513)
    prefBranch->AddObserver(kImageWarningPrefName, this, PR_TRUE);

    prefBranch->AddObserver(kImageBlockImageInMailNewsPrefName, this, PR_TRUE);

    PrefChanged(prefBranch, nsnull);
  }

  return NS_OK;
}

void
nsImgManager::PrefChanged(nsIPrefBranch *aPrefBranch,
                          const char    *aPref)
{
  PRBool val;

#define PREF_CHANGED(_P) (!aPref || !strcmp(aPref, _P))

  if (PREF_CHANGED(kImageBehaviorPrefName) &&
      NS_SUCCEEDED(aPrefBranch->GetIntPref(kImageBehaviorPrefName, &val)) &&
      val >= 0 && val <= 2)
    mBehaviorPref = val;

  if (PREF_CHANGED(kImageWarningPrefName) &&
      NS_SUCCEEDED(aPrefBranch->GetBoolPref(kImageWarningPrefName, &val)))
    mWarningPref = val;

  if (PREF_CHANGED(kImageBlockImageInMailNewsPrefName) &&
      NS_SUCCEEDED(aPrefBranch->GetBoolPref(kImageBlockImageInMailNewsPrefName, &val)))
    mBlockInMailNewsPref = val;
}

// nsIContentPolicy Implementation
NS_IMETHODIMP nsImgManager::ShouldLoad(PRInt32 aContentType, 
                                       nsIURI *aContentLoc,
                                       nsISupports *aContext,
                                       nsIDOMWindow *aWindow,
                                       PRBool *aShouldLoad)
{
  *aShouldLoad = PR_TRUE;
  nsresult rv = NS_OK;

  // we can't do anything w/ out this
  if (!aContentLoc)
    return rv;

  if (aContentType == nsIContentPolicy::IMAGE) {
    // we only want to check http, https, ftp
    PRBool isFtp;
    rv = aContentLoc->SchemeIs("ftp", &isFtp);
    NS_ENSURE_SUCCESS(rv,rv);

    PRBool needToCheck = isFtp;
    if (!needToCheck) {
      rv = aContentLoc->SchemeIs("http", &needToCheck);
      NS_ENSURE_SUCCESS(rv,rv);

      if (!needToCheck) {
        rv = aContentLoc->SchemeIs("https", &needToCheck);
        NS_ENSURE_SUCCESS(rv,rv);
      }
    }

    // for chrome:// and resources and others, no need to check.
    if (!needToCheck)
      return NS_OK;

    nsCOMPtr<nsIDocument> doc;
    nsCOMPtr<nsIContent> content = do_QueryInterface(aContext);
    if (content) {
      // XXXbz GetOwnerDocument
      doc = content->GetDocument();
      if (!doc) {
        nsINodeInfo *nodeinfo = content->GetNodeInfo();
        if (nodeinfo) {
          doc = nodeinfo->GetDocument();
        }
      }
    }

    if (!doc && aWindow) {
      nsCOMPtr<nsIDOMDocument> domDoc;
      aWindow->GetDocument(getter_AddRefs(domDoc));
      doc = do_QueryInterface(domDoc);
    }
    
    if (!doc) {
      // XXX what to do if there is really no document?
      return NS_OK;
    }
    
    nsIURI *baseURI = doc->GetBaseURI();
    if (!baseURI)
      return rv;

    nsCOMPtr<nsIDocShell> docshell = GetRootDocShell(aWindow);
    if (docshell) {
      PRUint32 appType;
      rv = docshell->GetAppType(&appType);
      if (NS_SUCCEEDED(rv) && appType == nsIDocShell::APP_TYPE_MAIL) {
        // never allow ftp for mail messages, 
        // because we don't want to send the users email address
        // as the anonymous password
        if (mBlockInMailNewsPref || isFtp) {
          *aShouldLoad = PR_FALSE;
          return NS_OK;
        }
      }
    }
      
    rv =  TestPermission(aContentLoc, baseURI, aShouldLoad);
    if (NS_FAILED(rv))
      return rv;
  }
  return NS_OK;
}

NS_IMETHODIMP nsImgManager::ShouldProcess(PRInt32 aContentType,
                                          nsIURI *aDocumentLoc,
                                          nsISupports *aContext,
                                          nsIDOMWindow *aWindow,
                                          PRBool *_retval)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsImgManager::TestPermission(nsIURI *aCurrentURI,
                             nsIURI *aFirstURI,
                             PRBool *aPermission)
{
  nsresult rv;
  *aPermission = PR_TRUE;

  // check the permission list first; if we find an entry, it overrides
  // default prefs. this is new behavior, see bug 184059.
  if (mPermissionManager) {
    PRUint32 temp;
    mPermissionManager->TestPermission(aCurrentURI, "image", &temp);

    // if we found an entry, use it
    if (temp != nsIPermissionManager::UNKNOWN_ACTION) {
      *aPermission = (temp != nsIPermissionManager::DENY_ACTION);
      return NS_OK;
    }
  }

  if (mBehaviorPref == IMAGE_DENY) {
    *aPermission = PR_FALSE;
    return NS_OK;
  }

  // Third party checking
  if (mBehaviorPref == IMAGE_NOFOREIGN) {
    // compare tails of names checking to see if they have a common domain
    // we do this by comparing the tails of both names where each tail 
    // includes at least one dot
    
    // A more generic method somewhere would be nice

    nsCAutoString currentHost;
    rv = aCurrentURI->GetAsciiHost(currentHost);
    if (NS_FAILED(rv)) return rv;

    // Search for two dots, starting at the end.
    // If there are no two dots found, ++dot will turn to zero,
    // that will return the entire string.
    PRInt32 dot = currentHost.RFindChar('.');
    dot = currentHost.RFindChar('.', dot-1);
    ++dot;

    // Get the domain, ie the last part of the host (www.domain.com -> domain.com)
    // This will break on co.uk
    const nsACString &tail = Substring(currentHost, dot, currentHost.Length() - dot);

    nsCAutoString firstHost;
    rv = aFirstURI->GetAsciiHost(firstHost);
    if (NS_FAILED(rv)) return rv;

    // If the tail is longer then the whole firstHost, it will never match
    if (firstHost.Length() < tail.Length()) {
      *aPermission = PR_FALSE;
      return NS_OK;
    }
    
    // Get the last part of the firstUri with the same length as |tail|
    const nsACString &firstTail = Substring(firstHost, firstHost.Length() - tail.Length(), tail.Length());

    // Check that both tails are the same, and that just before the tail in
    // |firstUri| there is a dot. That means both url are in the same domain
    if ((firstHost.Length() > tail.Length() && 
         firstHost.CharAt(firstHost.Length() - tail.Length() - 1) != '.') || 
        !tail.Equals(firstTail)) {
      *aPermission = PR_FALSE;
    }
  }
  
  return NS_OK;
}

NS_IMETHODIMP
nsImgManager::Observe(nsISupports     *aSubject,
                      const char      *aTopic,
                      const PRUnichar *aData)
{
  nsCOMPtr<nsIPrefBranch> prefBranch = do_QueryInterface(aSubject);
  NS_ASSERTION(!nsCRT::strcmp(NS_PREFBRANCH_PREFCHANGE_TOPIC_ID, aTopic),
               "unexpected topic - we only deal with pref changes!");

  if (prefBranch)
    PrefChanged(prefBranch, NS_LossyConvertUTF16toASCII(aData).get());
  return NS_OK;
}
