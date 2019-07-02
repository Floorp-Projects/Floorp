/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_VR_SERVICE_VRPUPPETCOMMANDBUFFER_H
#define GFX_VR_SERVICE_VRPUPPETCOMMANDBUFFER_H

#include <inttypes.h>
#include "mozilla/Mutex.h"
#include "nsTArray.h"
#include "moz_external_vr.h"
#include "mozilla/TimeStamp.h"

namespace mozilla {
namespace gfx {

/**
 * A Puppet Device command buffer consists of a stream of 64-bit
 * commands.
 * The first 8 bits identifies the command and informs format of
 * the remaining 56 bits.
 *
 * These commands will be streamed into a buffer until
 * VRPuppet_End (0x0000000000000000) has been received.
 *
 * When VRPuppet_End is received, this command buffer will
 * be executed asynchronously, collecting the timer results
 * requested.
 *
 * In addition to the effects seen through the WebXR/VR API, tests
 * can get further feedback from the Puppet device.
 * Command buffers can be constructed such that when expected states
 * are not reached, the timeout timer will expire.
 * Data recorded with timer commands is returned when the command
 * buffer is completed, to validate against accepted ranges and to
 * quantify performance regressions.
 * Images submitted to the Puppet display are rendered with the
 * 2d browser output in order to enable reftests to validate
 * output against reference images.
 *
 * The state of the virtual puppet device is expressed to the VR service
 * in the same manner as physical devices -- using the VRDisplayState,
 * VRHMDSensorState and VRControllerState structures.
 *
 * By enabling partial updates of these structures, the command buffer
 * size is reduced to the values that change each frame.  This enables
 * realtime capture of a session, with physical hardware, for
 * replay in automated tests and benchmarks.
 *
 * All updates to the state structures are atomically updated in the
 * VR session thread, triggered by VRPuppet_Commit (0x1500000000000000).
 *
 * Command buffers are expected to be serialized to a human readable,
 * ascii format if stored on disk.  The binary representation is not
 * guaranteed to be consistent between versions or target platforms.
 * They should be re-constructed with the VRServiceTest DOM api at
 * runtime.
 *
 * The command set:
 *
 * 0x0000000000000000 - VRPuppet_End()
 * - End of stream, resolve promise returned by VRServiceTest::Play()
 *
 * 0x0100000000000000 - VRPuppet_ClearAll()
 * - Clear all structs
 *
 * 0x02000000000000nn - VRPuppet_ClearController(n)
 * - Clear a single controller struct
 *
 * 0x03000000nnnnnnnn - VRPuppet_Timeout(n)
 * - Reset the timeout timer to n milliseconds
 * - Initially the timeout timer starts at 10 seconds
 *
 * 0x04000000nnnnnnnn - VRPuppet_Wait(n)
 * - Wait n milliseconds before advancing to next command
 *
 * 0x0500000000000000 - VRPuppet_WaitSubmit()
 * - Wait until a frame has been submitted before advancing to the next command
 *
 * 0x0600000000000000 - VRPuppet_WaitPresentationStart()
 * - Wait until a presentation becomes active
 *
 * 0x0700000000000000 - VRPuppet_WaitPresentationEnd()
 * - Wait until a presentation ends
 *
 * 0x0800cchhvvvvvvvv - VRPuppet_WaitHapticIntensity(c, h, v)
 * - Wait until controller at index c's haptic actuator at index h reaches value
 * v.
 * - v is a 16.16 fixed point value, with 1.0f being the highest intensity and
 * 0.0f indicating that the haptic actuator is not running
 *
 * 0x0900000000000000 - VRPuppet_CaptureFrame()
 * - Captures the submitted frame.  Must later call
 *   VRPuppet_AcknowledgeFrame or VRPuppet_RejectFrame
 *   to unblock
 *
 * 0x0900000000000000 - VRPuppet_AcknowledgeFrame()
 * - Acknowledge the submitted frame, unblocking the Submit call.
 *
 * 0x0a00000000000000 - VRPuppet_RejectFrame()
 * - Reject the submitted frame, unblocking the Submit call.
 *
 * 0x0b00000000000000 - VRPuppet_StartTimer()
 * - Starts the timer
 *
 * 0x0c00000000000000 - VRPuppet_StopTimer()
 * - Stops the timer, the elapsed duration is recorded for access after the end
 *   of stream
 *
 * 0x0d000000aaaaaaaa - VRPuppet_UpdateDisplay(a)
 * - Start writing data to the VRDisplayState struct, at offset a
 *
 * 0x0e000000aaaaaaaa - VRPuppet_UpdateSensor(a)
 * - Start writing data to the VRHMDSensorState struct, at offset a
 *
 * 0x0f000000aaaaaaaa - VRPuppet_UpdateControllers(a)
 * - Start writing data to the VRControllerState array, at offset a
 *
 * 0x100000000000000 - VRPuppet_Commit
 * - Atomically commit the VRPuppet_Data updates to VRDisplayState,
 *   VRHMDSensorState and VRControllerState.
 *
 * 0xf0000000000000dd - VRPuppet_Data(d)
 * - 1 byte of data
 *
 * 0xf10000000000dddd - VRPuppet_Data(d)
 * - 2 bytes of data
 *
 * 0xf200000000dddddd - VRPuppet_Data(d)
 * - 3 bytes of data
 *
 * 0xf3000000dddddddd - VRPuppet_Data(d)
 * - 4 bytes of data
 *
 * 0xf40000dddddddddd - VRPuppet_Data(d)
 * - 5 bytes of data
 *
 * 0xf500dddddddddddd - VRPuppet_Data(d)
 * - 6 bytes of data
 *
 * 0xf6dddddddddddddd - VRPuppet_Data(d)
 * - 7 bytes of data
 *
 */
enum class VRPuppet_Command : uint64_t {
  VRPuppet_End = 0x0000000000000000,
  VRPuppet_ClearAll = 0x0100000000000000,
  VRPuppet_ClearController = 0x0200000000000000,
  VRPuppet_Timeout = 0x0300000000000000,
  VRPuppet_Wait = 0x0400000000000000,
  VRPuppet_WaitSubmit = 0x0500000000000000,
  VRPuppet_WaitPresentationStart = 0x0600000000000000,
  VRPuppet_WaitPresentationEnd = 0x0700000000000000,
  VRPuppet_WaitHapticIntensity = 0x0800000000000000,
  VRPuppet_CaptureFrame = 0x0900000000000000,
  VRPuppet_AcknowledgeFrame = 0x0a00000000000000,
  VRPuppet_RejectFrame = 0x0b00000000000000,
  VRPuppet_StartTimer = 0x0c00000000000000,
  VRPuppet_StopTimer = 0x0d00000000000000,
  VRPuppet_UpdateDisplay = 0x0e00000000000000,
  VRPuppet_UpdateSensor = 0x0f00000000000000,
  VRPuppet_UpdateControllers = 0x1000000000000000,
  VRPuppet_Commit = 0x1100000000000000,
  VRPuppet_Data1 = 0xf000000000000000,
  VRPuppet_Data2 = 0xf100000000000000,
  VRPuppet_Data3 = 0xf200000000000000,
  VRPuppet_Data4 = 0xf300000000000000,
  VRPuppet_Data5 = 0xf400000000000000,
  VRPuppet_Data6 = 0xf500000000000000,
  VRPuppet_Data7 = 0xf600000000000000,
};

static const int kNumPuppetHaptics = 8;

class VRPuppetCommandBuffer {
 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(mozilla::gfx::VRPuppetCommandBuffer)
  static VRPuppetCommandBuffer& Get();

  // Interface to VRTestSystem
  void Submit(const InfallibleTArray<uint64_t>& aBuffer);
  void Reset();
  bool HasEnded();

  // Interface to PuppetSession
  void Run(VRSystemState& aState);
  void StartPresentation();
  void StopPresentation();
  bool SubmitFrame();
  void VibrateHaptic(uint32_t aControllerIdx, uint32_t aHapticIndex,
                     float aIntensity, float aDuration);
  void StopVibrateHaptic(uint32_t aControllerIdx);
  void StopAllHaptics();

  static void EncodeStruct(nsTArray<uint64_t>& aBuffer, uint8_t* aSrcStart,
                           uint8_t* aDstStart, size_t aLength,
                           gfx::VRPuppet_Command aUpdateCommand);

 private:
  VRPuppetCommandBuffer();
  ~VRPuppetCommandBuffer();
  void Run();
  bool RunCommand(uint64_t aCommand, double aDeltaTime);
  void WriteData(uint8_t aData);
  void SimulateHaptics(double aDeltaTime);
  void CompleteTest(bool aTimedOut);
  nsTArray<uint64_t> mBuffer;
  mozilla::Mutex mMutex;
  VRSystemState mPendingState;
  VRSystemState mCommittedState;
  double mHapticPulseRemaining[kVRControllerMaxCount][kNumPuppetHaptics];
  float mHapticPulseIntensity[kVRControllerMaxCount][kNumPuppetHaptics];

  size_t mDataOffset;
  bool mPresentationRequested;
  bool mFrameSubmitted;
  bool mFrameAccepted;
  double mTimeoutDuration;  // Seconds
  double mWaitRemaining;    // Seconds
  double mBlockedTime;      // Seconds
  double mTimerElapsed;     // Seconds
  TimeStamp mLastRunTimestamp;

  // Test Results:
  bool mEnded;
  bool mEndedWithTimeout;
  nsTArray<double> mTimerSamples;
};

}  // namespace gfx
}  // namespace mozilla

#endif  // GFX_VR_SERVICE_VRPUPPETCOMMANDBUFFER_H
