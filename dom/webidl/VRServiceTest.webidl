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
  void create();
  attribute boolean capPosition;
  attribute boolean capOrientation;
  attribute boolean capPresent;
  attribute boolean capExternal;
  attribute boolean capAngularAcceleration;
  attribute boolean capLinearAcceleration;
  attribute boolean capStageParameters;
  attribute boolean capMountDetection;
  attribute boolean capPositionEmulated;
  void setEyeFOV(VREye eye,
                 double upDegree, double rightDegree,
                 double downDegree, double leftDegree);
  void setEyeOffset(VREye eye, double offsetX,
                    double offsetY, double offsetZ);
  void setEyeResolution(unsigned long renderWidth,
                        unsigned long renderHeight);
  void setConnected(boolean connected);
  void setMounted(boolean mounted);
  void setStageSize(double width, double height);
  [Throws] void setSittingToStandingTransform(Float32Array sittingToStandingTransform);
  [Throws] void setPose(Float32Array? position, Float32Array? linearVelocity,
                        Float32Array? linearAcceleration, Float32Array? orientation,
                        Float32Array? angularVelocity, Float32Array? angularAcceleration);
};

[Pref="dom.vr.puppet.enabled",
 HeaderFile="mozilla/dom/VRServiceTest.h",
 Exposed=Window]
interface VRMockController {
  void create();
  void clear();
  attribute GamepadHand hand;
  attribute boolean capPosition;
  attribute boolean capOrientation;
  attribute boolean capAngularAcceleration;
  attribute boolean capLinearAcceleration;
  attribute unsigned long axisCount;
  attribute unsigned long buttonCount;
  attribute unsigned long hapticCount;
  [Throws] void setPose(Float32Array? position, Float32Array? linearVelocity,
                        Float32Array? linearAcceleration, Float32Array? orientation,
                        Float32Array? angularVelocity, Float32Array? angularAcceleration);
  void setButtonPressed(unsigned long buttonIdx, boolean pressed);
  void setButtonTouched(unsigned long buttonIdx, boolean touched);
  void setButtonTrigger(unsigned long buttonIdx, double trigger);
  void setAxisValue(unsigned long axisIdx, double value);
};

[Pref="dom.vr.puppet.enabled",
 HeaderFile="mozilla/dom/VRServiceTest.h",
 Exposed=Window]
interface VRServiceTest {
  VRMockDisplay getVRDisplay();
  [Throws] VRMockController getVRController(unsigned long controllerIdx);
  [Throws] Promise<void> run();
  [Throws] Promise<void> reset();
  void commit();
  void end();
  void clearAll();
  void timeout(unsigned long duration);
  void wait(unsigned long duration);
  void waitSubmit();
  void waitPresentationStart();
  void waitPresentationEnd();
  [Throws]
  void waitHapticIntensity(unsigned long controllerIdx, unsigned long hapticIdx, double intensity);
  void captureFrame();
  void acknowledgeFrame();
  void rejectFrame();
  void startTimer();
  void stopTimer();
};
