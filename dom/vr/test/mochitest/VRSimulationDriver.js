
var VRServiceTest;
var vrMockDisplay;

var VRSimulationDriver = (function() {
"use strict";

var AttachWebVRDisplay = function() {
  var promise = VRServiceTest.attachVRDisplay("VRDisplayTest");
  promise.then(function (display) {
    assert_true(display != null, "AttachWebVRDisplay should success.");
    vrMockDisplay = display;
  });

  return promise;
};

var SetVRDisplayPose = function(position,
                                linearVelocity, linearAcceleration,
                                orientation, angularVelocity,
                                angularAcceleration) {
  vrMockDisplay.setPose(position, linearVelocity, linearAcceleration,
                        orientation, angularVelocity, angularAcceleration);
};

var SetEyeResolution = function(width, height) {
  vrMockDisplay.setEyeResolution(width, height);
}

var SetEyeParameter = function(eye, offsetX, offsetY, offsetZ,
                               upDegree, rightDegree, downDegree, leftDegree) {
  vrMockDisplay.setEyeParameter(eye, offsetX, offsetY, offsetZ, upDegree, rightDegree,
                                downDegree, leftDegree);
}

var SetMountState = function(isMounted) {
  vrMockDisplay.setMountState(isMounted);
}

var UpdateVRDisplay = function() {
  vrMockDisplay.update();
}

var API = {
  AttachWebVRDisplay: AttachWebVRDisplay,
  SetVRDisplayPose: SetVRDisplayPose,
  SetEyeResolution: SetEyeResolution,
  SetEyeParameter: SetEyeParameter,
  SetMountState: SetMountState,
  UpdateVRDisplay: UpdateVRDisplay,

  none: false
};

return API;

}());