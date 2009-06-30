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

#include "base/task.h"

#include "mozilla/ipc/GeckoThread.h"
#include "mozilla/plugins/NPAPIPluginParent.h"

using mozilla::Monitor;
using mozilla::MonitorAutoEnter;
using mozilla::ipc::BrowserProcessSubThread;

template<>
struct RunnableMethodTraits<mozilla::plugins::NPAPIPluginParent>
{
    static void RetainCallee(mozilla::plugins::NPAPIPluginParent* obj) { }
    static void ReleaseCallee(mozilla::plugins::NPAPIPluginParent* obj) { }
};

namespace mozilla {
namespace plugins {


SharedLibrary*
NPAPIPluginParent::LoadModule(const char* aFullPath,
                              PRLibrary* aLibrary)
{
    _MOZ_LOG(__FUNCTION__);

    // Block on the child process being launched and initialized.
    NPAPIPluginParent* parent = new NPAPIPluginParent(aFullPath);

    // launch the process synchronously
    {MonitorAutoEnter mon(parent->mMonitor);
        BrowserProcessSubThread::GetMessageLoop(BrowserProcessSubThread::IO)
            ->PostTask(
                FROM_HERE,
                NewRunnableMethod(parent,
                                  &NPAPIPluginParent::LaunchSubprocess));
        mon.Wait();
    }

    parent->mNpapi.Open(parent->mSubprocess.GetChannel());

    // FIXME/cjones: leaking NPAPIPluginParents ...
    return parent->mShim;
}


NPAPIPluginParent::NPAPIPluginParent(const char* aFullPath) :
    mFullPath(aFullPath),       // what's this for?
    mSubprocess(aFullPath),
    mNpapi(this),
    mMonitor("mozilla.plugins.NPAPIPluginParent.LaunchPluginProcess"),
    mShim(new Shim(this))
{
}

NPAPIPluginParent::~NPAPIPluginParent()
{
    _MOZ_LOG("  (closing Shim ...)");
    delete mShim;
}

void
NPAPIPluginParent::LaunchSubprocess()
{
    MonitorAutoEnter mon(mMonitor);
    mSubprocess.Launch();
    mon.Notify();
}

void
NPAPIPluginParent::NPN_GetValue()
{
    _MOZ_LOG(__FUNCTION__);
}

NPError
NPAPIPluginParent::NP_Initialize(const NPNetscapeFuncs* npnIface,
                                 NPPluginFuncs* nppIface)
{
    _MOZ_LOG(__FUNCTION__);

    NPError rv = mNpapi.NP_Initialize();
    if (NPERR_NO_ERROR != rv)
        return rv;

    nppIface->version = mVersion;
    nppIface->javaClass = mJavaClass;

    // FIXME/cjones: /should/ dynamically allocate shim trampoline.
    // but here we just HACK
    nppIface->newp = Shim::NPP_New;
    nppIface->destroy = Shim::NPP_Destroy;
    nppIface->setwindow = Shim::NPP_SetWindow;
    nppIface->newstream = Shim::NPP_NewStream;
    nppIface->destroystream = Shim::NPP_DestroyStream;
    nppIface->asfile = Shim::NPP_StreamAsFile;
    nppIface->writeready = Shim::NPP_WriteReady;
    nppIface->write = Shim::NPP_Write;
    nppIface->print = Shim::NPP_Print;
    nppIface->event = Shim::NPP_HandleEvent;
    nppIface->urlnotify = Shim::NPP_URLNotify;
    nppIface->getvalue = Shim::NPP_GetValue;
    nppIface->setvalue = Shim::NPP_SetValue;

    return NPERR_NO_ERROR;
}

NPError
NPAPIPluginParent::NPP_New(NPMIMEType pluginType, NPP instance, uint16_t mode,
                           int16_t argc, char* argn[], char* argv[],
                           NPSavedData* saved)
{
    _MOZ_LOG(__FUNCTION__);

    // create our wrapper instance
    NPPInstanceParent* parentInstance = new NPPInstanceParent(mNPNIface);

    // create the instance on the other side
    StringArray names;
    StringArray values;

    for (int i = 0; i < argc; ++i) {
        names.push_back(argn[i]);
        values.push_back(argv[i]);
    }

    NPError rv = mNpapi.NPP_New(pluginType,
                                /*instance*/42,
                                mode, names,
                                values);
    printf ("[NPAPIPluginParent] %s: got return value %hd\n", __FUNCTION__,
            rv);

    if (NPERR_NO_ERROR != rv)
        return rv;


    // FIXME/cjones: HACK ALERT!  kill this and manage through NPAPI
    parentInstance->mNpp.SetChannel(mNpapi.HACK_getchannel_please());
    mNpapi.HACK_npp = &(parentInstance->mNpp);


    instance->pdata = (void*) parentInstance;
    return rv;
}

// HACKS
NPAPIPluginParent* NPAPIPluginParent::Shim::HACK_target;


} // namespace plugins
} // namespace mozilla

