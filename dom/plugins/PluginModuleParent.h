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
#include "nsplugindefs.h"

#include "base/string_util.h"

#include "mozilla/SharedLibrary.h"
#include "mozilla/plugins/PPluginModuleParent.h"
#include "mozilla/plugins/PluginInstanceParent.h"
#include "mozilla/plugins/PluginProcessParent.h"

#include "nsAutoPtr.h"
#include "nsTHashtable.h"
#include "nsHashKeys.h"

#undef _MOZ_LOG
#define _MOZ_LOG(s) printf("[PluginModuleParent] %s\n", s)

namespace mozilla {
namespace plugins {
//-----------------------------------------------------------------------------

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
class PluginModuleParent : public PPluginModuleParent
{
private:
    typedef mozilla::SharedLibrary SharedLibrary;

protected:
    PPluginInstanceParent*
    PPluginInstanceConstructor(const nsCString& aMimeType,
                               const uint16_t& aMode,
                               const nsTArray<nsCString>& aNames,
                               const nsTArray<nsCString>& aValues,
                               NPError* rv);

    virtual bool
    PPluginInstanceDestructor(PPluginInstanceParent* aActor,
                              NPError* _retval);

public:
    PluginModuleParent(const char* aFilePath);

    virtual ~PluginModuleParent();

    /**
     * LoadModule
     *
     * Returns a SharedLibrary from which plugin symbols should be
     * resolved.  This may or may not launch a plugin child process,
     * and may or may not be very expensive.
     */
    static SharedLibrary* LoadModule(const char* aFilePath,
                                     PRLibrary* aLibrary);

    // NPRemoteIdentifier funcs
    virtual bool
    RecvNPN_GetStringIdentifier(const nsCString& aString,
                                NPRemoteIdentifier* aId);
    virtual bool
    RecvNPN_GetIntIdentifier(const int32_t& aInt,
                             NPRemoteIdentifier* aId);
    virtual bool
    RecvNPN_UTF8FromIdentifier(const NPRemoteIdentifier& aId,
                               nsCString* aString);
    virtual bool
    RecvNPN_IntFromIdentifier(const NPRemoteIdentifier& aId,
                              int32_t* aInt);
    virtual bool
    RecvNPN_IdentifierIsString(const NPRemoteIdentifier& aId,
                               bool* aIsString);
    virtual bool
    RecvNPN_GetStringIdentifiers(const nsTArray<nsCString>& aNames,
                                 nsTArray<NPRemoteIdentifier>* aIds);

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

    NPError NP_Shutdown()
    {
        // FIXME/cjones: shut down all our instances, and kill
        // off the child process

        _MOZ_LOG(__FUNCTION__);
        return 1;
    }

    char* NP_GetPluginVersion()
    {
        _MOZ_LOG(__FUNCTION__);
        return (char*) "0.0";
    }

    char* NP_GetMIMEDescription()
    {
        _MOZ_LOG(__FUNCTION__);
        return (char*) "application/x-foobar";
    }

    NPError NP_GetValue(void *future,
                        nsPluginVariable aVariable, void *aValue)
    {
        _MOZ_LOG(__FUNCTION__);
        return 1;
    }


    // NPP-like API that Gecko calls are trampolined into.  These 
    // messages then get forwarded along to the plugin instance,
    // and then eventually the child process.

    NPError NPP_New(NPMIMEType pluginType, NPP instance, uint16_t mode,
                    int16_t argc, char* argn[], char* argv[],
                    NPSavedData* saved);

    static NPError NPP_Destroy(NPP instance, NPSavedData** save);

    static PluginInstanceParent* InstCast(NPP instance);
    static BrowserStreamParent* StreamCast(NPP instance, NPStream* s);

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

private:
    const char* mFilePath;
    PluginProcessParent mSubprocess;
    const NPNetscapeFuncs* mNPNIface;

    // NPObject interface

    /**
     * I look like a shared library, but return functions that trampoline
     * into my parent class.
     */

    /************************************************************************
     * HACK!!!
     *
     * TODO/cjones: I want this class to "look" just like a DSO, so as to 
     * need to change as few lines of modules/plugin/ as possible.  Ideally,
     * we would fill the NPPLuginFuncs interface with pointers to 
     * dynamically-allocated functions that trampoline into member functions
     * of this class; this leaves the NPP interface completely unchanged.
     *   For example:
     * NP_GetEntryPoints() {
     *   return { .newi = new TrampolineFunc(this, ::New); }
     * }
     * NPPluginFuncs nppIface = pluginShim->NP_GetEntryPoints();
     * ...
     * nppIface->newp(...);     // trampolines to this->New()
     *
     * But there doesn't seem to be a way to do this in C++ without falling
     * back on assembly language.  So for now, we limit ourselves to one
     * plugin DSO open at a time.
     ************************************************************************/
    class Shim : public SharedLibrary
    {
    public:
        Shim(PluginModuleParent* aTarget) :
            mTarget(aTarget)
        {
            HACK_target = mTarget;
        }
        virtual ~Shim()
        {
            mTarget = 0;
        }

        virtual symbol_type
        FindSymbol(const char* aSymbolName)
        {
            if (!strcmp("NP_Shutdown", aSymbolName))
                return (symbol_type) NP_Shutdown;
            if (!strcmp("NP_Initialize", aSymbolName))
                return (symbol_type) NP_Initialize;
            if (!strcmp("NP_GetMIMEDescription", aSymbolName))
                return (symbol_type) NP_GetMIMEDescription;
            if (!strcmp("NP_GetValue", aSymbolName))
                return (symbol_type) NP_GetValue;
#ifdef OS_WIN
            if (!strcmp("NP_GetEntryPoints", aSymbolName))
                return (symbol_type) NP_GetEntryPoints;
#endif

            _MOZ_LOG("WARNING! FAILED TO FIND SYMBOL");
            return 0;
        }

        virtual function_type
        FindFunctionSymbol(const char* aSymbolName)
        {
            if (!strcmp("NP_Shutdown", aSymbolName))
                return (function_type) NP_Shutdown;
            if (!strcmp("NP_Initialize", aSymbolName))
                return (function_type) NP_Initialize;
            if (!strcmp("NP_GetMIMEDescription", aSymbolName))
                return (function_type) NP_GetMIMEDescription;
            if (!strcmp("NP_GetValue", aSymbolName))
                return (function_type) NP_GetValue;
#ifdef OS_WINDOWS
            if (!strcmp("NP_GetEntryPoints", aSymbolName))
                return (function_type) NP_GetEntryPoints;
#endif

            _MOZ_LOG("WARNING! FAILED TO FIND SYMBOL");
            return 0;
        }

    private:
        PluginModuleParent* mTarget;

        // HACKS HACKS HACKS! from here on down

#ifdef OS_LINUX
        static NPError NP_Initialize(const NPNetscapeFuncs* npnIface,
                                     NPPluginFuncs* nppIface)
        {
            return HACK_target->NP_Initialize(npnIface, nppIface);
        }
#else
        static NPError NP_Initialize(const NPNetscapeFuncs* npnIface)
        {
            return HACK_target->NP_Initialize(npnIface);
        }
        static NPError NP_GetEntryPoints(NPPluginFuncs* nppIface)
        {
            return HACK_target->NP_GetEntryPoints(nppIface);
        }
#endif
        static NPError NP_Shutdown()
        {
            return HACK_target->NP_Shutdown();
        }
        static char* NP_GetPluginVersion()
        {
            return HACK_target->NP_GetPluginVersion();
        }
        static char* NP_GetMIMEDescription()
        {
            return HACK_target->NP_GetMIMEDescription();
        }
        static NPError NP_GetValue(void *future,
                                   nsPluginVariable aVariable, void *aValue)
        {
            return HACK_target->NP_GetValue(future, aVariable, aValue);
        }
        static NPError NPP_New(NPMIMEType pluginType, NPP instance,
                               uint16_t mode,
                               int16_t argc, char* argn[], char* argv[],
                               NPSavedData* saved)
        {
            return HACK_target->NPP_New(pluginType, instance, mode,
                                        argc, argn, argv,
                                        saved);
        }

        static PluginModuleParent* HACK_target;
        friend class PluginModuleParent;
    };

    friend class Shim;
    Shim* mShim;

    nsTHashtable<nsVoidPtrHashKey> mValidIdentifiers;
};

} // namespace plugins
} // namespace mozilla

#endif  // ifndef dom_plugins_PluginModuleParent_h
