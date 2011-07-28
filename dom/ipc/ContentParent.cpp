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

#include "ContentParent.h"

#include "TabParent.h"
#include "CrashReporterParent.h"
#include "History.h"
#include "mozilla/ipc/TestShellParent.h"
#include "mozilla/net/NeckoParent.h"
#include "nsHashPropertyBag.h"
#include "nsIFilePicker.h"
#include "nsIWindowWatcher.h"
#include "nsIDOMWindow.h"
#include "nsIPrefBranch.h"
#include "nsIPrefBranch2.h"
#include "nsIPrefService.h"
#include "nsIPrefLocalizedString.h"
#include "nsIObserverService.h"
#include "nsContentUtils.h"
#include "nsAutoPtr.h"
#include "nsCOMPtr.h"
#include "nsServiceManagerUtils.h"
#include "nsThreadUtils.h"
#include "nsChromeRegistryChrome.h"
#include "nsExternalHelperAppService.h"
#include "nsCExternalHandlerService.h"
#include "nsFrameMessageManager.h"
#include "nsIPresShell.h"
#include "nsIAlertsService.h"
#include "nsToolkitCompsCID.h"
#include "nsIDOMGeoGeolocation.h"
#include "nsIConsoleService.h"
#include "nsIScriptError.h"
#include "nsConsoleMessage.h"
#include "nsAppDirectoryServiceDefs.h"
#include "IDBFactory.h"
#if defined(MOZ_SYDNEYAUDIO)
#include "AudioParent.h"
#endif

#if defined(ANDROID) || defined(LINUX)
#include <sys/time.h>
#include <sys/resource.h>
#include "nsSystemInfo.h"
#endif

#ifdef MOZ_PERMISSIONS
#include "nsPermissionManager.h"
#endif

#ifdef MOZ_CRASHREPORTER
#include "nsICrashReporter.h"
#include "nsExceptionHandler.h"
#endif

#include "mozilla/dom/ExternalHelperAppParent.h"
#include "mozilla/dom/StorageParent.h"
#include "mozilla/Services.h"
#include "mozilla/unused.h"
#include "nsDeviceMotion.h"

#include "nsIMemoryReporter.h"
#include "nsMemoryReporterManager.h"
#include "mozilla/dom/PMemoryReportRequestParent.h"

#ifdef ANDROID
#include "gfxAndroidPlatform.h"
#include "AndroidBridge.h"
#endif

#include "nsIClipboard.h"
#include "nsWidgetsCID.h"
#include "nsISupportsPrimitives.h"
static NS_DEFINE_CID(kCClipboardCID, NS_CLIPBOARD_CID);
static const char* sClipboardTextFlavors[] = { kUnicodeMime };

using namespace mozilla::ipc;
using namespace mozilla::net;
using namespace mozilla::places;
using mozilla::unused; // heh
using base::KillProcess;

namespace mozilla {
namespace dom {

#define NS_IPC_IOSERVICE_SET_OFFLINE_TOPIC "ipc:network:set-offline"

class MemoryReportRequestParent : public PMemoryReportRequestParent
{
public:
    MemoryReportRequestParent();
    virtual ~MemoryReportRequestParent();

    virtual bool    Recv__delete__(const InfallibleTArray<MemoryReport>& report);
private:
    ContentParent* Owner()
    {
        return static_cast<ContentParent*>(Manager());
    }
};
    

MemoryReportRequestParent::MemoryReportRequestParent()
{
    MOZ_COUNT_CTOR(MemoryReportRequestParent);
}

bool
MemoryReportRequestParent::Recv__delete__(const InfallibleTArray<MemoryReport>& report)
{
    Owner()->SetChildMemoryReporters(report);
    return true;
}

MemoryReportRequestParent::~MemoryReportRequestParent()
{
    MOZ_COUNT_DTOR(MemoryReportRequestParent);
}

ContentParent* ContentParent::gSingleton;

ContentParent*
ContentParent::GetSingleton(PRBool aForceNew)
{
    if (gSingleton && !gSingleton->IsAlive())
        gSingleton = nsnull;
    
    if (!gSingleton && aForceNew) {
        nsRefPtr<ContentParent> parent = new ContentParent();
        gSingleton = parent;
        parent->Init();
    }

    return gSingleton;
}

void
ContentParent::Init()
{
    nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
    if (obs) {
        obs->AddObserver(this, "xpcom-shutdown", PR_FALSE);
        obs->AddObserver(this, NS_IPC_IOSERVICE_SET_OFFLINE_TOPIC, PR_FALSE);
        obs->AddObserver(this, "child-memory-reporter-request", PR_FALSE);
        obs->AddObserver(this, "memory-pressure", PR_FALSE);
#ifdef ACCESSIBILITY
        obs->AddObserver(this, "a11y-init-or-shutdown", PR_FALSE);
#endif
    }
    nsCOMPtr<nsIPrefBranch2> prefs(do_GetService(NS_PREFSERVICE_CONTRACTID));
    if (prefs) {
        prefs->AddObserver("", this, PR_FALSE);
    }
    nsCOMPtr<nsIThreadInternal>
            threadInt(do_QueryInterface(NS_GetCurrentThread()));
    if (threadInt) {
        threadInt->GetObserver(getter_AddRefs(mOldObserver));
        threadInt->SetObserver(this);
    }
    if (obs) {
        obs->NotifyObservers(nsnull, "ipc:content-created", nsnull);
    }

#ifdef ACCESSIBILITY
    // If accessibility is running in chrome process then start it in content
    // process.
    if (nsIPresShell::IsAccessibilityActive()) {
        SendActivateA11y();
    }
#endif
}

void
ContentParent::OnChannelConnected(int32 pid)
{
    ProcessHandle handle;
    if (!base::OpenPrivilegedProcessHandle(pid, &handle)) {
        NS_WARNING("Can't open handle to child process.");
    }
    else {
        SetOtherProcess(handle);

#if defined(ANDROID) || defined(LINUX)
        EnsurePrefService();
        nsCOMPtr<nsIPrefBranch> branch;
        branch = do_QueryInterface(mPrefService);

        // Check nice preference
        PRInt32 nice = 0;
        branch->GetIntPref("dom.ipc.content.nice", &nice);

        // Environment variable overrides preference
        char* relativeNicenessStr = getenv("MOZ_CHILD_PROCESS_RELATIVE_NICENESS");
        if (relativeNicenessStr) {
            nice = atoi(relativeNicenessStr);
        }

        /* make the GUI thread have higher priority on single-cpu devices */
        nsCOMPtr<nsIPropertyBag2> infoService = do_GetService(NS_SYSTEMINFO_CONTRACTID);
        if (infoService) {
            PRInt32 cpus;
            nsresult rv = infoService->GetPropertyAsInt32(NS_LITERAL_STRING("cpucount"), &cpus);
            if (NS_FAILED(rv)) {
                cpus = 1;
            }
            if (nice != 0 && cpus == 1) {
                setpriority(PRIO_PROCESS, pid, getpriority(PRIO_PROCESS, pid) + nice);
            }
        }
#endif
    }
}

namespace {
void
DelayedDeleteSubprocess(GeckoChildProcessHost* aSubprocess)
{
    XRE_GetIOMessageLoop()
        ->PostTask(FROM_HERE,
                   new DeleteTask<GeckoChildProcessHost>(aSubprocess));
}
}

void
ContentParent::ActorDestroy(ActorDestroyReason why)
{
    nsCOMPtr<nsIThreadObserver>
        kungFuDeathGrip(static_cast<nsIThreadObserver*>(this));
    nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
    if (obs) {
        obs->RemoveObserver(static_cast<nsIObserver*>(this), "xpcom-shutdown");
        obs->RemoveObserver(static_cast<nsIObserver*>(this), "memory-pressure");
        obs->RemoveObserver(static_cast<nsIObserver*>(this), "child-memory-reporter-request");
        obs->RemoveObserver(static_cast<nsIObserver*>(this), NS_IPC_IOSERVICE_SET_OFFLINE_TOPIC);
#ifdef ACCESSIBILITY
        obs->RemoveObserver(static_cast<nsIObserver*>(this), "a11y-init-or-shutdown");
#endif
    }

    // clear the child memory reporters
    InfallibleTArray<MemoryReport> empty;
    SetChildMemoryReporters(empty);

    // remove the global remote preferences observers
    nsCOMPtr<nsIPrefBranch2> prefs 
            (do_GetService(NS_PREFSERVICE_CONTRACTID));
    if (prefs) { 
        prefs->RemoveObserver("", this);
    }

    RecvRemoveGeolocationListener();
    RecvRemoveDeviceMotionListener();

    nsCOMPtr<nsIThreadInternal>
        threadInt(do_QueryInterface(NS_GetCurrentThread()));
    if (threadInt)
        threadInt->SetObserver(mOldObserver);
    if (mRunToCompletionDepth)
        mRunToCompletionDepth = 0;

    mIsAlive = false;

    if (obs) {
        nsRefPtr<nsHashPropertyBag> props = new nsHashPropertyBag();
        props->Init();

        if (AbnormalShutdown == why) {
            props->SetPropertyAsBool(NS_LITERAL_STRING("abnormal"), PR_TRUE);

#ifdef MOZ_CRASHREPORTER
            nsAutoString dumpID;

            nsCOMPtr<nsILocalFile> crashDump;
            TakeMinidump(getter_AddRefs(crashDump)) &&
                CrashReporter::GetIDFromMinidump(crashDump, dumpID);

            props->SetPropertyAsAString(NS_LITERAL_STRING("dumpID"), dumpID);

            if (!dumpID.IsEmpty()) {
                CrashReporter::AnnotationTable notes;
                notes.Init();
                notes.Put(NS_LITERAL_CSTRING("ProcessType"), NS_LITERAL_CSTRING("content"));

                char startTime[32];
                sprintf(startTime, "%lld", static_cast<PRInt64>(mProcessStartTime));
                notes.Put(NS_LITERAL_CSTRING("StartupTime"),
                          nsDependentCString(startTime));

                // TODO: Additional per-process annotations.
                CrashReporter::AppendExtraData(dumpID, notes);
            }
#endif

            obs->NotifyObservers((nsIPropertyBag2*) props, "ipc:content-shutdown", nsnull);
        }
    }

    MessageLoop::current()->
        PostTask(FROM_HERE,
                 NewRunnableFunction(DelayedDeleteSubprocess, mSubprocess));
    mSubprocess = NULL;
}

TabParent*
ContentParent::CreateTab(PRUint32 aChromeFlags)
{
  return static_cast<TabParent*>(SendPBrowserConstructor(aChromeFlags));
}

TestShellParent*
ContentParent::CreateTestShell()
{
  return static_cast<TestShellParent*>(SendPTestShellConstructor());
}

bool
ContentParent::DestroyTestShell(TestShellParent* aTestShell)
{
    return PTestShellParent::Send__delete__(aTestShell);
}

ContentParent::ContentParent()
    : mGeolocationWatchID(-1)
    , mRunToCompletionDepth(0)
    , mShouldCallUnblockChild(false)
    , mIsAlive(true)
    , mProcessStartTime(time(NULL))
{
    NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
    mSubprocess = new GeckoChildProcessHost(GeckoProcessType_Content);
    mSubprocess->AsyncLaunch();
    Open(mSubprocess->GetChannel(), mSubprocess->GetChildProcessHandle());

    nsCOMPtr<nsIChromeRegistry> registrySvc = nsChromeRegistry::GetService();
    nsChromeRegistryChrome* chromeRegistry =
        static_cast<nsChromeRegistryChrome*>(registrySvc.get());
    chromeRegistry->SendRegisteredChrome(this);
}

ContentParent::~ContentParent()
{
    if (OtherProcess())
        base::CloseProcessHandle(OtherProcess());

    NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
    //If the previous content process has died, a new one could have
    //been started since.
    if (gSingleton == this)
        gSingleton = nsnull;
}

bool
ContentParent::IsAlive()
{
    return mIsAlive;
}

bool
ContentParent::RecvReadPrefsArray(InfallibleTArray<PrefTuple> *prefs)
{
    EnsurePrefService();
    mPrefService->MirrorPreferences(prefs);
    return true;
}

bool
ContentParent::RecvReadFontList(InfallibleTArray<FontListEntry>* retValue)
{
#ifdef ANDROID
    gfxAndroidPlatform::GetPlatform()->GetFontList(retValue);
#endif
    return true;
}


void
ContentParent::EnsurePrefService()
{
    nsresult rv;
    if (!mPrefService) {
        mPrefService = do_GetService(NS_PREFSERVICE_CONTRACTID, &rv);
        NS_ASSERTION(NS_SUCCEEDED(rv), 
                     "We lost prefService in the Chrome process !");
    }
}

bool
ContentParent::RecvReadPermissions(InfallibleTArray<IPC::Permission>* aPermissions)
{
#ifdef MOZ_PERMISSIONS
    nsRefPtr<nsPermissionManager> permissionManager =
        nsPermissionManager::GetSingleton();
    NS_ABORT_IF_FALSE(permissionManager,
                 "We have no permissionManager in the Chrome process !");

    nsCOMPtr<nsISimpleEnumerator> enumerator;
    nsresult rv = permissionManager->GetEnumerator(getter_AddRefs(enumerator));
    NS_ABORT_IF_FALSE(NS_SUCCEEDED(rv), "Could not get enumerator!");
    while(1) {
        PRBool hasMore;
        enumerator->HasMoreElements(&hasMore);
        if (!hasMore)
            break;

        nsCOMPtr<nsISupports> supp;
        enumerator->GetNext(getter_AddRefs(supp));
        nsCOMPtr<nsIPermission> perm = do_QueryInterface(supp);

        nsCString host;
        perm->GetHost(host);
        nsCString type;
        perm->GetType(type);
        PRUint32 capability;
        perm->GetCapability(&capability);
        PRUint32 expireType;
        perm->GetExpireType(&expireType);
        PRInt64 expireTime;
        perm->GetExpireTime(&expireTime);

        aPermissions->AppendElement(IPC::Permission(host, type, capability,
                                                    expireType, expireTime));
    }

    // Ask for future changes
    permissionManager->ChildRequestPermissions();
#endif

    return true;
}

bool
ContentParent::RecvGetIndexedDBDirectory(nsString* aDirectory)
{
    indexedDB::IDBFactory::NoteUsedByProcessType(GeckoProcessType_Content);

    nsCOMPtr<nsIFile> dbDirectory;
    nsresult rv = indexedDB::IDBFactory::GetDirectory(getter_AddRefs(dbDirectory));

    if (NS_FAILED(rv)) {
        NS_ERROR("Failed to get IndexedDB directory");
        return true;
    }

    dbDirectory->GetPath(*aDirectory);

    return true;
}

bool
ContentParent::RecvSetClipboardText(const nsString& text, const PRInt32& whichClipboard)
{
    nsresult rv;
    nsCOMPtr<nsIClipboard> clipboard(do_GetService(kCClipboardCID, &rv));
    NS_ENSURE_SUCCESS(rv, true);

    nsCOMPtr<nsISupportsString> dataWrapper =
        do_CreateInstance(NS_SUPPORTS_STRING_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv, true);
    
    rv = dataWrapper->SetData(text);
    NS_ENSURE_SUCCESS(rv, true);
    
    nsCOMPtr<nsITransferable> trans = do_CreateInstance("@mozilla.org/widget/transferable;1", &rv);
    NS_ENSURE_SUCCESS(rv, true);
    
    // If our data flavor has already been added, this will fail. But we don't care
    trans->AddDataFlavor(kUnicodeMime);
    
    nsCOMPtr<nsISupports> nsisupportsDataWrapper =
        do_QueryInterface(dataWrapper);
    
    rv = trans->SetTransferData(kUnicodeMime, nsisupportsDataWrapper,
                                text.Length() * sizeof(PRUnichar));
    NS_ENSURE_SUCCESS(rv, true);
    
    clipboard->SetData(trans, NULL, whichClipboard);
    return true;
}

bool
ContentParent::RecvGetClipboardText(const PRInt32& whichClipboard, nsString* text)
{
    nsresult rv;
    nsCOMPtr<nsIClipboard> clipboard(do_GetService(kCClipboardCID, &rv));
    NS_ENSURE_SUCCESS(rv, true);

    nsCOMPtr<nsITransferable> trans = do_CreateInstance("@mozilla.org/widget/transferable;1", &rv);
    NS_ENSURE_SUCCESS(rv, true);
    
    clipboard->GetData(trans, whichClipboard);
    nsCOMPtr<nsISupports> tmp;
    PRUint32 len;
    rv  = trans->GetTransferData(kUnicodeMime, getter_AddRefs(tmp), &len);
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsISupportsString> supportsString = do_QueryInterface(tmp);
    // No support for non-text data
    NS_ENSURE_TRUE(supportsString, NS_ERROR_NOT_IMPLEMENTED);
    supportsString->GetData(*text);
    return true;
}

bool
ContentParent::RecvEmptyClipboard()
{
    nsresult rv;
    nsCOMPtr<nsIClipboard> clipboard(do_GetService(kCClipboardCID, &rv));
    NS_ENSURE_SUCCESS(rv, true);

    clipboard->EmptyClipboard(nsIClipboard::kGlobalClipboard);

    return true;
}

bool
ContentParent::RecvClipboardHasText(PRBool* hasText)
{
    nsresult rv;
    nsCOMPtr<nsIClipboard> clipboard(do_GetService(kCClipboardCID, &rv));
    NS_ENSURE_SUCCESS(rv, true);

    clipboard->HasDataMatchingFlavors(sClipboardTextFlavors, 1, 
                                      nsIClipboard::kGlobalClipboard, hasText);
    return true;
}

bool
ContentParent::RecvGetSystemColors(const PRUint32& colorsCount, InfallibleTArray<PRUint32>* colors)
{
#ifdef ANDROID
    NS_ASSERTION(AndroidBridge::Bridge() != nsnull, "AndroidBridge is not available");
    if (AndroidBridge::Bridge() == nsnull) {
        // Do not fail - the colors won't be right, but it's not critical
        return true;
    }

    colors->AppendElements(colorsCount);

    // The array elements correspond to the members of AndroidSystemColors structure,
    // so just pass the pointer to the elements buffer
    AndroidBridge::Bridge()->GetSystemColors((AndroidSystemColors*)colors->Elements());
#endif
    return true;
}

bool
ContentParent::RecvGetIconForExtension(const nsCString& aFileExt, const PRUint32& aIconSize, InfallibleTArray<PRUint8>* bits)
{
#ifdef ANDROID
    NS_ASSERTION(AndroidBridge::Bridge() != nsnull, "AndroidBridge is not available");
    if (AndroidBridge::Bridge() == nsnull) {
        // Do not fail - just no icon will be shown
        return true;
    }

    bits->AppendElements(aIconSize * aIconSize * 4);

    AndroidBridge::Bridge()->GetIconForExtension(aFileExt, aIconSize, bits->Elements());
#endif
    return true;
}

NS_IMPL_THREADSAFE_ISUPPORTS4(ContentParent,
                              nsIObserver,
                              nsIThreadObserver,
                              nsIDOMGeoPositionCallback,
                              nsIDeviceMotionListener)

NS_IMETHODIMP
ContentParent::Observe(nsISupports* aSubject,
                       const char* aTopic,
                       const PRUnichar* aData)
{
    if (!strcmp(aTopic, "xpcom-shutdown") && mSubprocess) {
        Close();
        NS_ASSERTION(!mSubprocess, "Close should have nulled mSubprocess");
    }

    if (!mIsAlive || !mSubprocess)
        return NS_OK;

    // listening for memory pressure event
    if (!strcmp(aTopic, "memory-pressure")) {
        unused << SendFlushMemory(nsDependentString(aData));
    }
    // listening for remotePrefs...
    else if (!strcmp(aTopic, "nsPref:changed")) {
        // We know prefs are ASCII here.
        NS_LossyConvertUTF16toASCII strData(aData);

        nsCOMPtr<nsIPrefServiceInternal> prefService =
          do_GetService("@mozilla.org/preferences-service;1");

        PRBool prefNeedUpdate;
        prefService->PrefHasUserValue(strData, &prefNeedUpdate);

        // If the pref does not have a user value, check if it exist on the
        // default branch or not
        if (!prefNeedUpdate) {
          nsCOMPtr<nsIPrefBranch> defaultBranch;
          nsCOMPtr<nsIPrefService> prefsService = do_QueryInterface(prefService);
          prefsService->GetDefaultBranch(nsnull, getter_AddRefs(defaultBranch));

          PRInt32 prefType = nsIPrefBranch::PREF_INVALID;
          defaultBranch->GetPrefType(strData.get(), &prefType);
          prefNeedUpdate = (prefType != nsIPrefBranch::PREF_INVALID);
        }

        if (prefNeedUpdate) {
          // Pref was created, or previously existed and its value
          // changed.
          PrefTuple pref;
#ifdef DEBUG
          nsresult rv =
#endif
          prefService->MirrorPreference(strData, &pref);
          NS_ASSERTION(NS_SUCCEEDED(rv), "Pref has value but can't mirror?");
          if (!SendPreferenceUpdate(pref)) {
              return NS_ERROR_NOT_AVAILABLE;
          }
        } else {
          // Pref wasn't found.  It was probably removed.
          if (!SendClearUserPreference(strData)) {
              return NS_ERROR_NOT_AVAILABLE;
          }
        }
    }
    else if (!strcmp(aTopic, NS_IPC_IOSERVICE_SET_OFFLINE_TOPIC)) {
      NS_ConvertUTF16toUTF8 dataStr(aData);
      const char *offline = dataStr.get();
      if (!SendSetOffline(!strcmp(offline, "true") ? true : false))
          return NS_ERROR_NOT_AVAILABLE;
    }
    // listening for alert notifications
    else if (!strcmp(aTopic, "alertfinished") ||
             !strcmp(aTopic, "alertclickcallback") ) {
        if (!SendNotifyAlertsObserver(nsDependentCString(aTopic),
                                      nsDependentString(aData)))
            return NS_ERROR_NOT_AVAILABLE;
    }
    else if (!strcmp(aTopic, "child-memory-reporter-request")) {
        SendPMemoryReportRequestConstructor();
    }
#ifdef ACCESSIBILITY
    // Make sure accessibility is running in content process when accessibility
    // gets initiated in chrome process.
    else if (aData && (*aData == '1') &&
             !strcmp(aTopic, "a11y-init-or-shutdown")) {
        SendActivateA11y();
    }
#endif

    return NS_OK;
}

PBrowserParent*
ContentParent::AllocPBrowser(const PRUint32& aChromeFlags)
{
  TabParent* parent = new TabParent();
  if (parent){
    NS_ADDREF(parent);
  }
  return parent;
}

bool
ContentParent::DeallocPBrowser(PBrowserParent* frame)
{
  TabParent* parent = static_cast<TabParent*>(frame);
  NS_RELEASE(parent);
  return true;
}

PCrashReporterParent*
ContentParent::AllocPCrashReporter()
{
  return new CrashReporterParent();
}

bool
ContentParent::DeallocPCrashReporter(PCrashReporterParent* crashreporter)
{
  delete crashreporter;
  return true;
}

PMemoryReportRequestParent*
ContentParent::AllocPMemoryReportRequest()
{
  MemoryReportRequestParent* parent = new MemoryReportRequestParent();
  return parent;
}

bool
ContentParent::DeallocPMemoryReportRequest(PMemoryReportRequestParent* actor)
{
  delete actor;
  return true;
}

void
ContentParent::SetChildMemoryReporters(const InfallibleTArray<MemoryReport>& report)
{
    nsCOMPtr<nsIMemoryReporterManager> mgr =
        do_GetService("@mozilla.org/memory-reporter-manager;1");
    for (PRInt32 i = 0; i < mMemoryReporters.Count(); i++)
        mgr->UnregisterReporter(mMemoryReporters[i]);

    for (PRUint32 i = 0; i < report.Length(); i++) {
        nsCString process  = report[i].process();
        nsCString path     = report[i].path();
        PRInt32   kind     = report[i].kind();
        PRInt32   units    = report[i].units();
        PRInt64   amount   = report[i].amount();
        nsCString desc     = report[i].desc();
        
        nsRefPtr<nsMemoryReporter> r =
            new nsMemoryReporter(process, path, kind, units, amount, desc);

        mMemoryReporters.AppendObject(r);
        mgr->RegisterReporter(r);
    }

    nsCOMPtr<nsIObserverService> obs =
        do_GetService("@mozilla.org/observer-service;1");
    if (obs)
        obs->NotifyObservers(nsnull, "child-memory-reporter-update", nsnull);
}

PTestShellParent*
ContentParent::AllocPTestShell()
{
  return new TestShellParent();
}

bool
ContentParent::DeallocPTestShell(PTestShellParent* shell)
{
  delete shell;
  return true;
}
 
PAudioParent*
ContentParent::AllocPAudio(const PRInt32& numChannels,
                           const PRInt32& rate,
                           const PRInt32& format)
{
#if defined(MOZ_SYDNEYAUDIO)
    AudioParent *parent = new AudioParent(numChannels, rate, format);
    NS_ADDREF(parent);
    return parent;
#else
    return nsnull;
#endif
}

bool
ContentParent::DeallocPAudio(PAudioParent* doomed)
{
#if defined(MOZ_SYDNEYAUDIO)
    AudioParent *parent = static_cast<AudioParent*>(doomed);
    NS_RELEASE(parent);
#endif
    return true;
}

PNeckoParent* 
ContentParent::AllocPNecko()
{
    return new NeckoParent();
}

bool 
ContentParent::DeallocPNecko(PNeckoParent* necko)
{
    delete necko;
    return true;
}

PExternalHelperAppParent*
ContentParent::AllocPExternalHelperApp(const IPC::URI& uri,
                                       const nsCString& aMimeContentType,
                                       const nsCString& aContentDisposition,
                                       const bool& aForceSave,
                                       const PRInt64& aContentLength,
                                       const IPC::URI& aReferrer)
{
    ExternalHelperAppParent *parent = new ExternalHelperAppParent(uri, aContentLength);
    parent->AddRef();
    parent->Init(this, aMimeContentType, aContentDisposition, aForceSave, aReferrer);
    return parent;
}

bool
ContentParent::DeallocPExternalHelperApp(PExternalHelperAppParent* aService)
{
    ExternalHelperAppParent *parent = static_cast<ExternalHelperAppParent *>(aService);
    parent->Release();
    return true;
}

PStorageParent*
ContentParent::AllocPStorage(const StorageConstructData& aData)
{
    return new StorageParent(aData);
}

bool
ContentParent::DeallocPStorage(PStorageParent* aActor)
{
    delete aActor;
    return true;
}

void
ContentParent::ReportChildAlreadyBlocked()
{
    if (!mRunToCompletionDepth) {
#ifdef DEBUG
        printf("Running to completion...\n");
#endif
        mRunToCompletionDepth = 1;
        mShouldCallUnblockChild = false;
    }
}
    
bool
ContentParent::RequestRunToCompletion()
{
    if (!mRunToCompletionDepth &&
        BlockChild()) {
#ifdef DEBUG
        printf("Running to completion...\n");
#endif
        mRunToCompletionDepth = 1;
        mShouldCallUnblockChild = true;
    }
    return !!mRunToCompletionDepth;
}

bool
ContentParent::RecvStartVisitedQuery(const IPC::URI& aURI)
{
    nsCOMPtr<nsIURI> newURI(aURI);
    nsCOMPtr<IHistory> history = services::GetHistoryService();
    NS_ABORT_IF_FALSE(history, "History must exist at this point.");
    if (history) {
      history->RegisterVisitedCallback(newURI, nsnull);
    }
    return true;
}


bool
ContentParent::RecvVisitURI(const IPC::URI& uri,
                                   const IPC::URI& referrer,
                                   const PRUint32& flags)
{
    nsCOMPtr<nsIURI> ourURI(uri);
    nsCOMPtr<nsIURI> ourReferrer(referrer);
    nsCOMPtr<IHistory> history = services::GetHistoryService();
    NS_ABORT_IF_FALSE(history, "History must exist at this point");
    if (history) {
      history->VisitURI(ourURI, ourReferrer, flags);
    }
    return true;
}


bool
ContentParent::RecvSetURITitle(const IPC::URI& uri,
                                      const nsString& title)
{
    nsCOMPtr<nsIURI> ourURI(uri);
    nsCOMPtr<IHistory> history = services::GetHistoryService();
    NS_ABORT_IF_FALSE(history, "History must exist at this point");
    if (history) {
      history->SetURITitle(ourURI, title);
    }
    return true;
}

bool
ContentParent::RecvShowFilePicker(const PRInt16& mode,
                                  const PRInt16& selectedType,
                                  const PRBool& addToRecentDocs,
                                  const nsString& title,
                                  const nsString& defaultFile,
                                  const nsString& defaultExtension,
                                  const InfallibleTArray<nsString>& filters,
                                  const InfallibleTArray<nsString>& filterNames,
                                  InfallibleTArray<nsString>* files,
                                  PRInt16* retValue,
                                  nsresult* result)
{
    nsCOMPtr<nsIFilePicker> filePicker = do_CreateInstance("@mozilla.org/filepicker;1");
    if (!filePicker) {
        *result = NS_ERROR_NOT_AVAILABLE;
        return true;
    }

    // as the parent given to the content process would be meaningless in this
    // process, always use active window as the parent
    nsCOMPtr<nsIWindowWatcher> ww = do_GetService(NS_WINDOWWATCHER_CONTRACTID);
    nsCOMPtr<nsIDOMWindow> window;
    ww->GetActiveWindow(getter_AddRefs(window));

    // initialize the "real" picker with all data given
    *result = filePicker->Init(window, title, mode);
    if (NS_FAILED(*result))
        return true;

    filePicker->SetAddToRecentDocs(addToRecentDocs);

    PRUint32 count = filters.Length();
    for (PRUint32 i = 0; i < count; ++i) {
        filePicker->AppendFilter(filterNames[i], filters[i]);
    }

    filePicker->SetDefaultString(defaultFile);
    filePicker->SetDefaultExtension(defaultExtension);
    filePicker->SetFilterIndex(selectedType);

    // and finally open the dialog
    *result = filePicker->Show(retValue);
    if (NS_FAILED(*result))
        return true;

    if (mode == nsIFilePicker::modeOpenMultiple) {
        nsCOMPtr<nsISimpleEnumerator> fileIter;
        *result = filePicker->GetFiles(getter_AddRefs(fileIter));

        nsCOMPtr<nsILocalFile> singleFile;
        PRBool loop = PR_TRUE;
        while (NS_SUCCEEDED(fileIter->HasMoreElements(&loop)) && loop) {
            fileIter->GetNext(getter_AddRefs(singleFile));
            if (singleFile) {
                nsAutoString filePath;
                singleFile->GetPath(filePath);
                files->AppendElement(filePath);
            }
        }
        return true;
    }
    nsCOMPtr<nsILocalFile> file;
    filePicker->GetFile(getter_AddRefs(file));

    // even with NS_OK file can be null if nothing was selected 
    if (file) {                                 
        nsAutoString filePath;
        file->GetPath(filePath);
        files->AppendElement(filePath);
    }

    return true;
}

bool
ContentParent::RecvLoadURIExternal(const IPC::URI& uri)
{
    nsCOMPtr<nsIExternalProtocolService> extProtService(do_GetService(NS_EXTERNALPROTOCOLSERVICE_CONTRACTID));
    if (!extProtService)
        return true;
    nsCOMPtr<nsIURI> ourURI(uri);
    extProtService->LoadURI(ourURI, nsnull);
    return true;
}

/* void onDispatchedEvent (in nsIThreadInternal thread); */
NS_IMETHODIMP
ContentParent::OnDispatchedEvent(nsIThreadInternal *thread)
{
    if (mOldObserver)
        return mOldObserver->OnDispatchedEvent(thread);

    return NS_OK;
}

/* void onProcessNextEvent (in nsIThreadInternal thread, in boolean mayWait, in unsigned long recursionDepth); */
NS_IMETHODIMP
ContentParent::OnProcessNextEvent(nsIThreadInternal *thread,
                                  PRBool mayWait,
                                  PRUint32 recursionDepth)
{
    if (mRunToCompletionDepth)
        ++mRunToCompletionDepth;

    if (mOldObserver)
        return mOldObserver->OnProcessNextEvent(thread, mayWait, recursionDepth);

    return NS_OK;
}

/* void afterProcessNextEvent (in nsIThreadInternal thread, in unsigned long recursionDepth); */
NS_IMETHODIMP
ContentParent::AfterProcessNextEvent(nsIThreadInternal *thread,
                                     PRUint32 recursionDepth)
{
    if (mRunToCompletionDepth &&
        !--mRunToCompletionDepth) {
#ifdef DEBUG
            printf("... ran to completion.\n");
#endif
            if (mShouldCallUnblockChild) {
                mShouldCallUnblockChild = false;
                UnblockChild();
            }
    }

    if (mOldObserver)
        return mOldObserver->AfterProcessNextEvent(thread, recursionDepth);

    return NS_OK;
}

bool
ContentParent::RecvShowAlertNotification(const nsString& aImageUrl, const nsString& aTitle,
                                         const nsString& aText, const PRBool& aTextClickable,
                                         const nsString& aCookie, const nsString& aName)
{
    nsCOMPtr<nsIAlertsService> sysAlerts(do_GetService(NS_ALERTSERVICE_CONTRACTID));
    if (sysAlerts) {
        sysAlerts->ShowAlertNotification(aImageUrl, aTitle, aText, aTextClickable,
                                         aCookie, this, aName);
    }

    return true;
}

bool
ContentParent::RecvSyncMessage(const nsString& aMsg, const nsString& aJSON,
                               InfallibleTArray<nsString>* aRetvals)
{
  nsRefPtr<nsFrameMessageManager> ppm = nsFrameMessageManager::sParentProcessManager;
  if (ppm) {
    ppm->ReceiveMessage(static_cast<nsIContentFrameMessageManager*>(ppm.get()),
                        aMsg,PR_TRUE, aJSON, nsnull, aRetvals);
  }
  return true;
}

bool
ContentParent::RecvAsyncMessage(const nsString& aMsg, const nsString& aJSON)
{
  nsRefPtr<nsFrameMessageManager> ppm = nsFrameMessageManager::sParentProcessManager;
  if (ppm) {
    ppm->ReceiveMessage(static_cast<nsIContentFrameMessageManager*>(ppm.get()),
                        aMsg, PR_FALSE, aJSON, nsnull, nsnull);
  }
  return true;
}

bool
ContentParent::RecvAddGeolocationListener()
{
  if (mGeolocationWatchID == -1) {
    nsCOMPtr<nsIDOMGeoGeolocation> geo = do_GetService("@mozilla.org/geolocation;1");
    if (!geo) {
      return true;
    }
    geo->WatchPosition(this, nsnull, nsnull, &mGeolocationWatchID);
  }
  return true;
}

bool
ContentParent::RecvRemoveGeolocationListener()
{
  if (mGeolocationWatchID != -1) {
    nsCOMPtr<nsIDOMGeoGeolocation> geo = do_GetService("@mozilla.org/geolocation;1");
    if (!geo) {
      return true;
    }
    geo->ClearWatch(mGeolocationWatchID);
    mGeolocationWatchID = -1;
  }
  return true;
}

bool
ContentParent::RecvAddDeviceMotionListener()
{
    nsCOMPtr<nsIDeviceMotion> dm = 
        do_GetService(NS_DEVICE_MOTION_CONTRACTID);
    if (dm)
        dm->AddListener(this);
    return true;
}

bool
ContentParent::RecvRemoveDeviceMotionListener()
{
    nsCOMPtr<nsIDeviceMotion> dm = 
        do_GetService(NS_DEVICE_MOTION_CONTRACTID);
    if (dm)
        dm->RemoveListener(this);
    return true;
}

NS_IMETHODIMP
ContentParent::HandleEvent(nsIDOMGeoPosition* postion)
{
  unused << SendGeolocationUpdate(GeoPosition(postion));
  return NS_OK;
}

bool
ContentParent::RecvConsoleMessage(const nsString& aMessage)
{
  nsCOMPtr<nsIConsoleService> svc(do_GetService(NS_CONSOLESERVICE_CONTRACTID));
  if (!svc)
    return true;
  
  nsRefPtr<nsConsoleMessage> msg(new nsConsoleMessage(aMessage.get()));
  svc->LogMessage(msg);
  return true;
}

bool
ContentParent::RecvScriptError(const nsString& aMessage,
                                      const nsString& aSourceName,
                                      const nsString& aSourceLine,
                                      const PRUint32& aLineNumber,
                                      const PRUint32& aColNumber,
                                      const PRUint32& aFlags,
                                      const nsCString& aCategory)
{
  nsCOMPtr<nsIConsoleService> svc(do_GetService(NS_CONSOLESERVICE_CONTRACTID));
  if (!svc)
      return true;

  nsCOMPtr<nsIScriptError> msg(do_CreateInstance(NS_SCRIPTERROR_CONTRACTID));
  nsresult rv = msg->Init(aMessage.get(), aSourceName.get(), aSourceLine.get(),
                          aLineNumber, aColNumber, aFlags, aCategory.get());
  if (NS_FAILED(rv))
    return true;

  svc->LogMessage(msg);
  return true;
}

NS_IMETHODIMP
ContentParent::OnMotionChange(nsIDeviceMotionData *aDeviceData) {
    PRUint32 type;
    double x, y, z;
    aDeviceData->GetType(&type);
    aDeviceData->GetX(&x);
    aDeviceData->GetY(&y);
    aDeviceData->GetZ(&z);

    unused << SendDeviceMotionChanged(type, x, y, z);
    return NS_OK;
}


} // namespace dom
} // namespace mozilla
