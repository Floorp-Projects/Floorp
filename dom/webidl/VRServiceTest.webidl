/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * This WebIDL is just for WebVR testing.
 */

[Pref="dom.vr.puppet.enabled",
 HeaderFile="mozilla/dom/VRServiceTest.h",
 Exposed=Window]
interface VRMockDisplay {
  undefined create();
  attribute boolean capPosition;
  attribute boolean capOrientation;
  attribute boolean capPresent;
  attribute boolean capExternal;
  attribute boolean capAngularAcceleration;
  attribute boolean capLinearAcceleration;
  attribute boolean capStageParameters;
  attribute boolean capMountDetection;
  attribute boolean capPositionEmulated;
  undefined setEyeFOV(VREye eye,
                      double upDegree, double rightDegree,
                      double downDegree, double leftDegree);
  undefined setEyeOffset(VREye eye, double offsetX,
                         double offsetY, double offsetZ);
  undefined setEyeResolution(unsigned long renderWidth,
                             unsigned long renderHeight);
  undefined setConnected(boolean connected);
  undefined setMounted(boolean mounted);
  undefined setStageSize(double width, double height);
  [Throws] undefined setSittingToStandingTransform(Float32Array sittingToStandingTransform);
  [Throws] undefined setPose(Float32Array? position, Float32Array? linearVelocity,
                             Float32Array? linearAcceleration, Float32Array? orientation,
                             Float32Array? angularVelocity, Float32Array? angularAcceleration);
};

[Pref="dom.vr.puppet.enabled",
 HeaderFile="mozilla/dom/VRServiceTest.h",
 Exposed=Window]
interface VRMockController {
  undefined create();
  undefined clear();
  attribute GamepadHand hand;
  attribute boolean capPosition;
  attribute boolean capOrientation;
  attribute boolean capAngularAcceleration;
  attribute boolean capLinearAcceleration;
  attribute unsigned long axisCount;
  attribute unsigned long buttonCount;
  attribute unsigned long hapticCount;
  [Throws] undefined setPose(Float32Array? position, Float32Array? linearVelocity,
                             Float32Array? linearAcceleration, Float32Array? orientation,
                             Float32Array? angularVelocity, Float32Array? angularAcceleration);
  undefined setButtonPressed(unsigned long buttonIdx, boolean pressed);
  undefined setButtonTouched(unsigned long buttonIdx, boolean touched);
  undefined setButtonTrigger(unsigned long buttonIdx, double trigger);
  undefined setAxisValue(unsigned long axisIdx, double value);
};

[Pref="dom.vr.puppet.enabled",
 HeaderFile="mozilla/dom/VRServiceTest.h",
 Exposed=Window]
interface VRServiceTest {
  VRMockDisplay getVRDisplay();
  [Throws] VRMockController getVRController(unsigned long controllerIdx);
  [NewObject] Promise<undefined> run();
  [NewObject] Promise<undefined> reset();
  undefined commit();
  undefined end();
  undefined clearAll();
  undefined timeout(unsigned long duration);
  undefined wait(unsigned long duration);
  undefined waitSubmit();
  undefined waitPresentationStart();
  undefined waitPresentationEnd();
  [Throws]
  undefined waitHapticIntensity(unsigned long controllerIdx, unsigned long hapticIdx, double intensity);
  undefined captureFrame();
  undefined acknowledgeFrame();
  undefined rejectFrame();
  undefined startTimer();
  undefined stopTimer();
};
