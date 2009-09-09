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
#include "mozilla/plugins/PluginStreamParent.h"

using mozilla::SharedLibrary;

namespace mozilla {
namespace plugins {

// HACKS
PluginModuleParent* PluginModuleParent::Shim::HACK_target;

SharedLibrary*
PluginModuleParent::LoadModule(const char* aFilePath, PRLibrary* aLibrary)
{
    _MOZ_LOG(__FUNCTION__);

    // Block on the child process being launched and initialized.
    PluginModuleParent* parent = new PluginModuleParent(aFilePath);
    parent->mSubprocess.Launch();
    parent->Open(parent->mSubprocess.GetChannel());

    // FIXME/cjones: leaking PluginModuleParents ...
    return parent->mShim;
}


PluginModuleParent::PluginModuleParent(const char* aFilePath) :
    mFilePath(aFilePath),
    mSubprocess(aFilePath),
    ALLOW_THIS_IN_INITIALIZER_LIST(mShim(new Shim(this)))
{
}

PluginModuleParent::~PluginModuleParent()
{
    _MOZ_LOG("  (closing Shim ...)");
    delete mShim;
}

PPluginInstanceProtocolParent*
PluginModuleParent::PPluginInstanceConstructor(const nsCString& aMimeType,
                                               const uint16_t& aMode,
                                               const nsTArray<nsCString>& aNames,
                                               const nsTArray<nsCString>& aValues,
                                               NPError* rv)
{
    NS_ERROR("Not reachable!");
    return NULL;
}

nsresult
PluginModuleParent::PPluginInstanceDestructor(PPluginInstanceProtocolParent* aActor,
                                              NPError* _retval)
{
    _MOZ_LOG(__FUNCTION__);
    delete aActor;
    return NS_OK;
}

void
PluginModuleParent::SetPluginFuncs(NPPluginFuncs* aFuncs)
{
    aFuncs->version = (NP_VERSION_MAJOR << 8) | NP_VERSION_MINOR;
    aFuncs->javaClass = nsnull;

    // FIXME/cjones: /should/ dynamically allocate shim trampoline.
    // but here we just HACK
    aFuncs->newp = Shim::NPP_New;
    aFuncs->destroy = Shim::NPP_Destroy;
    aFuncs->setwindow = Shim::NPP_SetWindow;
    aFuncs->newstream = Shim::NPP_NewStream;
    aFuncs->destroystream = PluginModuleParent::NPP_DestroyStream;
    aFuncs->asfile = PluginModuleParent::NPP_StreamAsFile;
    aFuncs->writeready = PluginModuleParent::NPP_WriteReady;
    aFuncs->write = PluginModuleParent::NPP_Write;
    aFuncs->print = Shim::NPP_Print;
    aFuncs->event = Shim::NPP_HandleEvent;
    aFuncs->urlnotify = Shim::NPP_URLNotify;
    aFuncs->getvalue = Shim::NPP_GetValue;
    aFuncs->setvalue = Shim::NPP_SetValue;
}

#ifdef OS_LINUX
NPError
PluginModuleParent::NP_Initialize(const NPNetscapeFuncs* npnIface,
                                 NPPluginFuncs* nppIface)
{
    _MOZ_LOG(__FUNCTION__);

    NPError prv;
    nsresult rv = CallNP_Initialize(&prv);
    if (NS_OK != rv)
        return NPERR_GENERIC_ERROR;
    else if (NPERR_NO_ERROR != prv)
        return prv;

    SetPluginFuncs(nppIface);
    return NPERR_NO_ERROR;
}
#else
NPError
PluginModuleParent::NP_Initialize(const NPNetscapeFuncs* npnIface)
{
    _MOZ_LOG(__FUNCTION__);

    NPError prv;
    nsresult rv = CallNP_Initialize(&prv);
    if (NS_FAILED(rv))
        return rv;
    return prv;
}

NPError
PluginModuleParent::NP_GetEntryPoints(NPPluginFuncs* nppIface)
{
    NS_ASSERTION(nppIface, "Null pointer!");

    SetPluginFuncs(nppIface);
    return NPERR_NO_ERROR;
}
#endif

NPError
PluginModuleParent::NPP_New(NPMIMEType pluginType,
                            NPP instance,
                            uint16_t mode,
                            int16_t argc,
                            char* argn[],
                            char* argv[],
                            NPSavedData* saved)
{
    _MOZ_LOG(__FUNCTION__);

    // create the instance on the other side
    nsTArray<nsCString> names;
    nsTArray<nsCString> values;

    for (int i = 0; i < argc; ++i) {
        names.AppendElement(nsDependentCString(argn[i]));
        values.AppendElement(nsDependentCString(argv[i]));
    }

    NPError prv = NPERR_GENERIC_ERROR;
    nsAutoPtr<PluginInstanceParent> parentInstance(
        static_cast<PluginInstanceParent*>(
            CallPPluginInstanceConstructor(new PluginInstanceParent(instance,
                                                                    mNPNIface),
                                           nsDependentCString(pluginType), mode,
                                           names,values, &prv)));
    printf ("[PluginModuleParent] %s: got return value %hd\n", __FUNCTION__,
            prv);

    if (NPERR_NO_ERROR != prv)
        return prv;
    NS_ASSERTION(parentInstance,
                 "if there's no parentInstance, there should be an error");

    instance->pdata = (void*) parentInstance.forget();
    return prv;
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

    NPError prv;
    if (CallPPluginInstanceDestructor(parentInstance, &prv)) {
        prv = NPERR_GENERIC_ERROR;
    }
    instance->pdata = nsnull;

    return prv;
 }

nsresult
PluginModuleParent::RecvNPN_GetStringIdentifier(const nsCString& aString,
                                                NPRemoteIdentifier* aId)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

nsresult
PluginModuleParent::RecvNPN_GetIntIdentifier(const int32_t& aInt,
                                             NPRemoteIdentifier* aId)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

nsresult
PluginModuleParent::RecvNPN_UTF8FromIdentifier(const NPRemoteIdentifier& aId,
                                               nsCString* aString)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

nsresult
PluginModuleParent::RecvNPN_IntFromIdentifier(const NPRemoteIdentifier& aId,
                                              int32_t* aInt)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

nsresult
PluginModuleParent::RecvNPN_IdentifierIsString(const NPRemoteIdentifier& aId,
                                               bool* aIsString)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

nsresult
PluginModuleParent::RecvNPN_GetStringIdentifiers(nsTArray<nsCString>* aNames,
                                                 nsTArray<NPRemoteIdentifier>* aIds)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

PluginInstanceParent*
PluginModuleParent::InstCast(NPP instance)
{
    PluginInstanceParent* ip =
        static_cast<PluginInstanceParent*>(instance->pdata);
    if (instance != ip->mNPP) {
        NS_RUNTIMEABORT("Corrupted plugin data.");
    }
    return ip;
}

PluginStreamParent*
PluginModuleParent::StreamCast(NPP instance,
                               NPStream* s)
{
    PluginInstanceParent* ip = InstCast(instance);
    PluginStreamParent* sp =
        static_cast<PluginStreamParent*>(s->pdata);
    if (sp->mNPP != ip || s != sp->mStream) {
        NS_RUNTIMEABORT("Corrupted plugin stream data.");
    }
    return sp;
}

NPError
PluginModuleParent::NPP_DestroyStream(NPP instance,
                                      NPStream* stream,
                                      NPReason reason)
{
    return InstCast(instance)->NPP_DestroyStream(stream, reason);
}

int32_t
PluginModuleParent::NPP_WriteReady(NPP instance,
                                   NPStream* stream)
{
    return StreamCast(instance, stream)->WriteReady();
}

int32_t
PluginModuleParent::NPP_Write(NPP instance,
                              NPStream* stream,
                              int32_t offset,
                              int32_t len,
                              void* buffer)
{
    return StreamCast(instance, stream)->Write(offset, len, buffer);
}

void
PluginModuleParent::NPP_StreamAsFile(NPP instance,
                                     NPStream* stream,
                                     const char* fname)
{
    StreamCast(instance, stream)->StreamAsFile(fname);
}

} // namespace plugins
} // namespace mozilla
