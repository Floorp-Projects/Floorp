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

[Pref="dom.vr.enabled"]
interface VRDevice {
  /**
   * An identifier for the distinct hardware unit that this
   * VR Device is a part of.  All VRDevice/Sensors that come
   * from the same hardware will have the same hardwareId
   */
  [Pure] readonly attribute DOMString hardwareUnitId;

  /**
   * An identifier for this distinct sensor/device on a physical
   * hardware device.  This shouldn't change across browser
   * restrats, allowing configuration data to be saved based on it.
   */
  [Pure] readonly attribute DOMString deviceId;

  /**
   * a device name, a user-readable name identifying it
   */
  [Pure] readonly attribute DOMString deviceName;
};

[Pref="dom.vr.enabled",
 HeaderFile="mozilla/dom/VRDevice.h"]
interface HMDVRDevice : VRDevice {
  /* The translation that should be applied to the view matrix for rendering each eye */
  DOMPoint getEyeTranslation(VREye whichEye);

  // the FOV that the HMD was configured with
  VRFieldOfView getCurrentEyeFieldOfView(VREye whichEye);

  // the recommended FOV, per eye.
  VRFieldOfView getRecommendedEyeFieldOfView(VREye whichEye);

  // the maximum FOV, per eye.  Above this, rendering will look broken.
  VRFieldOfView getMaximumEyeFieldOfView(VREye whichEye);

  // Set a field of view.  If either of the fields of view is null,
  // or if their values are all zeros, then the recommended field of view
  // for that eye will be used.
  void setFieldOfView(optional VRFieldOfViewInit leftFOV,
                      optional VRFieldOfViewInit rightFOV,
                      optional double zNear = 0.01,
                      optional double zFar = 10000.0);

  // return a recommended rect for this eye.  Only useful for Canvas rendering,
  // the x/y coordinates will be the location in the canvas where this eye should
  // begin, and the width/height are the dimensions.  Any canvas in the appropriate
  // ratio will work.
  DOMRect getRecommendedEyeRenderRect(VREye whichEye);

  // hack for testing
  void xxxToggleElementVR(Element element);
};

[Pref="dom.vr.enabled" ,
 HeaderFile="mozilla/dom/VRDevice.h"]
interface PositionSensorVRDevice : VRDevice {
  /*
   * Return a VRPositionState dictionary containing the state of this position sensor,
   * at an optional past time or predicted for a future time if timeOffset is != 0.
   *
   * The VRPositionState will contain the position, orientation, and velocity
   * and acceleration of each of these properties.  Use "hasPosition" and "hasOrientation"
   * to check if the associated members are valid; if these are false, those members
   * will be null.
   */
  VRPositionState getState(optional double timeOffset = 0.0);

  /* Zero this sensor, treating its current position and orientation
   * as the "origin/zero" values.
   */
  void zeroSensor();
};
