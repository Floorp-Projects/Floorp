/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef VORBISUTILS_H_
#define VORBISUTILS_H_

#ifdef MOZ_SAMPLE_TYPE_S16
#include <ogg/os_types.h>
typedef ogg_int32_t VorbisPCMValue;

#define MOZ_CLIP_TO_15(x) ((x)<-32768?-32768:(x)<=32767?(x):32767)
// Convert the output of vorbis_synthesis_pcmout to a AudioDataValue
#define MOZ_CONVERT_VORBIS_SAMPLE(x) \
 (static_cast<AudioDataValue>(MOZ_CLIP_TO_15((x)>>9)))

#else /* MOZ_SAMPLE_TYPE_FLOAT32 */

typedef float VorbisPCMValue;

#define MOZ_CONVERT_VORBIS_SAMPLE(x) (x)

#endif

#endif /* VORBISUTILS_H_ */
