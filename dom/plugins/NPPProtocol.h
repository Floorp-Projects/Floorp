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

#ifndef dom_plugins_NPPProtocol_h
#define dom_plugins_NPPProtocol_h 1

// standard includes
#include "IPC/IPCMessageUtils.h"
#include "mozilla/ipc/MessageTypes.h"

// extra includes
#include "npapi.h"
#include "X11/X.h"

namespace mozilla {
namespace plugins {
//-----------------------------------------------------------------------------

class NS_FINAL_CLASS NPPProtocol
{
public:
    /*interface*/ class Parent
    {
    public:
        virtual void NPN_GetValue() = 0;

    protected:
        Parent() { }
        virtual ~Parent() { }
        Parent(const Parent&);
        Parent& operator=(const Parent&);
    };

    /*interface*/ class Child
    {
    public:
        virtual NPError NPP_SetWindow(XID aWindow,
                                      int32_t aWidth,
                                      int32_t aHeight) = 0;

    protected:
        Child() { }
        virtual ~Child() { }
        Child(const Child&);
        Child& operator=(const Child&);
    };

    enum State {
        StateStart = 0,


        // placeholder
        StateLast
    };

private:
    NPPProtocol();
    virtual ~NPPProtocol() = 0;
    NPPProtocol& operator=(const NPPProtocol&);
    // FIXME/cjones state to allow dynamic checking?
};

//-----------------------------------------------------------------------------
// Start NPP protocol messages
//

// messages from parent->child
enum NPP_ParentToChildMsgType {
    NPP_ParentToChildStart = NPP_ParentToChildMsgStart << 12,
    NPP_ParentToChildPreStart = (NPP_ParentToChildMsgStart << 12) - 1,

    NPP_ParentToChildMsg_NPP_SetWindow__ID,

    NPP_ParentToChildMsg_Reply_NPN_GetValue__ID,

    NPP_ParentToChildEnd
};

class NPP_ParentToChildMsg_NPP_SetWindow :
        public IPC::MessageWithTuple<
    Tuple3<XID, int32_t, int32_t> >
{
public:
    enum { ID = NPP_ParentToChildMsg_NPP_SetWindow__ID };
    NPP_ParentToChildMsg_NPP_SetWindow(const XID& window,
                                       const int32_t& width,
                                       const int32_t& height) :
        IPC::MessageWithTuple<
        Tuple3<XID, int32_t, int32_t> >(
            MSG_ROUTING_CONTROL, ID, MakeTuple(window, width, height))
    {}
};

class NPP_ParentToChildMsg_Reply_NPN_GetValue : public IPC::Message
{
public:
    enum { ID = NPP_ParentToChildMsg_Reply_NPN_GetValue__ID };
    NPP_ParentToChildMsg_Reply_NPN_GetValue() :
        IPC::Message(MSG_ROUTING_CONTROL, ID, PRIORITY_NORMAL)
    {}
};


// messages from child->parent
enum NPP_ChildToParentMsgType {
    NPP_ChildToParentStart = NPP_ChildToParentMsgStart << 12,
    NPP_ChildToParentPreStart = (NPP_ChildToParentMsgStart << 12) - 1,

    NPP_ChildToParentMsg_NPN_GetValue__ID,

    NPP_ChildToParentMsg_Reply_NPP_SetWindow__ID,

    NPP_ChildToParentEnd
};

class NPP_ChildToParentMsg_NPN_GetValue : public IPC::Message
{
public:
    enum { ID = NPP_ChildToParentMsg_NPN_GetValue__ID };
    NPP_ChildToParentMsg_NPN_GetValue() :
        IPC::Message(MSG_ROUTING_CONTROL, ID, PRIORITY_NORMAL)
    {}
};

class NPP_ChildToParentMsg_Reply_NPP_SetWindow :
        public IPC::MessageWithTuple<NPError>
{
public:
    enum { ID = NPP_ChildToParentMsg_Reply_NPP_SetWindow__ID };
    NPP_ChildToParentMsg_Reply_NPP_SetWindow(const NPError& arg1) :
        IPC::MessageWithTuple<NPError>(MSG_ROUTING_CONTROL, ID, arg1)
    {}
};


} // namespace plugins
} // namespace mozilla

#endif // ifndef dom_plugins_NPPProtocol_h
