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

#include "mozilla/plugins/NPAPIPluginParent.h"

namespace mozilla {
namespace plugins {


SharedLibrary*
NPAPIPluginParent::LoadModule(const char* aFilePath, PRLibrary* aLibrary)
{
    _MOZ_LOG(__FUNCTION__);

    // Block on the child process being launched and initialized.
    NPAPIPluginParent* parent = new NPAPIPluginParent(aFilePath);
    parent->mSubprocess.Launch();
    parent->Open(parent->mSubprocess.GetChannel());

    // FIXME/cjones: leaking NPAPIPluginParents ...
    return parent->mShim;
}


NPAPIPluginParent::NPAPIPluginParent(const char* aFilePath) :
    mFilePath(aFilePath),
    mSubprocess(aFilePath),
    ALLOW_THIS_IN_INITIALIZER_LIST(mShim(new Shim(this)))
{
}

NPAPIPluginParent::~NPAPIPluginParent()
{
    _MOZ_LOG("  (closing Shim ...)");
    delete mShim;
}

NPPProtocolParent*
NPAPIPluginParent::NPPConstructor(const nsCString& aMimeType,
                                  const uint16_t& aMode,
                                  const nsTArray<nsCString>& aNames,
                                  const nsTArray<nsCString>& aValues,
                                  NPError* rv)
{
    _MOZ_LOG(__FUNCTION__);
    return new NPPInstanceParent(mNPNIface);
}

nsresult
NPAPIPluginParent::NPPDestructor(NPPProtocolParent* __a,
                                 NPError* rv)
{
    _MOZ_LOG(__FUNCTION__);
    delete __a;
    return NS_OK;
}

void
NPAPIPluginParent::SetPluginFuncs(NPPluginFuncs* aFuncs)
{
    aFuncs->version = (NP_VERSION_MAJOR << 8) | NP_VERSION_MINOR;
    aFuncs->javaClass = nsnull;

    // FIXME/cjones: /should/ dynamically allocate shim trampoline.
    // but here we just HACK
    aFuncs->newp = Shim::NPP_New;
    aFuncs->destroy = Shim::NPP_Destroy;
    aFuncs->setwindow = Shim::NPP_SetWindow;
    aFuncs->newstream = Shim::NPP_NewStream;
    aFuncs->destroystream = Shim::NPP_DestroyStream;
    aFuncs->asfile = Shim::NPP_StreamAsFile;
    aFuncs->writeready = Shim::NPP_WriteReady;
    aFuncs->write = Shim::NPP_Write;
    aFuncs->print = Shim::NPP_Print;
    aFuncs->event = Shim::NPP_HandleEvent;
    aFuncs->urlnotify = Shim::NPP_URLNotify;
    aFuncs->getvalue = Shim::NPP_GetValue;
    aFuncs->setvalue = Shim::NPP_SetValue;
}

#ifdef OS_LINUX
NPError
NPAPIPluginParent::NP_Initialize(const NPNetscapeFuncs* npnIface,
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
NPAPIPluginParent::NP_Initialize(const NPNetscapeFuncs* npnIface)
{
    _MOZ_LOG(__FUNCTION__);

    NPError prv;
    nsresult rv = CallNP_Initialize(&prv);
    if (NS_FAILED(rv))
        return rv;
    return prv;
}

NPError
NPAPIPluginParent::NP_GetEntryPoints(NPPluginFuncs* nppIface)
{
    NS_ASSERTION(nppIface, "Null pointer!");

    SetPluginFuncs(nppIface);
    return NPERR_NO_ERROR;
}
#endif

NPError
NPAPIPluginParent::NPP_New(NPMIMEType pluginType,
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
    nsAutoPtr<NPPInstanceParent> parentInstance(
        static_cast<NPPInstanceParent*>(
            CallNPPConstructor(nsDependentCString(pluginType), mode, names,
                               values, &prv)));
    printf ("[NPAPIPluginParent] %s: got return value %hd\n", __FUNCTION__,
            prv);

    if (NPERR_NO_ERROR != prv)
        return prv;
    NS_ASSERTION(parentInstance,
                 "if there's no parentInstance, there should be an error");

    instance->pdata = (void*) parentInstance.forget();
    return prv;
}

NPError
NPAPIPluginParent::NPP_Destroy(NPP instance,
                               NPSavedData** save)
{
    // FIXME/cjones:
    //  (1) send a "destroy" message to the child
    //  (2) the child shuts down its instance
    //  (3) remove both parent and child IDs from map
    //  (4) free parent

    _MOZ_LOG(__FUNCTION__);

    NPPInstanceParent* parentInstance =
        static_cast<NPPInstanceParent*>(instance->pdata);

    NPError prv;
    if (CallNPPDestructor(parentInstance, &prv)) {
        prv = NPERR_GENERIC_ERROR;
    }
    instance->pdata = nsnull;

    return prv;
 }

// HACKS
NPAPIPluginParent* NPAPIPluginParent::Shim::HACK_target;


} // namespace plugins
} // namespace mozilla

