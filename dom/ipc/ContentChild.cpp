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

#include "ContentChild.h"
#include "TabChild.h"

#include "mozilla/ipc/TestShellChild.h"
#include "mozilla/net/NeckoChild.h"
#include "mozilla/ipc/XPCShellEnvironment.h"
#include "mozilla/jsipc/PContextWrapperChild.h"

#include "nsIObserverService.h"
#include "nsTObserverArray.h"
#include "nsIObserver.h"
#include "nsIPrefService.h"
#include "nsIPrefBranch.h"
#include "nsServiceManagerUtils.h"
#include "nsXULAppAPI.h"
#include "nsWeakReference.h"

#include "base/message_loop.h"
#include "base/task.h"

#include "nsChromeRegistryContent.h"
#include "mozilla/chrome/RegistryMessageUtils.h"

using namespace mozilla::ipc;
using namespace mozilla::net;

namespace mozilla {
namespace dom {

class PrefObserver
{
public:
    /**
     * Pass |aHoldWeak=true| to force this to hold a weak ref to
     * |aObserver|.  Otherwise, this holds a strong ref.
     *
     * XXX/cjones: what do domain and prefRoot mean?
     */
    PrefObserver(nsIObserver *aObserver, bool aHoldWeak,
                 const nsCString& aPrefRoot, const nsCString& aDomain)
        : mPrefRoot(aPrefRoot)
        , mDomain(aDomain)
    {
        if (aHoldWeak) {
            nsCOMPtr<nsISupportsWeakReference> supportsWeakRef = 
                do_QueryInterface(aObserver);
            if (supportsWeakRef)
                mWeakObserver = do_GetWeakReference(aObserver);
        } else {
            mObserver = aObserver;
        }
    }

    ~PrefObserver() {}

    /**
     * Return true if this observer can no longer receive
     * notifications.
     */
    bool IsDead() const
    {
        nsCOMPtr<nsIObserver> observer = GetObserver();
        return !!observer;
    }

    /**
     * Return true iff a request to remove observers matching
     * <aObserver, aDomain, aPrefRoot> entails removal of this.
     */
    bool ShouldRemoveFrom(nsIObserver* aObserver,
                          const nsCString& aPrefRoot,
                          const nsCString& aDomain) const
    {
        nsCOMPtr<nsIObserver> observer = GetObserver();
        return (observer == aObserver &&
                mDomain == aDomain && mPrefRoot == aPrefRoot);
    }

    /**
     * Return true iff this should be notified of changes to |aPref|.
     */
    bool Observes(const nsCString& aPref) const
    {
        nsCAutoString myPref(mPrefRoot);
        myPref += mDomain;
        return StringBeginsWith(aPref, myPref);
    }

    /**
     * Notify this of a pref change that's relevant to our interests
     * (see Observes() above).  Return false iff this no longer cares
     * to observe any more pref changes.
     */
    bool Notify() const
    {
        nsCOMPtr<nsIObserver> observer = GetObserver();
        if (!observer) {
            return false;
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

private:
    already_AddRefed<nsIObserver> GetObserver() const
    {
        nsCOMPtr<nsIObserver> observer =
            mObserver ? mObserver : do_QueryReferent(mWeakObserver);
        return observer.forget();
    }

    // We only either hold a strong or a weak reference to the
    // observer, so only either mObserver or
    // GetReferent(mWeakObserver) is ever non-null.
    nsCOMPtr<nsIObserver> mObserver;
    nsWeakPtr mWeakObserver;
    nsCString mPrefRoot;
    nsCString mDomain;

    // disable these
    PrefObserver(const PrefObserver&);
    PrefObserver& operator=(const PrefObserver&);
};


ContentChild* ContentChild::sSingleton;

ContentChild::ContentChild()
    : mDead(false)
{
}

ContentChild::~ContentChild()
{
}

bool
ContentChild::Init(MessageLoop* aIOLoop,
                   base::ProcessHandle aParentHandle,
                   IPC::Channel* aChannel)
{
    NS_ASSERTION(!sSingleton, "only one ContentChild per child");
  
    Open(aChannel, aParentHandle, aIOLoop);
    sSingleton = this;

    return true;
}

PBrowserChild*
ContentChild::AllocPBrowser(const PRUint32& aChromeFlags)
{
  nsRefPtr<TabChild> iframe = new TabChild(aChromeFlags);
  return NS_SUCCEEDED(iframe->Init()) ? iframe.forget().get() : NULL;
}

bool
ContentChild::DeallocPBrowser(PBrowserChild* iframe)
{
    TabChild* child = static_cast<TabChild*>(iframe);
    NS_RELEASE(child);
    return true;
}

PTestShellChild*
ContentChild::AllocPTestShell()
{
    return new TestShellChild();
}

bool
ContentChild::DeallocPTestShell(PTestShellChild* shell)
{
    delete shell;
    return true;
}

bool
ContentChild::RecvPTestShellConstructor(PTestShellChild* actor)
{
    actor->SendPContextWrapperConstructor()->SendPObjectWrapperConstructor(true);
    return true;
}

PNeckoChild* 
ContentChild::AllocPNecko()
{
    return new NeckoChild();
}

bool 
ContentChild::DeallocPNecko(PNeckoChild* necko)
{
    delete necko;
    return true;
}

bool
ContentChild::RecvRegisterChrome(const nsTArray<ChromePackage>& packages,
                                 const nsTArray<ResourceMapping>& resources,
                                 const nsTArray<OverrideMapping>& overrides)
{
    nsCOMPtr<nsIChromeRegistry> registrySvc = nsChromeRegistry::GetService();
    nsChromeRegistryContent* chromeRegistry =
        static_cast<nsChromeRegistryContent*>(registrySvc.get());
    chromeRegistry->RegisterRemoteChrome(packages, resources, overrides);
    return true;
}

bool
ContentChild::RecvSetOffline(const PRBool& offline)
{
  nsCOMPtr<nsIIOService> io (do_GetIOService());
  NS_ASSERTION(io, "IO Service can not be null");

  io->SetOffline(offline);
    
  return true;
}

void
ContentChild::ActorDestroy(ActorDestroyReason why)
{
    if (AbnormalShutdown == why)
        NS_WARNING("shutting down because of crash!");

    // We might be holding the last ref to some of the observers in
    // mPrefObserverArray.  Some of them try to unregister themselves
    // in their dtors (sketchy).  To side-step uaf problems and so
    // forth, we set this mDead flag.  Then, if during a Clear() a
    // being-deleted observer tries to unregister itself, it hits the
    // |if (mDead)| special case below and we're safe.
    mDead = true;
    mPrefObservers.Clear();

    XRE_ShutdownChildProcess();
}

nsresult
ContentChild::AddRemotePrefObserver(const nsCString& aDomain, 
                                    const nsCString& aPrefRoot, 
                                    nsIObserver* aObserver, 
                                    PRBool aHoldWeak)
{
    if (aObserver) {
        mPrefObservers.AppendElement(
            new PrefObserver(aObserver, aHoldWeak, aPrefRoot, aDomain));
    }
    return NS_OK;
}

nsresult
ContentChild::RemoveRemotePrefObserver(const nsCString& aDomain, 
                                       const nsCString& aPrefRoot, 
                                       nsIObserver* aObserver)
{
    if (mDead) {
        // Silently ignore, we're about to exit.  See comment in
        // ActorDestroy().
        return NS_OK;
    }

    for (PRUint32 i = 0; i < mPrefObservers.Length();
         /*we mutate the array during the loop; ++i iff no mutation*/) {
        PrefObserver* observer = mPrefObservers[i];
        if (observer->IsDead()) {
            mPrefObservers.RemoveElementAt(i);
            continue;
        } else if (observer->ShouldRemoveFrom(aObserver, aPrefRoot, aDomain)) {
            mPrefObservers.RemoveElementAt(i);
            return NS_OK;
        }
        ++i;
    }

    NS_WARNING("RemoveRemotePrefObserver(): no observer was matched!");
    return NS_ERROR_UNEXPECTED;
}

bool
ContentChild::RecvNotifyRemotePrefObserver(const nsCString& aPref)
{
    for (PRUint32 i = 0; i < mPrefObservers.Length();
         /*we mutate the array during the loop; ++i iff no mutation*/) {
        PrefObserver* observer = mPrefObservers[i];
        if (observer->Observes(aPref) &&
            !observer->Notify()) {
            // |observer| had a weak ref that went away, so it no
            // longer cares about pref changes
            mPrefObservers.RemoveElementAt(i);
            continue;
        }
        ++i;
    }
    return true;
}

} // namespace dom
} // namespace mozilla
