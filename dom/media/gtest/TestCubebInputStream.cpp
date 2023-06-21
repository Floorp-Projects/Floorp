/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "CubebInputStream.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "mozilla/gtest/WaitFor.h"
#include "MockCubeb.h"

using namespace mozilla;

namespace {
#define DispatchFunction(f) \
  NS_DispatchToCurrentThread(NS_NewRunnableFunction(__func__, f))
}  // namespace

class MockListener : public CubebInputStream::Listener {
 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(MockListener, override);
  MOCK_METHOD2(DataCallback, long(const void* aBuffer, long aFrames));
  MOCK_METHOD1(StateCallback, void(cubeb_state aState));
  MOCK_METHOD0(DeviceChangedCallback, void());

 private:
  ~MockListener() = default;
};

TEST(TestCubebInputStream, DataCallback)
{
  using ::testing::Ne;
  using ::testing::NotNull;

  MockCubeb* cubeb = new MockCubeb();
  CubebUtils::ForceSetCubebContext(cubeb->AsCubebContext());

  const CubebUtils::AudioDeviceID deviceId = nullptr;
  const uint32_t channels = 2;

  uint32_t rate = 0;
  ASSERT_EQ(cubeb_get_preferred_sample_rate(cubeb->AsCubebContext(), &rate),
            CUBEB_OK);

  nsTArray<AudioDataValue> data;
  auto listener = MakeRefPtr<MockListener>();
  EXPECT_CALL(*listener, DataCallback(NotNull(), Ne(0)))
      .WillRepeatedly([&](const void* aBuffer, long aFrames) {
        const AudioDataValue* source =
            reinterpret_cast<const AudioDataValue*>(aBuffer);
        size_t sampleCount =
            static_cast<size_t>(aFrames) * static_cast<size_t>(channels);
        data.AppendElements(source, sampleCount);
        return aFrames;
      });

  EXPECT_CALL(*listener, StateCallback(CUBEB_STATE_STARTED));
  EXPECT_CALL(*listener, StateCallback(CUBEB_STATE_STOPPED)).Times(2);

  EXPECT_CALL(*listener, DeviceChangedCallback).Times(0);

  UniquePtr<CubebInputStream> cis;
  DispatchFunction([&] {
    cis = CubebInputStream::Create(deviceId, channels, rate, true,
                                   listener.get());
    ASSERT_TRUE(cis);
  });
  RefPtr<SmartMockCubebStream> stream = WaitFor(cubeb->StreamInitEvent());
  EXPECT_TRUE(stream->mHasInput);

  stream->SetInputRecordingEnabled(true);

  DispatchFunction([&] { ASSERT_EQ(cis->Start(), CUBEB_OK); });
  WaitFor(stream->FramesProcessedEvent());

  DispatchFunction([&] { ASSERT_EQ(cis->Stop(), CUBEB_OK); });
  WaitFor(stream->OutputVerificationEvent());

  nsTArray<AudioDataValue> record = stream->TakeRecordedInput();

  DispatchFunction([&] { cis = nullptr; });
  WaitFor(cubeb->StreamDestroyEvent());

  ASSERT_EQ(data, record);
}

TEST(TestCubebInputStream, ErrorCallback)
{
  using ::testing::Ne;
  using ::testing::NotNull;
  using ::testing::ReturnArg;

  MockCubeb* cubeb = new MockCubeb();
  CubebUtils::ForceSetCubebContext(cubeb->AsCubebContext());

  const CubebUtils::AudioDeviceID deviceId = nullptr;
  const uint32_t channels = 2;

  uint32_t rate = 0;
  ASSERT_EQ(cubeb_get_preferred_sample_rate(cubeb->AsCubebContext(), &rate),
            CUBEB_OK);

  auto listener = MakeRefPtr<MockListener>();
  EXPECT_CALL(*listener, DataCallback(NotNull(), Ne(0)))
      .WillRepeatedly(ReturnArg<1>());

  EXPECT_CALL(*listener, StateCallback(CUBEB_STATE_STARTED));
  EXPECT_CALL(*listener, StateCallback(CUBEB_STATE_ERROR));
  EXPECT_CALL(*listener, StateCallback(CUBEB_STATE_STOPPED));

  EXPECT_CALL(*listener, DeviceChangedCallback).Times(0);

  UniquePtr<CubebInputStream> cis;
  DispatchFunction([&] {
    cis = CubebInputStream::Create(deviceId, channels, rate, true,
                                   listener.get());
    ASSERT_TRUE(cis);
  });
  RefPtr<SmartMockCubebStream> stream = WaitFor(cubeb->StreamInitEvent());
  EXPECT_TRUE(stream->mHasInput);

  DispatchFunction([&] { ASSERT_EQ(cis->Start(), CUBEB_OK); });
  WaitFor(stream->FramesProcessedEvent());

  DispatchFunction([&] { stream->ForceError(); });
  WaitFor(stream->ErrorForcedEvent());

  // If stream ran into an error state, then it should be stopped.

  DispatchFunction([&] { cis = nullptr; });
  WaitFor(cubeb->StreamDestroyEvent());
}

TEST(TestCubebInputStream, DeviceChangedCallback)
{
  using ::testing::Ne;
  using ::testing::NotNull;
  using ::testing::ReturnArg;

  MockCubeb* cubeb = new MockCubeb();
  CubebUtils::ForceSetCubebContext(cubeb->AsCubebContext());

  const CubebUtils::AudioDeviceID deviceId = nullptr;
  const uint32_t channels = 2;

  uint32_t rate = 0;
  ASSERT_EQ(cubeb_get_preferred_sample_rate(cubeb->AsCubebContext(), &rate),
            CUBEB_OK);

  auto listener = MakeRefPtr<MockListener>();
  EXPECT_CALL(*listener, DataCallback(NotNull(), Ne(0)))
      .WillRepeatedly(ReturnArg<1>());

  // In real world, the stream might run into an error state when the
  // device-changed event is fired (e.g., the last default output device is
  // unplugged). But it's fine to not check here since we can control how
  // MockCubeb behaves.
  EXPECT_CALL(*listener, StateCallback(CUBEB_STATE_STARTED));
  EXPECT_CALL(*listener, StateCallback(CUBEB_STATE_STOPPED)).Times(2);

  EXPECT_CALL(*listener, DeviceChangedCallback);

  UniquePtr<CubebInputStream> cis;
  DispatchFunction([&] {
    cis = CubebInputStream::Create(deviceId, channels, rate, true,
                                   listener.get());
    ASSERT_TRUE(cis);
  });
  RefPtr<SmartMockCubebStream> stream = WaitFor(cubeb->StreamInitEvent());
  EXPECT_TRUE(stream->mHasInput);

  DispatchFunction([&] { ASSERT_EQ(cis->Start(), CUBEB_OK); });
  WaitFor(stream->FramesProcessedEvent());

  DispatchFunction([&] { stream->ForceDeviceChanged(); });
  WaitFor(stream->DeviceChangeForcedEvent());

  // The stream can keep running when its device is changed.
  DispatchFunction([&] { ASSERT_EQ(cis->Stop(), CUBEB_OK); });
  cubeb_state state = WaitFor(stream->StateEvent());
  EXPECT_EQ(state, CUBEB_STATE_STOPPED);

  DispatchFunction([&] { cis = nullptr; });
  WaitFor(cubeb->StreamDestroyEvent());
}
