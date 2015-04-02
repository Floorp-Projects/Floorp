/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

enum VREye {
  "left",
  "right"
};

[Pref="dom.vr.enabled",
 HeaderFile="mozilla/dom/VRDevice.h"]
interface VRFieldOfViewReadOnly {
  readonly attribute double upDegrees;
  readonly attribute double rightDegrees;
  readonly attribute double downDegrees;
  readonly attribute double leftDegrees;
};

[Pref="dom.vr.enabled",
 HeaderFile="mozilla/dom/VRDevice.h",
 Constructor(optional VRFieldOfViewInit fov),
 Constructor(double upDegrees, double rightDegrees, double downDegrees, double leftDegrees)]
interface VRFieldOfView : VRFieldOfViewReadOnly {
  inherit attribute double upDegrees;
  inherit attribute double rightDegrees;
  inherit attribute double downDegrees;
  inherit attribute double leftDegrees;
};

dictionary VRFieldOfViewInit {
  double upDegrees = 0.0;
  double rightDegrees = 0.0;
  double downDegrees = 0.0;
  double leftDegrees = 0.0;
};

[Pref="dom.vr.enabled",
 HeaderFile="mozilla/dom/VRDevice.h"]
interface VRPositionState {
  readonly attribute double timeStamp;

  readonly attribute boolean hasPosition;
  readonly attribute DOMPoint? position;
  readonly attribute DOMPoint? linearVelocity;
  readonly attribute DOMPoint? linearAcceleration;

  readonly attribute boolean hasOrientation;
  // XXX should be DOMQuaternion as soon as we add that
  readonly attribute DOMPoint? orientation;
  readonly attribute DOMPoint? angularVelocity;
  readonly attribute DOMPoint? angularAcceleration;
};

[Pref="dom.vr.enabled",
 HeaderFile="mozilla/dom/VRDevice.h"]
interface VREyeParameters {
  /* These values are expected to be static per-device/per-user */
  [Constant, Cached] readonly attribute VRFieldOfView minimumFieldOfView;
  [Constant, Cached] readonly attribute VRFieldOfView maximumFieldOfView;
  [Constant, Cached] readonly attribute VRFieldOfView recommendedFieldOfView;
  [Constant, Cached] readonly attribute DOMPoint eyeTranslation;

  /* These values will vary after a FOV has been set */
  [Constant, Cached] readonly attribute VRFieldOfView currentFieldOfView;
  [Constant, Cached] readonly attribute DOMRect renderRect;
};

[Pref="dom.vr.enabled"]
interface VRDevice {
  /**
   * An identifier for the distinct hardware unit that this
   * VR Device is a part of.  All VRDevice/Sensors that come
   * from the same hardware will have the same hardwareId
   */
  [Constant] readonly attribute DOMString hardwareUnitId;

  /**
   * An identifier for this distinct sensor/device on a physical
   * hardware device.  This shouldn't change across browser
   * restrats, allowing configuration data to be saved based on it.
   */
  [Constant] readonly attribute DOMString deviceId;

  /**
   * a device name, a user-readable name identifying it
   */
  [Constant] readonly attribute DOMString deviceName;
};

[Pref="dom.vr.enabled",
 HeaderFile="mozilla/dom/VRDevice.h"]
interface HMDVRDevice : VRDevice {
  // Return the current VREyeParameters for the given eye
  VREyeParameters getEyeParameters(VREye whichEye);

  // Set a field of view.  If either of the fields of view is null,
  // or if their values are all zeros, then the recommended field of view
  // for that eye will be used.
  void setFieldOfView(optional VRFieldOfViewInit leftFOV,
                      optional VRFieldOfViewInit rightFOV,
                      optional double zNear = 0.01,
                      optional double zFar = 10000.0);
};

[Pref="dom.vr.enabled" ,
 HeaderFile="mozilla/dom/VRDevice.h"]
interface PositionSensorVRDevice : VRDevice {
  /*
   * Return a VRPositionState dictionary containing the state of this position sensor
   * for the current frame if within a requestAnimationFrame callback, or for the
   * previous frame if not.
   *
   * The VRPositionState will contain the position, orientation, and velocity
   * and acceleration of each of these properties.  Use "hasPosition" and "hasOrientation"
   * to check if the associated members are valid; if these are false, those members
   * will be null.
   */
  [NewObject] VRPositionState getState();

  /*
   * Return the current instantaneous sensor state.
   */
  [NewObject] VRPositionState getImmediateState();

  /* Reset this sensor, treating its current position and orientation
   * as the "origin/zero" values.
   */
  void resetSensor();
};
