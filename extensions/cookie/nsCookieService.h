/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 * Portions created by the Initial Developer are Copyright (C) 2003
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Daniel Witte (dwitte@stanford.edu)
 *   Michiel van Leeuwen (mvl@exedo.nl)
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

#ifndef nsCookieService_h__
#define nsCookieService_h__

#include "nsICookieService.h"
#include "nsICookieManager.h"
#include "nsICookieManager2.h"
#include "nsIObserver.h"
#include "nsIWebProgressListener.h"
#include "nsWeakReference.h"

#include "nsCookie.h"
#include "nsString.h"
#include "nsVoidArray.h"

struct nsCookieAttributes;
class nsICookieConsent;
class nsICookiePermission;
class nsIPrefBranch;
class nsIObserverService;
class nsIURI;
class nsIPrompt;
class nsIChannel;
class nsITimer;
class nsIFile;
class nsInt64;

/******************************************************************************
 * nsCookieService:
 * class declaration
 ******************************************************************************/

class nsCookieService : public nsICookieService
                      , public nsICookieManager2
                      , public nsIObserver
                      , public nsIWebProgressListener
                      , public nsSupportsWeakReference
{
  public:
    // nsISupports
    NS_DECL_ISUPPORTS
    NS_DECL_NSIOBSERVER
    NS_DECL_NSIWEBPROGRESSLISTENER
    NS_DECL_NSICOOKIESERVICE
    NS_DECL_NSICOOKIEMANAGER
    NS_DECL_NSICOOKIEMANAGER2

    nsCookieService();
    virtual ~nsCookieService();
    static nsCookieService*       GetSingleton();

  protected:
    void                          InitPrefObservers();
    nsresult                      ReadPrefs();
    nsresult                      Read();
    nsresult                      Write();
    PRBool                        SetCookieInternal(nsIURI *aHostURI, nsDependentCString &aCookieHeader, nsInt64 aServerTime, nsCookieStatus aStatus, nsCookiePolicy aPolicy, PRUint32 aListPermission);
    nsresult                      AddInternal(nsCookie *aCookie, nsInt64 aCurrentTime, nsIURI *aHostURI, const char *aCookieHeader);
    static PRBool                 GetTokenValue(nsASingleFragmentCString::const_char_iterator &aIter, nsASingleFragmentCString::const_char_iterator &aEndIter, nsDependentSingleFragmentCSubstring &aTokenString, nsDependentSingleFragmentCSubstring &aTokenValue, PRBool &aEqualsFound);
    static PRBool                 ParseAttributes(nsDependentCString &aCookieHeader, nsCookieAttributes &aCookie);
    static PRBool                 IsIPAddress(const nsAFlatCString &aHost);
    static PRBool                 IsFromMailNews(const nsAFlatCString &aScheme);
    static PRBool                 IsInDomain(const nsACString &aDomain, const nsACString &aHost, PRBool aIsDomain = PR_TRUE);
    static PRBool                 IsForeign(nsIURI *aHostURI, nsIURI *aFirstURI);
    static nsCookiePolicy         GetP3PPolicy(PRInt32 aPolicy);
    PRInt32                       SiteP3PPolicy(nsIURI *aCurrentURI, nsIChannel *aChannel);
    nsCookieStatus                P3PDecision(nsIURI *aHostURI, nsIURI *aFirstURI, nsIChannel *aChannel);
    nsCookieStatus                CheckPrefs(nsIURI *aHostURI, nsIURI *aFirstURI, nsIChannel *aChannel, const char *aCookieHeader, PRUint32 &aListPermission);
    PRBool                        CheckDomain(nsCookieAttributes &aCookie, nsIURI *aHostURI);
    static PRBool                 CheckPath(nsCookieAttributes &aCookie, nsIURI *aHostURI);
    PRBool                        GetExpiry(nsCookieAttributes &aCookie, nsInt64 aServerTime, nsInt64 aCurrentTime, nsCookieStatus aStatus);
    void                          RemoveAllFromMemory();
    void                          RemoveExpiredCookies(nsInt64 aCurrentTime, PRInt32 &aOldestPosition);
    PRBool                        FindCookiesFromHost(nsCookie *aCookie, PRUint32 &aCountFromHost, nsInt64 aCurrentTime);
    PRBool                        FindPosition(nsCookie *aCookie, PRInt32 &aInsertPosition, PRInt32 &aDeletePosition, nsInt64 aCurrentTime);
    void                          UpdateCookieIcon();

    // Use LazyWrite to save the cookies file on a timer. It will write
    // the file only once if repeatedly hammered quickly.
    void                          LazyWrite(PRBool aForce);
    static void                   DoLazyWrite(nsITimer *aTimer, void *aClosure);

  protected:
    // cached members
    nsCOMPtr<nsIPrefBranch>       mPrefBranch;
    nsCOMPtr<nsIFile>             mCookieFile;
    nsCOMPtr<nsIObserverService>  mObserverService;
    nsCOMPtr<nsICookieConsent>    mP3PService;
    nsCOMPtr<nsICookiePermission> mPermissionService;

    // impl members
    nsCOMPtr<nsITimer>            mWriteTimer;
    nsVoidArray                   mCookieList;
    PRUint32                      mLoadCount;
    PRPackedBool                  mWritePending;
    PRPackedBool                  mCookieChanged;
    PRPackedBool                  mCookieIconVisible;

    // cached prefs
#ifdef MOZ_PHOENIX
    // unfortunately, we require this #ifdef for now, since Phoenix uses different
    // (more optimized) prefs to Mozilla.
    // the following variables are Phoenix hacks to reduce ifdefs in the code.
    PRPackedBool                  mCookiesEnabled_temp,               // These two prefs are collapsed
                                  mCookiesForDomainOnly_temp;         // into mCookiesPermissions.
#endif
    PRPackedBool                  mCookiesAskPermission, // Ask user permission before storing cookie
                                  mCookiesLifetimeEnabled,            // Cookie lifetime limit enabled
                                  mCookiesLifetimeCurrentSession,     // Limit cookie lifetime to current session
                                  mCookiesDisabledForMailNews,        // Disable cookies in mailnews
                                  mCookiesStrictDomains; // Optional pref to apply stricter domain checks
    PRUint8                       mCookiesPermissions;   // BEHAVIOR_{ACCEPT, REJECTFOREIGN, REJECT, P3P}
    PRInt32                       mCookiesLifetimeSec;                // Lifetime limit specified in seconds

    /* mCookiesP3PString (below) consists of 8 characters having the following interpretation:
     *   [0]: behavior for first-party cookies when site has no privacy policy
     *   [1]: behavior for third-party cookies when site has no privacy policy
     *   [2]: behavior for first-party cookies when site uses PII with no user consent
     *   [3]: behavior for third-party cookies when site uses PII with no user consent
     *   [4]: behavior for first-party cookies when site uses PII with implicit consent only
     *   [5]: behavior for third-party cookies when site uses PII with implicit consent only
     *   [6]: behavior for first-party cookies when site uses PII with explicit consent
     *   [7]: behavior for third-party cookies when site uses PII with explicit consent
     *
     *   (note: PII = personally identifiable information)
     *
     * each of the eight characters can be one of the following:
     *   'a': accept the cookie
     *   'd': accept the cookie but downgrade it to a session cookie
     *   'r': reject the cookie
     */
    nsXPIDLCString                mCookiesP3PString;

    // private static member, used to cache a ptr to nsCookieService,
    // so we can make nsCookieService a singleton xpcom object.
    static nsCookieService        *gCookieService;
};

#define NS_COOKIEMANAGER_CID {0xaaab6710,0xf2c,0x11d5,{0xa5,0x3b,0x0,0x10,0xa4,0x1,0xeb,0x10}}

#endif // nsCookieService_h__
