/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
  Tests covering gUM constraints API for audio, video and fake video. Exercise
  successful parsing code and ensure that unknown required constraints and
  overconstraining cases produce appropriate errors.

  TODO(jib): Merge desktop and mobile version of these tests again.
*/
var common_tests = [
  // Each test here tests a different constraint or codepath.
  { message: "unknown required constraint on video fails",
    constraints: { video: { somethingUnknown:0, require:["somethingUnknown"] },
                   fake: true },
    error: "NotFoundError" },
  { message: "unknown required constraint on audio fails",
    constraints: { audio: { somethingUnknown:0, require:["somethingUnknown"] },
                   fake: true },
    error: "NotFoundError" },
  { message: "video overconstrained by facingMode fails",
    constraints: { video: { facingMode:'left', require:["facingMode"] },
                   fake: true },
    error: "NotFoundError" },
  { message: "video overconstrained by facingMode array fails",
    constraints: { video: { facingMode:['left', 'right'], require:["facingMode"] },
                   fake: true },
    error: "NotFoundError" },
  { message: "audio overconstrained by facingMode fails",
    constraints: { audio: { facingMode:'left', require:["facingMode"] },
                   fake: true },
    error: "NotFoundError" },
  { message: "full screensharing requires permission",
    constraints: { video: { mediaSource:'screen' } },
    error: "PermissionDeniedError" },
  { message: "application screensharing requires permission",
    constraints: { video: { mediaSource:'application' } },
    error: "PermissionDeniedError" },
  { message: "window screensharing requires permission",
    constraints: { video: { mediaSource:'window' } },
    error: "PermissionDeniedError" },
  { message: "browser screensharing requires permission",
    constraints: { video: { mediaSource:'browser' } },
    error: "PermissionDeniedError" },
  { message: "unknown mediaSource fails",
    constraints: { video: { mediaSource:'uncle' } },
    error: "NotFoundError" },
  { message: "Success-path: optional video facingMode + audio ignoring facingMode",
    constraints: { fake: true,
                   audio: { mediaSource:'microphone',
                            facingMode:'left',
                            foo:0,
                            advanced: [{ facingMode:'environment' },
                                       { facingMode:'user' },
                                       { bar:0 }] },
                   video: { mediaSource:'camera',
                            facingMode:['left', 'right', 'user', 'environment'],
                            foo:0,
                            advanced: [{ facingMode:'environment' },
                                       { facingMode:['user'] },
                                       { bar:0 }] } },
    error: null }
];


/**
 * Starts the test run by running through each constraint
 * test by verifying that the right resolution and rejection is fired.
 */

function testConstraints(tests) {
  function testgum(prev, test) {
    return prev.then(() => navigator.mediaDevices.getUserMedia(test.constraints))
      .then(() => is(null, test.error, test.message),
            reason => is(reason.name, test.error, test.message + ": " + reason.message));
  }

  var p = new Promise(resolve => SpecialPowers.pushPrefEnv({
    set : [ ['media.getusermedia.browser.enabled', false],
            ['media.getusermedia.screensharing.enabled', false] ]
  }, resolve));

  tests.reduce(testgum, p)
    .catch(reason => ok(false, "Unexpected failure: " + reason.message))
    .then(SimpleTest.finish);
}
