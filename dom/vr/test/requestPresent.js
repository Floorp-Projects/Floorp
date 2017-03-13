// requestPresent.js
//
// This file provides helpers for testing VRDisplay requestPresent.

function attachVRDisplay(test) {
  assert_equals(typeof (navigator.getVRDisplays), "function", "'navigator.getVRDisplays()' must be defined.");
  return VRSimulationDriver.AttachWebVRDisplay();
}

function setupVRDisplay(test) {
  assert_equals(typeof (navigator.getVRDisplays), "function", "'navigator.getVRDisplays()' must be defined.");
  return VRSimulationDriver.AttachWebVRDisplay().then(() => {
    return navigator.getVRDisplays();
  }).then((displays) => {
    assert_equals(displays.length, 1, "displays.length must be one after attach.");
    vrDisplay = displays[0];
    return validateNewVRDisplay(test, vrDisplay);
  });
}

// Validate the settings off a freshly created VRDisplay (prior to calling
// requestPresent).
function validateNewVRDisplay(test, display) {
  assert_true(display.capabilities.canPresent, "display.capabilities.canPresent must always be true for HMDs.");
  assert_equals(display.capabilities.maxLayers, 1, "display.capabilities.maxLayers must always be 1 when display.capabilities.canPresent is true for current spec revision.");
  assert_false(display.isPresenting, "display.isPresenting must be false before calling requestPresent.");
  assert_equals(display.getLayers().length, 0, "display.getLayers() should have no layers if not presenting.");
  var promise = display.exitPresent();
  return promise_rejects(test, null, promise);
}

// Validate the settings off a VRDisplay after requestPresent promise is
// rejected or after exitPresent is fulfilled.
function validateDisplayNotPresenting(test, display) {
  assert_false(display.isPresenting, "display.isPresenting must be false if requestPresent is rejected or after exitPresent is fulfilled.");
  assert_equals(display.getLayers().length, 0, "display.getLayers() should have no layers if requestPresent is rejected  or after exitPresent is fulfilled.");
  var promise = display.exitPresent();
  return promise_rejects(test, null, promise);
}