/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

package org.webrtc.videoengine;

import java.util.ArrayList;
import java.util.List;

import android.Manifest;
import android.app.Activity;
import android.content.Context;
import android.util.Log;

import org.mozilla.gecko.GeckoAppShell;
import org.mozilla.gecko.annotation.WebRTCJNITarget;

import org.webrtc.CameraEnumerator;
import org.webrtc.CameraEnumerationAndroid.CaptureFormat;
import org.webrtc.Camera1Enumerator;
import org.webrtc.Camera2Enumerator;

public class VideoCaptureDeviceInfoAndroid {
  private final static String TAG = "WEBRTC-JC";

  // Returns information about all cameras on the device.
  // Since this reflects static information about the hardware present, there is
  // no need to call this function more than once in a single process.  It is
  // marked "private" as it is only called by native code.
  @WebRTCJNITarget
  private static CaptureCapabilityAndroid[] getDeviceInfo() {
      final Context context = GeckoAppShell.getApplicationContext();

      if (Camera2Enumerator.isSupported(context)) {
          return createDeviceList(new Camera2Enumerator(context));
      } else {
          return createDeviceList(new Camera1Enumerator());
      }
  }

  private static CaptureCapabilityAndroid[] createDeviceList(CameraEnumerator enumerator) {

      ArrayList<CaptureCapabilityAndroid> allDevices = new ArrayList<CaptureCapabilityAndroid>();

      for (String camera: enumerator.getDeviceNames()) {
          List<CaptureFormat> formats = enumerator.getSupportedFormats(camera);
          int numFormats = formats.size();
          if (numFormats <= 0) {
            continue;
          }

          CaptureCapabilityAndroid device = new CaptureCapabilityAndroid();

          // The only way to plumb through whether the device is front facing
          // or not is by the name, but the name we receive depends upon the
          // camera API in use. For the Camera1 API, this information is
          // already present, but that is not the case when using Camera2.
          // Later on, we look up the camera by name, so we have to use a
          // format this is easy to undo. Ideally, libwebrtc would expose
          // camera facing in VideoCaptureCapability and none of this would be
          // necessary.
          device.name = "Facing " + (enumerator.isFrontFacing(camera) ? "front" : "back") + ":" + camera;

          // This isn't part of the new API, but we don't call
          // GetDeviceOrientation() anywhere, so this value is unused.
          device.orientation = 0;

          device.width  = new int[numFormats];
          device.height = new int[numFormats];
          device.minMilliFPS = formats.get(0).framerate.min;
          device.maxMilliFPS = formats.get(0).framerate.max;
          int i = 0;
          for (CaptureFormat format: formats) {
              device.width[i] = format.width;
              device.height[i] = format.height;
              if (format.framerate.min < device.minMilliFPS) {
                device.minMilliFPS = format.framerate.min;
              }
              if (format.framerate.max > device.maxMilliFPS) {
                device.maxMilliFPS = format.framerate.max;
              }
              i++;
          }
          device.frontFacing = enumerator.isFrontFacing(camera);
          allDevices.add(device);
      }

      return allDevices.toArray(new CaptureCapabilityAndroid[0]);
  }
}
