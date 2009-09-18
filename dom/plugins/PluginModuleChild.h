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
 *   Ben Turner <bent.mozilla@gmail.com>.
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Chris Jones <jones.chris.g@gmail.com>
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

#ifndef dom_plugins_PluginModuleChild_h
#define dom_plugins_PluginModuleChild_h 1

#include <string>
#include <vector>

#include "base/basictypes.h"

#include "prlink.h"

#include "npapi.h"
#include "npfunctions.h"

#include "nsAutoPtr.h"
#include "nsDataHashtable.h"
#include "nsHashKeys.h"

#include "mozilla/plugins/PPluginModuleChild.h"
#include "mozilla/plugins/PluginInstanceChild.h"

// NOTE: stolen from nsNPAPIPlugin.h

/*
 * Use this macro before each exported function
 * (between the return address and the function
 * itself), to ensure that the function has the
 * right calling conventions on OS/2.
 */
#ifdef XP_OS2
#define NP_CALLBACK _System
#else
#define NP_CALLBACK
#endif

#if defined(XP_WIN)
#define NS_NPAPIPLUGIN_CALLBACK(_type, _name) _type (__stdcall * _name)
#elif defined(XP_OS2)
#define NS_NPAPIPLUGIN_CALLBACK(_type, _name) _type (_System * _name)
#else
#define NS_NPAPIPLUGIN_CALLBACK(_type, _name) _type (* _name)
#endif

typedef NS_NPAPIPLUGIN_CALLBACK(NPError, NP_GETENTRYPOINTS) (NPPluginFuncs* pCallbacks);
typedef NS_NPAPIPLUGIN_CALLBACK(NPError, NP_PLUGININIT) (const NPNetscapeFuncs* pCallbacks);
typedef NS_NPAPIPLUGIN_CALLBACK(NPError, NP_PLUGINUNIXINIT) (const NPNetscapeFuncs* pCallbacks, NPPluginFuncs* fCallbacks);
typedef NS_NPAPIPLUGIN_CALLBACK(NPError, NP_PLUGINSHUTDOWN) (void);
#ifdef XP_MACOSX
typedef NS_NPAPIPLUGIN_CALLBACK(NPError, NP_MAIN) (NPNetscapeFuncs* nCallbacks, NPPluginFuncs* pCallbacks, NPP_ShutdownProcPtr* unloadProcPtr);
#endif

#undef _MOZ_LOG
#define _MOZ_LOG(s)  printf("[NPAPIPluginChild] %s\n", s)

namespace mozilla {
namespace plugins {

class PluginScriptableObjectChild;

class PluginModuleChild : public PPluginModuleChild
{
protected:
    // Implement the PPluginModuleChild interface
    virtual bool AnswerNP_Initialize(NPError* rv);

    virtual PPluginInstanceChild*
    PPluginInstanceConstructor(const nsCString& aMimeType,
                               const uint16_t& aMode,
                               const nsTArray<nsCString>& aNames,
                               const nsTArray<nsCString>& aValues,
                               NPError* rv);

    virtual bool
    PPluginInstanceDestructor(PPluginInstanceChild* aActor,
                              NPError* rv);

    virtual bool
    AnswerPPluginInstanceConstructor(PPluginInstanceChild* aActor,
                                     const nsCString& aMimeType,
                                     const uint16_t& aMode,
                                     const nsTArray<nsCString>& aNames,
                                     const nsTArray<nsCString>& aValues,
                                     NPError* rv);

public:
    PluginModuleChild();
    virtual ~PluginModuleChild();

    bool Init(const std::string& aPluginFilename,
              MessageLoop* aIOLoop,
              IPC::Channel* aChannel);

    void CleanUp();

    static const NPNetscapeFuncs sBrowserFuncs;

    static PluginModuleChild* current();

    bool RegisterNPObject(NPObject* aObject,
                          PluginScriptableObjectChild* aActor);

    void UnregisterNPObject(NPObject* aObject);

    PluginScriptableObjectChild* GetActorForNPObject(NPObject* aObject);

private:
    bool InitGraphics();

    std::string mPluginFilename;
    PRLibrary* mLibrary;

    // we get this from the plugin
#ifdef OS_LINUX
    NP_PLUGINUNIXINIT mInitializeFunc;
#elif OS_WIN
    NP_PLUGININIT mInitializeFunc;
    NP_GETENTRYPOINTS mGetEntryPointsFunc;
#endif
    NP_PLUGINSHUTDOWN mShutdownFunc;
    NPPluginFuncs mFunctions;
    NPSavedData mSavedData;

    nsDataHashtable<nsVoidPtrHashKey, PluginScriptableObjectChild*> mObjectMap;
};

} /* namespace plugins */
} /* namespace mozilla */

#endif  // ifndef dom_plugins_PluginModuleChild_h
