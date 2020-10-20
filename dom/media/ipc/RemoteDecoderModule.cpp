/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "RemoteDecoderModule.h"

#include "mozilla/StaticPrefs_media.h"
#include "mozilla/SyncRunnable.h"
#include "mozilla/dom/ContentChild.h"  // for launching RDD w/ ContentChild
#include "mozilla/layers/SynchronousTask.h"

#ifdef MOZ_AV1
#  include "AOMDecoder.h"
#endif
#include "OpusDecoder.h"
#include "RemoteAudioDecoder.h"
#include "RemoteDecoderManagerChild.h"
#include "RemoteMediaDataDecoder.h"
#include "RemoteVideoDecoder.h"
#include "VideoUtils.h"
#include "VorbisDecoder.h"
#include "WAVDecoder.h"
#include "nsIXULRuntime.h"  // for BrowserTabsRemoteAutostart

namespace mozilla {

using dom::ContentChild;
using namespace ipc;
using namespace layers;

/* static */
void RemoteDecoderModule::Init() {
  MOZ_ASSERT(NS_IsMainThread());

  if (!BrowserTabsRemoteAutostart()) {
    return;
  }
  RemoteDecoderManagerChild::InitializeThread();
}

bool RemoteDecoderModule::SupportsMimeType(
    const nsACString& aMimeType, DecoderDoctorDiagnostics* aDiagnostics) const {
  bool supports = false;

#ifdef MOZ_AV1
  if (StaticPrefs::media_av1_enabled()) {
    supports |= AOMDecoder::IsAV1(aMimeType);
  }
#endif
#if !defined(__MINGW32__)
  // We can't let RDD handle the decision to support Vorbis decoding on
  // MinGW builds because of Bug 1597408 (Vorbis decoding on RDD causing
  // sandboxing failure on MinGW-clang).  Typically this would be dealt
  // with using defines in StaticPrefList.yaml, but we must handle it
  // here because of Bug 1598426 (the __MINGW32__ define isn't supported
  // in StaticPrefList.yaml).
  if (StaticPrefs::media_rdd_vorbis_enabled()) {
    supports |= VorbisDataDecoder::IsVorbis(aMimeType);
  }
#endif
  if (StaticPrefs::media_rdd_wav_enabled()) {
    supports |= WaveDataDecoder::IsWave(aMimeType);
  }
  if (StaticPrefs::media_rdd_opus_enabled()) {
    supports |= OpusDataDecoder::IsOpus(aMimeType);
  }

  MOZ_LOG(
      sPDMLog, LogLevel::Debug,
      ("Sandbox decoder %s requested type", supports ? "supports" : "rejects"));
  return supports;
}

already_AddRefed<MediaDataDecoder> RemoteDecoderModule::CreateAudioDecoder(
    const CreateDecoderParams& aParams) {
  RemoteDecoderManagerChild::LaunchRDDProcessIfNeeded();

  // OpusDataDecoder will check this option to provide the same info
  // that IsDefaultPlaybackDeviceMono provides.  We want to avoid calls
  // to IsDefaultPlaybackDeviceMono on RDD because initializing audio
  // backends on RDD will be blocked by the sandbox.
  if (OpusDataDecoder::IsOpus(aParams.mConfig.mMimeType) &&
      IsDefaultPlaybackDeviceMono()) {
    CreateDecoderParams params = aParams;
    params.mOptions += CreateDecoderParams::Option::DefaultPlaybackDeviceMono;
    return RemoteDecoderManagerChild::CreateAudioDecoder(params);
  }

  return RemoteDecoderManagerChild::CreateAudioDecoder(aParams);
}

already_AddRefed<MediaDataDecoder> RemoteDecoderModule::CreateVideoDecoder(
    const CreateDecoderParams& aParams) {
  RemoteDecoderManagerChild::LaunchRDDProcessIfNeeded();
  return RemoteDecoderManagerChild::CreateVideoDecoder(
      aParams, RemoteDecodeIn::RddProcess);
}

}  // namespace mozilla
