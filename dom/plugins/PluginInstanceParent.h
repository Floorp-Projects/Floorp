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

#ifndef dom_plugins_PluginInstanceParent_h
#define dom_plugins_PluginInstanceParent_h 1

#include "mozilla/plugins/PPluginInstanceParent.h"
#include "mozilla/plugins/PluginScriptableObjectParent.h"

#include "npfunctions.h"

#undef _MOZ_LOG
#define _MOZ_LOG(s) printf("[PluginInstanceParent] %s\n", s)

namespace mozilla {
namespace plugins {

class PBrowserStreamParent;
class BrowserStreamParent;

class PluginInstanceParent : public PPluginInstanceParent
{
    friend class PluginModuleParent;
    friend class BrowserStreamParent;

public:
    PluginInstanceParent(NPP npp, const NPNetscapeFuncs* npniface)
        : mNPP(npp)
        , mNPNIface(npniface)
    {
    }

    virtual ~PluginInstanceParent()
    {
    }
  
    virtual PPluginScriptableObjectParent*
    PPluginScriptableObjectConstructor();

    virtual nsresult
    PPluginScriptableObjectDestructor(PPluginScriptableObjectParent* aObject);

    virtual PBrowserStreamParent*
    PBrowserStreamConstructor(const nsCString& url,
                              const uint32_t& length,
                              const uint32_t& lastmodified,
                              const PStreamNotifyParent* notifyData,
                              const nsCString& headers,
                              const nsCString& mimeType,
                              const bool& seekable,
                              NPError* rv,
                              uint16_t *stype);

    virtual nsresult
    AnswerPBrowserStreamDestructor(PBrowserStreamParent* stream,
                                   const NPError& reason,
                                   const bool& artificial);

    virtual nsresult
    PBrowserStreamDestructor(PBrowserStreamParent* stream,
                             const NPError& reason,
                             const bool& artificial);

    virtual nsresult
    AnswerNPN_GetValue_NPNVjavascriptEnabledBool(bool* value, NPError* result);
    virtual nsresult
    AnswerNPN_GetValue_NPNVisOfflineBool(bool* value, NPError* result);
    virtual nsresult
    AnswerNPN_GetValue_NPNVWindowNPObject(
                                       PPluginScriptableObjectParent** value,
                                       NPError* result);
    virtual nsresult
    AnswerNPN_GetValue_NPNVPluginElementNPObject(
                                       PPluginScriptableObjectParent** value,
                                       NPError* result);
    virtual nsresult
    AnswerNPN_GetValue_NPNVprivateModeBool(bool* value, NPError* result);

    virtual nsresult
    AnswerNPN_GetURL(const nsCString& url, const nsCString& target,
                     NPError *result);

    virtual nsresult
    AnswerNPN_PostURL(const nsCString& url, const nsCString& target,
                      const nsCString& buffer, const bool& file,
                      NPError* result);

    virtual PStreamNotifyParent*
    PStreamNotifyConstructor(const nsCString& url, const nsCString& target,
                             const bool& post, const nsCString& buffer,
                             const bool& file,
                             NPError* result);

    virtual nsresult
    PStreamNotifyDestructor(PStreamNotifyParent* notifyData,
                            const NPReason& reason);

    NPError NPP_SetWindow(NPWindow* aWindow);
    NPError NPP_GetValue(NPPVariable variable, void *ret_value);

    NPError NPP_SetValue(NPNVariable variable, void *value)
    {
        _MOZ_LOG(__FUNCTION__);
        return 1;
    }


    NPError NPP_NewStream(NPMIMEType type, NPStream* stream,
                          NPBool seekable, uint16_t* stype);
    NPError NPP_DestroyStream(NPStream* stream, NPReason reason);

    void NPP_Print(NPPrint* platformPrint)
    {
        _MOZ_LOG(__FUNCTION__);
    }

    int16_t NPP_HandleEvent(void* event)
    {
        _MOZ_LOG(__FUNCTION__);
        return 0;
    }

    void NPP_URLNotify(const char* url, NPReason reason, void* notifyData)
    {
        _MOZ_LOG(__FUNCTION__);
    }

private:
    NPP mNPP;
    const NPNetscapeFuncs* mNPNIface;
};


} // namespace plugins
} // namespace mozilla

#endif // ifndef dom_plugins_PluginInstanceParent_h
