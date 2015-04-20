/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et ft=cpp : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MediaChild.h"

#include "mozilla/ipc/BackgroundChild.h"
#include "mozilla/ipc/PBackgroundChild.h"
#include "nsGlobalWindow.h"
#include "mozilla/MediaManager.h"
#include "prlog.h"
#include "nsQueryObject.h"

#undef LOG
#if defined(PR_LOGGING)
PRLogModuleInfo *gMediaChildLog;
#define LOG(args) PR_LOG(gMediaChildLog, PR_LOG_DEBUG, args)
#else
#define LOG(args)
#endif


namespace mozilla {
namespace media {

static Child* sChild;

template<typename ValueType> void
ChildPledge<ValueType>::ActorCreated(PBackgroundChild* aActor)
{
  if (!sChild) {
    // Create PMedia by sending a message to the parent
    sChild = static_cast<Child*>(aActor->SendPMediaConstructor());
  }
  Run(sChild);
}

template<typename ValueType> void
ChildPledge<ValueType>::ActorFailed()
{
  Pledge<ValueType>::Reject(NS_ERROR_UNEXPECTED);
}

template<typename ValueType> NS_IMPL_ADDREF(ChildPledge<ValueType>)
template<typename ValueType> NS_IMPL_RELEASE(ChildPledge<ValueType>)
template<typename ValueType> NS_INTERFACE_MAP_BEGIN(ChildPledge<ValueType>)
NS_INTERFACE_MAP_ENTRY(nsIIPCBackgroundChildCreateCallback)
NS_INTERFACE_MAP_END

already_AddRefed<ChildPledge<nsCString>>
GetOriginKey(const nsCString& aOrigin, bool aPrivateBrowsing)
{
  class Pledge : public ChildPledge<nsCString>
  {
  public:
    explicit Pledge(const nsCString& aOrigin, bool aPrivateBrowsing)
    : mOrigin(aOrigin), mPrivateBrowsing(aPrivateBrowsing) {}
  private:
    ~Pledge() {}
    void Run(PMediaChild* aChild)
    {
      Child* child = static_cast<Child*>(aChild);

      uint32_t id = child->AddRequestPledge(*this);
      child->SendGetOriginKey(id, mOrigin, mPrivateBrowsing);
    }
    const nsCString mOrigin;
    const bool mPrivateBrowsing;
  };

  nsRefPtr<ChildPledge<nsCString>> p = new Pledge(aOrigin, aPrivateBrowsing);
  nsCOMPtr<nsIIPCBackgroundChildCreateCallback> cb = do_QueryObject(p);
  bool ok = ipc::BackgroundChild::GetOrCreateForCurrentThread(cb);
  MOZ_RELEASE_ASSERT(ok);
  return p.forget();
}

already_AddRefed<ChildPledge<bool>>
SanitizeOriginKeys(const uint64_t& aSinceWhen)
{
  class Pledge : public ChildPledge<bool>
  {
  public:
    explicit Pledge(const uint64_t& aSinceWhen) : mSinceWhen(aSinceWhen) {}
  private:
    void Run(PMediaChild* aMedia)
    {
      aMedia->SendSanitizeOriginKeys(mSinceWhen);
      mValue = true;
      LOG(("SanitizeOriginKeys since %llu", mSinceWhen));
      Resolve();
    }
    const uint64_t mSinceWhen;
  };

  nsRefPtr<ChildPledge<bool>> p = new Pledge(aSinceWhen);
  nsCOMPtr<nsIIPCBackgroundChildCreateCallback> cb = do_QueryObject(p);
  bool ok = ipc::BackgroundChild::GetOrCreateForCurrentThread(cb);
  MOZ_RELEASE_ASSERT(ok);
  return p.forget();
}

Child::Child()
{
#if defined(PR_LOGGING)
  if (!gMediaChildLog) {
    gMediaChildLog = PR_NewLogModule("MediaChild");
  }
#endif
  LOG(("media::Child: %p", this));
  MOZ_COUNT_CTOR(Child);
}

Child::~Child()
{
  LOG(("~media::Child: %p", this));
  sChild = nullptr;
  MOZ_COUNT_DTOR(Child);
}

uint32_t Child::sRequestCounter = 0;

uint32_t
Child::AddRequestPledge(ChildPledge<nsCString>& aPledge)
{
  uint32_t id = ++sRequestCounter;
  nsRefPtr<ChildPledge<nsCString>> ptr(&aPledge);
  mRequestPledges.AppendElement(PledgeEntry(id, ptr));
  return id;
}

already_AddRefed<ChildPledge<nsCString>>
Child::RemoveRequestPledge(uint32_t aRequestId)
{
  for (PledgeEntry& entry : mRequestPledges) {
    if (entry.first == aRequestId) {
      nsRefPtr<ChildPledge<nsCString>> ref;
      ref.swap(entry.second);
      mRequestPledges.RemoveElement(entry);
      return ref.forget();
    }
  }
  MOZ_ASSERT_UNREACHABLE("Received response with no matching media::ChildPledge!");
  return nullptr;
}

bool
Child::RecvGetOriginKeyResponse(const uint32_t& aRequestId, const nsCString& aKey)
{
  nsRefPtr<ChildPledge<nsCString>> pledge = RemoveRequestPledge(aRequestId);
  if (pledge) {
    pledge->Resolve(aKey);
  }
  return true;
}

PMediaChild*
AllocPMediaChild()
{
  Child* obj = new Child();
  obj->AddRef();
  return obj;
}

bool
DeallocPMediaChild(media::PMediaChild *aActor)
{
  static_cast<Child*>(aActor)->Release();
  return true;
}

}
}
