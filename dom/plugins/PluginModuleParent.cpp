/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: sw=4 ts=4 et :
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is Mozilla Plugin App.
 *
 * The Initial Developer of the Original Code is
 *   Chris Jones <jones.chris.g@gmail.com>
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

#ifdef MOZ_WIDGET_GTK2
#include <glib.h>
#elif XP_MACOSX
#include "PluginUtilsOSX.h"
#include "PluginInterposeOSX.h"
#endif
#ifdef MOZ_WIDGET_QT
#include <QtCore/QCoreApplication>
#include <QtCore/QEventLoop>
#endif

#include "base/process_util.h"

#include "mozilla/unused.h"
#include "mozilla/ipc/SyncChannel.h"
#include "mozilla/plugins/PluginModuleParent.h"
#include "mozilla/plugins/BrowserStreamParent.h"
#include "PluginIdentifierParent.h"

#include "nsAutoPtr.h"
#include "nsContentUtils.h"
#include "nsCRT.h"
#ifdef MOZ_CRASHREPORTER
#include "nsExceptionHandler.h"
#endif
#include "nsNPAPIPlugin.h"

using base::KillProcess;

using mozilla::PluginLibrary;
using mozilla::ipc::SyncChannel;

using namespace mozilla::plugins;

static const char kTimeoutPref[] = "dom.ipc.plugins.timeoutSecs";
static const char kLaunchTimeoutPref[] = "dom.ipc.plugins.processLaunchTimeoutSecs";

template<>
struct RunnableMethodTraits<mozilla::plugins::PluginModuleParent>
{
    typedef mozilla::plugins::PluginModuleParent Class;
    static void RetainCallee(Class* obj) { }
    static void ReleaseCallee(Class* obj) { }
};

// static
PluginLibrary*
PluginModuleParent::LoadModule(const char* aFilePath)
{
    PLUGIN_LOG_DEBUG_FUNCTION;

    PRInt32 prefSecs = nsContentUtils::GetIntPref(kLaunchTimeoutPref, 0);

    // Block on the child process being launched and initialized.
    nsAutoPtr<PluginModuleParent> parent(new PluginModuleParent(aFilePath));
    bool launched = parent->mSubprocess->Launch(prefSecs * 1000);
    if (!launched) {
        // Need to set this so the destructor doesn't complain.
        parent->mShutdown = true;
        return nsnull;
    }
    parent->Open(parent->mSubprocess->GetChannel(),
                 parent->mSubprocess->GetChildProcessHandle());

    TimeoutChanged(kTimeoutPref, parent);
    return parent.forget();
}


PluginModuleParent::PluginModuleParent(const char* aFilePath)
    : mSubprocess(new PluginProcessParent(aFilePath))
    , mPluginThread(0)
    , mShutdown(false)
    , mNPNIface(NULL)
    , mPlugin(NULL)
    , mProcessStartTime(time(NULL))
    , mTaskFactory(this)
{
    NS_ASSERTION(mSubprocess, "Out of memory!");

    if (!mIdentifiers.Init()) {
        NS_ERROR("Out of memory");
    }

    nsContentUtils::RegisterPrefCallback(kTimeoutPref, TimeoutChanged, this);
}

PluginModuleParent::~PluginModuleParent()
{
    NS_ASSERTION(OkToCleanup(), "unsafe destruction");

#ifdef OS_MACOSX
    if (mCATimer) {
        mCATimer->Cancel();
    }
#endif

    if (!mShutdown) {
        NS_WARNING("Plugin host deleted the module without shutting down.");
        NPError err;
        NP_Shutdown(&err);
    }
    NS_ASSERTION(mShutdown, "NP_Shutdown didn't");

    if (mSubprocess) {
        mSubprocess->Delete();
        mSubprocess = nsnull;
    }

    nsContentUtils::UnregisterPrefCallback(kTimeoutPref, TimeoutChanged, this);
}

#ifdef MOZ_CRASHREPORTER
void
PluginModuleParent::WritePluginExtraDataForMinidump(const nsAString& id)
{
    typedef nsDependentCString CS;

    CrashReporter::AnnotationTable notes;
    if (!notes.Init(32))
        return;

    notes.Put(CS("ProcessType"), CS("plugin"));

    char startTime[32];
    sprintf(startTime, "%lld", static_cast<PRInt64>(mProcessStartTime));
    notes.Put(CS("StartupTime"), CS(startTime));

    // Get the plugin filename, try to get just the file leafname
    const std::string& pluginFile = mSubprocess->GetPluginFilePath();
    size_t filePos = pluginFile.rfind(FILE_PATH_SEPARATOR);
    if (filePos == std::string::npos)
        filePos = 0;
    else
        filePos++;
    notes.Put(CS("PluginFilename"), CS(pluginFile.substr(filePos).c_str()));

    //TODO: add plugin name and version: bug 539841
    // (as PluginName, PluginVersion)
    notes.Put(CS("PluginName"), CS(""));
    notes.Put(CS("PluginVersion"), CS(""));

    if (!mCrashNotes.IsEmpty())
        notes.Put(CS("Notes"), CS(mCrashNotes.get()));

    if (!mHangID.IsEmpty())
        notes.Put(CS("HangID"), NS_ConvertUTF16toUTF8(mHangID));

    if (!CrashReporter::AppendExtraData(id, notes))
        NS_WARNING("problem appending plugin data to .extra");
}

void
PluginModuleParent::WriteExtraDataForHang()
{
    // this writes HangID
    WritePluginExtraDataForMinidump(mPluginDumpID);

    CrashReporter::AnnotationTable notes;
    if (!notes.Init(4))
        return;

    notes.Put(nsDependentCString("HangID"), NS_ConvertUTF16toUTF8(mHangID));
    if (!CrashReporter::AppendExtraData(mBrowserDumpID, notes))
        NS_WARNING("problem appending browser data to .extra");
}
#endif  // MOZ_CRASHREPORTER

bool
PluginModuleParent::RecvAppendNotesToCrashReport(const nsCString& aNotes)
{
    mCrashNotes.Append(aNotes);
    return true;
}

int
PluginModuleParent::TimeoutChanged(const char* aPref, void* aModule)
{
    NS_ASSERTION(NS_IsMainThread(), "Wrong thead!");
    NS_ABORT_IF_FALSE(!strcmp(aPref, kTimeoutPref),
                      "unexpected pref callback");

    PRInt32 timeoutSecs = nsContentUtils::GetIntPref(kTimeoutPref, 0);
    int32 timeoutMs = (timeoutSecs > 0) ? (1000 * timeoutSecs) :
                      SyncChannel::kNoTimeout;

    static_cast<PluginModuleParent*>(aModule)->SetReplyTimeoutMs(timeoutMs);
    return 0;
}

void
PluginModuleParent::CleanupFromTimeout()
{
    if (!mShutdown)
        Close();
}

bool
PluginModuleParent::ShouldContinueFromReplyTimeout()
{
#ifdef MOZ_CRASHREPORTER
    nsCOMPtr<nsILocalFile> pluginDump;
    nsCOMPtr<nsILocalFile> browserDump;
    CrashReporter::ProcessHandle child;
#ifdef XP_MACOSX
    child = mSubprocess->GetChildTask();
#else
    child = OtherProcess();
#endif
    if (CrashReporter::CreatePairedMinidumps(child,
                                             mPluginThread,
                                             &mHangID,
                                             getter_AddRefs(pluginDump),
                                             getter_AddRefs(browserDump)) &&
        CrashReporter::GetIDFromMinidump(pluginDump, mPluginDumpID) &&
        CrashReporter::GetIDFromMinidump(browserDump, mBrowserDumpID)) {

        PLUGIN_LOG_DEBUG(
            ("generated paired browser/plugin minidumps: %s/%s (ID=%s)",
             NS_ConvertUTF16toUTF8(mBrowserDumpID).get(),
             NS_ConvertUTF16toUTF8(mPluginDumpID).get(),
             NS_ConvertUTF16toUTF8(mHangID).get()));
    }
    else {
        NS_WARNING("failed to capture paired minidumps from hang");
    }
#endif

    // this must run before the error notification from the channel,
    // or not at all
    MessageLoop::current()->PostTask(
        FROM_HERE,
        mTaskFactory.NewRunnableMethod(
            &PluginModuleParent::CleanupFromTimeout));

    if (!KillProcess(OtherProcess(), 1, false))
        NS_WARNING("failed to kill subprocess!");

    return false;
}

void
PluginModuleParent::ActorDestroy(ActorDestroyReason why)
{
    switch (why) {
    case AbnormalShutdown: {
#ifdef MOZ_CRASHREPORTER
        nsCOMPtr<nsILocalFile> pluginDump;
        if (TakeMinidump(getter_AddRefs(pluginDump)) &&
            CrashReporter::GetIDFromMinidump(pluginDump, mPluginDumpID)) {
            PLUGIN_LOG_DEBUG(("got child minidump: %s",
                              NS_ConvertUTF16toUTF8(mPluginDumpID).get()));
            WritePluginExtraDataForMinidump(mPluginDumpID);
        }
        else if (!mPluginDumpID.IsEmpty() && !mBrowserDumpID.IsEmpty()) {
            WriteExtraDataForHang();
        }
        else {
            NS_WARNING("[PluginModuleParent::ActorDestroy] abnormal shutdown without minidump!");
        }
#endif

        mShutdown = true;
        // Defer the PluginCrashed method so that we don't re-enter
        // and potentially modify the actor child list while enumerating it.
        if (mPlugin)
            MessageLoop::current()->PostTask(
                FROM_HERE,
                mTaskFactory.NewRunnableMethod(
                    &PluginModuleParent::NotifyPluginCrashed));
        break;
    }
    case NormalShutdown:
        mShutdown = true;
        break;

    default:
        NS_ERROR("Unexpected shutdown reason for toplevel actor.");
    }
}

void
PluginModuleParent::NotifyPluginCrashed()
{
    if (!OkToCleanup()) {
        // there's still plugin code on the C++ stack.  try again
        MessageLoop::current()->PostDelayedTask(
            FROM_HERE,
            mTaskFactory.NewRunnableMethod(
                &PluginModuleParent::NotifyPluginCrashed), 10);
        return;
    }

    if (mPlugin)
        mPlugin->PluginCrashed(mPluginDumpID, mBrowserDumpID);
}

PPluginIdentifierParent*
PluginModuleParent::AllocPPluginIdentifier(const nsCString& aString,
                                           const int32_t& aInt)
{
    NPIdentifier npident = aString.IsVoid() ?
        mozilla::plugins::parent::_getintidentifier(aInt) :
        mozilla::plugins::parent::_getstringidentifier(aString.get());

    if (!npident) {
        NS_WARNING("Failed to get identifier!");
        return nsnull;
    }

    PluginIdentifierParent* ident = new PluginIdentifierParent(npident);
    mIdentifiers.Put(npident, ident);
    return ident;
}

bool
PluginModuleParent::DeallocPPluginIdentifier(PPluginIdentifierParent* aActor)
{
    delete aActor;
    return true;
}

PPluginInstanceParent*
PluginModuleParent::AllocPPluginInstance(const nsCString& aMimeType,
                                         const uint16_t& aMode,
                                         const nsTArray<nsCString>& aNames,
                                         const nsTArray<nsCString>& aValues,
                                         NPError* rv)
{
    NS_ERROR("Not reachable!");
    return NULL;
}

bool
PluginModuleParent::DeallocPPluginInstance(PPluginInstanceParent* aActor)
{
    PLUGIN_LOG_DEBUG_METHOD;
    delete aActor;
    return true;
}

void
PluginModuleParent::SetPluginFuncs(NPPluginFuncs* aFuncs)
{
    aFuncs->version = (NP_VERSION_MAJOR << 8) | NP_VERSION_MINOR;
    aFuncs->javaClass = nsnull;

    aFuncs->newp = nsnull; // Gecko should always call this through a PluginLibrary object
    aFuncs->destroy = NPP_Destroy;
    aFuncs->setwindow = NPP_SetWindow;
    aFuncs->newstream = NPP_NewStream;
    aFuncs->destroystream = NPP_DestroyStream;
    aFuncs->asfile = NPP_StreamAsFile;
    aFuncs->writeready = NPP_WriteReady;
    aFuncs->write = NPP_Write;
    aFuncs->print = NPP_Print;
    aFuncs->event = NPP_HandleEvent;
    aFuncs->urlnotify = NPP_URLNotify;
    aFuncs->getvalue = NPP_GetValue;
    aFuncs->setvalue = NPP_SetValue;
}

NPError
PluginModuleParent::NPP_Destroy(NPP instance,
                                NPSavedData** /*saved*/)
{
    // FIXME/cjones:
    //  (1) send a "destroy" message to the child
    //  (2) the child shuts down its instance
    //  (3) remove both parent and child IDs from map
    //  (4) free parent
    PLUGIN_LOG_DEBUG_FUNCTION;

    PluginInstanceParent* parentInstance =
        static_cast<PluginInstanceParent*>(instance->pdata);

    if (!parentInstance)
        return NPERR_NO_ERROR;

    NPError retval = parentInstance->Destroy();
    instance->pdata = nsnull;

    unused << PluginInstanceParent::Call__delete__(parentInstance);
    return retval;
}

NPError
PluginModuleParent::NPP_NewStream(NPP instance, NPMIMEType type,
                                  NPStream* stream, NPBool seekable,
                                  uint16_t* stype)
{
    PluginInstanceParent* i = InstCast(instance);
    if (!i)
        return NPERR_GENERIC_ERROR;

    return i->NPP_NewStream(type, stream, seekable,
                            stype);
}

NPError
PluginModuleParent::NPP_SetWindow(NPP instance, NPWindow* window)
{
    PluginInstanceParent* i = InstCast(instance);
    if (!i)
        return NPERR_GENERIC_ERROR;

    return i->NPP_SetWindow(window);
}

NPError
PluginModuleParent::NPP_DestroyStream(NPP instance,
                                      NPStream* stream,
                                      NPReason reason)
{
    PluginInstanceParent* i = InstCast(instance);
    if (!i)
        return NPERR_GENERIC_ERROR;

    return i->NPP_DestroyStream(stream, reason);
}

int32_t
PluginModuleParent::NPP_WriteReady(NPP instance,
                                   NPStream* stream)
{
    BrowserStreamParent* s = StreamCast(instance, stream);
    if (!s)
        return -1;

    return s->WriteReady();
}

int32_t
PluginModuleParent::NPP_Write(NPP instance,
                              NPStream* stream,
                              int32_t offset,
                              int32_t len,
                              void* buffer)
{
    BrowserStreamParent* s = StreamCast(instance, stream);
    if (!s)
        return -1;

    return s->Write(offset, len, buffer);
}

void
PluginModuleParent::NPP_StreamAsFile(NPP instance,
                                     NPStream* stream,
                                     const char* fname)
{
    BrowserStreamParent* s = StreamCast(instance, stream);
    if (!s)
        return;

    s->StreamAsFile(fname);
}

void
PluginModuleParent::NPP_Print(NPP instance, NPPrint* platformPrint)
{
    PluginInstanceParent* i = InstCast(instance);
    if (i)
        i->NPP_Print(platformPrint);
}

int16_t
PluginModuleParent::NPP_HandleEvent(NPP instance, void* event)
{
    PluginInstanceParent* i = InstCast(instance);
    if (!i)
        return false;

    return i->NPP_HandleEvent(event);
}

void
PluginModuleParent::NPP_URLNotify(NPP instance, const char* url,
                                  NPReason reason, void* notifyData)
{
    PluginInstanceParent* i = InstCast(instance);
    if (!i)
        return;

    i->NPP_URLNotify(url, reason, notifyData);
}

NPError
PluginModuleParent::NPP_GetValue(NPP instance,
                                 NPPVariable variable, void *ret_value)
{
    PluginInstanceParent* i = InstCast(instance);
    if (!i)
        return NPERR_GENERIC_ERROR;

    return i->NPP_GetValue(variable, ret_value);
}

NPError
PluginModuleParent::NPP_SetValue(NPP instance, NPNVariable variable,
                                 void *value)
{
    PluginInstanceParent* i = InstCast(instance);
    if (!i)
        return NPERR_GENERIC_ERROR;

    return i->NPP_SetValue(variable, value);
}

bool
PluginModuleParent::AnswerNPN_UserAgent(nsCString* userAgent)
{
    *userAgent = NullableString(mNPNIface->uagent(nsnull));
    return true;
}

PPluginIdentifierParent*
PluginModuleParent::GetIdentifierForNPIdentifier(NPIdentifier aIdentifier)
{
    PluginIdentifierParent* ident;
    if (!mIdentifiers.Get(aIdentifier, &ident)) {
        nsCString string;
        int32_t intval = -1;
        if (mozilla::plugins::parent::_identifierisstring(aIdentifier)) {
            NPUTF8* chars =
                mozilla::plugins::parent::_utf8fromidentifier(aIdentifier);
            if (!chars) {
                return nsnull;
            }
            string.Adopt(chars);
        }
        else {
            intval = mozilla::plugins::parent::_intfromidentifier(aIdentifier);
            string.SetIsVoid(PR_TRUE);
        }
        ident = new PluginIdentifierParent(aIdentifier);
        if (!SendPPluginIdentifierConstructor(ident, string, intval))
            return nsnull;

        mIdentifiers.Put(aIdentifier, ident);
    }
    return ident;
}

PluginInstanceParent*
PluginModuleParent::InstCast(NPP instance)
{
    PluginInstanceParent* ip =
        static_cast<PluginInstanceParent*>(instance->pdata);

    // If the plugin crashed and the PluginInstanceParent was deleted,
    // instance->pdata will be NULL.
    if (!ip)
        return NULL;

    if (instance != ip->mNPP) {
        NS_RUNTIMEABORT("Corrupted plugin data.");
    }
    return ip;
}

BrowserStreamParent*
PluginModuleParent::StreamCast(NPP instance,
                               NPStream* s)
{
    PluginInstanceParent* ip = InstCast(instance);
    if (!ip)
        return NULL;

    BrowserStreamParent* sp =
        static_cast<BrowserStreamParent*>(static_cast<AStream*>(s->pdata));
    if (sp->mNPP != ip || s != sp->mStream) {
        NS_RUNTIMEABORT("Corrupted plugin stream data.");
    }
    return sp;
}

bool
PluginModuleParent::HasRequiredFunctions()
{
    return true;
}

nsresult
PluginModuleParent::AsyncSetWindow(NPP instance, NPWindow* window)
{
    PluginInstanceParent* i = InstCast(instance);
    if (!i)
        return NS_ERROR_FAILURE;

    return i->AsyncSetWindow(window);
}

nsresult
PluginModuleParent::NotifyPainted(NPP instance)
{
    PluginInstanceParent* i = InstCast(instance);
    if (!i)
        return NS_ERROR_FAILURE;

    return i->NotifyPainted();
}

nsresult
PluginModuleParent::GetSurface(NPP instance, gfxASurface** aSurface)
{
    PluginInstanceParent* i = InstCast(instance);
    if (!i)
        return NS_ERROR_FAILURE;

    return i->GetSurface(aSurface);
}

nsresult
PluginModuleParent::UseAsyncPainting(NPP instance, PRBool* aIsAsync)
{
    PluginInstanceParent* i = InstCast(instance);
    if (!i)
        return NS_ERROR_FAILURE;

    return i->UseAsyncPainting(aIsAsync);
}


#if defined(XP_UNIX) && !defined(XP_MACOSX)
nsresult
PluginModuleParent::NP_Initialize(NPNetscapeFuncs* bFuncs, NPPluginFuncs* pFuncs, NPError* error)
{
    PLUGIN_LOG_DEBUG_METHOD;

    mNPNIface = bFuncs;

    if (mShutdown) {
        *error = NPERR_GENERIC_ERROR;
        return NS_ERROR_FAILURE;
    }

    if (!CallNP_Initialize(&mPluginThread, error)) {
        return NS_ERROR_FAILURE;
    }
    else if (*error != NPERR_NO_ERROR) {
        return NS_OK;
    }

    SetPluginFuncs(pFuncs);
    return NS_OK;
}
#else
nsresult
PluginModuleParent::NP_Initialize(NPNetscapeFuncs* bFuncs, NPError* error)
{
    PLUGIN_LOG_DEBUG_METHOD;

    mNPNIface = bFuncs;

    if (mShutdown) {
        *error = NPERR_GENERIC_ERROR;
        return NS_ERROR_FAILURE;
    }

    if (!CallNP_Initialize(&mPluginThread, error))
        return NS_ERROR_FAILURE;

    return NS_OK;
}
#endif

nsresult
PluginModuleParent::NP_Shutdown(NPError* error)
{
    PLUGIN_LOG_DEBUG_METHOD;

    if (mShutdown) {
        *error = NPERR_GENERIC_ERROR;
        return NS_ERROR_FAILURE;
    }

    bool ok = CallNP_Shutdown(error);

    // if NP_Shutdown() is nested within another RPC call, this will
    // break things.  but lord help us if we're doing that anyway; the
    // plugin dso will have been unloaded on the other side by the
    // CallNP_Shutdown() message
    Close();

    return ok ? NS_OK : NS_ERROR_FAILURE;
}

nsresult
PluginModuleParent::NP_GetMIMEDescription(const char** mimeDesc)
{
    PLUGIN_LOG_DEBUG_METHOD;

    *mimeDesc = "application/x-foobar";
    return NS_OK;
}

nsresult
PluginModuleParent::NP_GetValue(void *future, NPPVariable aVariable,
                                   void *aValue, NPError* error)
{
    PR_LOG(gPluginLog, PR_LOG_WARNING, ("%s Not implemented, requested variable %i", __FUNCTION__,
                                        (int) aVariable));

    //TODO: implement this correctly
    *error = NPERR_GENERIC_ERROR;
    return NS_OK;
}

#if defined(XP_WIN) || defined(XP_MACOSX) || defined(XP_OS2)
nsresult
PluginModuleParent::NP_GetEntryPoints(NPPluginFuncs* pFuncs, NPError* error)
{
    NS_ASSERTION(pFuncs, "Null pointer!");

    SetPluginFuncs(pFuncs);
    *error = NPERR_NO_ERROR;
    return NS_OK;
}
#endif

nsresult
PluginModuleParent::NPP_New(NPMIMEType pluginType, NPP instance,
                            uint16_t mode, int16_t argc, char* argn[],
                            char* argv[], NPSavedData* saved,
                            NPError* error)
{
    PLUGIN_LOG_DEBUG_METHOD;

    if (mShutdown) {
        *error = NPERR_GENERIC_ERROR;
        return NS_ERROR_FAILURE;
    }

    // create the instance on the other side
    nsTArray<nsCString> names;
    nsTArray<nsCString> values;

    for (int i = 0; i < argc; ++i) {
        names.AppendElement(NullableString(argn[i]));
        values.AppendElement(NullableString(argv[i]));
    }

    PluginInstanceParent* parentInstance =
        new PluginInstanceParent(this, instance,
                                 nsDependentCString(pluginType), mNPNIface);

    if (!parentInstance->Init()) {
        delete parentInstance;
        return NS_ERROR_FAILURE;
    }

    instance->pdata = parentInstance;

    if (!CallPPluginInstanceConstructor(parentInstance,
                                        nsDependentCString(pluginType), mode,
                                        names, values, error)) {
        // |parentInstance| is automatically deleted.
        instance->pdata = nsnull;
        // if IPC is down, we'll get an immediate "failed" return, but
        // without *error being set.  So make sure that the error
        // condition is signaled to nsNPAPIPluginInstance
        if (NPERR_NO_ERROR == *error)
            *error = NPERR_GENERIC_ERROR;
        return NS_ERROR_FAILURE;
    }

    if (*error != NPERR_NO_ERROR) {
        NPP_Destroy(instance, 0);
        return NS_ERROR_FAILURE;
    }

    return NS_OK;
}

bool
PluginModuleParent::AnswerNPN_GetValue_WithBoolReturn(const NPNVariable& aVariable,
                                                      NPError* aError,
                                                      bool* aBoolVal)
{
    NPBool boolVal = false;
    *aError = mozilla::plugins::parent::_getvalue(nsnull, aVariable, &boolVal);
    *aBoolVal = boolVal ? true : false;
    return true;
}

#if defined(MOZ_WIDGET_QT)
static const int kMaxtimeToProcessEvents = 30;
bool
PluginModuleParent::AnswerProcessSomeEvents()
{
    PLUGIN_LOG_DEBUG(("Spinning mini nested loop ..."));
    QCoreApplication::processEvents(QEventLoop::AllEvents, kMaxtimeToProcessEvents);

    PLUGIN_LOG_DEBUG(("... quitting mini nested loop"));

    return true;
}

#elif defined(XP_MACOSX)
bool
PluginModuleParent::AnswerProcessSomeEvents()
{
    mozilla::plugins::PluginUtilsOSX::InvokeNativeEventLoop();
    return true;
}

#elif !defined(MOZ_WIDGET_GTK2)
bool
PluginModuleParent::AnswerProcessSomeEvents()
{
    NS_RUNTIMEABORT("unreached");
    return false;
}

#else
static const int kMaxChancesToProcessEvents = 20;

bool
PluginModuleParent::AnswerProcessSomeEvents()
{
    PLUGIN_LOG_DEBUG(("Spinning mini nested loop ..."));

    int i = 0;
    for (; i < kMaxChancesToProcessEvents; ++i)
        if (!g_main_context_iteration(NULL, FALSE))
            break;

    PLUGIN_LOG_DEBUG(("... quitting mini nested loop; processed %i tasks", i));

    return true;
}
#endif

bool
PluginModuleParent::RecvProcessNativeEventsInRPCCall()
{
    PLUGIN_LOG_DEBUG(("%s", FULLFUNCTION));
#if defined(OS_WIN)
    ProcessNativeEventsInRPCCall();
    return true;
#else
    NS_NOTREACHED(
        "PluginInstanceParent::RecvProcessNativeEventsInRPCCall not implemented!");
    return false;
#endif
}

bool
PluginModuleParent::RecvPluginShowWindow(const uint32_t& aWindowId, const bool& aModal,
                                         const int32_t& aX, const int32_t& aY,
                                         const size_t& aWidth, const size_t& aHeight)
{
    PLUGIN_LOG_DEBUG(("%s", FULLFUNCTION));
#if defined(XP_MACOSX)
    CGRect windowBound = ::CGRectMake(aX, aY, aWidth, aHeight);
    mac_plugin_interposing::parent::OnPluginShowWindow(aWindowId, windowBound, aModal);
    return true;
#else
    NS_NOTREACHED(
        "PluginInstanceParent::RecvPluginShowWindow not implemented!");
    return false;
#endif
}

bool
PluginModuleParent::RecvPluginHideWindow(const uint32_t& aWindowId)
{
    PLUGIN_LOG_DEBUG(("%s", FULLFUNCTION));
#if defined(XP_MACOSX)
    mac_plugin_interposing::parent::OnPluginHideWindow(aWindowId, OtherSidePID());
    return true;
#else
    NS_NOTREACHED(
        "PluginInstanceParent::RecvPluginHideWindow not implemented!");
    return false;
#endif
}

#ifdef OS_MACOSX
#define DEFAULT_REFRESH_MS 20 // CoreAnimation: 50 FPS

void
CAUpdate(nsITimer *aTimer, void *aClosure) {
    nsTObserverArray<PluginInstanceParent*> *ips =
        static_cast<nsTObserverArray<PluginInstanceParent*> *>(aClosure);
    nsTObserverArray<PluginInstanceParent*>::ForwardIterator iter(*ips);
    while (iter.HasMore()) {
        iter.GetNext()->Invalidate();
    }
}

void
PluginModuleParent::AddToRefreshTimer(PluginInstanceParent *aInstance) {
    if (mCATimerTargets.Contains(aInstance)) {
        return;
    }

    mCATimerTargets.AppendElement(aInstance);
    if (mCATimerTargets.Length() == 1) {
        if (!mCATimer) {
            nsresult rv;
            nsCOMPtr<nsITimer> xpcomTimer = do_CreateInstance(NS_TIMER_CONTRACTID, &rv);
            if (NS_FAILED(rv)) {
                NS_WARNING("Could not create Core Animation timer for plugin.");
                return;
            }
            mCATimer = xpcomTimer;
        }
        mCATimer->InitWithFuncCallback(CAUpdate, &mCATimerTargets, DEFAULT_REFRESH_MS,
                                       nsITimer::TYPE_REPEATING_SLACK);
    }
}

void
PluginModuleParent::RemoveFromRefreshTimer(PluginInstanceParent *aInstance) {
    PRBool visibleRemoved = mCATimerTargets.RemoveElement(aInstance);
    if (visibleRemoved && mCATimerTargets.IsEmpty()) {
        mCATimer->Cancel();
    }
}
#endif
