/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AudioDeviceInfo.h"
#include "MediaManager.h"
#include "gmock/gmock.h"
#include "gtest/gtest-printers.h"
#include "gtest/gtest.h"
#include "mozilla/Attributes.h"
#include "mozilla/UniquePtr.h"
#include "nsTArray.h"
#include "webrtc/MediaEngineFake.h"

using ::testing::Return;
using namespace mozilla;

void PrintTo(const nsString& aValue, ::std::ostream* aStream) {
  NS_ConvertUTF16toUTF8 str(aValue);
  (*aStream) << str.get();
}
void PrintTo(const nsCString& aValue, ::std::ostream* aStream) {
  (*aStream) << aValue.get();
}

RefPtr<AudioDeviceInfo> MakeAudioDeviceInfo(const nsAString& aName,
                                            const nsAString& aGroupId,
                                            uint16_t aType) {
  return MakeRefPtr<AudioDeviceInfo>(
      nullptr, aName, aGroupId, u"Vendor"_ns, aType,
      AudioDeviceInfo::STATE_ENABLED, AudioDeviceInfo::PREF_NONE,
      AudioDeviceInfo::FMT_F32LE, AudioDeviceInfo::FMT_F32LE, 2u, 44100u,
      44100u, 44100u, 0, 0);
}

RefPtr<MediaDevice> MakeCameraDevice(const nsString& aName,
                                     const nsString& aGroupId) {
  return new MediaDevice(new MediaEngineFake(), dom::MediaSourceEnum::Camera,
                         aName, u""_ns, aGroupId, MediaDevice::IsScary::No);
}

RefPtr<MediaDevice> MakeMicDevice(const nsString& aName,
                                  const nsString& aGroupId) {
  return new MediaDevice(
      new MediaEngineFake(),
      MakeAudioDeviceInfo(aName, aGroupId, AudioDeviceInfo::TYPE_INPUT),
      u""_ns);
}

RefPtr<MediaDevice> MakeSpeakerDevice(const nsString& aName,
                                      const nsString& aGroupId) {
  return new MediaDevice(
      new MediaEngineFake(),
      MakeAudioDeviceInfo(aName, aGroupId, AudioDeviceInfo::TYPE_OUTPUT),
      u"ID"_ns);
}

/* Verify that when an audio input device name contains the video input device
 * name the video device group id is updated to become equal to the audio
 * device group id. */
TEST(TestGroupId, MatchInput_PartOfName)
{
  MediaManager::MediaDeviceSet devices;
  MediaManager::MediaDeviceSet audios;

  devices.AppendElement(
      MakeCameraDevice(u"Vendor Model"_ns, u"Cam-Model-GroupId"_ns));

  auto mic =
      MakeMicDevice(u"Vendor Model Analog Stereo"_ns, u"Mic-Model-GroupId"_ns);
  devices.AppendElement(mic);
  audios.AppendElement(mic);

  MediaManager::GuessVideoDeviceGroupIDs(devices, audios);

  EXPECT_EQ(devices[0]->mRawGroupID, devices[1]->mRawGroupID)
      << "Video group id is the same as audio input group id.";
}

/* Verify that when an audio input device name is the same as the video input
 * device name the video device group id is updated to become equal to the audio
 * device group id. */
TEST(TestGroupId, MatchInput_FullName)
{
  MediaManager::MediaDeviceSet devices;
  MediaManager::MediaDeviceSet audios;

  devices.AppendElement(
      MakeCameraDevice(u"Vendor Model"_ns, u"Cam-Model-GroupId"_ns));

  auto mic = MakeMicDevice(u"Vendor Model"_ns, u"Mic-Model-GroupId"_ns);
  devices.AppendElement(mic);
  audios.AppendElement(mic);

  MediaManager::GuessVideoDeviceGroupIDs(devices, audios);

  EXPECT_EQ(devices[0]->mRawGroupID, devices[1]->mRawGroupID)
      << "Video group id is the same as audio input group id.";
}

/* Verify that when an audio input device name does not contain the video input
 * device name the video device group id does not change. */
TEST(TestGroupId, NoMatchInput)
{
  MediaManager::MediaDeviceSet devices;
  MediaManager::MediaDeviceSet audios;

  nsString Cam_Model_GroupId = u"Cam-Model-GroupId"_ns;
  devices.AppendElement(
      MakeCameraDevice(u"Vendor Model"_ns, Cam_Model_GroupId));

  audios.AppendElement(
      MakeMicDevice(u"Model Analog Stereo"_ns, u"Mic-Model-GroupId"_ns));

  MediaManager::GuessVideoDeviceGroupIDs(devices, audios);

  EXPECT_EQ(devices[0]->mRawGroupID, Cam_Model_GroupId)
      << "Video group id has not been updated.";
  EXPECT_NE(devices[0]->mRawGroupID, audios[0]->mRawGroupID)
      << "Video group id is different than audio input group id.";
}

/* Verify that when more that one audio input and more than one audio output
 * device name contain the video input device name the video device group id
 * does not change. */
TEST(TestGroupId, NoMatch_TwoIdenticalDevices)
{
  MediaManager::MediaDeviceSet devices;
  MediaManager::MediaDeviceSet audios;

  nsString Cam_Model_GroupId = u"Cam-Model-GroupId"_ns;
  devices.AppendElement(
      MakeCameraDevice(u"Vendor Model"_ns, Cam_Model_GroupId));

  audios.AppendElement(
      MakeMicDevice(u"Vendor Model Analog Stereo"_ns, u"Mic-Model-GroupId"_ns));
  audios.AppendElement(
      MakeMicDevice(u"Vendor Model Analog Stereo"_ns, u"Mic-Model-GroupId"_ns));

  audios.AppendElement(MakeSpeakerDevice(u"Vendor Model Analog Stereo"_ns,
                                         u"Speaker-Model-GroupId"_ns));
  audios.AppendElement(MakeSpeakerDevice(u"Vendor Model Analog Stereo"_ns,
                                         u"Speaker-Model-GroupId"_ns));

  MediaManager::GuessVideoDeviceGroupIDs(devices, audios);

  EXPECT_EQ(devices[0]->mRawGroupID, Cam_Model_GroupId)
      << "Video group id has not been updated.";
  EXPECT_NE(devices[0]->mRawGroupID, audios[0]->mRawGroupID)
      << "Video group id is different from audio input group id.";
  EXPECT_NE(devices[0]->mRawGroupID, audios[2]->mRawGroupID)
      << "Video group id is different from audio output group id.";
}

/* Verify that when more that one audio input device name contain the video
 * input device name the video device group id is not updated by audio input
 * device group id but it continues looking at audio output devices where it
 * finds a match so video input group id is updated by audio output group id. */
TEST(TestGroupId, Match_TwoIdenticalInputsMatchOutput)
{
  MediaManager::MediaDeviceSet devices;
  MediaManager::MediaDeviceSet audios;

  nsString Cam_Model_GroupId = u"Cam-Model-GroupId"_ns;
  devices.AppendElement(
      MakeCameraDevice(u"Vendor Model"_ns, Cam_Model_GroupId));

  audios.AppendElement(
      MakeMicDevice(u"Vendor Model Analog Stereo"_ns, u"Mic-Model-GroupId"_ns));
  audios.AppendElement(
      MakeMicDevice(u"Vendor Model Analog Stereo"_ns, u"Mic-Model-GroupId"_ns));

  audios.AppendElement(MakeSpeakerDevice(u"Vendor Model Analog Stereo"_ns,
                                         u"Speaker-Model-GroupId"_ns));

  MediaManager::GuessVideoDeviceGroupIDs(devices, audios);

  EXPECT_EQ(devices[0]->mRawGroupID, audios[2]->mRawGroupID)
      << "Video group id is the same as audio output group id.";
}

/* Verify that when more that one audio input and more than one audio output
 * device names contain the video input device name the video device group id
 * does not change. */
TEST(TestGroupId, NoMatch_ThreeIdenticalDevices)
{
  MediaManager::MediaDeviceSet devices;
  MediaManager::MediaDeviceSet audios;

  nsString Cam_Model_GroupId = u"Cam-Model-GroupId"_ns;
  devices.AppendElement(
      MakeCameraDevice(u"Vendor Model"_ns, Cam_Model_GroupId));

  audios.AppendElement(
      MakeMicDevice(u"Vendor Model Analog Stereo"_ns, u"Mic-Model-GroupId"_ns));
  audios.AppendElement(
      MakeMicDevice(u"Vendor Model Analog Stereo"_ns, u"Mic-Model-GroupId"_ns));
  audios.AppendElement(
      MakeMicDevice(u"Vendor Model Analog Stereo"_ns, u"Mic-Model-GroupId"_ns));

  audios.AppendElement(MakeSpeakerDevice(u"Vendor Model Analog Stereo"_ns,
                                         u"Speaker-Model-GroupId"_ns));
  audios.AppendElement(MakeSpeakerDevice(u"Vendor Model Analog Stereo"_ns,
                                         u"Speaker-Model-GroupId"_ns));
  audios.AppendElement(MakeSpeakerDevice(u"Vendor Model Analog Stereo"_ns,
                                         u"Speaker-Model-GroupId"_ns));

  MediaManager::GuessVideoDeviceGroupIDs(devices, audios);

  EXPECT_EQ(devices[0]->mRawGroupID, Cam_Model_GroupId)
      << "Video group id has not been updated.";
  EXPECT_NE(devices[0]->mRawGroupID, audios[0]->mRawGroupID)
      << "Video group id is different from audio input group id.";
  EXPECT_NE(devices[0]->mRawGroupID, audios[3]->mRawGroupID)
      << "Video group id is different from audio output group id.";
}

/* Verify that when an audio output device name contains the video input device
 * name the video device group id is updated to become equal to the audio
 * device group id. */
TEST(TestGroupId, MatchOutput)
{
  MediaManager::MediaDeviceSet devices;
  MediaManager::MediaDeviceSet audios;

  devices.AppendElement(
      MakeCameraDevice(u"Vendor Model"_ns, u"Cam-Model-GroupId"_ns));

  audios.AppendElement(
      MakeMicDevice(u"Mic Analog Stereo"_ns, u"Mic-Model-GroupId"_ns));

  audios.AppendElement(MakeSpeakerDevice(u"Vendor Model Analog Stereo"_ns,
                                         u"Speaker-Model-GroupId"_ns));

  MediaManager::GuessVideoDeviceGroupIDs(devices, audios);

  EXPECT_EQ(devices[0]->mRawGroupID, audios[1]->mRawGroupID)
      << "Video group id is the same as audio output group id.";
}

/* Verify that when an audio input device name is the same as audio output
 * device and video input device name the video device group id is updated to
 * become equal to the audio input device group id. */
TEST(TestGroupId, InputOutputSameName)
{
  MediaManager::MediaDeviceSet devices;
  MediaManager::MediaDeviceSet audios;

  devices.AppendElement(
      MakeCameraDevice(u"Vendor Model"_ns, u"Cam-Model-GroupId"_ns));

  audios.AppendElement(
      MakeMicDevice(u"Vendor Model"_ns, u"Mic-Model-GroupId"_ns));

  audios.AppendElement(
      MakeSpeakerDevice(u"Vendor Model"_ns, u"Speaker-Model-GroupId"_ns));

  MediaManager::GuessVideoDeviceGroupIDs(devices, audios);

  EXPECT_EQ(devices[0]->mRawGroupID, audios[0]->mRawGroupID)
      << "Video input group id is the same as audio input group id.";
}

/* Verify that when an audio input device name contains the video input device
 * and the audio input group id is an empty string, the video device group id
 * is updated to become equal to the audio device group id. */
TEST(TestGroupId, InputEmptyGroupId)
{
  MediaManager::MediaDeviceSet devices;
  MediaManager::MediaDeviceSet audios;

  devices.AppendElement(
      MakeCameraDevice(u"Vendor Model"_ns, u"Cam-Model-GroupId"_ns));

  audios.AppendElement(MakeMicDevice(u"Vendor Model"_ns, u""_ns));

  MediaManager::GuessVideoDeviceGroupIDs(devices, audios);

  EXPECT_EQ(devices[0]->mRawGroupID, audios[0]->mRawGroupID)
      << "Video input group id is the same as audio input group id.";
}

/* Verify that when an audio output device name contains the video input device
 * and the audio output group id is an empty string, the video device group id
 * is updated to become equal to the audio output device group id. */
TEST(TestGroupId, OutputEmptyGroupId)
{
  MediaManager::MediaDeviceSet devices;
  MediaManager::MediaDeviceSet audios;

  devices.AppendElement(
      MakeCameraDevice(u"Vendor Model"_ns, u"Cam-Model-GroupId"_ns));

  audios.AppendElement(MakeSpeakerDevice(u"Vendor Model"_ns, u""_ns));

  MediaManager::GuessVideoDeviceGroupIDs(devices, audios);

  EXPECT_EQ(devices[0]->mRawGroupID, audios[0]->mRawGroupID)
      << "Video input group id is the same as audio output group id.";
}
