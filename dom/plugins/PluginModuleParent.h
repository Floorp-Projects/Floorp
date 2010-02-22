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

#ifndef dom_plugins_PluginModuleParent_h
#define dom_plugins_PluginModuleParent_h 1

#include <cstring>

#include "base/basictypes.h"

#include "prlink.h"

#include "npapi.h"
#include "npfunctions.h"

#include "base/string_util.h"

#include "mozilla/PluginLibrary.h"
#include "mozilla/plugins/PPluginModuleParent.h"
#include "mozilla/plugins/PluginInstanceParent.h"
#include "mozilla/plugins/PluginProcessParent.h"

#include "nsAutoPtr.h"
#include "nsTHashtable.h"
#include "nsHashKeys.h"
#include "nsIFileStreams.h"

namespace mozilla {
namespace plugins {
//-----------------------------------------------------------------------------

class BrowserStreamParent;

/**
 * PluginModuleParent
 *
 * This class implements the NPP API from the perspective of the rest
 * of Gecko, forwarding NPP calls along to the child process that is
 * actually running the plugin.
 *
 * This class /also/ implements a version of the NPN API, because the
 * child process needs to make these calls back into Gecko proper.
 * This class is responsible for "actually" making those function calls.
 */
class PluginModuleParent : public PPluginModuleParent, PluginLibrary
{
private:
    typedef mozilla::PluginLibrary PluginLibrary;

protected:
    PPluginInstanceParent*
    AllocPPluginInstance(const nsCString& aMimeType,
                         const uint16_t& aMode,
                         const nsTArray<nsCString>& aNames,
                         const nsTArray<nsCString>& aValues,
                         NPError* rv);

    virtual bool
    DeallocPPluginInstance(PPluginInstanceParent* aActor);

public:
    PluginModuleParent(const char* aFilePath);
    virtual ~PluginModuleParent();

    NS_OVERRIDE virtual void SetPlugin(nsNPAPIPlugin* plugin)
    {
        mPlugin = plugin;
    }

    NS_OVERRIDE virtual void ActorDestroy(ActorDestroyReason why);

    /**
     * LoadModule
     *
     * This may or may not launch a plugin child process,
     * and may or may not be very expensive.
     */
    static PluginLibrary* LoadModule(const char* aFilePath);

    const NPNetscapeFuncs* GetNetscapeFuncs() {
        return mNPNIface;
    }

    base::ProcessHandle ChildProcessHandle() { return mSubprocess->GetChildProcessHandle(); }

    bool EnsureValidNPIdentifier(NPIdentifier aIdentifier);

protected:
    NS_OVERRIDE
    virtual bool ShouldContinueFromReplyTimeout();

    virtual bool
    AnswerNPN_UserAgent(nsCString* userAgent);

    // NPRemoteIdentifier funcs
    virtual bool
    RecvNPN_GetStringIdentifier(const nsCString& aString,
                                NPRemoteIdentifier* aId);
    virtual bool
    RecvNPN_GetIntIdentifier(const int32_t& aInt,
                             NPRemoteIdentifier* aId);
    virtual bool
    RecvNPN_UTF8FromIdentifier(const NPRemoteIdentifier& aId,
                               NPError* err,
                               nsCString* aString);
    virtual bool
    RecvNPN_IntFromIdentifier(const NPRemoteIdentifier& aId,
                              NPError* err,
                              int32_t* aInt);
    virtual bool
    RecvNPN_IdentifierIsString(const NPRemoteIdentifier& aId,
                               bool* aIsString);
    virtual bool
    RecvNPN_GetStringIdentifiers(const nsTArray<nsCString>& aNames,
                                 nsTArray<NPRemoteIdentifier>* aIds);

    virtual bool
    AnswerNPN_GetValue_WithBoolReturn(const NPNVariable& aVariable,
                                      NPError* aError,
                                      bool* aBoolVal);

    static PluginInstanceParent* InstCast(NPP instance);
    static BrowserStreamParent* StreamCast(NPP instance, NPStream* s);

private:
    void SetPluginFuncs(NPPluginFuncs* aFuncs);

    // Implement the module-level functions from NPAPI; these are
    // normally resolved directly from the DSO.
#ifdef OS_LINUX
    NPError NP_Initialize(const NPNetscapeFuncs* npnIface,
                          NPPluginFuncs* nppIface);
#else
    NPError NP_Initialize(const NPNetscapeFuncs* npnIface);
    NPError NP_GetEntryPoints(NPPluginFuncs* nppIface);
#endif

    // NPP-like API that Gecko calls are trampolined into.  These 
    // messages then get forwarded along to the plugin instance,
    // and then eventually the child process.

    static NPError NPP_Destroy(NPP instance, NPSavedData** save);

    static NPError NPP_SetWindow(NPP instance, NPWindow* window);
    static NPError NPP_NewStream(NPP instance, NPMIMEType type, NPStream* stream,
                                 NPBool seekable, uint16_t* stype);
    static NPError NPP_DestroyStream(NPP instance,
                                     NPStream* stream, NPReason reason);
    static int32_t NPP_WriteReady(NPP instance, NPStream* stream);
    static int32_t NPP_Write(NPP instance, NPStream* stream,
                             int32_t offset, int32_t len, void* buffer);
    static void NPP_StreamAsFile(NPP instance,
                                 NPStream* stream, const char* fname);
    static void NPP_Print(NPP instance, NPPrint* platformPrint);
    static int16_t NPP_HandleEvent(NPP instance, void* event);
    static void NPP_URLNotify(NPP instance, const char* url,
                              NPReason reason, void* notifyData);
    static NPError NPP_GetValue(NPP instance,
                                NPPVariable variable, void *ret_value);
    static NPError NPP_SetValue(NPP instance, NPNVariable variable,
                                void *value);

    NPIdentifier GetValidNPIdentifier(NPRemoteIdentifier aRemoteIdentifier);

    virtual bool HasRequiredFunctions();

#if defined(XP_UNIX) && !defined(XP_MACOSX)
    virtual nsresult NP_Initialize(NPNetscapeFuncs* bFuncs, NPPluginFuncs* pFuncs, NPError* error);
#else
    virtual nsresult NP_Initialize(NPNetscapeFuncs* bFuncs, NPError* error);
#endif
    virtual nsresult NP_Shutdown(NPError* error);
    virtual nsresult NP_GetMIMEDescription(const char** mimeDesc);
    virtual nsresult NP_GetValue(void *future, NPPVariable aVariable,
                                 void *aValue, NPError* error);
#if defined(XP_WIN) || defined(XP_MACOSX) || defined(XP_OS2)
    virtual nsresult NP_GetEntryPoints(NPPluginFuncs* pFuncs, NPError* error);
#endif
    virtual nsresult NPP_New(NPMIMEType pluginType, NPP instance,
                             uint16_t mode, int16_t argc, char* argn[],
                             char* argv[], NPSavedData* saved,
                             NPError* error);
private:
    void WriteExtraDataForMinidump(nsIFile* dumpFile);
    void WriteExtraDataEntry(nsIFileOutputStream* stream,
                             const char* key,
                             const char* value);
    void CleanupFromTimeout();
    static int TimeoutChanged(const char* aPref, void* aModule);

    PluginProcessParent* mSubprocess;
    bool mShutdown;
    const NPNetscapeFuncs* mNPNIface;
    nsTHashtable<nsVoidPtrHashKey> mValidIdentifiers;
    nsNPAPIPlugin* mPlugin;
    time_t mProcessStartTime;
};

} // namespace plugins
} // namespace mozilla

#endif  // ifndef dom_plugins_PluginModuleParent_h
