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
#include "mozilla/Attributes.h"
#include "mozilla/ipc/FileDescriptor.h"
#include "mozilla/ipc/Shmem.h"
#include "mozilla/ipc/Transport.h"
#include "mozilla/ipc/MessageLink.h"
#include "mozilla/LinkedList.h"
#include "mozilla/Maybe.h"
#include "mozilla/Mutex.h"
#include "MainThreadUtils.h"

#if defined(ANDROID) && defined(DEBUG)
#include <android/log.h>
#endif

template<typename T> class nsTHashtable;
template<typename T> class nsPtrHashKey;

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
    SHMEM_CREATED_MESSAGE_TYPE = kuint16max - 4,
    GOODBYE_MESSAGE_TYPE       = kuint16max - 3,
    CANCEL_MESSAGE_TYPE        = kuint16max - 2,

    // kuint16max - 1 is used by ipc_channel.h.
};

} // namespace

namespace mozilla {
namespace dom {
class ContentParent;
} // namespace dom

namespace net {
class NeckoParent;
} // namespace net

namespace ipc {

#ifdef XP_WIN
const base::ProcessHandle kInvalidProcessHandle = INVALID_HANDLE_VALUE;

// In theory, on Windows, this is a valid process ID, but in practice they are
// currently divisible by four. Process IDs share the kernel handle allocation
// code and they are guaranteed to be divisible by four.
// As this could change for process IDs we shouldn't generally rely on this
// property, however even if that were to change, it seems safe to rely on this
// particular value never being used.
const base::ProcessId kInvalidProcessId = kuint32max;
#else
const base::ProcessHandle kInvalidProcessHandle = -1;
const base::ProcessId kInvalidProcessId = -1;
#endif

// Scoped base::ProcessHandle to ensure base::CloseProcessHandle is called.
struct ScopedProcessHandleTraits
{
  typedef base::ProcessHandle type;

  static type empty()
  {
    return kInvalidProcessHandle;
  }

  static void release(type aProcessHandle)
  {
    if (aProcessHandle && aProcessHandle != kInvalidProcessHandle) {
      base::CloseProcessHandle(aProcessHandle);
    }
  }
};
typedef mozilla::Scoped<ScopedProcessHandleTraits> ScopedProcessHandle;

class ProtocolFdMapping;
class ProtocolCloneContext;

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

    Trigger(Action action, int32_t msg) :
        mAction(action),
        mMsg(msg)
    {}

    Action mAction;
    int32_t mMsg;
};

class ProtocolCloneContext
{
  typedef mozilla::dom::ContentParent ContentParent;
  typedef mozilla::net::NeckoParent NeckoParent;

  RefPtr<ContentParent> mContentParent;
  NeckoParent* mNeckoParent;

public:
  ProtocolCloneContext();

  ~ProtocolCloneContext();

  void SetContentParent(ContentParent* aContentParent);

  ContentParent* GetContentParent() { return mContentParent; }

  void SetNeckoParent(NeckoParent* aNeckoParent)
  {
    mNeckoParent = aNeckoParent;
  }

  NeckoParent* GetNeckoParent() { return mNeckoParent; }
};

template<class ListenerT>
class IProtocolManager
{
public:
    enum ActorDestroyReason {
        FailedConstructor,
        Deletion,
        AncestorDeletion,
        NormalShutdown,
        AbnormalShutdown
    };

    typedef base::ProcessId ProcessId;

    virtual int32_t Register(ListenerT*) = 0;
    virtual int32_t RegisterID(ListenerT*, int32_t) = 0;
    virtual ListenerT* Lookup(int32_t) = 0;
    virtual void Unregister(int32_t) = 0;
    virtual void RemoveManagee(int32_t, ListenerT*) = 0;

    virtual Shmem::SharedMemory* CreateSharedMemory(
        size_t, SharedMemory::SharedMemoryType, bool, int32_t*) = 0;
    virtual Shmem::SharedMemory* LookupSharedMemory(int32_t) = 0;
    virtual bool IsTrackingSharedMemory(Shmem::SharedMemory*) = 0;
    virtual bool DestroySharedMemory(Shmem&) = 0;

    // XXX odd ducks, acknowledged
    virtual ProcessId OtherPid() const = 0;
    virtual MessageChannel* GetIPCChannel() = 0;

    // The implementation of function is generated by code generator.
    virtual void CloneManagees(ListenerT* aSource,
                               ProtocolCloneContext* aCtx) = 0;

    Maybe<ListenerT*> ReadActor(const IPC::Message* aMessage, PickleIterator* aIter, bool aNullable,
                                const char* aActorDescription, int32_t aProtocolTypeId);
};

typedef IPCMessageStart ProtocolId;

/**
 * All RPC protocols should implement this interface.
 */
class IProtocol : public MessageListener
{
public:
    /**
     * This function is used to clone this protocol actor.
     *
     * see IProtocol::CloneProtocol()
     */
    virtual IProtocol*
    CloneProtocol(MessageChannel* aChannel,
                  ProtocolCloneContext* aCtx) = 0;
};

template<class PFooSide>
class Endpoint;

/**
 * All top-level protocols should inherit this class.
 *
 * IToplevelProtocol tracks all top-level protocol actors created from
 * this protocol actor.
 */
class IToplevelProtocol : private LinkedListElement<IToplevelProtocol>
{
    friend class LinkedList<IToplevelProtocol>;
    friend class LinkedListElement<IToplevelProtocol>;

    template<class PFooSide> friend class Endpoint;

protected:
    explicit IToplevelProtocol(ProtocolId aProtoId);
    ~IToplevelProtocol();

    /**
     * Add an actor to the list of actors that have been opened by this
     * protocol.
     */
    void AddOpenedActor(IToplevelProtocol* aActor);

public:
    void SetTransport(Transport* aTrans)
    {
        mTrans = aTrans;
    }

    Transport* GetTransport() const { return mTrans; }

    ProtocolId GetProtocolId() const { return mProtocolId; }

    void GetOpenedActors(nsTArray<IToplevelProtocol*>& aActors);

    virtual MessageChannel* GetIPCChannel() = 0;

    // This Unsafe version should only be used when all other threads are
    // frozen, since it performs no locking. It also takes a stack-allocated
    // array and its size (number of elements) rather than an nsTArray. The Nuwa
    // code that calls this function is not allowed to allocate memory.
    size_t GetOpenedActorsUnsafe(IToplevelProtocol** aActors, size_t aActorsMax);

    virtual IToplevelProtocol*
    CloneToplevel(const InfallibleTArray<ProtocolFdMapping>& aFds,
                  base::ProcessHandle aPeerProcess,
                  ProtocolCloneContext* aCtx);

    void CloneOpenedToplevels(IToplevelProtocol* aTemplate,
                              const InfallibleTArray<ProtocolFdMapping>& aFds,
                              base::ProcessHandle aPeerProcess,
                              ProtocolCloneContext* aCtx);

private:
    void AddOpenedActorLocked(IToplevelProtocol* aActor);
    void GetOpenedActorsLocked(nsTArray<IToplevelProtocol*>& aActors);

    LinkedList<IToplevelProtocol> mOpenActors; // All protocol actors opened by this.
    IToplevelProtocol* mOpener;

    ProtocolId mProtocolId;
    Transport* mTrans;
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

inline bool
LoggingEnabledFor(const char *aTopLevelProtocol)
{
#if defined(DEBUG)
    const char *filter = PR_GetEnv("MOZ_IPC_MESSAGE_LOG");
    if (!filter) {
        return false;
    }
    return strcmp(filter, "1") == 0 || strcmp(filter, aTopLevelProtocol) == 0;
#else
    return false;
#endif
}

enum class MessageDirection {
    eSending,
    eReceiving,
};

MOZ_NEVER_INLINE void
LogMessageForProtocol(const char* aTopLevelProtocol, base::ProcessId aOtherPid,
                      const char* aContextDescription,
                      const char* aMessageDescription,
                      MessageDirection aDirection);

MOZ_NEVER_INLINE void
ProtocolErrorBreakpoint(const char* aMsg);

// The code generator calls this function for errors which come from the
// methods of protocols.  Doing this saves codesize by making the error
// cases significantly smaller.
MOZ_NEVER_INLINE void
FatalError(const char* aProtocolName, const char* aMsg, bool aIsParent);

// The code generator calls this function for errors which are not
// protocol-specific: errors in generated struct methods or errors in
// transition functions, for instance.  Doing this saves codesize by
// by making the error cases significantly smaller.
MOZ_NEVER_INLINE void
LogicError(const char* aMsg);

MOZ_NEVER_INLINE void
ActorIdReadError(const char* aActorDescription);

MOZ_NEVER_INLINE void
BadActorIdError(const char* aActorDescription);

MOZ_NEVER_INLINE void
ActorLookupError(const char* aActorDescription);

MOZ_NEVER_INLINE void
MismatchedActorTypeError(const char* aActorDescription);

MOZ_NEVER_INLINE void
UnionTypeReadError(const char* aUnionName);

MOZ_NEVER_INLINE void
ArrayLengthReadError(const char* aElementName);

struct PrivateIPDLInterface {};

nsresult
Bridge(const PrivateIPDLInterface&,
       MessageChannel*, base::ProcessId, MessageChannel*, base::ProcessId,
       ProtocolId, ProtocolId);

bool
Open(const PrivateIPDLInterface&,
     MessageChannel*, base::ProcessId, Transport::Mode,
     ProtocolId, ProtocolId);

bool
UnpackChannelOpened(const PrivateIPDLInterface&,
                    const IPC::Message&,
                    TransportDescriptor*, base::ProcessId*, ProtocolId*);

template<typename ListenerT>
Maybe<ListenerT*>
IProtocolManager<ListenerT>::ReadActor(const IPC::Message* aMessage, PickleIterator* aIter, bool aNullable,
                                       const char* aActorDescription, int32_t aProtocolTypeId)
{
    int32_t id;
    if (!IPC::ReadParam(aMessage, aIter, &id)) {
        ActorIdReadError(aActorDescription);
        return Nothing();
    }

    if (id == 1 || (id == 0 && !aNullable)) {
        BadActorIdError(aActorDescription);
        return Nothing();
    }

    if (id == 0) {
        return Some(static_cast<ListenerT*>(nullptr));
    }

    ListenerT* listener = this->Lookup(id);
    if (!listener) {
        ActorLookupError(aActorDescription);
        return Nothing();
    }

    if (static_cast<MessageListener*>(listener)->GetProtocolTypeId() != aProtocolTypeId) {
        MismatchedActorTypeError(aActorDescription);
        return Nothing();
    }

    return Some(listener);
}

#if defined(XP_WIN)
// This is a restricted version of Windows' DuplicateHandle() function
// that works inside the sandbox and can send handles but not retrieve
// them.  Unlike DuplicateHandle(), it takes a process ID rather than
// a process handle.  It returns true on success, false otherwise.
bool
DuplicateHandle(HANDLE aSourceHandle,
                DWORD aTargetProcessId,
                HANDLE* aTargetHandle,
                DWORD aDesiredAccess,
                DWORD aOptions);
#endif

/**
 * Annotate the crash reporter with the error code from the most recent system
 * call. Returns the system error.
 */
#ifdef MOZ_CRASHREPORTER
void AnnotateSystemError();
void AnnotateProcessInformation(base::ProcessId aPid);
#else
#define AnnotateSystemError() do { } while (0)
#define AnnotateProcessInformation(...) do { } while (0)
#endif

/**
 * An endpoint represents one end of a partially initialized IPDL channel. To
 * set up a new top-level protocol:
 *
 * Endpoint<PFooParent> parentEp;
 * Endpoint<PFooChild> childEp;
 * nsresult rv;
 * rv = PFoo::CreateEndpoints(parentPid, childPid, &parentEp, &childEp);
 *
 * You're required to pass in parentPid and childPid, which are the pids of the
 * processes in which the parent and child endpoints will be used.
 *
 * Endpoints can be passed in IPDL messages or sent to other threads using
 * PostTask. Once an Endpoint has arrived at its destination process and thread,
 * you need to create the top-level actor and bind it to the endpoint:
 *
 * FooParent* parent = new FooParent();
 * bool rv1 = parentEp.Bind(parent, processActor);
 * bool rv2 = parent->SendBar(...);
 *
 * (See Bind below for an explanation of processActor.) Once the actor is bound
 * to the endpoint, it can send and receive messages.
 */
template<class PFooSide>
class Endpoint
{
public:
    typedef base::ProcessId ProcessId;

    Endpoint()
      : mValid(false)
    {}

    Endpoint(const PrivateIPDLInterface&,
             mozilla::ipc::Transport::Mode aMode,
             TransportDescriptor aTransport,
             ProcessId aMyPid,
             ProcessId aOtherPid,
             ProtocolId aProtocolId)
      : mValid(true)
      , mMode(aMode)
      , mTransport(aTransport)
      , mMyPid(aMyPid)
      , mOtherPid(aOtherPid)
      , mProtocolId(aProtocolId)
    {}

    Endpoint(Endpoint&& aOther)
      : mValid(aOther.mValid)
      , mMode(aOther.mMode)
      , mTransport(aOther.mTransport)
      , mMyPid(aOther.mMyPid)
      , mOtherPid(aOther.mOtherPid)
      , mProtocolId(aOther.mProtocolId)
    {
        aOther.mValid = false;
    }

    Endpoint& operator=(Endpoint&& aOther)
    {
        mValid = aOther.mValid;
        mMode = aOther.mMode;
        mTransport = aOther.mTransport;
        mMyPid = aOther.mMyPid;
        mOtherPid = aOther.mOtherPid;
        mProtocolId = aOther.mProtocolId;

        aOther.mValid = false;
        return *this;
    }

    ~Endpoint() {
        if (mValid) {
            CloseDescriptor(mTransport);
        }
    }

    // This method binds aActor to this endpoint. After this call, the actor can
    // be used to send and receive messages. The endpoint becomes invalid. The
    // |aProcessActor| parameter is used to associate protocols with content
    // processes. In practice, this parameter should always be a ContentParent
    // or ContentChild, depending on which process you are in. It is used to
    // find all the channels that need to be "frozen" or "revived" when creating
    // or cloning the Nuwa process.
    bool Bind(PFooSide* aActor, IToplevelProtocol* aProcessActor)
    {
        MOZ_RELEASE_ASSERT(mValid);
        MOZ_RELEASE_ASSERT(mMyPid == base::GetCurrentProcId());

        Transport* t = mozilla::ipc::OpenDescriptor(mTransport, mMode);
        if (!t) {
            return false;
        }
        if (!aActor->Open(t, mOtherPid, XRE_GetIOMessageLoop(),
                          mMode == Transport::MODE_SERVER ? ParentSide : ChildSide)) {
            return false;
        }
        mValid = false;
        aActor->SetTransport(t);
        if (aProcessActor) {
            aProcessActor->AddOpenedActor(aActor);
        }
        return true;
    }


private:
    friend struct IPC::ParamTraits<Endpoint<PFooSide>>;

    Endpoint(const Endpoint&) = delete;
    Endpoint& operator=(const Endpoint&) = delete;

    bool mValid;
    mozilla::ipc::Transport::Mode mMode;
    TransportDescriptor mTransport;
    ProcessId mMyPid, mOtherPid;
    ProtocolId mProtocolId;
};

// This function is used internally to create a pair of Endpoints. See the
// comment above Endpoint for a description of how it might be used.
template<class PFooParent, class PFooChild>
nsresult
CreateEndpoints(const PrivateIPDLInterface& aPrivate,
                base::ProcessId aParentDestPid,
                base::ProcessId aChildDestPid,
                ProtocolId aProtocol,
                ProtocolId aChildProtocol,
                Endpoint<PFooParent>* aParentEndpoint,
                Endpoint<PFooChild>* aChildEndpoint)
{
  MOZ_RELEASE_ASSERT(aParentDestPid);
  MOZ_RELEASE_ASSERT(aChildDestPid);

  TransportDescriptor parentTransport, childTransport;
  nsresult rv;
  if (NS_FAILED(rv = CreateTransport(aParentDestPid, &parentTransport, &childTransport))) {
    return rv;
  }

  *aParentEndpoint = Endpoint<PFooParent>(aPrivate, mozilla::ipc::Transport::MODE_SERVER,
                                          parentTransport, aParentDestPid, aChildDestPid, aProtocol);

  *aChildEndpoint = Endpoint<PFooChild>(aPrivate, mozilla::ipc::Transport::MODE_CLIENT,
                                        childTransport, aChildDestPid, aParentDestPid, aChildProtocol);

  return NS_OK;
}

} // namespace ipc

template<typename Protocol>
using ManagedContainer = nsTHashtable<nsPtrHashKey<Protocol>>;

template<typename Protocol>
Protocol*
LoneManagedOrNullAsserts(const ManagedContainer<Protocol>& aManagees)
{
    if (aManagees.IsEmpty()) {
        return nullptr;
    }
    MOZ_ASSERT(aManagees.Count() == 1);
    return aManagees.ConstIter().Get()->GetKey();
}

// appId's are for B2G only currently, where managees.Count() == 1. This is
// not guaranteed currently in Desktop, so for paths used for desktop,
// don't assert there's one managee.
template<typename Protocol>
Protocol*
SingleManagedOrNull(const ManagedContainer<Protocol>& aManagees)
{
    if (aManagees.Count() != 1) {
        return nullptr;
    }
    return aManagees.ConstIter().Get()->GetKey();
}

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

    static bool Read(const Message* aMsg, PickleIterator* aIter, paramType* aResult)
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

template<class PFooSide>
struct ParamTraits<mozilla::ipc::Endpoint<PFooSide>>
{
    typedef mozilla::ipc::Endpoint<PFooSide> paramType;

    static void Write(Message* aMsg, const paramType& aParam)
    {
        MOZ_RELEASE_ASSERT(aParam.mValid);

        IPC::WriteParam(aMsg, static_cast<uint32_t>(aParam.mMode));

        // We duplicate the descriptor so that our own file descriptor remains
        // valid after the write. An alternative would be to set
        // aParam.mTransport.mValid to false, but that won't work because aParam
        // is const.
        mozilla::ipc::TransportDescriptor desc = mozilla::ipc::DuplicateDescriptor(aParam.mTransport);
        IPC::WriteParam(aMsg, desc);

        IPC::WriteParam(aMsg, aParam.mMyPid);
        IPC::WriteParam(aMsg, aParam.mOtherPid);
        IPC::WriteParam(aMsg, static_cast<uint32_t>(aParam.mProtocolId));
    }

    static bool Read(const Message* aMsg, PickleIterator* aIter, paramType* aResult)
    {
        MOZ_RELEASE_ASSERT(!aResult->mValid);
        aResult->mValid = true;
        uint32_t mode, protocolId;
        if (!IPC::ReadParam(aMsg, aIter, &mode) ||
            !IPC::ReadParam(aMsg, aIter, &aResult->mTransport) ||
            !IPC::ReadParam(aMsg, aIter, &aResult->mMyPid) ||
            !IPC::ReadParam(aMsg, aIter, &aResult->mOtherPid) ||
            !IPC::ReadParam(aMsg, aIter, &protocolId)) {
            return false;
        }
        aResult->mMode = Channel::Mode(mode);
        aResult->mProtocolId = mozilla::ipc::ProtocolId(protocolId);
        return true;
    }

    static void Log(const paramType& aParam, std::wstring* aLog)
    {
        aLog->append(StringPrintf(L"Endpoint"));
    }
};

} // namespace IPC


#endif  // mozilla_ipc_ProtocolUtils_h
