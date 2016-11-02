/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
#include "mozilla/UniquePtr.h"
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

class MessageChannel;

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
        mMessage(msg)
    {
      MOZ_ASSERT(0 <= msg && msg < INT32_MAX);
    }

    uint32_t mAction : 1;
    uint32_t mMessage : 31;
};

// What happens if Interrupt calls race?
enum RacyInterruptPolicy {
    RIPError,
    RIPChildWins,
    RIPParentWins
};

class IProtocol : public HasResultCodes
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
    typedef IPC::Message Message;
    typedef IPC::MessageInfo MessageInfo;

    IProtocol(Side aSide) : mSide(aSide), mManager(nullptr), mChannel(nullptr) {}

    virtual int32_t Register(IProtocol*);
    virtual int32_t RegisterID(IProtocol*, int32_t);
    virtual IProtocol* Lookup(int32_t);
    virtual void Unregister(int32_t);
    virtual void RemoveManagee(int32_t, IProtocol*) = 0;

    virtual Shmem::SharedMemory* CreateSharedMemory(
        size_t, SharedMemory::SharedMemoryType, bool, int32_t*);
    virtual Shmem::SharedMemory* LookupSharedMemory(int32_t);
    virtual bool IsTrackingSharedMemory(Shmem::SharedMemory*);
    virtual bool DestroySharedMemory(Shmem&);

    // XXX odd ducks, acknowledged
    virtual ProcessId OtherPid() const;

    virtual const char* ProtocolName() const = 0;
    void FatalError(const char* const aErrorMsg) const;
    virtual void HandleFatalError(const char* aProtocolName, const char* aErrorMsg) const;

    Maybe<IProtocol*> ReadActor(const IPC::Message* aMessage, PickleIterator* aIter, bool aNullable,
                                const char* aActorDescription, int32_t aProtocolTypeId);

    virtual Result OnMessageReceived(const Message& aMessage) = 0;
    virtual Result OnMessageReceived(const Message& aMessage, Message *& aReply) = 0;
    virtual Result OnCallReceived(const Message& aMessage, Message *& aReply) = 0;

    virtual int32_t GetProtocolTypeId() = 0;

    IProtocol* Manager() const { return mManager; }
    virtual const MessageChannel* GetIPCChannel() const { return mChannel; }
    virtual MessageChannel* GetIPCChannel() { return mChannel; }

    bool AllocShmem(size_t aSize, Shmem::SharedMemory::SharedMemoryType aType, Shmem* aOutMem);
    bool AllocUnsafeShmem(size_t aSize, Shmem::SharedMemory::SharedMemoryType aType, Shmem* aOutMem);
    bool DeallocShmem(Shmem& aMem);

protected:
    void SetManager(IProtocol* aManager) { mManager = aManager; }
    void SetIPCChannel(MessageChannel* aChannel) { mChannel = aChannel; }

private:
    Side mSide;
    IProtocol* mManager;
    MessageChannel* mChannel;
};

typedef IPCMessageStart ProtocolId;

template<class PFooSide>
class Endpoint;

/**
 * All top-level protocols should inherit this class.
 *
 * IToplevelProtocol tracks all top-level protocol actors created from
 * this protocol actor.
 */
class IToplevelProtocol : public IProtocol
{
    template<class PFooSide> friend class Endpoint;

protected:
    explicit IToplevelProtocol(ProtocolId aProtoId, Side aSide);
    ~IToplevelProtocol();

public:
    void SetTransport(UniquePtr<Transport> aTrans)
    {
        mTrans = Move(aTrans);
    }

    Transport* GetTransport() const { return mTrans.get(); }

    ProtocolId GetProtocolId() const { return mProtocolId; }

    base::ProcessId OtherPid() const;
    void SetOtherProcessId(base::ProcessId aOtherPid);

    virtual void OnChannelClose() = 0;
    virtual void OnChannelError() = 0;
    virtual void ProcessingError(Result aError, const char* aMsgName) {}
    virtual void OnChannelConnected(int32_t peer_pid) {}

    virtual bool ShouldContinueFromReplyTimeout() {
        return false;
    }

    // WARNING: This function is called with the MessageChannel monitor held.
    virtual void IntentionalCrash() {
        MOZ_CRASH("Intentional IPDL crash");
    }

    // The code here is only useful for fuzzing. It should not be used for any
    // other purpose.
#ifdef DEBUG
    // Returns true if we should simulate a timeout.
    // WARNING: This is a testing-only function that is called with the
    // MessageChannel monitor held. Don't do anything fancy here or we could
    // deadlock.
    virtual bool ArtificialTimeout() {
        return false;
    }

    // Returns true if we want to cause the worker thread to sleep with the
    // monitor unlocked.
    virtual bool NeedArtificialSleep() {
        return false;
    }

    // This function should be implemented to sleep for some amount of time on
    // the worker thread. Will only be called if NeedArtificialSleep() returns
    // true.
    virtual void ArtificialSleep() {}
#else
    bool ArtificialTimeout() { return false; }
    bool NeedArtificialSleep() { return false; }
    void ArtificialSleep() {}
#endif

    virtual void EnteredCxxStack() {}
    virtual void ExitedCxxStack() {}
    virtual void EnteredCall() {}
    virtual void ExitedCall() {}

    bool IsOnCxxStack() const;

    virtual RacyInterruptPolicy MediateInterruptRace(const MessageInfo& parent,
                                                     const MessageInfo& child)
    {
        return RIPChildWins;
    }

    /**
     * Return true if windows messages can be handled while waiting for a reply
     * to a sync IPDL message.
     */
    virtual bool HandleWindowsMessages(const Message& aMsg) const { return true; }

    virtual void OnEnteredSyncSend() {
    }
    virtual void OnExitedSyncSend() {
    }

    virtual void ProcessRemoteNativeEventsInInterruptCall() {
    }

private:
    ProtocolId mProtocolId;
    UniquePtr<Transport> mTrans;
    base::ProcessId mOtherPid;
};

class IShmemAllocator
{
public:
  virtual bool AllocShmem(size_t aSize,
                          mozilla::ipc::SharedMemory::SharedMemoryType aShmType,
                          mozilla::ipc::Shmem* aShmem) = 0;
  virtual bool AllocUnsafeShmem(size_t aSize,
                                mozilla::ipc::SharedMemory::SharedMemoryType aShmType,
                                mozilla::ipc::Shmem* aShmem) = 0;
  virtual bool DeallocShmem(mozilla::ipc::Shmem& aShmem) = 0;
};

#define FORWARD_SHMEM_ALLOCATOR_TO(aImplClass) \
  virtual bool AllocShmem(size_t aSize, \
                          mozilla::ipc::SharedMemory::SharedMemoryType aShmType, \
                          mozilla::ipc::Shmem* aShmem) override \
  { return aImplClass::AllocShmem(aSize, aShmType, aShmem); } \
  virtual bool AllocUnsafeShmem(size_t aSize, \
                                mozilla::ipc::SharedMemory::SharedMemoryType aShmType, \
                                mozilla::ipc::Shmem* aShmem) override \
  { return aImplClass::AllocUnsafeShmem(aSize, aShmType, aShmem); } \
  virtual bool DeallocShmem(mozilla::ipc::Shmem& aShmem) override \
  { return aImplClass::DeallocShmem(aShmem); }

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
                      uint32_t aMessageId,
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
#else
#define AnnotateSystemError() do { } while (0)
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

    ProcessId OtherPid() const {
        return mOtherPid;
    }

    // This method binds aActor to this endpoint. After this call, the actor can
    // be used to send and receive messages. The endpoint becomes invalid.
    bool Bind(PFooSide* aActor)
    {
        MOZ_RELEASE_ASSERT(mValid);
        MOZ_RELEASE_ASSERT(mMyPid == base::GetCurrentProcId());

        UniquePtr<Transport> t = mozilla::ipc::OpenDescriptor(mTransport, mMode);
        if (!t) {
            return false;
        }
        if (!aActor->Open(t.get(), mOtherPid, XRE_GetIOMessageLoop(),
                          mMode == Transport::MODE_SERVER ? ParentSide : ChildSide)) {
            return false;
        }
        mValid = false;
        aActor->SetTransport(Move(t));
        return true;
    }

    bool IsValid() const {
        return mValid;
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

#if defined(MOZ_CRASHREPORTER) && defined(XP_MACOSX)
void AnnotateCrashReportWithErrno(const char* tag, int error);
#else
static inline void AnnotateCrashReportWithErrno(const char* tag, int error)
{}
#endif

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
    AnnotateCrashReportWithErrno("IpcCreateEndpointsNsresult", int(rv));
    return rv;
  }

  *aParentEndpoint = Endpoint<PFooParent>(aPrivate, mozilla::ipc::Transport::MODE_SERVER,
                                          parentTransport, aParentDestPid, aChildDestPid, aProtocol);

  *aChildEndpoint = Endpoint<PFooChild>(aPrivate, mozilla::ipc::Transport::MODE_CLIENT,
                                        childTransport, aChildDestPid, aParentDestPid, aChildProtocol);

  return NS_OK;
}

void
TableToArray(const nsTHashtable<nsPtrHashKey<void>>& aTable,
             nsTArray<void*>& aArray);

const char* StringFromIPCMessageType(uint32_t aMessageType);

} // namespace ipc

template<typename Protocol>
class ManagedContainer : public nsTHashtable<nsPtrHashKey<Protocol>>
{
  typedef nsTHashtable<nsPtrHashKey<Protocol>> BaseClass;

public:
  // Having the core logic work on void pointers, rather than typed pointers,
  // means that we can have one instance of this code out-of-line, rather
  // than several hundred instances of this code out-of-lined.  (Those
  // repeated instances don't necessarily get folded together by the linker
  // because they contain member offsets and such that differ between the
  // functions.)  We do have to pay for it with some eye-bleedingly bad casts,
  // though.
  void ToArray(nsTArray<Protocol*>& aArray) const {
    ::mozilla::ipc::TableToArray(*reinterpret_cast<const nsTHashtable<nsPtrHashKey<void>>*>
                                 (static_cast<const BaseClass*>(this)),
                                 reinterpret_cast<nsTArray<void*>&>(aArray));
  }
};

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
        IPC::WriteParam(aMsg, aParam.mValid);
        if (!aParam.mValid) {
            return;
        }

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

        if (!IPC::ReadParam(aMsg, aIter, &aResult->mValid)) {
            return false;
        }
        if (!aResult->mValid) {
            // Object is empty, but read succeeded.
            return true;
        }

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
