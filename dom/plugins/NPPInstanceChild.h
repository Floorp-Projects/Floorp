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

#ifndef dom_plugins_NPPInstanceChild_h
#define dom_plugins_NPPInstanceChild_h 1

#include "mozilla/plugins/NPPProtocolChild.h"
#include "mozilla/plugins/NPObjectChild.h"

#include "npfunctions.h"

#undef _MOZ_LOG
#define _MOZ_LOG(s) printf("[NPPInstanceChild] %s\n", s)

namespace mozilla {
namespace plugins {

class NPPInstanceChild : public NPPProtocolChild
{
#ifdef OS_WIN
    friend LRESULT CALLBACK PluginWindowProc(HWND hWnd,
                                             UINT message,
                                             WPARAM wParam,
                                             LPARAM lParam);
#endif

protected:
    friend class NPBrowserStreamChild;

    virtual nsresult AnswerNPP_SetWindow(const NPWindow& window, NPError* rv);

    virtual nsresult AnswerNPP_GetValue(const nsString& key, nsString* value);

    virtual NPObjectProtocolChild*
    NPObjectConstructor(NPError* _retval);

    virtual nsresult
    NPObjectDestructor(NPObjectProtocolChild* aObject,
                       NPError* _retval);


    virtual NPBrowserStreamProtocolChild*
    NPBrowserStreamConstructor(const nsCString& url, const uint32_t& length,
                               const uint32_t& lastmodified,
                               const nsCString& headers,
                               const nsCString& mimeType, const bool& seekable,
                               NPError* rv, uint16_t *stype);

    virtual nsresult
    NPBrowserStreamDestructor(NPBrowserStreamProtocolChild* stream,
                              const NPError& reason, const bool& artificial);

public:
    NPPInstanceChild(const NPPluginFuncs* aPluginIface) :
        mPluginIface(aPluginIface)
#if defined(OS_LINUX)
        , mPlug(0)
#elif defined(OS_WIN)
        , mPluginWindowHWND(0)
        , mPluginWndProc(0)
        , mPluginParentHWND(0)
#endif
    {
        memset(&mWindow, 0, sizeof(mWindow));
        mData.ndata = (void*) this;
#if defined(OS_LINUX)
        memset(&mWsInfo, 0, sizeof(mWsInfo));
#endif
    }

    virtual ~NPPInstanceChild();

    bool Initialize();

    NPP GetNPP()
    {
        return &mData;
    }

    NPError NPN_GetValue(NPNVariable aVariable, void* aValue);

private:

#if defined(OS_WIN)
    static bool RegisterWindowClass();
    bool CreatePluginWindow();
    void DestroyPluginWindow();
    void ReparentPluginWindow(HWND hWndParent);
    void SizePluginWindow(int width, int height);
    static LRESULT CALLBACK DummyWindowProc(HWND hWnd,
                                            UINT message,
                                            WPARAM wParam,
                                            LPARAM lParam);
    static LRESULT CALLBACK PluginWindowProc(HWND hWnd,
                                             UINT message,
                                             WPARAM wParam,
                                             LPARAM lParam);
#endif

    const NPPluginFuncs* mPluginIface;
    NPP_t mData;
#ifdef OS_LINUX
    GtkWidget* mPlug;
#endif
    NPWindow mWindow;
#ifdef OS_LINUX
    NPSetWindowCallbackStruct mWsInfo;
#elif defined(OS_WIN)
    HWND mPluginWindowHWND;
    WNDPROC mPluginWndProc;
    HWND mPluginParentHWND;
#endif
};

} // namespace plugins
} // namespace mozilla

#endif // ifndef dom_plugins_NPPInstanceChild_h
