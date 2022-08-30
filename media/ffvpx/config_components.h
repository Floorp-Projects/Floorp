/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZ_FFVPX_CONFIG__COMPONENTS_H
#define MOZ_FFVPX_CONFIG__COMPONENTS_H


#ifdef CONFIG_VAAPI
#undef CONFIG_VAAPI
#undef CONFIG_VAAPI_1
#undef CONFIG_VP8_VAAPI_HWACCEL
#undef CONFIG_VP9_VAAPI_HWACCEL
#undef CONFIG_AV1_VAAPI_HWACCEL
#endif

#if defined(MOZ_FFVPX_AUDIOONLY)
#    include "config_components_audio_only.h"
#else
#    include "config_components_audio_video.h"
#endif

#include "config_override.h"

#endif // MOZ_FFVPX_CONFIG__COMPONENTS_H
