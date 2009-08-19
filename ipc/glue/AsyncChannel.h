/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: sw=4 ts=4 et :
 */
/* ***** BEGIN LICENSE BLOCK *****
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

#ifndef ipc_glue_AsyncChannel_h
#define ipc_glue_AsyncChannel_h 1

#include "base/basictypes.h"
#include "base/message_loop.h"
#include "chrome/common/ipc_channel.h"


//-----------------------------------------------------------------------------
namespace {
enum Result {
    MsgProcessed,
    MsgNotKnown,
    MsgNotAllowed,
    MsgPayloadError,
    MsgRouteError,
    MsgValueError,
};
} // namespace {

namespace mozilla {
namespace ipc {

class AsyncChannel : public IPC::Channel::Listener
{
protected:
    enum ChannelState {
        ChannelClosed,
        ChannelOpening,
        ChannelConnected,
        ChannelError
    };

public:
    typedef IPC::Channel Transport;
    typedef IPC::Message Message;

    class /*NS_INTERFACE_CLASS*/ AsyncListener
    {
    public:
        virtual ~AsyncListener() { }
        virtual Result OnMessageReceived(const Message& aMessage) = 0;
    };

    AsyncChannel(AsyncListener* aListener) :
        mTransport(0),
        mListener(aListener),
        mChannelState(ChannelClosed),
        mIOLoop(),
        mWorkerLoop()
    {
    }

    virtual ~AsyncChannel()
    {
        if (mTransport)
            Close();
        mTransport = 0;
    }

    // Open  from the perspective of the RPC layer; the transport
    // should already be connected, or ready to connect.
    bool Open(Transport* aTransport, MessageLoop* aIOLoop=0);
    
    // Close from the perspective of the RPC layer; leaves the
    // underlying transport channel open, however.
    void Close();

    // Asynchronously send a message to the other side of the channel
    bool Send(Message* msg);

    // Implement the IPC::Channel::Listener interface
    virtual void OnMessageReceived(const Message& msg);
    virtual void OnChannelConnected(int32 peer_pid);
    virtual void OnChannelError();

protected:
    // Additional methods that execute on the worker thread
    void OnDispatchMessage(const Message& aMsg);

    // Additional methods that execute on the IO thread
    void OnChannelOpened();
    void OnSend(Message* aMsg);

    Transport* mTransport;
    AsyncListener* mListener;
    ChannelState mChannelState;
    MessageLoop* mIOLoop;       // thread where IO happens
    MessageLoop* mWorkerLoop;   // thread where work is done
};


} // namespace ipc
} // namespace mozilla
#endif  // ifndef ipc_glue_AsyncChannel_h
