/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_VIDEO_CAPTURE_OBJC_DEVICE_INFO_OBJC_H_
#define MODULES_VIDEO_CAPTURE_OBJC_DEVICE_INFO_OBJC_H_

#import <AVFoundation/AVFoundation.h>

#include "webrtc/modules/video_capture/video_capture_defines.h"

@interface DeviceInfoIosObjC : NSObject {
  NSArray* _observers;
  NSLock* _lock;
  webrtc::videocapturemodule::DeviceInfoIos* _owner;
}

+ (int)captureDeviceCount;
+ (AVCaptureDevice*)captureDeviceForIndex:(int)index;
+ (AVCaptureDevice*)captureDeviceForUniqueId:(NSString*)uniqueId;
+ (NSString*)deviceNameForIndex:(int)index;
+ (NSString*)deviceUniqueIdForIndex:(int)index;
+ (NSString*)deviceNameForUniqueId:(NSString*)uniqueId;
+ (webrtc::VideoCaptureCapability)capabilityForPreset:(NSString*)preset;

- (void)registerOwner:(webrtc::videocapturemodule::DeviceInfoIos*)owner;
- (void)configureObservers;

@end

#endif  // MODULES_VIDEO_CAPTURE_OBJC_DEVICE_INFO_OBJC_H_
