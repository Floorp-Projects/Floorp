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

#ifndef mozilla_dom_ContentChild_h
#define mozilla_dom_ContentChild_h

#include "mozilla/dom/PContentChild.h"

#include "nsTArray.h"

struct ChromePackage;
class nsIObserver;
struct ResourceMapping;
struct OverrideMapping;

namespace mozilla {
namespace dom {

class AlertObserver;
class PrefObserver;

class ContentChild : public PContentChild
{
public:
    ContentChild();
    virtual ~ContentChild();

    bool Init(MessageLoop* aIOLoop,
              base::ProcessHandle aParentHandle,
              IPC::Channel* aChannel);

    static ContentChild* GetSingleton() {
        NS_ASSERTION(sSingleton, "not initialized");
        return sSingleton;
    }

    /* if you remove this, please talk to cjones or dougt */
    virtual bool RecvDummy(Shmem& foo) { return true; }

    virtual PBrowserChild* AllocPBrowser(const PRUint32& aChromeFlags);
    virtual bool DeallocPBrowser(PBrowserChild*);

    virtual PTestShellChild* AllocPTestShell();
    virtual bool DeallocPTestShell(PTestShellChild*);
    virtual bool RecvPTestShellConstructor(PTestShellChild*);

    virtual PNeckoChild* AllocPNecko();
    virtual bool DeallocPNecko(PNeckoChild*);

    virtual PExternalHelperAppChild *AllocPExternalHelperApp(
            const IPC::URI& uri,
            const nsCString& aMimeContentType,
            const nsCString& aContentDisposition,
            const bool& aForceSave,
            const PRInt64& aContentLength);
    virtual bool DeallocPExternalHelperApp(PExternalHelperAppChild *aService);

    virtual bool RecvRegisterChrome(const nsTArray<ChromePackage>& packages,
                                    const nsTArray<ResourceMapping>& resources,
                                    const nsTArray<OverrideMapping>& overrides);

    virtual bool RecvSetOffline(const PRBool& offline);

    virtual bool RecvNotifyVisited(const IPC::URI& aURI);
    // auto remove when alertfinished is received.
    nsresult AddRemoteAlertObserver(const nsString& aData, nsIObserver* aObserver);

    virtual bool RecvPreferenceUpdate(const nsCString& aDomain);
    
    virtual bool RecvNotifyAlertsObserver(const nsCString& aType, const nsString& aData);

    virtual bool RecvAsyncMessage(const nsString& aMsg, const nsString& aJSON);

    virtual bool RecvGeolocationUpdate(const GeoPosition& somewhere);

private:
    NS_OVERRIDE
    virtual void ActorDestroy(ActorDestroyReason why);

    NS_OVERRIDE
    virtual void ProcessingError(Result what);

    /**
     * Exit *now*.  Do not shut down XPCOM, do not pass Go, do not run
     * static destructors, do not collect $200.
     */
    NS_NORETURN void QuickExit();

    nsTArray<nsAutoPtr<AlertObserver> > mAlertObservers;
    nsTArray<nsAutoPtr<PrefObserver> > mPrefObservers;
    bool mDead;

    static ContentChild* sSingleton;

    DISALLOW_EVIL_CONSTRUCTORS(ContentChild);
};

} // namespace dom
} // namespace mozilla

#endif
