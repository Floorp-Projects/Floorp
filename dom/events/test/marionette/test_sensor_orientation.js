/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 120000;
MARIONETTE_HEAD_JS = 'head.js';

function doTest(aAzimuth, aPitch, aRoll) {
  log("Testing [azimuth, pitch, roll] = " + Array.slice(arguments));

  return setEmulatorOrientationValues(aAzimuth, aPitch, aRoll)
    .then(() => waitForWindowEvent("deviceorientation"))
    .then(function(aEvent) {
      is(aEvent.alpha, aAzimuth, "azimuth");
      is(aEvent.beta,  aPitch, "pitch");
      is(aEvent.gamma, aRoll, "roll");
    });
}

function testAllPermutations() {
  const angles = [-180, -90, 0, 90, 180];
  let promise = Promise.resolve();
  for (let i = 0; i < angles.length; i++) {
    for (let j = 0; j < angles.length; j++) {
      for (let k = 0; k < angles.length; k++) {
        promise =
          promise.then(doTest.bind(null, angles[i], angles[j], angles[k]));
      }
    }
  }
  return promise;
}

startTestBase(function() {
  let origValues;

  return Promise.resolve()

    // Retrieve original status.
    .then(() => getEmulatorOrientationValues())
    .then(function(aValues) {
      origValues = aValues;
      is(typeof origValues, "object", "typeof origValues");
      is(origValues.length, 3, "origValues.length");
    })

    // Test original status
    .then(() => doTest.apply(null, origValues))

    .then(testAllPermutations)

    // Restore original status.
    .then(() => setEmulatorOrientationValues.apply(null, origValues));
});
