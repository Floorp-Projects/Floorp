/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "RemoteDecoderModule.h"

#include "base/thread.h"
#include "mozilla/dom/ContentChild.h"  // for launching RDD w/ ContentChild
#include "mozilla/layers/SynchronousTask.h"
#include "mozilla/StaticPrefs.h"

#ifdef MOZ_AV1
#include "AOMDecoder.h"
#endif
#include "RemoteDecoderManagerChild.h"
#include "RemoteMediaDataDecoder.h"
#include "RemoteVideoDecoderChild.h"

namespace mozilla {

using base::Thread;
using dom::ContentChild;
using namespace ipc;
using namespace layers;

bool RemoteDecoderModule::SupportsMimeType(
    const nsACString& aMimeType, DecoderDoctorDiagnostics* aDiagnostics) const {
  bool supports = false;

#ifdef MOZ_AV1
  if (StaticPrefs::MediaAv1Enabled()) {
    supports |= AOMDecoder::IsAV1(aMimeType);
  }
#endif
  MOZ_LOG(
      sPDMLog, LogLevel::Debug,
      ("Sandbox decoder %s requested type", supports ? "supports" : "rejects"));
  return supports;
}

already_AddRefed<MediaDataDecoder> RemoteDecoderModule::CreateVideoDecoder(
    const CreateDecoderParams& aParams) {
  if (XRE_IsContentProcess()) {
    ContentChild* contentChild = ContentChild::GetSingleton();
    contentChild->LaunchRDDProcess();
  }

  if (!RemoteDecoderManagerChild::GetManagerThread()) {
    return nullptr;
  }

  RemoteVideoDecoderChild* child = new RemoteVideoDecoderChild();
  RefPtr<RemoteMediaDataDecoder> object = new RemoteMediaDataDecoder(
      child, RemoteDecoderManagerChild::GetManagerThread(),
      RemoteDecoderManagerChild::GetManagerAbstractThread());

  // (per Matt Woodrow) We can't use NS_DISPATCH_SYNC here since that
  // can spin the event loop while it waits.
  SynchronousTask task("InitIPDL");
  MediaResult result(NS_OK);
  RemoteDecoderManagerChild::GetManagerThread()->Dispatch(
      NS_NewRunnableFunction("dom::RemoteDecoderModule::CreateVideoDecoder",
                             [&, child]() {
                               AutoCompleteTask complete(&task);
                               result = child->InitIPDL(aParams.VideoConfig(),
                                                        aParams.mRate.mValue,
                                                        aParams.mOptions);
                             }),
      NS_DISPATCH_NORMAL);
  task.Wait();

  if (NS_FAILED(result)) {
    if (aParams.mError) {
      *aParams.mError = result;
    }
    return nullptr;
  }

  return object.forget();
}

}  // namespace mozilla
