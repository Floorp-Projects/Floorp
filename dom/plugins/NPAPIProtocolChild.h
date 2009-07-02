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

#ifndef dom_plugins_NPAPIProtocolChildImpl_h
#define dom_plugins_NPAPIProtocolChildImpl_h 1

#include "chrome/common/ipc_channel.h"
#include "chrome/common/ipc_message.h"

#include "mozilla/ipc/RPCChannel.h"
#include "mozilla/plugins/NPAPIProtocol.h"

// FIXME/cjones: HACK ALERT!!!
#include "mozilla/plugins/NPPProtocolChild.h"


#ifdef DEBUG
#  undef _MSG_LOG0
#  undef _MSG_LOG

#  define _MSG_LOG0(fmt, ...)                                       \
    do {                                                            \
        printf("[NPAPIProtocolChild] %s "fmt"\n", __VA_ARGS__);     \
    } while(0)

#  define _MSG_LOG(fmt, ...)                    \
    _MSG_LOG0(fmt, __FUNCTION__,## __VA_ARGS__)
#else
#  define _MSG_LOG(fmt, ...)
#endif

namespace mozilla {
namespace plugins {
//-----------------------------------------------------------------------------

/**
 * The child-side of the protocol.  Inheriting from ::Parent looks
 * odd, but remember that this is the interface that code in the
 * child process sees.
 */
class NS_FINAL_CLASS NPAPIProtocolChild : 
        public NPAPIProtocol::Parent,
        public mozilla::ipc::RPCChannel::Listener
{
private:
    typedef IPC::Message Message;
    typedef mozilla::ipc::RPCChannel RPCChannel;

public:
    NPAPIProtocolChild(NPAPIProtocol::Child* aChild) :
        mChild(aChild),
        mRpc(this)
    {
    }

    ~NPAPIProtocolChild()
    {
    }

    bool Open(IPC::Channel* aChannel, MessageLoop* aIOLoop)
    {
        return mRpc.Open(aChannel, aIOLoop);
    }

    void Close()
    {
        mRpc.Close();
    }

    void NextState(NPAPIProtocol::State aNextState)
    {
        
    }

    // Implement the parent interface
    virtual void NPN_GetValue(/*...*/)
    {
        _MSG_LOG("outcall NPN_GetValue()");
        // mRpc->Send(...);
    }
    // ...

    // Implement the RPCChannel::Listener interface
    virtual Result OnCallReceived(const Message& msg, Message*& reply)
    {
        switch(msg.type()) {
        case NPAPI_ParentToChildMsg_NP_Initialize__ID: {
            _MSG_LOG("incall NP_Initialize()");

            NPError val0 = mChild->NP_Initialize();
            reply = new NPAPI_ChildToParentMsg_Reply_NP_Initialize(val0);
            reply->set_reply();
            return MsgProcessed;
        }

        case NPAPI_ParentToChildMsg_NPP_New__ID: {
            mozilla::ipc::String aMimeType;
            int aHandle;
            uint16_t aMode;
            mozilla::ipc::StringArray aNames;
            mozilla::ipc::StringArray aValues;
            
            NPAPI_ParentToChildMsg_NPP_New::Param p;
            NS_ASSERTION(NPAPI_ParentToChildMsg_NPP_New::Read(&msg, &p),
                         "bad types in NPP_New() msg");
            aMimeType = p.a;
            aHandle = p.b;
            aMode = p.c;
            aNames = p.d;
            aValues = p.e;

            _MSG_LOG("incall NPP_New(%s, %d, %d, <vec>, <vec>)",
                     aMimeType.c_str(), aHandle, aMode);

            NPError val0 = mChild->NPP_New(aMimeType,
                                           aHandle,
                                           aMode,
                                           aNames,
                                           aValues);
            reply = new NPAPI_ChildToParentMsg_Reply_NPP_New(val0);
            reply->set_reply();
            return MsgProcessed;
        }

        default:
            // FIXME/cjones: HACK ALERT! use routing, do checks, etc.
            return HACK_npp->OnCallReceived(msg, reply);
            
            
//            return MsgNotKnown;
        }
    }

    // FIXME/cjones: kill this and manage through NPAPI
    RPCChannel* HACK_getchannel_please() { return &mRpc; }
    NPPProtocolChild* HACK_npp;
    // FIXME


private:
    NPAPIProtocol::Child* mChild;
    mozilla::ipc::RPCChannel mRpc;
};


} // namespace plugins
} // namespace mozilla

#endif  // ifndef dom_plugins_NPAPIProtocolChildImpl_h
