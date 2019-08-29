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
#include "webrtc/MediaEngineSource.h"

using ::testing::Return;
using namespace mozilla;

void PrintTo(const nsString& aValue, ::std::ostream* aStream) {
  NS_ConvertUTF16toUTF8 str(aValue);
  (*aStream) << str.get();
}
void PrintTo(const nsCString& aValue, ::std::ostream* aStream) {
  (*aStream) << aValue.get();
}

class MockMediaEngineSource : public MediaEngineSource {
 public:
  MOCK_CONST_METHOD0(GetMediaSource, dom::MediaSourceEnum());

  /* Unused overrides */
  MOCK_CONST_METHOD0(GetName, nsString());
  MOCK_CONST_METHOD0(GetUUID, nsCString());
  MOCK_CONST_METHOD0(GetGroupId, nsString());
  MOCK_CONST_METHOD1(GetSettings, void(dom::MediaTrackSettings&));
  MOCK_METHOD4(Allocate, nsresult(const dom::MediaTrackConstraints&,
                                  const MediaEnginePrefs&,
                                  const ipc::PrincipalInfo&, const char**));
  MOCK_METHOD3(SetTrack, void(const RefPtr<SourceMediaStream>&, TrackID,
                              const PrincipalHandle&));
  MOCK_METHOD0(Start, nsresult());
  MOCK_METHOD3(Reconfigure, nsresult(const dom::MediaTrackConstraints&,
                                     const MediaEnginePrefs&, const char**));
  MOCK_METHOD0(Stop, nsresult());
  MOCK_METHOD0(Deallocate, nsresult());
};

RefPtr<AudioDeviceInfo> MakeAudioDeviceInfo(const nsString aName) {
  return MakeRefPtr<AudioDeviceInfo>(
      nullptr, aName, NS_LITERAL_STRING("GroupId"), NS_LITERAL_STRING("Vendor"),
      AudioDeviceInfo::TYPE_OUTPUT, AudioDeviceInfo::STATE_ENABLED,
      AudioDeviceInfo::PREF_NONE, AudioDeviceInfo::FMT_F32LE,
      AudioDeviceInfo::FMT_F32LE, 2u, 44100u, 44100u, 44100u, 0, 0);
}

RefPtr<MediaDevice> MakeCameraDevice(const nsString& aName,
                                     const nsString& aGroupId) {
  auto v = MakeRefPtr<MockMediaEngineSource>();
  EXPECT_CALL(*v, GetMediaSource())
      .WillRepeatedly(Return(dom::MediaSourceEnum::Camera));

  return MakeRefPtr<MediaDevice>(v, aName, NS_LITERAL_STRING(""), aGroupId,
                                 NS_LITERAL_STRING(""));
}

RefPtr<MediaDevice> MakeMicDevice(const nsString& aName,
                                  const nsString& aGroupId) {
  auto a = MakeRefPtr<MockMediaEngineSource>();
  EXPECT_CALL(*a, GetMediaSource())
      .WillRepeatedly(Return(dom::MediaSourceEnum::Microphone));

  return MakeRefPtr<MediaDevice>(a, aName, NS_LITERAL_STRING(""), aGroupId,
                                 NS_LITERAL_STRING(""));
}

RefPtr<MediaDevice> MakeSpeakerDevice(const nsString& aName,
                                      const nsString& aGroupId) {
  return MakeRefPtr<MediaDevice>(MakeAudioDeviceInfo(aName),
                                 NS_LITERAL_STRING("ID"), aGroupId,
                                 NS_LITERAL_STRING("RawID"));
}

/* Verify that when an audio input device name contains the video input device
 * name the video device group id is updated to become equal to the audio
 * device group id. */
TEST(TestGroupId, MatchInput_PartOfName)
{
  MediaManager::MediaDeviceSet devices;
  MediaManager::MediaDeviceSet audios;

  devices.AppendElement(
      MakeCameraDevice(NS_LITERAL_STRING("Vendor Model"),
                       NS_LITERAL_STRING("Cam-Model-GroupId")));

  auto mic = MakeMicDevice(NS_LITERAL_STRING("Vendor Model Analog Stereo"),
                           NS_LITERAL_STRING("Mic-Model-GroupId"));
  devices.AppendElement(mic);
  audios.AppendElement(mic);

  MediaManager::GuessVideoDeviceGroupIDs(devices, audios);

  EXPECT_EQ(devices[0]->mGroupID, devices[1]->mGroupID)
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
      MakeCameraDevice(NS_LITERAL_STRING("Vendor Model"),
                       NS_LITERAL_STRING("Cam-Model-GroupId")));

  auto mic = MakeMicDevice(NS_LITERAL_STRING("Vendor Model"),
                           NS_LITERAL_STRING("Mic-Model-GroupId"));
  devices.AppendElement(mic);
  audios.AppendElement(mic);

  MediaManager::GuessVideoDeviceGroupIDs(devices, audios);

  EXPECT_EQ(devices[0]->mGroupID, devices[1]->mGroupID)
      << "Video group id is the same as audio input group id.";
}

/* Verify that when an audio input device name does not contain the video input
 * device name the video device group id does not change. */
TEST(TestGroupId, NoMatchInput)
{
  MediaManager::MediaDeviceSet devices;
  MediaManager::MediaDeviceSet audios;

  nsString Cam_Model_GroupId = NS_LITERAL_STRING("Cam-Model-GroupId");
  devices.AppendElement(
      MakeCameraDevice(NS_LITERAL_STRING("Vendor Model"), Cam_Model_GroupId));

  audios.AppendElement(MakeMicDevice(NS_LITERAL_STRING("Model Analog Stereo"),
                                     NS_LITERAL_STRING("Mic-Model-GroupId")));

  MediaManager::GuessVideoDeviceGroupIDs(devices, audios);

  EXPECT_EQ(devices[0]->mGroupID, Cam_Model_GroupId)
      << "Video group id has not been updated.";
  EXPECT_NE(devices[0]->mGroupID, audios[0]->mGroupID)
      << "Video group id is different than audio input group id.";
}

/* Verify that when more that one audio input and more than one audio output
 * device name contain the video input device name the video device group id
 * does not change. */
TEST(TestGroupId, NoMatch_TwoIdenticalDevices)
{
  MediaManager::MediaDeviceSet devices;
  MediaManager::MediaDeviceSet audios;

  nsString Cam_Model_GroupId = NS_LITERAL_STRING("Cam-Model-GroupId");
  devices.AppendElement(
      MakeCameraDevice(NS_LITERAL_STRING("Vendor Model"), Cam_Model_GroupId));

  audios.AppendElement(
      MakeMicDevice(NS_LITERAL_STRING("Vendor Model Analog Stereo"),
                    NS_LITERAL_STRING("Mic-Model-GroupId")));
  audios.AppendElement(
      MakeMicDevice(NS_LITERAL_STRING("Vendor Model Analog Stereo"),
                    NS_LITERAL_STRING("Mic-Model-GroupId")));

  audios.AppendElement(
      MakeSpeakerDevice(NS_LITERAL_STRING("Vendor Model Analog Stereo"),
                        NS_LITERAL_STRING("Speaker-Model-GroupId")));
  audios.AppendElement(
      MakeSpeakerDevice(NS_LITERAL_STRING("Vendor Model Analog Stereo"),
                        NS_LITERAL_STRING("Speaker-Model-GroupId")));

  MediaManager::GuessVideoDeviceGroupIDs(devices, audios);

  EXPECT_EQ(devices[0]->mGroupID, Cam_Model_GroupId)
      << "Video group id has not been updated.";
  EXPECT_NE(devices[0]->mGroupID, audios[0]->mGroupID)
      << "Video group id is different from audio input group id.";
  EXPECT_NE(devices[0]->mGroupID, audios[2]->mGroupID)
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

  nsString Cam_Model_GroupId = NS_LITERAL_STRING("Cam-Model-GroupId");
  devices.AppendElement(
      MakeCameraDevice(NS_LITERAL_STRING("Vendor Model"), Cam_Model_GroupId));

  audios.AppendElement(
      MakeMicDevice(NS_LITERAL_STRING("Vendor Model Analog Stereo"),
                    NS_LITERAL_STRING("Mic-Model-GroupId")));
  audios.AppendElement(
      MakeMicDevice(NS_LITERAL_STRING("Vendor Model Analog Stereo"),
                    NS_LITERAL_STRING("Mic-Model-GroupId")));

  audios.AppendElement(
      MakeSpeakerDevice(NS_LITERAL_STRING("Vendor Model Analog Stereo"),
                        NS_LITERAL_STRING("Speaker-Model-GroupId")));

  MediaManager::GuessVideoDeviceGroupIDs(devices, audios);

  EXPECT_EQ(devices[0]->mGroupID, audios[2]->mGroupID)
      << "Video group id is the same as audio output group id.";
}

/* Verify that when more that one audio input and more than one audio output
 * device names contain the video input device name the video device group id
 * does not change. */
TEST(TestGroupId, NoMatch_ThreeIdenticalDevices)
{
  MediaManager::MediaDeviceSet devices;
  MediaManager::MediaDeviceSet audios;

  nsString Cam_Model_GroupId = NS_LITERAL_STRING("Cam-Model-GroupId");
  devices.AppendElement(
      MakeCameraDevice(NS_LITERAL_STRING("Vendor Model"), Cam_Model_GroupId));

  audios.AppendElement(
      MakeMicDevice(NS_LITERAL_STRING("Vendor Model Analog Stereo"),
                    NS_LITERAL_STRING("Mic-Model-GroupId")));
  audios.AppendElement(
      MakeMicDevice(NS_LITERAL_STRING("Vendor Model Analog Stereo"),
                    NS_LITERAL_STRING("Mic-Model-GroupId")));
  audios.AppendElement(
      MakeMicDevice(NS_LITERAL_STRING("Vendor Model Analog Stereo"),
                    NS_LITERAL_STRING("Mic-Model-GroupId")));

  audios.AppendElement(
      MakeSpeakerDevice(NS_LITERAL_STRING("Vendor Model Analog Stereo"),
                        NS_LITERAL_STRING("Speaker-Model-GroupId")));
  audios.AppendElement(
      MakeSpeakerDevice(NS_LITERAL_STRING("Vendor Model Analog Stereo"),
                        NS_LITERAL_STRING("Speaker-Model-GroupId")));
  audios.AppendElement(
      MakeSpeakerDevice(NS_LITERAL_STRING("Vendor Model Analog Stereo"),
                        NS_LITERAL_STRING("Speaker-Model-GroupId")));

  MediaManager::GuessVideoDeviceGroupIDs(devices, audios);

  EXPECT_EQ(devices[0]->mGroupID, Cam_Model_GroupId)
      << "Video group id has not been updated.";
  EXPECT_NE(devices[0]->mGroupID, audios[0]->mGroupID)
      << "Video group id is different from audio input group id.";
  EXPECT_NE(devices[0]->mGroupID, audios[3]->mGroupID)
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
      MakeCameraDevice(NS_LITERAL_STRING("Vendor Model"),
                       NS_LITERAL_STRING("Cam-Model-GroupId")));

  audios.AppendElement(MakeMicDevice(NS_LITERAL_STRING("Mic Analog Stereo"),
                                     NS_LITERAL_STRING("Mic-Model-GroupId")));

  audios.AppendElement(
      MakeSpeakerDevice(NS_LITERAL_STRING("Vendor Model Analog Stereo"),
                        NS_LITERAL_STRING("Speaker-Model-GroupId")));

  MediaManager::GuessVideoDeviceGroupIDs(devices, audios);

  EXPECT_EQ(devices[0]->mGroupID, audios[1]->mGroupID)
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
      MakeCameraDevice(NS_LITERAL_STRING("Vendor Model"),
                       NS_LITERAL_STRING("Cam-Model-GroupId")));

  audios.AppendElement(MakeMicDevice(NS_LITERAL_STRING("Vendor Model"),
                                     NS_LITERAL_STRING("Mic-Model-GroupId")));

  audios.AppendElement(
      MakeSpeakerDevice(NS_LITERAL_STRING("Vendor Model"),
                        NS_LITERAL_STRING("Speaker-Model-GroupId")));

  MediaManager::GuessVideoDeviceGroupIDs(devices, audios);

  EXPECT_EQ(devices[0]->mGroupID, audios[0]->mGroupID)
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
      MakeCameraDevice(NS_LITERAL_STRING("Vendor Model"),
                       NS_LITERAL_STRING("Cam-Model-GroupId")));

  audios.AppendElement(
      MakeMicDevice(NS_LITERAL_STRING("Vendor Model"), NS_LITERAL_STRING("")));

  MediaManager::GuessVideoDeviceGroupIDs(devices, audios);

  EXPECT_EQ(devices[0]->mGroupID, audios[0]->mGroupID)
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
      MakeCameraDevice(NS_LITERAL_STRING("Vendor Model"),
                       NS_LITERAL_STRING("Cam-Model-GroupId")));

  audios.AppendElement(MakeSpeakerDevice(NS_LITERAL_STRING("Vendor Model"),
                                         NS_LITERAL_STRING("")));

  MediaManager::GuessVideoDeviceGroupIDs(devices, audios);

  EXPECT_EQ(devices[0]->mGroupID, audios[0]->mGroupID)
      << "Video input group id is the same as audio output group id.";
}
