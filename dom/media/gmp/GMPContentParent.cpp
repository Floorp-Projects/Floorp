/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GMPContentParent.h"
#include "GMPAudioDecoderParent.h"
#include "GMPDecryptorParent.h"
#include "GMPParent.h"
#include "GMPServiceChild.h"
#include "GMPVideoDecoderParent.h"
#include "GMPVideoEncoderParent.h"
#include "mozIGeckoMediaPluginService.h"
#include "mozilla/Logging.h"
#include "mozilla/unused.h"

namespace mozilla {

#ifdef LOG
#undef LOG
#endif

extern LogModule* GetGMPLog();

#define LOGD(msg) MOZ_LOG(GetGMPLog(), mozilla::LogLevel::Debug, msg)
#define LOG(level, msg) MOZ_LOG(GetGMPLog(), (level), msg)

#ifdef __CLASS__
#undef __CLASS__
#endif
#define __CLASS__ "GMPContentParent"

namespace gmp {

GMPContentParent::GMPContentParent(GMPParent* aParent)
  : mParent(aParent)
{
  if (mParent) {
    SetDisplayName(mParent->GetDisplayName());
    SetPluginId(mParent->GetPluginId());
  }
}

GMPContentParent::~GMPContentParent()
{
  XRE_GetIOMessageLoop()->PostTask(FROM_HERE,
                                   new DeleteTask<Transport>(GetTransport()));
}

class ReleaseGMPContentParent : public nsRunnable
{
public:
  explicit ReleaseGMPContentParent(GMPContentParent* aToRelease)
    : mToRelease(aToRelease)
  {
  }

  NS_IMETHOD Run()
  {
    return NS_OK;
  }

private:
  RefPtr<GMPContentParent> mToRelease;
};

void
GMPContentParent::ActorDestroy(ActorDestroyReason aWhy)
{
  MOZ_ASSERT(mAudioDecoders.IsEmpty() &&
             mDecryptors.IsEmpty() &&
             mVideoDecoders.IsEmpty() &&
             mVideoEncoders.IsEmpty());
  NS_DispatchToCurrentThread(new ReleaseGMPContentParent(this));
}

void
GMPContentParent::CheckThread()
{
  MOZ_ASSERT(mGMPThread == NS_GetCurrentThread());
}

void
GMPContentParent::AudioDecoderDestroyed(GMPAudioDecoderParent* aDecoder)
{
  MOZ_ASSERT(GMPThread() == NS_GetCurrentThread());

  MOZ_ALWAYS_TRUE(mAudioDecoders.RemoveElement(aDecoder));
  CloseIfUnused();
}

void
GMPContentParent::VideoDecoderDestroyed(GMPVideoDecoderParent* aDecoder)
{
  MOZ_ASSERT(GMPThread() == NS_GetCurrentThread());

  // If the constructor fails, we'll get called before it's added
  Unused << NS_WARN_IF(!mVideoDecoders.RemoveElement(aDecoder));
  CloseIfUnused();
}

void
GMPContentParent::VideoEncoderDestroyed(GMPVideoEncoderParent* aEncoder)
{
  MOZ_ASSERT(GMPThread() == NS_GetCurrentThread());

  // If the constructor fails, we'll get called before it's added
  Unused << NS_WARN_IF(!mVideoEncoders.RemoveElement(aEncoder));
  CloseIfUnused();
}

void
GMPContentParent::DecryptorDestroyed(GMPDecryptorParent* aSession)
{
  MOZ_ASSERT(GMPThread() == NS_GetCurrentThread());

  MOZ_ALWAYS_TRUE(mDecryptors.RemoveElement(aSession));
  CloseIfUnused();
}

void
GMPContentParent::CloseIfUnused()
{
  if (mAudioDecoders.IsEmpty() &&
      mDecryptors.IsEmpty() &&
      mVideoDecoders.IsEmpty() &&
      mVideoEncoders.IsEmpty()) {
    RefPtr<GMPContentParent> toClose;
    if (mParent) {
      toClose = mParent->ForgetGMPContentParent();
    } else {
      toClose = this;
      RefPtr<GeckoMediaPluginServiceChild> gmp(
        GeckoMediaPluginServiceChild::GetSingleton());
      gmp->RemoveGMPContentParent(toClose);
    }
    NS_DispatchToCurrentThread(NS_NewRunnableMethod(toClose,
                                                    &GMPContentParent::Close));
  }
}

nsresult
GMPContentParent::GetGMPDecryptor(GMPDecryptorParent** aGMPDP)
{
  PGMPDecryptorParent* pdp = SendPGMPDecryptorConstructor();
  if (!pdp) {
    return NS_ERROR_FAILURE;
  }
  GMPDecryptorParent* dp = static_cast<GMPDecryptorParent*>(pdp);
  // This addref corresponds to the Proxy pointer the consumer is returned.
  // It's dropped by calling Close() on the interface.
  NS_ADDREF(dp);
  mDecryptors.AppendElement(dp);
  *aGMPDP = dp;

  return NS_OK;
}

nsIThread*
GMPContentParent::GMPThread()
{
  if (!mGMPThread) {
    nsCOMPtr<mozIGeckoMediaPluginService> mps = do_GetService("@mozilla.org/gecko-media-plugin-service;1");
    MOZ_ASSERT(mps);
    if (!mps) {
      return nullptr;
    }
    // Not really safe if we just grab to the mGMPThread, as we don't know
    // what thread we're running on and other threads may be trying to
    // access this without locks!  However, debug only, and primary failure
    // mode outside of compiler-helped TSAN is a leak.  But better would be
    // to use swap() under a lock.
    mps->GetThread(getter_AddRefs(mGMPThread));
    MOZ_ASSERT(mGMPThread);
  }

  return mGMPThread;
}

nsresult
GMPContentParent::GetGMPAudioDecoder(GMPAudioDecoderParent** aGMPAD)
{
  PGMPAudioDecoderParent* pvap = SendPGMPAudioDecoderConstructor();
  if (!pvap) {
    return NS_ERROR_FAILURE;
  }
  GMPAudioDecoderParent* vap = static_cast<GMPAudioDecoderParent*>(pvap);
  // This addref corresponds to the Proxy pointer the consumer is returned.
  // It's dropped by calling Close() on the interface.
  NS_ADDREF(vap);
  *aGMPAD = vap;
  mAudioDecoders.AppendElement(vap);

  return NS_OK;
}

nsresult
GMPContentParent::GetGMPVideoDecoder(GMPVideoDecoderParent** aGMPVD)
{
  // returned with one anonymous AddRef that locks it until Destroy
  PGMPVideoDecoderParent* pvdp = SendPGMPVideoDecoderConstructor();
  if (!pvdp) {
    return NS_ERROR_FAILURE;
  }
  GMPVideoDecoderParent *vdp = static_cast<GMPVideoDecoderParent*>(pvdp);
  // This addref corresponds to the Proxy pointer the consumer is returned.
  // It's dropped by calling Close() on the interface.
  NS_ADDREF(vdp);
  *aGMPVD = vdp;
  mVideoDecoders.AppendElement(vdp);

  return NS_OK;
}

nsresult
GMPContentParent::GetGMPVideoEncoder(GMPVideoEncoderParent** aGMPVE)
{
  // returned with one anonymous AddRef that locks it until Destroy
  PGMPVideoEncoderParent* pvep = SendPGMPVideoEncoderConstructor();
  if (!pvep) {
    return NS_ERROR_FAILURE;
  }
  GMPVideoEncoderParent *vep = static_cast<GMPVideoEncoderParent*>(pvep);
  // This addref corresponds to the Proxy pointer the consumer is returned.
  // It's dropped by calling Close() on the interface.
  NS_ADDREF(vep);
  *aGMPVE = vep;
  mVideoEncoders.AppendElement(vep);

  return NS_OK;
}

PGMPVideoDecoderParent*
GMPContentParent::AllocPGMPVideoDecoderParent()
{
  GMPVideoDecoderParent* vdp = new GMPVideoDecoderParent(this);
  NS_ADDREF(vdp);
  return vdp;
}

bool
GMPContentParent::DeallocPGMPVideoDecoderParent(PGMPVideoDecoderParent* aActor)
{
  GMPVideoDecoderParent* vdp = static_cast<GMPVideoDecoderParent*>(aActor);
  NS_RELEASE(vdp);
  return true;
}

PGMPVideoEncoderParent*
GMPContentParent::AllocPGMPVideoEncoderParent()
{
  GMPVideoEncoderParent* vep = new GMPVideoEncoderParent(this);
  NS_ADDREF(vep);
  return vep;
}

bool
GMPContentParent::DeallocPGMPVideoEncoderParent(PGMPVideoEncoderParent* aActor)
{
  GMPVideoEncoderParent* vep = static_cast<GMPVideoEncoderParent*>(aActor);
  NS_RELEASE(vep);
  return true;
}

PGMPDecryptorParent*
GMPContentParent::AllocPGMPDecryptorParent()
{
  GMPDecryptorParent* ksp = new GMPDecryptorParent(this);
  NS_ADDREF(ksp);
  return ksp;
}

bool
GMPContentParent::DeallocPGMPDecryptorParent(PGMPDecryptorParent* aActor)
{
  GMPDecryptorParent* ksp = static_cast<GMPDecryptorParent*>(aActor);
  NS_RELEASE(ksp);
  return true;
}

PGMPAudioDecoderParent*
GMPContentParent::AllocPGMPAudioDecoderParent()
{
  GMPAudioDecoderParent* vdp = new GMPAudioDecoderParent(this);
  NS_ADDREF(vdp);
  return vdp;
}

bool
GMPContentParent::DeallocPGMPAudioDecoderParent(PGMPAudioDecoderParent* aActor)
{
  GMPAudioDecoderParent* vdp = static_cast<GMPAudioDecoderParent*>(aActor);
  NS_RELEASE(vdp);
  return true;
}

} // namespace gmp
} // namespace mozilla
