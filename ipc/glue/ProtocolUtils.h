/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: sw=4 ts=4 et :
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_ipc_ProtocolUtils_h
#define mozilla_ipc_ProtocolUtils_h 1

#include "base/process.h"
#include "base/process_util.h"
#include "chrome/common/ipc_message_utils.h"

#include "prenv.h"

#include "IPCMessageStart.h"
#include "mozilla/ipc/Shmem.h"
#include "mozilla/ipc/Transport.h"

// WARNING: this takes into account the private, special-message-type
// enum in ipc_channel.h.  They need to be kept in sync.
namespace {
// XXX the max message ID is actually kuint32max now ... when this
// changed, the assumptions of the special message IDs changed in that
// they're not carving out messages from likely-unallocated space, but
// rather carving out messages from the end of space allocated to
// protocol 0.  Oops!  We can get away with this until protocol 0
// starts approaching its 65,536th message.
enum {
    CHANNEL_OPENED_MESSAGE_TYPE = kuint16max - 6,
    SHMEM_DESTROYED_MESSAGE_TYPE = kuint16max - 5,
    UNBLOCK_CHILD_MESSAGE_TYPE = kuint16max - 4,
    BLOCK_CHILD_MESSAGE_TYPE   = kuint16max - 3,
    SHMEM_CREATED_MESSAGE_TYPE = kuint16max - 2,
    GOODBYE_MESSAGE_TYPE       = kuint16max - 1
};
}

namespace mozilla {
namespace ipc {

class AsyncChannel;

// Used to pass references to protocol actors across the wire.
// Actors created on the parent-side have a positive ID, and actors
// allocated on the child side have a negative ID.
struct ActorHandle
{
    int mId;
};

// Used internally to represent a "trigger" that might cause a state
// transition.  Triggers are normalized across parent+child to Send
// and Recv (instead of child-in, child-out, parent-in, parent-out) so
// that they can share the same state machine implementation.  To
// further normalize, |Send| is used for 'call', |Recv| for 'answer'.
struct Trigger
{
    enum Action { Send, Recv };

    Trigger(Action action, int32 msg) :
        mAction(action),
        mMsg(msg)
    {}

    Action mAction;
    int32 mMsg;
};

template<class ListenerT>
class /*NS_INTERFACE_CLASS*/ IProtocolManager
{
public:
    enum ActorDestroyReason {
        FailedConstructor,
        Deletion,
        AncestorDeletion,
        NormalShutdown,
        AbnormalShutdown
    };

    typedef base::ProcessHandle ProcessHandle;

    virtual int32 Register(ListenerT*) = 0;
    virtual int32 RegisterID(ListenerT*, int32) = 0;
    virtual ListenerT* Lookup(int32) = 0;
    virtual void Unregister(int32) = 0;
    virtual void RemoveManagee(int32, ListenerT*) = 0;

    virtual Shmem::SharedMemory* CreateSharedMemory(
        size_t, SharedMemory::SharedMemoryType, bool, int32*) = 0;
    virtual bool AdoptSharedMemory(Shmem::SharedMemory*, int32*) = 0;
    virtual Shmem::SharedMemory* LookupSharedMemory(int32) = 0;
    virtual bool IsTrackingSharedMemory(Shmem::SharedMemory*) = 0;
    virtual bool DestroySharedMemory(Shmem&) = 0;

    // XXX odd ducks, acknowledged
    virtual ProcessHandle OtherProcess() const = 0;
    virtual AsyncChannel* GetIPCChannel() = 0;
};


inline bool
LoggingEnabled()
{
#if defined(DEBUG)
    return !!PR_GetEnv("MOZ_IPC_MESSAGE_LOG");
#else
    return false;
#endif
}

inline void
ProtocolErrorBreakpoint(const char* aMsg)
{
#if defined(DEBUG)
    printf("Protocol error: %s\n", aMsg);
#endif
}

typedef IPCMessageStart ProtocolId;

struct PrivateIPDLInterface {};

bool
Bridge(const PrivateIPDLInterface&,
       AsyncChannel*, base::ProcessHandle, AsyncChannel*, base::ProcessHandle,
       ProtocolId);

bool
Open(const PrivateIPDLInterface&,
     AsyncChannel*, base::ProcessHandle, Transport::Mode,
     ProtocolId);

bool
UnpackChannelOpened(const PrivateIPDLInterface&,
                    const IPC::Message&,
                    TransportDescriptor*, base::ProcessId*, ProtocolId*);

} // namespace ipc
} // namespace mozilla


namespace IPC {

template <>
struct ParamTraits<mozilla::ipc::ActorHandle>
{
    typedef mozilla::ipc::ActorHandle paramType;

    static void Write(Message* aMsg, const paramType& aParam)
    {
        IPC::WriteParam(aMsg, aParam.mId);
    }

    static bool Read(const Message* aMsg, void** aIter, paramType* aResult)
    {
        int id;
        if (IPC::ReadParam(aMsg, aIter, &id)) {
            aResult->mId = id;
            return true;
        }
        return false;
    }

    static void Log(const paramType& aParam, std::wstring* aLog)
    {
        aLog->append(StringPrintf(L"(%d)", aParam.mId));
    }
};

} // namespace IPC


#endif  // mozilla_ipc_ProtocolUtils_h
