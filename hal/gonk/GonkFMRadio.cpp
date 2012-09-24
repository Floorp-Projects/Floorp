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

#include <errno.h>
#include <fcntl.h>
#include <linux/videodev2.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>

namespace mozilla {
namespace hal_impl {

uint32_t GetFMRadioFrequency();

static int sRadioFD;
static bool sRadioEnabled;
static pthread_t sMonitorThread;

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
public:
  RadioUpdate(hal::FMRadioOperation op)
    : mOp(op)
  {}

  NS_IMETHOD Run() {
    hal::FMRadioOperationInformation info;
    info.operation() = mOp;
    info.status() = hal::FM_RADIO_OPERATION_STATUS_SUCCESS;
    info.frequency() = GetFMRadioFrequency();
    hal::NotifyFMRadioStatus(info);
    return NS_OK;
  }
};

static void *
pollTavaruaRadio(void *)
{
  uint8_t buf[128];
  struct v4l2_buffer buffer = {0};
  buffer.index = 1;
  buffer.type = V4L2_BUF_TYPE_PRIVATE;
  buffer.length = sizeof(buf);
  buffer.m.userptr = (long unsigned int)buf;

  while (sRadioEnabled) {
    int rc = ioctl(sRadioFD, VIDIOC_DQBUF, &buffer);
    if (rc)
      return NULL;

    for (unsigned int i = 0; i < buffer.bytesused; i++) {
      switch (buf[i]) {
      case TAVARUA_EVT_RADIO_READY:
        NS_DispatchToMainThread(new RadioUpdate(hal::FM_RADIO_OPERATION_ENABLE));
        break;
      case TAVARUA_EVT_SEEK_COMPLETE:
        NS_DispatchToMainThread(new RadioUpdate(hal::FM_RADIO_OPERATION_SEEK));
        break;
      default:
        break;
      }
    }
  }

  return NULL;
}

void
EnableFMRadio(const hal::FMRadioSettings& aInfo)
{
  if (sRadioEnabled) {
    HAL_LOG(("Radio already enabled!"));
    return;
  }

  mozilla::ScopedClose fd(open("/dev/radio0", O_RDWR));
  if (fd < 0) {
    HAL_LOG(("Unable to open radio device"));
    return;
  }

  struct v4l2_capability cap;
  int rc = ioctl(fd, VIDIOC_QUERYCAP, &cap);
  if (rc < 0) {
    HAL_LOG(("Unable to query radio device"));
    return;
  }

  HAL_LOG(("Radio: %s (%s)\n", cap.driver, cap.card));

  if (!(cap.capabilities & V4L2_CAP_RADIO)) {
    HAL_LOG(("/dev/radio0 isn't a radio"));
    return;
  }

  if (!(cap.capabilities & V4L2_CAP_TUNER)) {
    HAL_LOG(("/dev/radio0 doesn't support the tuner interface"));
    return;
  }
  sRadioFD = fd;

  // Tavarua specific start
  char command[64];
  snprintf(command, sizeof(command), "/system/bin/fm_qsoc_patches %d 0", cap.version);
  rc = system(command);
  if (rc) {
    HAL_LOG(("Unable to initialize radio"));
    return;
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

  rc = setControl(V4L2_CID_PRIVATE_TAVARUA_SET_AUDIO_PATH, FM_DIGITAL_PATH);
  if (rc < 0) {
    HAL_LOG(("Unable to set audio path"));
    return;
  }

  pthread_create(&sMonitorThread, NULL, pollTavaruaRadio, NULL);
  // Tavarua specific end

  fd.forget();
  sRadioEnabled = true;
}

void
DisableFMRadio()
{
  if (!sRadioEnabled)
    return;

  sRadioEnabled = false;

  // Tavarua specific start
  int rc = setControl(V4L2_CID_PRIVATE_TAVARUA_STATE, FM_OFF);
  if (rc < 0) {
    HAL_LOG(("Unable to turn off radio"));
  }
  // Tavarua specific end

  pthread_join(sMonitorThread, NULL);

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
  int rc = ioctl(sRadioFD, VIDIOC_S_HW_FREQ_SEEK, &seek);
  if (rc < 0) {
    HAL_LOG(("Could not initiate hardware seek"));
    return;
  }
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
