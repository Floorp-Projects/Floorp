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
 * The Original Code is Mozilla IPC.
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

#ifndef mozilla_ipc_ProtocolUtils_h
#define mozilla_ipc_ProtocolUtils_h 1

#include "base/process.h"
#include "base/process_util.h"
#include "chrome/common/ipc_message_utils.h"

#include "prenv.h"

#include "mozilla/ipc/Shmem.h"

// WARNING: this takes into account the private, special-message-type
// enum in ipc_channel.h.  They need to be kept in sync.
namespace {
enum {
    SHMEM_DESTROYED_MESSAGE_TYPE = kuint16max - 5,
    UNBLOCK_CHILD_MESSAGE_TYPE = kuint16max - 4,
    BLOCK_CHILD_MESSAGE_TYPE   = kuint16max - 3,
    SHMEM_CREATED_MESSAGE_TYPE = kuint16max - 2,
    GOODBYE_MESSAGE_TYPE       = kuint16max - 1,
};
}

namespace mozilla {
namespace ipc {


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

    // XXX odd duck, acknowledged
    virtual ProcessHandle OtherProcess() const = 0;
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
