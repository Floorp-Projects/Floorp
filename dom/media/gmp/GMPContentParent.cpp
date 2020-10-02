/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GMPContentParent.h"
#include "GMPLog.h"
#include "GMPParent.h"
#include "GMPServiceChild.h"
#include "GMPVideoDecoderParent.h"
#include "GMPVideoEncoderParent.h"
#include "ChromiumCDMParent.h"
#include "mozIGeckoMediaPluginService.h"
#include "mozilla/Logging.h"
#include "mozilla/Unused.h"
#include "base/task.h"

namespace mozilla {
namespace gmp {

static const char* GetBoolString(bool aBool) {
  return aBool ? "true" : "false";
}

GMPContentParent::GMPContentParent(GMPParent* aParent)
    : mParent(aParent), mPluginId(0) {
  GMP_LOG_DEBUG("GMPContentParent::GMPContentParent(this=%p), aParent=%p", this,
                aParent);
  if (mParent) {
    SetDisplayName(mParent->GetDisplayName());
    SetPluginId(mParent->GetPluginId());
  }
}

GMPContentParent::~GMPContentParent() {
  GMP_LOG_DEBUG(
      "GMPContentParent::~GMPContentParent(this=%p) mVideoDecoders.IsEmpty=%s, "
      "mVideoEncoders.IsEmpty=%s, mChromiumCDMs.IsEmpty=%s, "
      "mCloseBlockerCount=%" PRIu32,
      this, GetBoolString(mVideoDecoders.IsEmpty()),
      GetBoolString(mVideoEncoders.IsEmpty()),
      GetBoolString(mChromiumCDMs.IsEmpty()), mCloseBlockerCount);
}

void GMPContentParent::ActorDestroy(ActorDestroyReason aWhy) {
  GMP_LOG_DEBUG("GMPContentParent::ActorDestroy(this=%p, aWhy=%d)", this,
                static_cast<int>(aWhy));
  MOZ_ASSERT(mVideoDecoders.IsEmpty() && mVideoEncoders.IsEmpty() &&
             mChromiumCDMs.IsEmpty());
}

void GMPContentParent::CheckThread() {
  MOZ_ASSERT(GMPEventTarget()->IsOnCurrentThread());
}

void GMPContentParent::ChromiumCDMDestroyed(ChromiumCDMParent* aCDM) {
  GMP_LOG_DEBUG("GMPContentParent::ChromiumCDMDestroyed(this=%p, aCDM=%p)",
                this, aCDM);
  MOZ_ASSERT(GMPEventTarget()->IsOnCurrentThread());

  MOZ_ALWAYS_TRUE(mChromiumCDMs.RemoveElement(aCDM));
  CloseIfUnused();
}

void GMPContentParent::VideoDecoderDestroyed(GMPVideoDecoderParent* aDecoder) {
  GMP_LOG_DEBUG("GMPContentParent::VideoDecoderDestroyed(this=%p, aDecoder=%p)",
                this, aDecoder);
  MOZ_ASSERT(GMPEventTarget()->IsOnCurrentThread());

  // If the constructor fails, we'll get called before it's added
  Unused << NS_WARN_IF(!mVideoDecoders.RemoveElement(aDecoder));
  CloseIfUnused();
}

void GMPContentParent::VideoEncoderDestroyed(GMPVideoEncoderParent* aEncoder) {
  GMP_LOG_DEBUG("GMPContentParent::VideoEncoderDestroyed(this=%p, aEncoder=%p)",
                this, aEncoder);
  MOZ_ASSERT(GMPEventTarget()->IsOnCurrentThread());

  // If the constructor fails, we'll get called before it's added
  Unused << NS_WARN_IF(!mVideoEncoders.RemoveElement(aEncoder));
  CloseIfUnused();
}

void GMPContentParent::AddCloseBlocker() {
  MOZ_ASSERT(GMPEventTarget()->IsOnCurrentThread());
  ++mCloseBlockerCount;
  GMP_LOG_DEBUG(
      "GMPContentParent::AddCloseBlocker(this=%p) mCloseBlockerCount=%" PRIu32,
      this, mCloseBlockerCount);
}

void GMPContentParent::RemoveCloseBlocker() {
  MOZ_ASSERT(GMPEventTarget()->IsOnCurrentThread());
  --mCloseBlockerCount;
  GMP_LOG_DEBUG(
      "GMPContentParent::RemoveCloseBlocker(this=%p) "
      "mCloseBlockerCount=%" PRIu32,
      this, mCloseBlockerCount);
  CloseIfUnused();
}

void GMPContentParent::CloseIfUnused() {
  GMP_LOG_DEBUG(
      "GMPContentParent::CloseIfUnused(this=%p) mVideoDecoders.IsEmpty=%s, "
      "mVideoEncoders.IsEmpty=%s, mChromiumCDMs.IsEmpty=%s, "
      "mCloseBlockerCount=%" PRIu32,
      this, GetBoolString(mVideoDecoders.IsEmpty()),
      GetBoolString(mVideoEncoders.IsEmpty()),
      GetBoolString(mChromiumCDMs.IsEmpty()), mCloseBlockerCount);
  if (mVideoDecoders.IsEmpty() && mVideoEncoders.IsEmpty() &&
      mChromiumCDMs.IsEmpty() && mCloseBlockerCount == 0) {
    RefPtr<GMPContentParent> toClose;
    if (mParent) {
      toClose = mParent->ForgetGMPContentParent();
    } else {
      toClose = this;
      RefPtr<GeckoMediaPluginServiceChild> gmp(
          GeckoMediaPluginServiceChild::GetSingleton());
      gmp->RemoveGMPContentParent(toClose);
    }
    NS_DispatchToCurrentThread(NewRunnableMethod(
        "gmp::GMPContentParent::Close", toClose, &GMPContentParent::Close));
  }
}

nsCOMPtr<nsISerialEventTarget> GMPContentParent::GMPEventTarget() {
  if (!mGMPEventTarget) {
    GMP_LOG_DEBUG("GMPContentParent::GMPEventTarget(this=%p)", this);
    nsCOMPtr<mozIGeckoMediaPluginService> mps =
        do_GetService("@mozilla.org/gecko-media-plugin-service;1");
    MOZ_ASSERT(mps);
    if (!mps) {
      return nullptr;
    }
    // Not really safe if we just grab to the mGMPEventTarget, as we don't know
    // what thread we're running on and other threads may be trying to
    // access this without locks!  However, debug only, and primary failure
    // mode outside of compiler-helped TSAN is a leak.  But better would be
    // to use swap() under a lock.
    nsCOMPtr<nsIThread> gmpThread;
    mps->GetThread(getter_AddRefs(gmpThread));
    MOZ_ASSERT(gmpThread);

    mGMPEventTarget = gmpThread->SerialEventTarget();
  }

  return mGMPEventTarget;
}

already_AddRefed<ChromiumCDMParent> GMPContentParent::GetChromiumCDM() {
  GMP_LOG_DEBUG("GMPContentParent::GetChromiumCDM(this=%p)", this);

  RefPtr<ChromiumCDMParent> parent = new ChromiumCDMParent(this, GetPluginId());
  if (!SendPChromiumCDMConstructor(parent)) {
    return nullptr;
  }

  // TODO: Remove parent from mChromiumCDMs in ChromiumCDMParent::Destroy().
  mChromiumCDMs.AppendElement(parent);

  return parent.forget();
}

nsresult GMPContentParent::GetGMPVideoDecoder(GMPVideoDecoderParent** aGMPVD,
                                              uint32_t aDecryptorId) {
  GMP_LOG_DEBUG("GMPContentParent::GetGMPVideoDecoder(this=%p)", this);

  RefPtr<GMPVideoDecoderParent> vdp = new GMPVideoDecoderParent(this);
  if (!SendPGMPVideoDecoderConstructor(vdp, aDecryptorId)) {
    return NS_ERROR_FAILURE;
  }

  // This addref corresponds to the Proxy pointer the consumer is returned.
  // It's dropped by calling Close() on the interface.
  vdp.get()->AddRef();
  *aGMPVD = vdp;
  mVideoDecoders.AppendElement(vdp);

  return NS_OK;
}

nsresult GMPContentParent::GetGMPVideoEncoder(GMPVideoEncoderParent** aGMPVE) {
  GMP_LOG_DEBUG("GMPContentParent::GetGMPVideoEncoder(this=%p)", this);

  RefPtr<GMPVideoEncoderParent> vep = new GMPVideoEncoderParent(this);
  if (!SendPGMPVideoEncoderConstructor(vep)) {
    return NS_ERROR_FAILURE;
  }

  // This addref corresponds to the Proxy pointer the consumer is returned.
  // It's dropped by calling Close() on the interface.
  vep.get()->AddRef();
  *aGMPVE = vep;
  mVideoEncoders.AppendElement(vep);

  return NS_OK;
}

}  // namespace gmp
}  // namespace mozilla
