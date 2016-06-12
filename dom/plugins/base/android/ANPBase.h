/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <stdlib.h>
#include "android_npapi.h"
#include "nsISupportsImpl.h"

#define NOT_IMPLEMENTED_FATAL() do {                                    \
    __android_log_print(ANDROID_LOG_ERROR, "GeckoPlugins",              \
                        "%s not implemented %s, %d",                    \
                        __PRETTY_FUNCTION__, __FILE__, __LINE__);       \
    abort();                                                            \
  } while(0)

#define NOT_IMPLEMENTED()                                               \
    __android_log_print(ANDROID_LOG_ERROR, "GeckoPlugins",              \
                        "!!!!!!!!!!!!!!  %s not implemented %s, %d",    \
                        __PRETTY_FUNCTION__, __FILE__, __LINE__);       \

void InitAudioTrackInterfaceV0(ANPAudioTrackInterfaceV0 *i);
void InitAudioTrackInterfaceV1(ANPAudioTrackInterfaceV1* i);
void InitCanvasInterface(ANPCanvasInterfaceV0 *i);
void InitEventInterface(ANPEventInterfaceV0 *i);
void InitLogInterface(ANPLogInterfaceV0 *i);
void InitPaintInterface(ANPPaintInterfaceV0 *i);
void InitSurfaceInterface(ANPSurfaceInterfaceV0 *i);
void InitSystemInterfaceV1(ANPSystemInterfaceV1 *i);
void InitSystemInterfaceV2(ANPSystemInterfaceV2 *i);
void InitTypeFaceInterface(ANPTypefaceInterfaceV0 *i);
void InitWindowInterface(ANPWindowInterfaceV0 *i);
void InitWindowInterfaceV2(ANPWindowInterfaceV2 *i);
void InitVideoInterfaceV1(ANPVideoInterfaceV1 *i);
void InitOpenGLInterface(ANPOpenGLInterfaceV0 *i);
void InitNativeWindowInterface(ANPNativeWindowInterfaceV0 *i);
