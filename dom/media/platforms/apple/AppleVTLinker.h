/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef AppleVTLinker_h
#define AppleVTLinker_h

extern "C" {
#pragma GCC visibility push(default)
#include "VideoToolbox/VideoToolbox.h"
#pragma GCC visibility pop
}

#include "nscore.h"

namespace mozilla {

class AppleVTLinker
{
public:
  static bool Link();
  static void Unlink();
  static CFStringRef skPropEnableHWAccel;
  static CFStringRef skPropUsingHWAccel;

private:
  static void* sLink;

  static enum LinkStatus {
    LinkStatus_INIT = 0,
    LinkStatus_FAILED,
    LinkStatus_SUCCEEDED
  } sLinkStatus;

  static CFStringRef GetIOConst(const char* symbol);
};

#define LINK_FUNC(func) extern typeof(func)* func;
#include "AppleVTFunctions.h"
#undef LINK_FUNC

} // namespace mozilla

#endif // AppleVTLinker_h
