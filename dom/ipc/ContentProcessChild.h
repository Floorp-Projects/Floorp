/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set sw=4 ts=8 et tw=80 : */
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
 * The Original Code is Mozilla Content App.
 *
 * The Initial Developer of the Original Code is
 *   The Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Frederic Plourde <frederic.plourde@collabora.co.uk>
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

#ifndef mozilla_dom_ContentProcessChild_h
#define mozilla_dom_ContentProcessChild_h

#include "mozilla/dom/PContentProcessChild.h"

#include "nsIObserverService.h"
#include "nsTObserverArray.h"
#include "nsIObserver.h"
#include "nsIPrefService.h"
#include "nsIPrefBranch.h"
#include "nsServiceManagerUtils.h"
#include "nsTArray.h"
#include "nsAutoPtr.h"
#include "nsWeakReference.h"

struct ChromePackage;
struct ResourceMapping;
struct OverrideMapping;

namespace mozilla {
namespace dom {

class ContentProcessChild : public PContentProcessChild
{
public:
    ContentProcessChild();
    virtual ~ContentProcessChild();

    class nsPrefObserverStorage {
    public:
        nsPrefObserverStorage(nsIObserver *aObserver, nsCString aDomain,
                              nsCString aPrefRoot, bool aHoldWeak) {
            mDomain = aDomain;
            mPrefRoot = aPrefRoot;
            mObserver = aObserver;
            if (aHoldWeak) {
                nsCOMPtr<nsISupportsWeakReference> weakRefFactory = 
                    do_QueryInterface(aObserver);
                if (weakRefFactory)
                    mWeakRef = do_GetWeakReference(aObserver);
            } else {
                mWeakRef = nsnull;
            }
        }

        ~nsPrefObserverStorage() {
        }

        bool NotifyObserver() {
            nsCOMPtr<nsIObserver> observer;
            if (mWeakRef) {
                observer = do_QueryReferent(mWeakRef);
                if (!observer) {
                    // this weak referenced observer went away, tell
                    // the caller so he can remove the observer from the list
                    return false;
                }
            } else {
                observer = mObserver;
            }

            nsCOMPtr<nsIPrefBranch> prefBranch;
            nsCOMPtr<nsIPrefService> prefService = 
                                      do_GetService(NS_PREFSERVICE_CONTRACTID);
            if (prefService) {
                prefService->GetBranch(mPrefRoot.get(), 
                                       getter_AddRefs(prefBranch));
                observer->Observe(prefBranch, "nsPref:changed",
                                  NS_ConvertASCIItoUTF16(mDomain).get());
            }
            return true;
        }

        nsIObserver* GetObserver() { return mObserver; }
        const nsCString& GetDomain() { return mDomain; }
        const nsCString& GetPrefRoot() { return mPrefRoot; }

    private:
        nsCOMPtr<nsIObserver> mObserver;
        nsWeakPtr mWeakRef;
        nsCString mPrefRoot;
        nsCString mDomain;
    };

    bool Init(MessageLoop* aIOLoop,
              base::ProcessHandle aParentHandle,
              IPC::Channel* aChannel);

    static ContentProcessChild* GetSingleton() {
        NS_ASSERTION(sSingleton, "not initialized");
        return sSingleton;
    }

    /* if you remove this, please talk to cjones or dougt */
    virtual bool RecvDummy(Shmem& foo) { return true; }

    virtual PIFrameEmbeddingChild* AllocPIFrameEmbedding();
    virtual bool DeallocPIFrameEmbedding(PIFrameEmbeddingChild*);

    virtual PTestShellChild* AllocPTestShell();
    virtual bool DeallocPTestShell(PTestShellChild*);
    virtual bool RecvPTestShellConstructor(PTestShellChild*);

    virtual PNeckoChild* AllocPNecko();
    virtual bool DeallocPNecko(PNeckoChild*);

    virtual bool RecvRegisterChrome(const nsTArray<ChromePackage>& packages,
                                    const nsTArray<ResourceMapping>& resources,
                                    const nsTArray<OverrideMapping>& overrides);

    virtual bool RecvSetOffline(const PRBool& offline);

    nsresult AddRemotePrefObserver(const nsCString &aDomain, 
                                   const nsCString &aPrefRoot, 
                                   nsIObserver *aObserver, PRBool aHoldWeak);
    nsresult RemoveRemotePrefObserver(const nsCString &aDomain, 
                                      const nsCString &aPrefRoot, 
                                      nsIObserver *aObserver);
    inline void ClearPrefObservers() {
        mPrefObserverArray.Clear();
    }

    virtual bool RecvNotifyRemotePrefObserver(
            const nsCString& aDomain);
    


private:
    NS_OVERRIDE
    virtual void ActorDestroy(ActorDestroyReason why);

    static ContentProcessChild* sSingleton;

    nsTArray< nsAutoPtr<nsPrefObserverStorage> > mPrefObserverArray;

    DISALLOW_EVIL_CONSTRUCTORS(ContentProcessChild);
};

} // namespace dom
} // namespace mozilla

#endif
