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

#include "mozilla/plugins/PluginModuleParent.h"
#include "mozilla/plugins/BrowserStreamParent.h"

#include "nsNPAPIPlugin.h"

using mozilla::PluginLibrary;

using mozilla::ipc::NPRemoteIdentifier;

using namespace mozilla::plugins;

PR_STATIC_ASSERT(sizeof(NPIdentifier) == sizeof(void*));

// static
PluginLibrary*
PluginModuleParent::LoadModule(const char* aFilePath)
{
    _MOZ_LOG(__FUNCTION__);

    // Block on the child process being launched and initialized.
    PluginModuleParent* parent = new PluginModuleParent(aFilePath);
    parent->mSubprocess->Launch();
    parent->Open(parent->mSubprocess->GetChannel(),
                 parent->mSubprocess->GetChildProcessHandle());

    return parent;
}


PluginModuleParent::PluginModuleParent(const char* aFilePath)
    : mSubprocess(new PluginProcessParent(aFilePath))
    , mShutdown(false)
    , mNPNIface(NULL)
{
    NS_ASSERTION(mSubprocess, "Out of memory!");

    if (!mValidIdentifiers.Init()) {
        NS_ERROR("Out of memory");
    }
}

PluginModuleParent::~PluginModuleParent()
{
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
}

void
PluginModuleParent::ActorDestroy(ActorDestroyReason why)
{
    switch (why) {
    case AbnormalShutdown:
        // TODObsmedberg: notify the plugin host to forget this plugin module
        // and instantiate us again.
        // FALL THROUGH

    case NormalShutdown:
        mShutdown = true;
        break;

    default:
        NS_ERROR("Unexpected shutdown reason for toplevel actor.");
    }
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
    _MOZ_LOG(__FUNCTION__);
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
                                NPSavedData** save)
{
    // FIXME/cjones:
    //  (1) send a "destroy" message to the child
    //  (2) the child shuts down its instance
    //  (3) remove both parent and child IDs from map
    //  (4) free parent
    _MOZ_LOG(__FUNCTION__);

    PluginInstanceParent* parentInstance =
        static_cast<PluginInstanceParent*>(instance->pdata);

    if (!parentInstance)
        return NPERR_NO_ERROR;

    parentInstance->Destroy();

    NPError prv;
    if (!PPluginInstanceParent::Call__delete__(parentInstance, &prv)) {
        prv = NPERR_GENERIC_ERROR;
    }
    instance->pdata = nsnull;

    return prv;
}

bool
PluginModuleParent::EnsureValidNPIdentifier(NPIdentifier aIdentifier)
{
    if (!mValidIdentifiers.GetEntry(aIdentifier)) {
        nsVoidPtrHashKey* newEntry = mValidIdentifiers.PutEntry(aIdentifier);
        if (!newEntry) {
            NS_ERROR("Out of memory?");
            return false;
        }
    }
    return true;
}

NPIdentifier
PluginModuleParent::GetValidNPIdentifier(NPRemoteIdentifier aRemoteIdentifier)
{
    NS_ASSERTION(mValidIdentifiers.IsInitialized(), "Not initialized!");
    if (aRemoteIdentifier &&
        mValidIdentifiers.GetEntry((NPIdentifier)aRemoteIdentifier)) {
        return (NPIdentifier)aRemoteIdentifier;
    }
    return 0;
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

bool
PluginModuleParent::RecvNPN_GetStringIdentifier(const nsCString& aString,
                                                NPRemoteIdentifier* aId)
{
    if (aString.IsVoid()) {
        NS_ERROR("Someone sent over a void string?!");
        return false;
    }

    NPIdentifier ident =
        mozilla::plugins::parent::_getstringidentifier(aString.BeginReading());
    if (!ident) {
        *aId = 0;
        return true;
    }

    if (!EnsureValidNPIdentifier(ident)) {
        NS_ERROR("Out of memory?");
        return false;
    }

    *aId = (NPRemoteIdentifier)ident;
    return true;
}

bool
PluginModuleParent::RecvNPN_GetIntIdentifier(const int32_t& aInt,
                                             NPRemoteIdentifier* aId)
{
    NPIdentifier ident = mozilla::plugins::parent::_getintidentifier(aInt);
    if (!ident) {
        *aId = 0;
        return true;
    }

    if (!EnsureValidNPIdentifier(ident)) {
        NS_ERROR("Out of memory?");
        return false;
    }

    *aId = (NPRemoteIdentifier)ident;
    return true;
}

bool
PluginModuleParent::RecvNPN_UTF8FromIdentifier(const NPRemoteIdentifier& aId,
                                               NPError *err,
                                               nsCString* aString)
{
    NPIdentifier ident = GetValidNPIdentifier(aId);
    if (!ident) {
        *err = NPERR_INVALID_PARAM;
        return true;
    }

    NPUTF8* val = mozilla::plugins::parent::_utf8fromidentifier(ident);
    if (!val) {
        *err = NPERR_INVALID_PARAM;
        return true;
    }

    aString->Assign(val);
    *err = NPERR_NO_ERROR;
    return true;
}

bool
PluginModuleParent::RecvNPN_IntFromIdentifier(const NPRemoteIdentifier& aId,
                                              NPError* err,
                                              int32_t* aInt)
{
    NPIdentifier ident = GetValidNPIdentifier(aId);
    if (!ident) {
        *err = NPERR_INVALID_PARAM;
        return true;
    }

    *aInt = mozilla::plugins::parent::_intfromidentifier(ident);
    *err = NPERR_NO_ERROR;
    return true;
}

bool
PluginModuleParent::RecvNPN_IdentifierIsString(const NPRemoteIdentifier& aId,
                                               bool* aIsString)
{
    NPIdentifier ident = GetValidNPIdentifier(aId);
    if (!ident) {
        *aIsString = false;
        return true;
    }

    *aIsString = mozilla::plugins::parent::_identifierisstring(ident);
    return true;
}

bool
PluginModuleParent::RecvNPN_GetStringIdentifiers(const nsTArray<nsCString>& aNames,
                                                 nsTArray<NPRemoteIdentifier>* aIds)
{
    NS_ASSERTION(aIds->IsEmpty(), "Non-empty array!");

    PRUint32 count = aNames.Length();
    if (!count) {
        NS_ERROR("No names to get!");
        return false;
    }

    nsAutoTArray<NPUTF8*, 10> buffers;
    nsAutoTArray<NPIdentifier, 10> ids;

    if (!(buffers.SetLength(count) &&
          ids.SetLength(count) &&
          aIds->SetCapacity(count))) {
        NS_ERROR("Out of memory?");
        return false;
    }

    for (PRUint32 index = 0; index < count; index++) {
        buffers[index] = const_cast<NPUTF8*>(aNames[index].BeginReading());
        NS_ASSERTION(buffers[index], "Null pointer should be impossible!");
    }

    mozilla::plugins::parent::_getstringidentifiers(
        const_cast<const NPUTF8**>(buffers.Elements()), count, ids.Elements());

    for (PRUint32 index = 0; index < count; index++) {
        NPIdentifier& id = ids[index];
        if (id) {
            if (!EnsureValidNPIdentifier(id)) {
                NS_ERROR("Out of memory?");
                return false;
            }
        }
        aIds->AppendElement((NPRemoteIdentifier)id);
    }

    return true;
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

#if defined(XP_UNIX) && !defined(XP_MACOSX)
nsresult
PluginModuleParent::NP_Initialize(NPNetscapeFuncs* bFuncs, NPPluginFuncs* pFuncs, NPError* error)
{
    _MOZ_LOG(__FUNCTION__);

    mNPNIface = bFuncs;

    if (mShutdown) {
        *error = NPERR_GENERIC_ERROR;
        return NS_ERROR_FAILURE;
    }

    if (!CallNP_Initialize(error)) {
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
    _MOZ_LOG(__FUNCTION__);

    mNPNIface = bFuncs;

    if (mShutdown) {
        *error = NPERR_GENERIC_ERROR;
        return NS_ERROR_FAILURE;
    }

    if (!CallNP_Initialize(error))
        return NS_ERROR_FAILURE;

    return NS_OK;
}
#endif

nsresult
PluginModuleParent::NP_Shutdown(NPError* error)
{
    _MOZ_LOG(__FUNCTION__);

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
PluginModuleParent::NP_GetMIMEDescription(char** mimeDesc)
{
    _MOZ_LOG(__FUNCTION__);

    *mimeDesc = (char*)"application/x-foobar";
    return NS_OK;
}

nsresult
PluginModuleParent::NP_GetValue(void *future, NPPVariable aVariable,
                                   void *aValue, NPError* error)
{
    _MOZ_LOG(__FUNCTION__);

    //TODO: implement this correctly
    printf("[%s] Not yet implemented\n", __FUNCTION__);

    *error = NPERR_GENERIC_ERROR;
    return NS_OK;
}

#if defined(XP_WIN) || defined(XP_MACOSX)
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
    _MOZ_LOG(__FUNCTION__);

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
        new PluginInstanceParent(this, instance, mNPNIface);

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

    printf ("[PluginModuleParent] %s: got return value %hd\n", __FUNCTION__,
            *error);

    if (*error != NPERR_NO_ERROR) {
        PPluginInstanceParent::Call__delete__(parentInstance, error);
        instance->pdata = nsnull;
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
