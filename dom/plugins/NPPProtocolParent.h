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

#ifndef dom_plugins_NPPProtocolParent_h
#define dom_plugins_NPPProtocolParent_h 1

#include "mozilla/plugins/NPPProtocol.h"

#include "npapi.h"

#include "nsDebug.h"
#include "IPC/IPCMessageUtils.h"

#include "mozilla/ipc/RPCChannel.h"

#ifdef DEBUG
#  undef _MSG_LOG0
#  undef _MSG_LOG

#  define _MSG_LOG0(fmt, ...)                                       \
    do {                                                            \
        printf("[NPAPIProtocolParent] %s "fmt"\n", __VA_ARGS__);    \
    } while(0)

#  define _MSG_LOG(fmt, ...)                    \
    _MSG_LOG0(fmt, __FUNCTION__,## __VA_ARGS__)
#else
#  define _MSG_LOG(fmt, ...)
#endif

namespace mozilla {
namespace plugins {
//-----------------------------------------------------------------------------

class NS_FINAL_CLASS NPPProtocolParent :
        public NPPProtocol::Child,
        public mozilla::ipc::RPCChannel::Listener
{
private:
    typedef IPC::Message Message;
    typedef mozilla::ipc::RPCChannel RPCChannel;

public:
    NPPProtocolParent(NPPProtocol::Parent* aParent) :
        mParent(aParent)
    {

    }

    virtual ~NPPProtocolParent()
    {

    }

    // FIXME/cjones: how is this protocol opened/closed with respect
    // the higher level protocol?

    void SetChannel(RPCChannel* aRpc)
    {
        // FIXME also assert on channel state
        NS_ASSERTION(aRpc, "need a valid RPC channel");
        mRpc = aRpc;
    }

    void NextState(NPPProtocol::State aNextState)
    {
        // FIXME impl
    }

    // Implement the child interface
    virtual NPError NPP_SetWindow(XID aWindow,
                                  int32_t aWidth,
                                  int32_t aHeight)
    {
        _MSG_LOG("outcall NPP_SetWindow(%lx, %d, %d)",
                 aWindow, aWidth, aHeight);

        Message reply;

        mRpc->Call(new NPP_ParentToChildMsg_NPP_SetWindow(aWindow,
                                                          aWidth,
                                                          aHeight),
                   &reply);
        NS_ASSERTION(NPP_ChildToParentMsg_Reply_NPP_SetWindow__ID
                     == reply.type(),
                     "wrong reply msg to NPP_SetWindow()");

        NPError ret0;
        NS_ASSERTION(NPP_ChildToParentMsg_Reply_NPP_SetWindow
                     ::Read(&reply, &ret0),
                     "bad types in reply msg to NPP_SetWindow()");

        return ret0;
    }

    virtual Result OnCallReceived(const Message& msg, Message*& reply)
    {
        switch(msg.type()) {
        case NPP_ChildToParentMsg_NPN_GetValue__ID: {
            _MSG_LOG("incall NPN_GetValue()");

            // nothing to unpack
            mParent->NPN_GetValue();

            reply = new NPP_ParentToChildMsg_Reply_NPN_GetValue();
            return MsgProcessed;
        }

        default:
            return MsgNotKnown;
        }
    }

private:
    NPPProtocol::Parent* mParent;
    NPPProtocol::State mState;
    RPCChannel* mRpc;
};


} // namespace plugins
} // namespace mozilla

#endif // ifndef dom_plugins_NPPProtocolParent_h
