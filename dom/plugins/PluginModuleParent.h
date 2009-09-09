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
#include "mozilla/plugins/PPluginModuleProtocolParent.h"
#include "mozilla/plugins/PluginInstanceParent.h"
#include "mozilla/plugins/PluginProcessParent.h"

#include "nsAutoPtr.h"

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
class PluginModuleParent : public PPluginModuleProtocolParent
{
private:
    typedef mozilla::SharedLibrary SharedLibrary;

protected:
    PPluginInstanceProtocolParent*
    PPluginInstanceConstructor(const nsCString& aMimeType,
                               const uint16_t& aMode,
                               const nsTArray<nsCString>& aNames,
                               const nsTArray<nsCString>& aValues,
                               NPError* rv);

    virtual nsresult
    PPluginInstanceDestructor(PPluginInstanceProtocolParent* aActor,
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
    virtual nsresult
    RecvNPN_GetStringIdentifier(const nsCString& aString,
                                NPRemoteIdentifier* aId);
    virtual nsresult
    RecvNPN_GetIntIdentifier(const int32_t& aInt,
                             NPRemoteIdentifier* aId);
    virtual nsresult
    RecvNPN_UTF8FromIdentifier(const NPRemoteIdentifier& aId,
                               nsCString* aString);
    virtual nsresult
    RecvNPN_IntFromIdentifier(const NPRemoteIdentifier& aId,
                              int32_t* aInt);
    virtual nsresult
    RecvNPN_IdentifierIsString(const NPRemoteIdentifier& aId,
                               bool* aIsString);
    virtual nsresult
    RecvNPN_GetStringIdentifiers(nsTArray<nsCString>* aNames,
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

    NPError NPP_Destroy(NPP instance, NPSavedData** save);

    static inline PluginInstanceParent& InstCast(void* p)
    {
        return *static_cast<PluginInstanceParent*>(p);
    }

    static PluginInstanceParent* InstCast(NPP instance);
    static BrowserStreamParent* StreamCast(NPP instance, NPStream* s);

    static inline const PluginInstanceParent& InstCast(const void* p)
    {
        return *static_cast<const PluginInstanceParent*>(p);
    }

    NPError NPP_SetWindow(NPP instance, NPWindow* window)
    {
        return InstCast(instance->pdata).NPP_SetWindow(window);
    }

    NPError NPP_NewStream(NPP instance, NPMIMEType type, NPStream* stream,
                          NPBool seekable, uint16_t* stype)
    {
        return InstCast(instance->pdata).NPP_NewStream(type, stream, seekable, stype);
    }

    static NPError NPP_DestroyStream(NPP instance,
                                     NPStream* stream, NPReason reason);
    static int32_t NPP_WriteReady(NPP instance, NPStream* stream);
    static int32_t NPP_Write(NPP instance, NPStream* stream,
                             int32_t offset, int32_t len, void* buffer);
    static void NPP_StreamAsFile(NPP instance,
                                 NPStream* stream, const char* fname);

    void NPP_Print(NPP instance, NPPrint* platformPrint)
    {
        return InstCast(instance->pdata).NPP_Print(platformPrint);
    }

    int16_t NPP_HandleEvent(NPP instance, void* event)
    {
        return InstCast(instance->pdata).NPP_HandleEvent(event);
    }

    void NPP_URLNotify(NPP instance,
                       const char* url, NPReason reason, void* notifyData)
    {
        return InstCast(instance->pdata).NPP_URLNotify(url,
                                                       reason, notifyData);
    }

    NPError NPP_GetValue(NPP instance,
                            NPPVariable variable, void *ret_value)
    {
        return InstCast(instance->pdata).NPP_GetValue(variable, ret_value);
    }

    NPError NPP_SetValue(NPP instance, NPNVariable variable, void *value)
    {
        return InstCast(instance->pdata).NPP_SetValue(variable, value);
    }

#if 0
    // NPN-like API that IPC messages from the child process end up
    // calling into.  We make the "real" calls back into Gecko from
    // here, then wrap up the responses and send them back to the 
    // child process.

    NPIdentifier
    NPN_GetIntIdentifier(int32_t aInt)
    {
        return mNPNIface->getintidentifier(aInt);
    }

    NPIdentifier
    NPN_GetStringIdentifier(const NPUTF8* aName)
    {
        return mNPNIface->getstringidentifier(aName);
    }

    void
    NPN_GetStringIdentifiers(const NPUTF8** aNames,
                             int32_t aNamesCount,
                             NPIdentifier* aIdentifiers)
    {
        return mNPNIface->getstringidentifiers(aNames,
                                               aNamesCount,
                                               aIdentifiers);
    }

    NPUTF8*
    NPN_UTF8FromIdentifier(NPIdentifier aIdentifier)
    {
        return mNPNIface->utf8fromidentifier(aIdentifier);
    }

    int32_t
    NPN_IntFromIdentifier(NPIdentifier aIdentifier)
    {
        return mNPNIface->intfromidentifier(aIdentifier);
    }

    // NPRuntime bindings from Gecko->child and child->Gecko.
    static base::hash_map<int, PluginNPObject*> sNPObjects;
    static int sNextNPObjectId = 0;

    static NPObject*
    ScriptableAllocate(NPP aNPP,
                       NPClass* aClass)
    {
        PluginNPObject* obj = (PluginNPObject*)NPN_MemAlloc(sizeof(PluginNPObject));
        if (obj) {
            obj->objectId = sNextNPObjectId++;
            obj->classId = -1;
        }
        sNPObjects[obj->objectId] = obj;
        return obj;
    }

    static void
    ScriptableDeallocate(NPObject* aNPObj)
    {
        PluginNPObject* obj = static_cast<PluginNPObject*>(aNPObj);
        base::hash_map<int, PluginNPObject*>::iterator iter =
            sNPObjects.find(obj->objectId);
        if (iter != sNPObjects.end()) {
            sNPObjects.erase(iter);
        }
        NPN_MemFree(obj);
    }

    static void
    ScriptableInvalidate(NPObject* aNPObj)
    {
    }

    static bool
    ScriptableHasMethod(NPObject* aNPObj,
                        NPIdentifier aName)
    {
        return false;
    }

    static bool
    ScriptableInvoke(NPObject* aNPObj,
                     NPIdentifier aName,
                     const NPVariant* aArgs,
                     uint32_t aArgsCount,
                     NPVariant* aResult)
    {
        return false;
    }

    static bool
    ScriptableInvokeDefault(NPObject* aNPObj,
                            const NPVariant* aArgs,
                            uint32_t aArgsCount,
                            NPVariant* aResult)
    {
        return false;
    }

    static bool
    ScriptableHasProperty(NPObject* aNPObj,
                          NPIdentifier aName)
    {
        return false;
    }

    static bool
    ScriptableGetProperty(NPObject* aNPObj,
                          NPIdentifier aName,
                          NPVariant* aResult)
    {
        return false;
    }

    static bool
    ScriptableSetProperty(NPObject* aNPObj,
                          NPIdentifier aName,
                          const NPVariant* aValue)
    {
        return false;
    }

    static bool
    ScriptableRemoveProperty(NPObject* aNPObj,
                             NPIdentifier aName)
    {
        return false;
    }

    static bool
    ScriptableEnumerate(NPObject* aNPObj,
                        NPIdentifier** aIdentifier,
                        uint32_t* aCount)
    {
        return false;
    }

    static bool
    ScriptableConstruct(NPObject* aNPObj,
                        const NPVariant* aArgs,
                        uint32_t aArgsCount,
                        NPVariant* aResult)
    {
        return false;
    }

    static NPClass sNPClass = {
        NP_CLASS_STRUCT_VERSION,
        ScriptableAllocate,
        ScriptableDeallocate,
        ScriptableInvalidate,
        ScriptableHasMethod,
        ScriptableInvoke,
        ScriptableInvokeDefault,
        ScriptableHasProperty,
        ScriptableGetProperty,
        ScriptableSetProperty,
        ScriptableRemoveProperty,
        ScriptableEnumerate,
        ScriptableConstruct
    };
#endif


private:
    const char* mFilePath;
    PluginProcessParent mSubprocess;
    const NPNetscapeFuncs* mNPNIface;

    // NPObject interface

#if 0
    struct PluginNPObject : NPObject
    {
        int objectId;
        int classId;
    };
#endif

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
        static NPError NPP_Destroy(NPP instance, NPSavedData** save)
        {
            return HACK_target->NPP_Destroy(instance, save);
        }
        static NPError NPP_SetWindow(NPP instance, NPWindow* window)
        {
            return HACK_target->NPP_SetWindow(instance, window);
        }
        static NPError NPP_NewStream(NPP instance,
                                     NPMIMEType type, NPStream* stream,
                                     NPBool seekable, uint16_t* stype)
        {
            return HACK_target->NPP_NewStream(instance, type, stream,
                                              seekable, stype);
        }
        static void NPP_Print(NPP instance, NPPrint* platformPrint)
        {
            return HACK_target->NPP_Print(instance, platformPrint);
        }
        static int16_t NPP_HandleEvent(NPP instance, void* event)
        {
            return HACK_target->NPP_HandleEvent(instance, event);
        }
        static void NPP_URLNotify(NPP instance, const char* url,
                                  NPReason reason, void* notifyData)
        {
            return HACK_target->NPP_URLNotify(instance, url, reason,
                                              notifyData);
        }
        static NPError NPP_GetValue(NPP instance,
                                    NPPVariable variable, void *ret_value)
        {
            return HACK_target->NPP_GetValue(instance, variable, ret_value);
        }
        static NPError NPP_SetValue(NPP instance,
                                    NPNVariable variable, void *value)
        {
            return HACK_target->NPP_SetValue(instance, variable, value);
        }

        static PluginModuleParent* HACK_target;
        friend class PluginModuleParent;
    };

    friend class Shim;
    Shim* mShim;
};

} // namespace plugins
} // namespace mozilla

#endif  // ifndef dom_plugins_PluginModuleParent_h
