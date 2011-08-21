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

#ifndef mozilla_dom_ContentParent_h
#define mozilla_dom_ContentParent_h

#include "base/waitable_event_watcher.h"

#include "mozilla/dom/PContentParent.h"
#include "mozilla/dom/PMemoryReportRequestParent.h"
#include "mozilla/ipc/GeckoChildProcessHost.h"

#include "nsIObserver.h"
#include "nsIThreadInternal.h"
#include "nsNetUtil.h"
#include "nsIPrefService.h"
#include "nsIPermissionManager.h"
#include "nsIDOMGeoPositionCallback.h"
#include "nsIDeviceMotion.h"
#include "nsIMemoryReporter.h"
#include "nsCOMArray.h"

class nsFrameMessageManager;
namespace mozilla {

namespace ipc {
class TestShellParent;
}

namespace dom {

class TabParent;
class PStorageParent;

class ContentParent : public PContentParent
                    , public nsIObserver
                    , public nsIThreadObserver
                    , public nsIDOMGeoPositionCallback
                    , public nsIDeviceMotionListener
{
private:
    typedef mozilla::ipc::GeckoChildProcessHost GeckoChildProcessHost;
    typedef mozilla::ipc::TestShellParent TestShellParent;

public:
    static ContentParent* GetNewOrUsed();
    static void GetAll(nsTArray<ContentParent*>& aArray);

    NS_DECL_ISUPPORTS
    NS_DECL_NSIOBSERVER
    NS_DECL_NSITHREADOBSERVER
    NS_DECL_NSIDOMGEOPOSITIONCALLBACK
    NS_DECL_NSIDEVICEMOTIONLISTENER

    TabParent* CreateTab(PRUint32 aChromeFlags);

    TestShellParent* CreateTestShell();
    bool DestroyTestShell(TestShellParent* aTestShell);

    void ReportChildAlreadyBlocked();
    bool RequestRunToCompletion();

    bool IsAlive();

    void SetChildMemoryReporters(const InfallibleTArray<MemoryReport>& report);

    bool NeedsPermissionsUpdate() {
        return mSendPermissionUpdates;
    }

protected:
    void OnChannelConnected(int32 pid);
    virtual void ActorDestroy(ActorDestroyReason why);

private:
    static nsTArray<ContentParent*>* gContentParents;

    // Hide the raw constructor methods since we don't want client code
    // using them.
    using PContentParent::SendPBrowserConstructor;
    using PContentParent::SendPTestShellConstructor;

    ContentParent();
    virtual ~ContentParent();

    void Init();

    virtual PBrowserParent* AllocPBrowser(const PRUint32& aChromeFlags);
    virtual bool DeallocPBrowser(PBrowserParent* frame);

    virtual PCrashReporterParent* AllocPCrashReporter();
    virtual bool DeallocPCrashReporter(PCrashReporterParent* crashreporter);

    virtual PMemoryReportRequestParent* AllocPMemoryReportRequest();
    virtual bool DeallocPMemoryReportRequest(PMemoryReportRequestParent* actor);

    virtual PTestShellParent* AllocPTestShell();
    virtual bool DeallocPTestShell(PTestShellParent* shell);

    virtual PAudioParent* AllocPAudio(const PRInt32&,
                                     const PRInt32&,
                                     const PRInt32&);
    virtual bool DeallocPAudio(PAudioParent*);

    virtual PNeckoParent* AllocPNecko();
    virtual bool DeallocPNecko(PNeckoParent* necko);

    virtual PExternalHelperAppParent* AllocPExternalHelperApp(
            const IPC::URI& uri,
            const nsCString& aMimeContentType,
            const nsCString& aContentDisposition,
            const bool& aForceSave,
            const PRInt64& aContentLength,
            const IPC::URI& aReferrer);
    virtual bool DeallocPExternalHelperApp(PExternalHelperAppParent* aService);

    virtual PStorageParent* AllocPStorage(const StorageConstructData& aData);
    virtual bool DeallocPStorage(PStorageParent* aActor);

    virtual bool RecvReadPrefsArray(InfallibleTArray<PrefTuple> *retValue);
    virtual bool RecvReadFontList(InfallibleTArray<FontListEntry>* retValue);

    void EnsurePrefService();

    virtual bool RecvReadPermissions(InfallibleTArray<IPC::Permission>* aPermissions);

    virtual bool RecvGetIndexedDBDirectory(nsString* aDirectory);

    virtual bool RecvSetClipboardText(const nsString& text, const PRInt32& whichClipboard);
    virtual bool RecvGetClipboardText(const PRInt32& whichClipboard, nsString* text);
    virtual bool RecvEmptyClipboard();
    virtual bool RecvClipboardHasText(PRBool* hasText);

    virtual bool RecvGetSystemColors(const PRUint32& colorsCount, InfallibleTArray<PRUint32>* colors);
    virtual bool RecvGetIconForExtension(const nsCString& aFileExt, const PRUint32& aIconSize, InfallibleTArray<PRUint8>* bits);
    virtual bool RecvGetShowPasswordSetting(PRBool* showPassword);

    virtual bool RecvStartVisitedQuery(const IPC::URI& uri);

    virtual bool RecvVisitURI(const IPC::URI& uri,
                              const IPC::URI& referrer,
                              const PRUint32& flags);

    virtual bool RecvSetURITitle(const IPC::URI& uri,
                                 const nsString& title);
    
    virtual bool RecvShowFilePicker(const PRInt16& mode,
                                    const PRInt16& selectedType,
                                    const PRBool& addToRecentDocs,
                                    const nsString& title,
                                    const nsString& defaultFile,
                                    const nsString& defaultExtension,
                                    const InfallibleTArray<nsString>& filters,
                                    const InfallibleTArray<nsString>& filterNames,
                                    InfallibleTArray<nsString>* files,
                                    PRInt16* retValue,
                                    nsresult* result);
 
    virtual bool RecvShowAlertNotification(const nsString& aImageUrl, const nsString& aTitle,
                                           const nsString& aText, const PRBool& aTextClickable,
                                           const nsString& aCookie, const nsString& aName);

    virtual bool RecvLoadURIExternal(const IPC::URI& uri);

    virtual bool RecvSyncMessage(const nsString& aMsg, const nsString& aJSON,
                                 InfallibleTArray<nsString>* aRetvals);
    virtual bool RecvAsyncMessage(const nsString& aMsg, const nsString& aJSON);

    virtual bool RecvAddGeolocationListener();
    virtual bool RecvRemoveGeolocationListener();
    virtual bool RecvAddDeviceMotionListener();
    virtual bool RecvRemoveDeviceMotionListener();

    virtual bool RecvConsoleMessage(const nsString& aMessage);
    virtual bool RecvScriptError(const nsString& aMessage,
                                 const nsString& aSourceName,
                                 const nsString& aSourceLine,
                                 const PRUint32& aLineNumber,
                                 const PRUint32& aColNumber,
                                 const PRUint32& aFlags,
                                 const nsCString& aCategory);

    GeckoChildProcessHost* mSubprocess;

    PRInt32 mGeolocationWatchID;
    int mRunToCompletionDepth;
    bool mShouldCallUnblockChild;
    nsCOMPtr<nsIThreadObserver> mOldObserver;

    // This is a cache of all of the memory reporters
    // registered in the child process.  To update this, one
    // can broadcast the topic "child-memory-reporter-request" using
    // the nsIObserverService.
    nsCOMArray<nsIMemoryReporter> mMemoryReporters;

    bool mIsAlive;
    nsCOMPtr<nsIPrefServiceInternal> mPrefService;
    time_t mProcessStartTime;

    bool mSendPermissionUpdates;

    nsRefPtr<nsFrameMessageManager> mMessageManager;
};

} // namespace dom
} // namespace mozilla

#endif
