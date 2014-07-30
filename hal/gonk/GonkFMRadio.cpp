/* Copyright 2012 Mozilla Foundation and Mozilla contributors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "Hal.h"
#include "tavarua.h"
#include "nsThreadUtils.h"
#include "mozilla/FileUtils.h"

#include <cutils/properties.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/videodev2.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>


/* Bionic might not have the newer version of the v4l2 headers that
 * define these controls, so we define them here if they're not found.
 */
#ifndef V4L2_CTRL_CLASS_FM_RX
#define V4L2_CTRL_CLASS_FM_RX 0x00a10000
#define V4L2_CID_FM_RX_CLASS_BASE (V4L2_CTRL_CLASS_FM_RX | 0x900)
#define V4L2_CID_TUNE_DEEMPHASIS  (V4L2_CID_FM_RX_CLASS_BASE + 1)
#define V4L2_DEEMPHASIS_DISABLED  0
#define V4L2_DEEMPHASIS_50_uS     1
#define V4L2_DEEMPHASIS_75_uS     2
#define V4L2_CID_RDS_RECEPTION    (V4L2_CID_FM_RX_CLASS_BASE + 2)
#endif

namespace mozilla {
namespace hal_impl {

uint32_t GetFMRadioFrequency();

static int sRadioFD;
static bool sRadioEnabled;
static pthread_t sRadioThread;
static hal::FMRadioSettings sRadioSettings;
static int sMsmFMVersion;
static bool sMsmFMMode;

static int
setControl(uint32_t id, int32_t value)
{
  struct v4l2_control control;
  control.id = id;
  control.value = value;
  return ioctl(sRadioFD, VIDIOC_S_CTRL, &control);
}

class RadioUpdate : public nsRunnable {
  hal::FMRadioOperation mOp;
  hal::FMRadioOperationStatus mStatus;
public:
  RadioUpdate(hal::FMRadioOperation op, hal::FMRadioOperationStatus status)
    : mOp(op)
    , mStatus(status)
  {}

  NS_IMETHOD Run() {
    hal::FMRadioOperationInformation info;
    info.operation() = mOp;
    info.status() = mStatus;
    info.frequency() = GetFMRadioFrequency();
    hal::NotifyFMRadioStatus(info);
    return NS_OK;
  }
};

/* Runs on the radio thread */
static void
initMsmFMRadio(hal::FMRadioSettings &aInfo)
{
  mozilla::ScopedClose fd(sRadioFD);
  char version[64];
  int rc;
  snprintf(version, sizeof(version), "%d", sMsmFMVersion);
  property_set("hw.fm.version", version);

  /* Set the mode for soc downloader */
  property_set("hw.fm.mode", "normal");
  /* start fm_dl service */
  property_set("ctl.start", "fm_dl");

  /*
   * Fix bug 800263. Wait until the FM radio chips initialization is done
   * then set other properties, or the system will hang and reboot. This
   * work around is from codeaurora
   * (git://codeaurora.org/platform/frameworks/base.git).
   */
  for (int i = 0; i < 4; ++i) {
    sleep(1);
    char value[PROPERTY_VALUE_MAX];
    property_get("hw.fm.init", value, "0");
    if (!strcmp(value, "1")) {
      break;
    }
  }

  rc = setControl(V4L2_CID_PRIVATE_TAVARUA_STATE, FM_RECV);
  if (rc < 0) {
    HAL_LOG(("Unable to turn on radio |%s|", strerror(errno)));
    return;
  }

  int preEmphasis = aInfo.preEmphasis() <= 50;
  rc = setControl(V4L2_CID_PRIVATE_TAVARUA_EMPHASIS, preEmphasis);
  if (rc) {
    HAL_LOG(("Unable to configure preemphasis"));
    return;
  }

  rc = setControl(V4L2_CID_PRIVATE_TAVARUA_RDS_STD, 0);
  if (rc) {
    HAL_LOG(("Unable to configure RDS"));
    return;
  }

  int spacing;
  switch (aInfo.spaceType()) {
  case 50:
    spacing = FM_CH_SPACE_50KHZ;
    break;
  case 100:
    spacing = FM_CH_SPACE_100KHZ;
    break;
  case 200:
    spacing = FM_CH_SPACE_200KHZ;
    break;
  default:
    HAL_LOG(("Unsupported space value - %d", aInfo.spaceType()));
    return;
  }

  rc = setControl(V4L2_CID_PRIVATE_TAVARUA_SPACING, spacing);
  if (rc) {
    HAL_LOG(("Unable to configure spacing"));
    return;
  }

  /*
   * Frequency conversions
   *
   * HAL uses units of 1k for frequencies
   * V4L2 uses units of 62.5kHz
   * Multiplying by (10000 / 625) converts from HAL units to V4L2.
   */

  struct v4l2_tuner tuner = {0};
  tuner.rangelow = (aInfo.lowerLimit() * 10000) / 625;
  tuner.rangehigh = (aInfo.upperLimit() * 10000) / 625;
  tuner.audmode = V4L2_TUNER_MODE_STEREO;
  rc = ioctl(fd, VIDIOC_S_TUNER, &tuner);
  if (rc < 0) {
    HAL_LOG(("Unable to adjust band limits"));
    return;
  }

  rc = setControl(V4L2_CID_PRIVATE_TAVARUA_REGION, TAVARUA_REGION_OTHER);
  if (rc < 0) {
    HAL_LOG(("Unable to configure region"));
    return;
  }

  // Some devices do not support analog audio routing. This should be
  // indicated by the 'ro.moz.fm.noAnalog' property at build time.
  char propval[PROPERTY_VALUE_MAX];
  property_get("ro.moz.fm.noAnalog", propval, "");
  bool noAnalog = !strcmp(propval, "true");

  rc = setControl(V4L2_CID_PRIVATE_TAVARUA_SET_AUDIO_PATH,
                  noAnalog ? FM_DIGITAL_PATH : FM_ANALOG_PATH);
  if (rc < 0) {
    HAL_LOG(("Unable to set audio path"));
    return;
  }

  if (!noAnalog) {
    /* Set the mode for soc downloader */
    property_set("hw.fm.mode", "config_dac");
    /* Use analog mode FM */
    property_set("hw.fm.isAnalog", "true");
    /* start fm_dl service */
    property_set("ctl.start", "fm_dl");

    for (int i = 0; i < 4; ++i) {
      sleep(1);
      char value[PROPERTY_VALUE_MAX];
      property_get("hw.fm.init", value, "0");
      if (!strcmp(value, "1")) {
        break;
      }
    }
  }

  fd.forget();
  sRadioEnabled = true;
}

/* Runs on the radio thread */
static void *
runMsmFMRadio(void *)
{
  initMsmFMRadio(sRadioSettings);
  if (!sRadioEnabled) {
    NS_DispatchToMainThread(new RadioUpdate(hal::FM_RADIO_OPERATION_ENABLE,
                                            hal::FM_RADIO_OPERATION_STATUS_FAIL));
    return nullptr;
  }

  uint8_t buf[128];
  struct v4l2_buffer buffer = {0};
  buffer.index = 1;
  buffer.type = V4L2_BUF_TYPE_PRIVATE;
  buffer.length = sizeof(buf);
  buffer.m.userptr = (long unsigned int)buf;

  while (sRadioEnabled) {
    if (ioctl(sRadioFD, VIDIOC_DQBUF, &buffer) < 0) {
      if (errno == EINTR)
        continue;
      break;
    }

    /* The tavarua driver reports a number of things asynchronously.
     * In those cases, the status update comes from this thread. */
    for (unsigned int i = 0; i < buffer.bytesused; i++) {
      switch (buf[i]) {
      case TAVARUA_EVT_RADIO_READY:
        // The driver sends RADIO_READY both when we turn the radio on and when we turn 
        // the radio off.
        if (sRadioEnabled) {
          NS_DispatchToMainThread(new RadioUpdate(hal::FM_RADIO_OPERATION_ENABLE,
                                                  hal::FM_RADIO_OPERATION_STATUS_SUCCESS));
        }
        break;

      case TAVARUA_EVT_SEEK_COMPLETE:
        NS_DispatchToMainThread(new RadioUpdate(hal::FM_RADIO_OPERATION_SEEK,
                                                hal::FM_RADIO_OPERATION_STATUS_SUCCESS));
        break;
      case TAVARUA_EVT_TUNE_SUCC:
        NS_DispatchToMainThread(new RadioUpdate(hal::FM_RADIO_OPERATION_TUNE,
                                                hal::FM_RADIO_OPERATION_STATUS_SUCCESS));
        break;
      default:
        break;
      }
    }
  }

  return nullptr;
}

/* This runs on the main thread but most of the
 * initialization is pushed to the radio thread. */
void
EnableFMRadio(const hal::FMRadioSettings& aInfo)
{
  if (sRadioEnabled) {
    HAL_LOG(("Radio already enabled!"));
    return;
  }

  hal::FMRadioOperationInformation info;
  info.operation() = hal::FM_RADIO_OPERATION_ENABLE;
  info.status() = hal::FM_RADIO_OPERATION_STATUS_FAIL;

  mozilla::ScopedClose fd(open("/dev/radio0", O_RDWR));
  if (fd < 0) {
    HAL_LOG(("Unable to open radio device"));
    hal::NotifyFMRadioStatus(info);
    return;
  }

  struct v4l2_capability cap;
  int rc = ioctl(fd, VIDIOC_QUERYCAP, &cap);
  if (rc < 0) {
    HAL_LOG(("Unable to query radio device"));
    hal::NotifyFMRadioStatus(info);
    return;
  }

  sMsmFMMode = !strcmp((char *)cap.driver, "radio-tavarua") ||
      !strcmp((char *)cap.driver, "radio-iris");
  HAL_LOG(("Radio: %s (%s)\n", cap.driver, cap.card));

  if (!(cap.capabilities & V4L2_CAP_RADIO)) {
    HAL_LOG(("/dev/radio0 isn't a radio"));
    hal::NotifyFMRadioStatus(info);
    return;
  }

  if (!(cap.capabilities & V4L2_CAP_TUNER)) {
    HAL_LOG(("/dev/radio0 doesn't support the tuner interface"));
    hal::NotifyFMRadioStatus(info);
    return;
  }
  sRadioSettings = aInfo;

  if (sMsmFMMode) {
    sRadioFD = fd.forget();
    sMsmFMVersion = cap.version;
    if (pthread_create(&sRadioThread, nullptr, runMsmFMRadio, nullptr)) {
      HAL_LOG(("Couldn't create radio thread"));
      hal::NotifyFMRadioStatus(info);
    }
    return;
  }

  struct v4l2_tuner tuner = {0};
  tuner.type = V4L2_TUNER_RADIO;
  tuner.rangelow = (aInfo.lowerLimit() * 10000) / 625;
  tuner.rangehigh = (aInfo.upperLimit() * 10000) / 625;
  tuner.audmode = V4L2_TUNER_MODE_STEREO;
  rc = ioctl(fd, VIDIOC_S_TUNER, &tuner);
  if (rc < 0) {
    HAL_LOG(("Unable to adjust band limits"));
  }

  int emphasis;
  switch (aInfo.preEmphasis()) {
  case 0:
    emphasis = V4L2_DEEMPHASIS_DISABLED;
    break;
  case 50:
    emphasis = V4L2_DEEMPHASIS_50_uS;
    break;
  case 75:
    emphasis = V4L2_DEEMPHASIS_75_uS;
    break;
  default:
    MOZ_CRASH("Invalid preemphasis setting");
    break;
  }
  rc = setControl(V4L2_CID_TUNE_DEEMPHASIS, emphasis);
  if (rc < 0) {
    HAL_LOG(("Unable to configure deemphasis"));
  }

  sRadioFD = fd.forget();
  sRadioEnabled = true;

  info.status() = hal::FM_RADIO_OPERATION_STATUS_SUCCESS;
  hal::NotifyFMRadioStatus(info);
}

void
DisableFMRadio()
{
  if (!sRadioEnabled)
    return;

  sRadioEnabled = false;

  if (sMsmFMMode) {
    int rc = setControl(V4L2_CID_PRIVATE_TAVARUA_STATE, FM_OFF);
    if (rc < 0) {
      HAL_LOG(("Unable to turn off radio"));
    }

    pthread_join(sRadioThread, nullptr);
  }

  close(sRadioFD);

  hal::FMRadioOperationInformation info;
  info.operation() = hal::FM_RADIO_OPERATION_DISABLE;
  info.status() = hal::FM_RADIO_OPERATION_STATUS_SUCCESS;
  hal::NotifyFMRadioStatus(info);
}

void
FMRadioSeek(const hal::FMRadioSeekDirection& aDirection)
{
  struct v4l2_hw_freq_seek seek = {0};
  seek.type = V4L2_TUNER_RADIO;
  seek.seek_upward = aDirection == hal::FMRadioSeekDirection::FM_RADIO_SEEK_DIRECTION_UP;

  /* ICS and older don't have the spacing field */
#if ANDROID_VERSION == 15
  seek.reserved[0] = sRadioSettings.spaceType() * 1000;
#else
  seek.spacing = sRadioSettings.spaceType() * 1000;
#endif

  int rc = ioctl(sRadioFD, VIDIOC_S_HW_FREQ_SEEK, &seek);
  if (sMsmFMMode && rc >= 0)
    return;

  hal::FMRadioOperationInformation info;
  info.operation() = hal::FM_RADIO_OPERATION_SEEK;
  info.status() = rc < 0 ? hal::FM_RADIO_OPERATION_STATUS_FAIL :
                           hal::FM_RADIO_OPERATION_STATUS_SUCCESS;
  hal::NotifyFMRadioStatus(info);

  if (rc < 0) {
    HAL_LOG(("Could not initiate hardware seek"));
    return;
  }

  info.operation() = hal::FM_RADIO_OPERATION_TUNE;
  info.status() = hal::FM_RADIO_OPERATION_STATUS_SUCCESS;
  hal::NotifyFMRadioStatus(info);
}

void
GetFMRadioSettings(hal::FMRadioSettings* aInfo)
{
  if (!sRadioEnabled) {
    return;
  }

  struct v4l2_tuner tuner = {0};
  int rc = ioctl(sRadioFD, VIDIOC_G_TUNER, &tuner);
  if (rc < 0) {
    HAL_LOG(("Could not query fm radio for settings"));
    return;
  }

  aInfo->upperLimit() = (tuner.rangehigh * 625) / 10000;
  aInfo->lowerLimit() = (tuner.rangelow * 625) / 10000;
}

void
SetFMRadioFrequency(const uint32_t frequency)
{
  struct v4l2_frequency freq = {0};
  freq.type = V4L2_TUNER_RADIO;
  freq.frequency = (frequency * 10000) / 625;

  int rc = ioctl(sRadioFD, VIDIOC_S_FREQUENCY, &freq);
  if (rc < 0)
    HAL_LOG(("Could not set radio frequency"));

  if (sMsmFMMode && rc >= 0)
    return;

  hal::FMRadioOperationInformation info;
  info.operation() = hal::FM_RADIO_OPERATION_TUNE;
  info.status() = rc < 0 ? hal::FM_RADIO_OPERATION_STATUS_FAIL :
                           hal::FM_RADIO_OPERATION_STATUS_SUCCESS;
  hal::NotifyFMRadioStatus(info);
}

uint32_t
GetFMRadioFrequency()
{
  if (!sRadioEnabled)
    return 0;

  struct v4l2_frequency freq;
  int rc = ioctl(sRadioFD, VIDIOC_G_FREQUENCY, &freq);
  if (rc < 0) {
    HAL_LOG(("Could not get radio frequency"));
    return 0;
  }

  return (freq.frequency * 625) / 10000;
}

bool
IsFMRadioOn()
{
  return sRadioEnabled;
}

uint32_t
GetFMRadioSignalStrength()
{
  struct v4l2_tuner tuner = {0};
  int rc = ioctl(sRadioFD, VIDIOC_G_TUNER, &tuner);
  if (rc < 0) {
    HAL_LOG(("Could not query fm radio for signal strength"));
    return 0;
  }

  return tuner.signal;
}

void
CancelFMRadioSeek()
{}

} // hal_impl
} // namespace mozilla
