/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsImgManager.h"
#include "nsIDocument.h"
#include "nsIContent.h"
#include "nsINodeInfo.h"
#include "nsCOMPtr.h"
#include "nsXPIDLString.h"
#include "nsIURI.h"
#include "nsIServiceManager.h"
#include "nsIScriptGlobalObject.h"
#include "nsIDOMWindow.h"
#include "nsIDocShellTreeItem.h"
#include "nsCRT.h"
#include "nsIPrefBranchInternal.h"
#include "nsIObserverService.h"
#include "nsString.h"

// Possible behavior pref values
#define IMAGE_ACCEPT 0
#define IMAGE_NOFOREIGN 1
#define IMAGE_DENY 2

static const char kImageBehaviorPrefName[] = "network.image.imageBehavior";
static const char kImageWarningPrefName[] = "network.image.warnAboutImages";
static const char kImageBlockerPrefName[] = "imageblocker.enabled";
static const char kImageBlockImageInMailNewsPrefName[] = "mailnews.message_display.disable_remote_image";

static const PRInt32 kImageBehaviorPrefDefault = IMAGE_ACCEPT;
static const PRBool kImageWarningPrefDefault = PR_FALSE;
static const PRBool kImageBlockerPrefDefault = PR_FALSE;
static const PRBool kImageBlockImageInMailNewsPrefDefault = PR_FALSE;

////////////////////////////////////////////////////////////////////////////////
// nsImgManager Implementation
////////////////////////////////////////////////////////////////////////////////

NS_IMPL_ISUPPORTS4(nsImgManager, 
                   nsIImgManager, 
                   nsIContentPolicy,
                   nsIObserver,
                   nsSupportsWeakReference);

nsImgManager::nsImgManager()
{
}

nsImgManager::~nsImgManager()
{
}

nsresult nsImgManager::Init()
{
  nsresult rv;

  // On error, just don't use the host based lookup anymore. We can do the
  // other things, like mailnews blocking
  mPermissionManager = do_GetService(NS_PERMISSIONMANAGER_CONTRACTID);

  mPrefBranch = do_GetService(NS_PREFSERVICE_CONTRACTID, &rv);
 
  if (NS_SUCCEEDED(rv)) {
    nsCOMPtr<nsIPrefBranchInternal> prefInternal = do_QueryInterface(mPrefBranch, &rv);
    if (NS_SUCCEEDED(rv)) {
      prefInternal->AddObserver(kImageBehaviorPrefName, this, PR_TRUE);

      // We don't do anything with it yet, but let it be. (bug 110112, 146513)
      prefInternal->AddObserver(kImageWarningPrefName, this, PR_TRUE);

      // What is this pref, and how do you set it?
      prefInternal->AddObserver(kImageBlockerPrefName, this, PR_TRUE);
      prefInternal->AddObserver(kImageBlockImageInMailNewsPrefName, this, PR_TRUE);
    }
  }

  rv = ReadPrefs();
  NS_ASSERTION(NS_SUCCEEDED(rv), "Error occured reading image preferences");

  return NS_OK;
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

  // we can't do anything w/ out these.
  if (!aContentLoc || !aContext) return rv;

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

    nsCOMPtr<nsIURI> baseURI;
    nsCOMPtr<nsIDocument> doc;
    nsCOMPtr<nsINodeInfo> nodeinfo;
    nsCOMPtr<nsIContent> content = do_QueryInterface(aContext);
    NS_ASSERTION(content, "no content available");
    if (content) {
      rv = content->GetDocument(getter_AddRefs(doc));
      if (NS_FAILED(rv) || !doc) {
        rv = content->GetNodeInfo(getter_AddRefs(nodeinfo));
        if (NS_FAILED(rv) || !nodeinfo) return rv;

        rv = nodeinfo->GetDocument(getter_AddRefs(doc));
        if (NS_FAILED(rv) || !doc) return rv;
      }

      rv = doc->GetBaseURL(getter_AddRefs(baseURI));
      if (NS_FAILED(rv) || !baseURI) return rv;

      nsCOMPtr<nsIDocShell> docshell;
      rv = GetRootDocShell(aWindow, getter_AddRefs(docshell));
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
      if (NS_FAILED(rv)) return rv;
    }
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

  // return if imageblocker is not enabled
  // TODO: Why? Where is the pref set?
  if (!mBlockerPref) {
    *aPermission = (mBehaviorPref != IMAGE_DENY);
    return NS_OK;
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
      return NS_OK;
    }
  }
  
  if (mPermissionManager) {
    PRUint32 temp;
    mPermissionManager->TestPermission(aCurrentURI, "image", &temp);
    // Blacklist for now
    *aPermission = (temp != nsIPermissionManager::DENY_ACTION);
  } else {
    // no permission manager, return ok
    *aPermission = PR_TRUE;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsImgManager::Observe(nsISupports *aSubject,
                      const char *aTopic,
                      const PRUnichar *aData)
{
  nsresult rv;

  // check the topic, and the cached prefservice
  if (!mPrefBranch) {
    NS_ERROR("No prefbranch");
    return NS_ERROR_FAILURE;
  }
  
  if (!nsCRT::strcmp(NS_PREFBRANCH_PREFCHANGE_TOPIC_ID, aTopic)) {
    // which pref changed?
    NS_LossyConvertUCS2toASCII pref(aData);

    if (pref.Equals(kImageBehaviorPrefName)) {
      rv = mPrefBranch->GetIntPref(kImageBehaviorPrefName, &mBehaviorPref);
      if (NS_FAILED(rv) || mBehaviorPref < 0 || mBehaviorPref > 2) {
        mBehaviorPref = kImageBehaviorPrefDefault;
      }
    } else if (pref.Equals(kImageWarningPrefName)) {
      rv = mPrefBranch->GetIntPref(kImageWarningPrefName, &mWarningPref);
      if (NS_FAILED(rv)) {
        mWarningPref = kImageWarningPrefDefault;
      }
    } else if (pref.Equals(kImageBlockerPrefName)) {
      rv = mPrefBranch->GetIntPref(kImageBlockerPrefName, &mBlockerPref);
      if (NS_FAILED(rv)) {
        mBlockerPref = kImageBlockerPrefDefault;
      }
    } else if (pref.Equals(kImageBlockImageInMailNewsPrefName)) {
      rv = mPrefBranch->GetBoolPref(kImageBlockImageInMailNewsPrefName, &mBlockInMailNewsPref);
      if (NS_FAILED(rv)) {
        mBlockInMailNewsPref = kImageBlockImageInMailNewsPrefDefault;
      }
    }
  }
  
  return NS_OK;
}

nsresult
nsImgManager::ReadPrefs()
{
  nsresult rv, rv2 = NS_OK;

  // check the prefservice is cached
  if (!mPrefBranch) {
    NS_ERROR("No prefbranch");
    return NS_ERROR_FAILURE;
  }

  rv = mPrefBranch->GetIntPref(kImageBehaviorPrefName, &mBehaviorPref);
  if (NS_FAILED(rv) || mBehaviorPref < 0 || mBehaviorPref > 2) {
    rv2 = rv;
    mBehaviorPref = kImageBehaviorPrefDefault;
  }

  rv = mPrefBranch->GetBoolPref(kImageBlockerPrefName, &mBlockerPref);
  if (NS_FAILED(rv)) {
    rv2 = rv;
    mBlockerPref = kImageWarningPrefDefault;
  }

  rv = mPrefBranch->GetBoolPref(kImageWarningPrefName, &mWarningPref);
  if (NS_FAILED(rv)) {
    rv2 = rv;
    mWarningPref = kImageBlockerPrefDefault;
  }

  rv = mPrefBranch->GetBoolPref(kImageBlockImageInMailNewsPrefName, &mBlockInMailNewsPref);
  if (NS_FAILED(rv)) {
    rv2 = rv;
    mBlockInMailNewsPref = kImageBlockImageInMailNewsPrefDefault;
  }

  return rv2;
}

NS_IMETHODIMP
nsImgManager::GetRootDocShell(nsIDOMWindow *aWindow, nsIDocShell **result)
{
  nsresult rv;

  nsCOMPtr<nsIScriptGlobalObject> globalObj(do_QueryInterface(aWindow));
  if (!globalObj)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIDocShell> docshell;
  rv = globalObj->GetDocShell(getter_AddRefs(docshell));
  if (NS_FAILED(rv))
    return rv;

  nsCOMPtr<nsIDocShellTreeItem> docshellTreeItem(do_QueryInterface(docshell, &rv));
  if (NS_FAILED(rv))
    return rv;

  nsCOMPtr<nsIDocShellTreeItem> rootItem;
  rv = docshellTreeItem->GetRootTreeItem(getter_AddRefs(rootItem));
  if (NS_FAILED(rv))
    return rv;

  return CallQueryInterface(rootItem, result);
}
