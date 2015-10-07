/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et ft=cpp : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MediaChild.h"
#include "MediaParent.h"

#include "nsGlobalWindow.h"
#include "mozilla/MediaManager.h"
#include "mozilla/Logging.h"
#include "nsQueryObject.h"

#undef LOG
PRLogModuleInfo *gMediaChildLog;
#define LOG(args) MOZ_LOG(gMediaChildLog, mozilla::LogLevel::Debug, args)

namespace mozilla {
namespace media {

already_AddRefed<Pledge<nsCString>>
GetOriginKey(const nsCString& aOrigin, bool aPrivateBrowsing, bool aPersist)
{
  nsRefPtr<MediaManager> mgr = MediaManager::GetInstance();
  MOZ_ASSERT(mgr);

  nsRefPtr<Pledge<nsCString>> p = new Pledge<nsCString>();
  uint32_t id = mgr->mGetOriginKeyPledges.Append(*p);

  if (XRE_GetProcessType() == GeckoProcessType_Default) {
    mgr->GetNonE10sParent()->RecvGetOriginKey(id, aOrigin, aPrivateBrowsing,
                                              aPersist);
  } else {
    Child::Get()->SendGetOriginKey(id, aOrigin, aPrivateBrowsing, aPersist);
  }
  return p.forget();
}

void
SanitizeOriginKeys(const uint64_t& aSinceWhen, bool aOnlyPrivateBrowsing)
{
  LOG(("SanitizeOriginKeys since %llu %s", aSinceWhen,
       (aOnlyPrivateBrowsing? "in Private Browsing." : ".")));

  if (XRE_GetProcessType() == GeckoProcessType_Default) {
    // Avoid opening MediaManager in this case, since this is called by
    // sanitize.js when cookies are cleared, which can happen on startup.
    ScopedDeletePtr<Parent<NonE10s>> tmpParent(new Parent<NonE10s>(true));
    tmpParent->RecvSanitizeOriginKeys(aSinceWhen, aOnlyPrivateBrowsing);
  } else {
    Child::Get()->SendSanitizeOriginKeys(aSinceWhen, aOnlyPrivateBrowsing);
  }
}

static Child* sChild;

Child* Child::Get()
{
  MOZ_ASSERT(XRE_GetProcessType() == GeckoProcessType_Content);
  MOZ_ASSERT(NS_IsMainThread());
  if (!sChild) {
    sChild = static_cast<Child*>(dom::ContentChild::GetSingleton()->SendPMediaConstructor());
  }
  return sChild;
}

Child::Child()
  : mActorDestroyed(false)
{
  if (!gMediaChildLog) {
    gMediaChildLog = PR_NewLogModule("MediaChild");
  }
  LOG(("media::Child: %p", this));
  MOZ_COUNT_CTOR(Child);
}

Child::~Child()
{
  LOG(("~media::Child: %p", this));
  sChild = nullptr;
  MOZ_COUNT_DTOR(Child);
}

void Child::ActorDestroy(ActorDestroyReason aWhy)
{
  mActorDestroyed = true;
}

bool
Child::RecvGetOriginKeyResponse(const uint32_t& aRequestId, const nsCString& aKey)
{
  nsRefPtr<MediaManager> mgr = MediaManager::GetInstance();
  if (!mgr) {
    return false;
  }
  nsRefPtr<Pledge<nsCString>> pledge = mgr->mGetOriginKeyPledges.Remove(aRequestId);
  if (pledge) {
    pledge->Resolve(aKey);
  }
  return true;
}

PMediaChild*
AllocPMediaChild()
{
  return new Child();
}

bool
DeallocPMediaChild(media::PMediaChild *aActor)
{
  delete static_cast<Child*>(aActor);
  return true;
}

} // namespace media
} // namespace mozilla
