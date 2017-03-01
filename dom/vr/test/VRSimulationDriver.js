
var VRServiceTest;

var VRSimulationDriver = (function() {
"use strict";

var AttachWebVRDisplay = function() {
  return VRServiceTest.attachVRDisplay("VRDisplayTest");
};

var SetVRDisplayPose = function(vrDisplay, position,
                                linearVelocity, linearAcceleration,
                                orientation, angularVelocity,
                                angularAcceleration) {
  vrDisplay.setPose(position, linearVelocity, linearAcceleration,
                    orientation, angularVelocity, angularAcceleration);
};

var SetEyeResolution = function(width, height) {
  vrDisplay.setEyeResolution(width, height);
}

var SetEyeParameter = function(vrDisplay, eye, offsetX, offsetY, offsetZ,
                               upDegree, rightDegree, downDegree, leftDegree) {
  vrDisplay.setEyeParameter(eye, offsetX, offsetY, offsetZ, upDegree, rightDegree,
                            downDegree, leftDegree);
}

var UpdateVRDisplay = function(vrDisplay) {
  vrDisplay.update();
}

var API = {
  AttachWebVRDisplay: AttachWebVRDisplay,
  SetVRDisplayPose: SetVRDisplayPose,
  SetEyeResolution: SetEyeResolution,
  SetEyeParameter: SetEyeParameter,
  UpdateVRDisplay: UpdateVRDisplay,

  none: false
};

return API;

}());