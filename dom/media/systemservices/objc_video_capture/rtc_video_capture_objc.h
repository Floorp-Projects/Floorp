/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_VIDEO_CAPTURE_OBJC_RTC_VIDEO_CAPTURE_OBJC_H_
#define MODULES_VIDEO_CAPTURE_OBJC_RTC_VIDEO_CAPTURE_OBJC_H_

#import <Foundation/Foundation.h>
#ifdef WEBRTC_IOS
#  import <UIKit/UIKit.h>
#endif

#include "video_capture.h"

// The following class listens to a notification with name:
// 'StatusBarOrientationDidChange'.
// This notification must be posted in order for the capturer to reflect the
// orientation change in video w.r.t. the application orientation.
@interface RTCVideoCaptureIosObjC : NSObject <AVCaptureVideoDataOutputSampleBufferDelegate>

@property webrtc::VideoRotation frameRotation;

// custom initializer. Instance of VideoCaptureIos is needed
// for callback purposes.
// default init methods have been overridden to return nil.
- (id)initWithOwner:(webrtc::videocapturemodule::VideoCaptureIos*)owner;
- (BOOL)setCaptureDeviceByUniqueId:(NSString*)uniqueId;
- (BOOL)startCaptureWithCapability:(const webrtc::VideoCaptureCapability&)capability;
- (BOOL)stopCapture;

@end
#endif  // MODULES_VIDEO_CAPTURE_OBJC_RTC_VIDEO_CAPTURE_OBJC_H_
