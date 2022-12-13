/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Assertions.h"

// The code in this file is a workaround for building with ALSA versions prior
// to 1.1. Various functions that the alsa crate (a dependency of the midir
// crate) use are missing from those versions. The functions are not actually
// used so we provide dummy implementations that return an error. This file
// can be safely removed when the Linux sysroot will be updated to Debian 9
// (or higher)
#include <alsa/asoundlib.h>

extern "C" {

#define ALSA_DIVERT(func)                       \
  int func(void) {                              \
    MOZ_CRASH(#func "should never be called."); \
    return -1;                                  \
  }

#if (SND_LIB_MAJOR == 1) && (SND_LIB_MINOR == 0) && (SND_LIB_SUBMINOR < 29)
ALSA_DIVERT(snd_pcm_sw_params_set_tstamp_type)
ALSA_DIVERT(snd_pcm_sw_params_get_tstamp_type)
#endif

#if (SND_LIB_MAJOR == 1) && (SND_LIB_MINOR < 1)
ALSA_DIVERT(snd_pcm_hw_params_supports_audio_ts_type)
ALSA_DIVERT(snd_pcm_status_set_audio_htstamp_config)
#endif
}
