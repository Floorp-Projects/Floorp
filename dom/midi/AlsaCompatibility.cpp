/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Assertions.h"

// The code in this file is a workaround for building with ALSA versions prior
// to 1.0.29. The snd_pcm_sw_params_set_tstamp_type() and
// snd_pcm_sw_params_get_tstamp_type() functions are missing from those versions
// and we need them for the alsa crate which in turn is a dependency of the
// midir crate. The functions are not actually used so we provide dummy
// implementations that return an error. This file can be safely removed when
// the Linux sysroot will be updated to Debian 9 (or higher)
#include <alsa/asoundlib.h>

#if (SND_LIB_MAJOR == 1) && (SND_LIB_MINOR == 0) && (SND_LIB_SUBMINOR < 29)

extern "C" {

int snd_pcm_sw_params_set_tstamp_type(void) {
  MOZ_CRASH(
      "The replacement for snd_pcm_sw_params_set_tstamp_type() should never be "
      "called");
  return -1;
}

int snd_pcm_sw_params_get_tstamp_type(void) {
  MOZ_CRASH(
      "The replacement for snd_pcm_sw_params_get_tstamp_type() should never be "
      "called");
  return -1;
}
}

#endif
