/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __FFmpegRuntimeLinker_h__
#define __FFmpegRuntimeLinker_h__

extern "C" {
#pragma GCC visibility push(default)
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
#pragma GCC visibility pop
}

#include "nsAutoPtr.h"

namespace mozilla
{

class FFmpegRuntimeLinker
{
public:
  static bool Link();
  static void Unlink();

private:
  static void* sLinkedLibs[];

  static enum LinkStatus {
    LinkStatus_INIT = 0,
    LinkStatus_FAILED,
    LinkStatus_SUCCEEDED
  } sLinkStatus;
};

#define AV_FUNC(lib, func) extern typeof(func)* func;
#include "FFmpegFunctionList.h"
#undef AV_FUNC
}

#endif // __FFmpegRuntimeLinker_h__
