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

import java.io.IOException;
import java.util.List;

import android.content.Context;
import android.util.Log;
import android.view.Surface;
import android.view.WindowManager;

import java.util.concurrent.CountDownLatch;

import org.mozilla.gecko.annotation.WebRTCJNITarget;

import org.webrtc.CameraEnumerator;
import org.webrtc.Camera1Enumerator;
import org.webrtc.Camera2Enumerator;
import org.webrtc.CameraVideoCapturer;
import org.webrtc.CapturerObserver;
import org.webrtc.EglBase;
import org.webrtc.SurfaceTextureHelper;
import org.webrtc.VideoFrame;
import org.webrtc.VideoFrame.I420Buffer;

public class VideoCaptureAndroid implements CameraVideoCapturer.CameraEventsHandler, CapturerObserver {
  private final static String TAG = "WEBRTC-JC";

  private final String deviceName;
  private volatile long native_capturer;  // |VideoCaptureAndroid*| in C++.
  private Context context;
  private CameraVideoCapturer cameraVideoCapturer;
  private EglBase eglBase;
  private SurfaceTextureHelper surfaceTextureHelper;

  // This class is recreated everytime we start/stop capture, so we
  // can safely create the CountDownLatches here.
  private final CountDownLatch capturerStarted = new CountDownLatch(1);
  private boolean capturerStartedSucceeded = false;
  private final CountDownLatch capturerStopped = new CountDownLatch(1);

 @WebRTCJNITarget
 public VideoCaptureAndroid(String deviceName, long native_capturer) {
    // Remove the camera facing information from the name.
    String[] parts = deviceName.split("Facing (front|back):");
    if (parts.length == 2) {
      this.deviceName = parts[1];
    } else {
      Log.e(TAG, "VideoCaptureAndroid: Expected facing mode as part of name: " + deviceName);
      this.deviceName = deviceName;
    }
    this.native_capturer = native_capturer;
    this.context = GetContext();

    CameraEnumerator enumerator;
    if (Camera2Enumerator.isSupported(context)) {
      enumerator = new Camera2Enumerator(context);
    } else {
      enumerator = new Camera1Enumerator();
    }
    try {
      cameraVideoCapturer = enumerator.createCapturer(this.deviceName, this);
      eglBase = EglBase.create();
      surfaceTextureHelper = SurfaceTextureHelper.create("VideoCaptureAndroidSurfaceTextureHelper", eglBase.getEglBaseContext());
      cameraVideoCapturer.initialize(surfaceTextureHelper, context, this);
    } catch (java.lang.IllegalArgumentException e) {
      Log.e(TAG, "VideoCaptureAndroid: Exception while creating capturer: " + e);
    }
  }

  // Return the global application context.
  @WebRTCJNITarget
  private static native Context GetContext();

  // Called by native code.  Returns true if capturer is started.
  //
  // Note that this actually opens the camera, and Camera callbacks run on the
  // thread that calls open(), so this is done on the CameraThread.  Since ViE
  // API needs a synchronous success return value we wait for the result.
  @WebRTCJNITarget
  private synchronized boolean startCapture(
      final int width, final int height,
      final int min_mfps, final int max_mfps) {
    Log.d(TAG, "startCapture: " + width + "x" + height + "@" +
        min_mfps + ":" + max_mfps);

    if (cameraVideoCapturer == null) {
      return false;
    }

    cameraVideoCapturer.startCapture(width, height, max_mfps);
    try {
      capturerStarted.await();
    } catch (InterruptedException e) {
      return false;
    }
    return capturerStartedSucceeded;
  }

  @WebRTCJNITarget
  private void unlinkCapturer() {
    // stopCapture might fail. That might leave the callbacks dangling, so make
    // sure those don't call into dead code.
    // Note that onPreviewCameraFrame isn't synchronized, so there's no point in
    // synchronizing us either. ProvideCameraFrame has to do the null check.
    native_capturer = 0;
  }

  // Called by native code.  Returns true when camera is known to be stopped.
  @WebRTCJNITarget
  private synchronized boolean stopCapture() {
    Log.d(TAG, "stopCapture");
    if (cameraVideoCapturer == null) {
      return false;
    }

    try {
      cameraVideoCapturer.stopCapture();
      capturerStopped.await();
    } catch (InterruptedException e) {
      return false;
    }
    Log.d(TAG, "stopCapture done");
    return true;
  }

  @WebRTCJNITarget
  private int getDeviceOrientation() {
    int orientation = 0;
    if (context != null) {
      WindowManager wm = (WindowManager) context.getSystemService(
          Context.WINDOW_SERVICE);
      switch(wm.getDefaultDisplay().getRotation()) {
        case Surface.ROTATION_90:
          orientation = 90;
          break;
        case Surface.ROTATION_180:
          orientation = 180;
          break;
        case Surface.ROTATION_270:
          orientation = 270;
          break;
        case Surface.ROTATION_0:
        default:
          orientation = 0;
          break;
      }
    }
    return orientation;
  }

  @WebRTCJNITarget
  private native void ProvideCameraFrame(
      int width, int height,
      java.nio.ByteBuffer dataY, int strideY,
      java.nio.ByteBuffer dataU, int strideU,
      java.nio.ByteBuffer dataV, int strideV,
      int rotation, long timeStamp, long captureObject);

  //
  // CameraVideoCapturer.CameraEventsHandler interface
  //

  // Camera error handler - invoked when camera can not be opened
  // or any camera exception happens on camera thread.
  public void onCameraError(String errorDescription) {}

  // Called when camera is disconnected.
  public void onCameraDisconnected() {}

  // Invoked when camera stops receiving frames.
  public void onCameraFreezed(String errorDescription) {}

  // Callback invoked when camera is opening.
  public void onCameraOpening(String cameraName) {}

  // Callback invoked when first camera frame is available after camera is started.
  public void onFirstFrameAvailable() {}

  // Callback invoked when camera is closed.
  public void onCameraClosed() {}

  //
  // CapturerObserver interface
  //

  // Notify if the capturer have been started successfully or not.
  public void onCapturerStarted(boolean success) {
    capturerStartedSucceeded = success;
    capturerStarted.countDown();
  }

  // Notify that the capturer has been stopped.
  public void onCapturerStopped() {
    capturerStopped.countDown();
  }

  // Delivers a captured frame.
  public void onFrameCaptured(VideoFrame frame) {
    if (native_capturer != 0) {
      I420Buffer i420Buffer = frame.getBuffer().toI420();
      ProvideCameraFrame(i420Buffer.getWidth(), i420Buffer.getHeight(),
          i420Buffer.getDataY(), i420Buffer.getStrideY(),
          i420Buffer.getDataU(), i420Buffer.getStrideU(),
          i420Buffer.getDataV(), i420Buffer.getStrideV(),
          frame.getRotation(),
          frame.getTimestampNs() / 1000000, native_capturer);

      i420Buffer.release();
    }
  }
}
