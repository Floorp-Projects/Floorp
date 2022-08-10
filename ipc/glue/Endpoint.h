/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#ifndef IPC_GLUE_ENDPOINT_H_
#define IPC_GLUE_ENDPOINT_H_

#include <utility>
#include "CrashAnnotations.h"
#include "base/process.h"
#include "base/process_util.h"
#include "mozilla/Assertions.h"
#include "mozilla/Maybe.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/ipc/MessageLink.h"
#include "mozilla/ipc/ProtocolUtils.h"
#include "mozilla/ipc/NodeController.h"
#include "mozilla/ipc/ScopedPort.h"
#include "nsXULAppAPI.h"
#include "nscore.h"

namespace IPC {
template <class P>
struct ParamTraits;
}

namespace mozilla {
namespace ipc {

namespace endpoint_detail {

template <class T>
static auto ActorNeedsOtherPidHelper(int)
    -> decltype(std::declval<T>().OtherPid(), std::true_type{});
template <class>
static auto ActorNeedsOtherPidHelper(long) -> std::false_type;

template <typename T>
constexpr bool ActorNeedsOtherPid =
    decltype(ActorNeedsOtherPidHelper<T>(0))::value;

}  // namespace endpoint_detail

struct PrivateIPDLInterface {};

class UntypedEndpoint {
 public:
  using ProcessId = base::ProcessId;

  UntypedEndpoint() = default;

  UntypedEndpoint(const PrivateIPDLInterface&, ScopedPort aPort,
                  const nsID& aMessageChannelId,
                  ProcessId aMyPid = base::kInvalidProcessId,
                  ProcessId aOtherPid = base::kInvalidProcessId)
      : mPort(std::move(aPort)),
        mMessageChannelId(aMessageChannelId),
        mMyPid(aMyPid),
        mOtherPid(aOtherPid) {}

  UntypedEndpoint(const UntypedEndpoint&) = delete;
  UntypedEndpoint(UntypedEndpoint&& aOther) = default;

  UntypedEndpoint& operator=(const UntypedEndpoint&) = delete;
  UntypedEndpoint& operator=(UntypedEndpoint&& aOther) = default;

  // This method binds aActor to this endpoint. After this call, the actor can
  // be used to send and receive messages. The endpoint becomes invalid.
  //
  // If specified, aEventTarget is the target the actor will be bound to, and
  // must be on the current thread. Otherwise, GetCurrentSerialEventTarget() is
  // used.
  bool Bind(IToplevelProtocol* aActor,
            nsISerialEventTarget* aEventTarget = nullptr) {
    MOZ_RELEASE_ASSERT(IsValid());
    MOZ_RELEASE_ASSERT(mMyPid == base::kInvalidProcessId ||
                       mMyPid == base::GetCurrentProcId());
    MOZ_RELEASE_ASSERT(!aEventTarget || aEventTarget->IsOnCurrentThread());
    return aActor->Open(std::move(mPort), mMessageChannelId, mOtherPid,
                        aEventTarget);
  }

  bool IsValid() const { return mPort.IsValid(); }

 protected:
  friend struct IPC::ParamTraits<UntypedEndpoint>;

  ScopedPort mPort;
  nsID mMessageChannelId{};
  ProcessId mMyPid = base::kInvalidProcessId;
  ProcessId mOtherPid = base::kInvalidProcessId;
};

/**
 * An endpoint represents one end of a partially initialized IPDL channel. To
 * set up a new top-level protocol:
 *
 * Endpoint<PFooParent> parentEp;
 * Endpoint<PFooChild> childEp;
 * nsresult rv;
 * rv = PFoo::CreateEndpoints(&parentEp, &childEp);
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
 *
 * If creating endpoints for a [NeedsOtherPid] actor, you're required to also
 * pass in parentPid and childPid, which are the pids of the processes in which
 * the parent and child endpoints will be used.
 */
template <class PFooSide>
class Endpoint final : public UntypedEndpoint {
 public:
  using UntypedEndpoint::IsValid;
  using UntypedEndpoint::UntypedEndpoint;

  base::ProcessId OtherPid() const {
    static_assert(
        endpoint_detail::ActorNeedsOtherPid<PFooSide>,
        "OtherPid may only be called on Endpoints for actors which are "
        "[NeedsOtherPid]");
    MOZ_RELEASE_ASSERT(mOtherPid != base::kInvalidProcessId);
    return mOtherPid;
  }

  // This method binds aActor to this endpoint. After this call, the actor can
  // be used to send and receive messages. The endpoint becomes invalid.
  //
  // If specified, aEventTarget is the target the actor will be bound to, and
  // must be on the current thread. Otherwise, GetCurrentSerialEventTarget() is
  // used.
  bool Bind(PFooSide* aActor, nsISerialEventTarget* aEventTarget = nullptr) {
    return UntypedEndpoint::Bind(aActor, aEventTarget);
  }
};

#if defined(XP_MACOSX)
void AnnotateCrashReportWithErrno(CrashReporter::Annotation tag, int error);
#else
inline void AnnotateCrashReportWithErrno(CrashReporter::Annotation tag,
                                         int error) {}
#endif

// This function is used internally to create a pair of Endpoints. See the
// comment above Endpoint for a description of how it might be used.
template <class PFooParent, class PFooChild>
nsresult CreateEndpoints(const PrivateIPDLInterface& aPrivate,
                         Endpoint<PFooParent>* aParentEndpoint,
                         Endpoint<PFooChild>* aChildEndpoint) {
  static_assert(
      !endpoint_detail::ActorNeedsOtherPid<PFooParent> &&
          !endpoint_detail::ActorNeedsOtherPid<PFooChild>,
      "Pids are required when creating endpoints for [NeedsOtherPid] actors");

  auto [parentPort, childPort] =
      NodeController::GetSingleton()->CreatePortPair();
  nsID channelId = nsID::GenerateUUID();
  *aParentEndpoint =
      Endpoint<PFooParent>(aPrivate, std::move(parentPort), channelId);
  *aChildEndpoint =
      Endpoint<PFooChild>(aPrivate, std::move(childPort), channelId);
  return NS_OK;
}

template <class PFooParent, class PFooChild>
nsresult CreateEndpoints(const PrivateIPDLInterface& aPrivate,
                         base::ProcessId aParentDestPid,
                         base::ProcessId aChildDestPid,
                         Endpoint<PFooParent>* aParentEndpoint,
                         Endpoint<PFooChild>* aChildEndpoint) {
  MOZ_RELEASE_ASSERT(aParentDestPid != base::kInvalidProcessId);
  MOZ_RELEASE_ASSERT(aChildDestPid != base::kInvalidProcessId);

  auto [parentPort, childPort] =
      NodeController::GetSingleton()->CreatePortPair();
  nsID channelId = nsID::GenerateUUID();
  *aParentEndpoint =
      Endpoint<PFooParent>(aPrivate, std::move(parentPort), channelId,
                           aParentDestPid, aChildDestPid);
  *aChildEndpoint = Endpoint<PFooChild>(
      aPrivate, std::move(childPort), channelId, aChildDestPid, aParentDestPid);
  return NS_OK;
}

class UntypedManagedEndpoint {
 public:
  bool IsValid() const { return mInner.isSome(); }

  UntypedManagedEndpoint(const UntypedManagedEndpoint&) = delete;
  UntypedManagedEndpoint& operator=(const UntypedManagedEndpoint&) = delete;

 protected:
  UntypedManagedEndpoint() = default;
  explicit UntypedManagedEndpoint(IProtocol* aActor);

  UntypedManagedEndpoint(UntypedManagedEndpoint&& aOther) noexcept
      : mInner(std::move(aOther.mInner)) {
    aOther.mInner = Nothing();
  }
  UntypedManagedEndpoint& operator=(UntypedManagedEndpoint&& aOther) noexcept {
    this->~UntypedManagedEndpoint();
    new (this) UntypedManagedEndpoint(std::move(aOther));
    return *this;
  }

  ~UntypedManagedEndpoint() noexcept;

  bool BindCommon(IProtocol* aActor, IProtocol* aManager);

 private:
  friend struct IPDLParamTraits<UntypedManagedEndpoint>;

  struct Inner {
    // Pointers to the toplevel actor which will manage this connection. When
    // created, only `mOtherSide` will be set, and will reference the
    // toplevel actor which the other side is managed by. After being sent over
    // IPC, only `mToplevel` will be set, and will be the toplevel actor for the
    // channel which received the IPC message.
    RefPtr<WeakActorLifecycleProxy> mOtherSide;
    RefPtr<WeakActorLifecycleProxy> mToplevel;

    int32_t mId = 0;
    ProtocolId mType = LastMsgIndex;
    int32_t mManagerId = 0;
    ProtocolId mManagerType = LastMsgIndex;
  };
  Maybe<Inner> mInner;
};

/**
 * A managed endpoint represents one end of a partially initialized managed
 * IPDL actor. It is used for situations where the usual IPDL Constructor
 * methods do not give sufficient control over the construction of actors, such
 * as when constructing actors within replies, or constructing multiple related
 * actors simultaneously.
 *
 * FooParent* parent = new FooParent();
 * ManagedEndpoint<PFooChild> childEp = parentMgr->OpenPFooEndpoint(parent);
 *
 * ManagedEndpoints should be sent using IPDL messages or other mechanisms to
 * the other side of the manager channel. Once the ManagedEndpoint has arrived
 * at its destination, you can create the actor, and bind it to the endpoint.
 *
 * FooChild* child = new FooChild();
 * childMgr->BindPFooEndpoint(childEp, child);
 *
 * WARNING: If the remote side of an endpoint has not been bound before it
 * begins to receive messages, an IPC routing error will occur, likely causing
 * a browser crash.
 */
template <class PFooSide>
class ManagedEndpoint : public UntypedManagedEndpoint {
 public:
  ManagedEndpoint() = default;
  ManagedEndpoint(ManagedEndpoint&&) noexcept = default;
  ManagedEndpoint& operator=(ManagedEndpoint&&) noexcept = default;

  ManagedEndpoint(const PrivateIPDLInterface&, IProtocol* aActor)
      : UntypedManagedEndpoint(aActor) {}

  bool Bind(const PrivateIPDLInterface&, PFooSide* aActor, IProtocol* aManager,
            ManagedContainer<PFooSide>& aContainer) {
    if (!BindCommon(aActor, aManager)) {
      return false;
    }
    aContainer.Insert(aActor);
    return true;
  }

  // Only invalid ManagedEndpoints can be equal, as valid endpoints are unique.
  bool operator==(const ManagedEndpoint& _o) const {
    return !IsValid() && !_o.IsValid();
  }
};

}  // namespace ipc
}  // namespace mozilla

#endif  // IPC_GLUE_ENDPOINT_H_
