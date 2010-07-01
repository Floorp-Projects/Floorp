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

#include "ContentProcessChild.h"
#include "TabChild.h"

#include "mozilla/ipc/TestShellChild.h"
#include "mozilla/net/NeckoChild.h"
#include "mozilla/ipc/XPCShellEnvironment.h"
#include "mozilla/jsipc/PContextWrapperChild.h"

#include "nsXULAppAPI.h"

#include "base/message_loop.h"
#include "base/task.h"

#include "nsChromeRegistryContent.h"
#include "mozilla/chrome/RegistryMessageUtils.h"

using namespace mozilla::ipc;
using namespace mozilla::net;

namespace mozilla {
namespace dom {

ContentProcessChild* ContentProcessChild::sSingleton;

ContentProcessChild::ContentProcessChild()
{
}

ContentProcessChild::~ContentProcessChild()
{
}

bool
ContentProcessChild::Init(MessageLoop* aIOLoop,
                          base::ProcessHandle aParentHandle,
                          IPC::Channel* aChannel)
{
    NS_ASSERTION(!sSingleton, "only one ContentProcessChild per child");
  
    Open(aChannel, aParentHandle, aIOLoop);
    sSingleton = this;

    return true;
}

PIFrameEmbeddingChild*
ContentProcessChild::AllocPIFrameEmbedding()
{
  nsRefPtr<TabChild> iframe = new TabChild();
  return NS_SUCCEEDED(iframe->Init()) ? iframe.forget().get() : NULL;
}

bool
ContentProcessChild::DeallocPIFrameEmbedding(PIFrameEmbeddingChild* iframe)
{
    TabChild* child = static_cast<TabChild*>(iframe);
    NS_RELEASE(child);
    return true;
}

PTestShellChild*
ContentProcessChild::AllocPTestShell()
{
    return new TestShellChild();
}

bool
ContentProcessChild::DeallocPTestShell(PTestShellChild* shell)
{
    delete shell;
    return true;
}

bool
ContentProcessChild::RecvPTestShellConstructor(PTestShellChild* actor)
{
    actor->SendPContextWrapperConstructor()->SendPObjectWrapperConstructor(true);
    return true;
}

PNeckoChild* 
ContentProcessChild::AllocPNecko()
{
    return new NeckoChild();
}

bool 
ContentProcessChild::DeallocPNecko(PNeckoChild* necko)
{
    delete necko;
    return true;
}

bool
ContentProcessChild::RecvRegisterChrome(const nsTArray<ChromePackage>& packages,
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
ContentProcessChild::RecvSetOffline(const PRBool& offline)
{
  nsCOMPtr<nsIIOService> io (do_GetIOService());
  NS_ASSERTION(io, "IO Service can not be null");

  io->SetOffline(offline);
    
  return true;
}

void
ContentProcessChild::ActorDestroy(ActorDestroyReason why)
{
    if (AbnormalShutdown == why)
        NS_WARNING("shutting down because of crash!");

    ClearPrefObservers();
    XRE_ShutdownChildProcess();
}

nsresult
ContentProcessChild::AddRemotePrefObserver(const nsCString &aDomain, 
                                           const nsCString &aPrefRoot, 
                                           nsIObserver *aObserver, 
                                           PRBool aHoldWeak)
{
    nsPrefObserverStorage* newObserver = 
        new nsPrefObserverStorage(aObserver, aDomain, aPrefRoot, aHoldWeak);

    mPrefObserverArray.AppendElement(newObserver);
    return NS_OK;
}

nsresult
ContentProcessChild::RemoveRemotePrefObserver(const nsCString &aDomain, 
                                              const nsCString &aPrefRoot, 
                                              nsIObserver *aObserver)
{
    if (mPrefObserverArray.IsEmpty())
        return NS_OK;

    nsPrefObserverStorage *entry;
    for (PRUint32 i = 0; i < mPrefObserverArray.Length(); ++i) {
        entry = mPrefObserverArray[i];
        if (entry && entry->GetObserver() == aObserver &&
                     entry->GetDomain().Equals(aDomain)) {
            // Remove this observer from our array
            mPrefObserverArray.RemoveElementAt(i);
            return NS_OK;
        }
    }
    NS_WARNING("No preference Observer was matched !");
    return NS_ERROR_UNEXPECTED;
}

bool
ContentProcessChild::RecvNotifyRemotePrefObserver(const nsCString& aDomain)
{
    nsPrefObserverStorage *entry;
    for (PRUint32 i = 0; i < mPrefObserverArray.Length(); ) {
        entry = mPrefObserverArray[i];
        nsCAutoString prefName(entry->GetPrefRoot() + entry->GetDomain());
        // aDomain here is returned from our global pref observer from parent
        // so it's really a full pref name (i.e. root + domain)
        if (StringBeginsWith(aDomain, prefName)) {
            if (!entry->NotifyObserver()) {
                // remove the observer from the list
                mPrefObserverArray.RemoveElementAt(i);
                continue;
            }
        }
        ++i;
    }
    return true;
}

} // namespace dom
} // namespace mozilla
