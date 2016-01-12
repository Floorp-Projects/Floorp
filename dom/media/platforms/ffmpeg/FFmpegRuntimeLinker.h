/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __FFmpegRuntimeLinker_h__
#define __FFmpegRuntimeLinker_h__

#include "PlatformDecoderModule.h"
#include <stdint.h>

struct PRLibrary;

namespace mozilla
{

enum {
  AV_FUNC_AVUTIL_MASK = 1 << 8,
  AV_FUNC_53 = 1 << 0,
  AV_FUNC_54 = 1 << 1,
  AV_FUNC_55 = 1 << 2,
  AV_FUNC_56 = 1 << 3,
  AV_FUNC_57 = 1 << 4,
  AV_FUNC_AVUTIL_53 = AV_FUNC_53 | AV_FUNC_AVUTIL_MASK,
  AV_FUNC_AVUTIL_54 = AV_FUNC_54 | AV_FUNC_AVUTIL_MASK,
  AV_FUNC_AVUTIL_55 = AV_FUNC_55 | AV_FUNC_AVUTIL_MASK,
  AV_FUNC_AVUTIL_56 = AV_FUNC_56 | AV_FUNC_AVUTIL_MASK,
  AV_FUNC_AVUTIL_57 = AV_FUNC_57 | AV_FUNC_AVUTIL_MASK,
  AV_FUNC_AVCODEC_ALL = AV_FUNC_53 | AV_FUNC_54 | AV_FUNC_55 | AV_FUNC_56 | AV_FUNC_57,
  AV_FUNC_AVUTIL_ALL = AV_FUNC_AVCODEC_ALL | AV_FUNC_AVUTIL_MASK
};

class FFmpegRuntimeLinker
{
public:
  static bool Link();
  static void Unlink();
  static already_AddRefed<PlatformDecoderModule> CreateDecoderModule();
  static bool GetVersion(uint32_t& aMajor, uint32_t& aMinor, uint32_t& aMicro);

private:
  static PRLibrary* sLinkedLib;
  static PRLibrary* sLinkedUtilLib;

  static bool Bind(const char* aLibName);

  static enum LinkStatus {
    LinkStatus_INIT = 0,
    LinkStatus_FAILED,
    LinkStatus_SUCCEEDED
  } sLinkStatus;
};

}

#endif // __FFmpegRuntimeLinker_h__
