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
  { message: "audio overconstrained by facingMode fails",
    constraints: { audio: { facingMode:'left', require:["facingMode"] },
                   fake: true },
    error: "NotFoundError" },
  { message: "Success-path: optional video facingMode + audio ignoring facingMode",
    constraints: { fake: true,
                   audio: { facingMode:'left',
                            foo:0,
                            advanced: [{ facingMode:'environment' },
                                       { facingMode:'user' },
                                       { bar:0 }] },
                   video: { // TODO: Bug 767924 sequences in unions
                            //facingMode:['left', 'right', 'user', 'environment'],
                            //require:["facingMode"],
                            facingMode:'left',
                            foo:0,
                            advanced: [{ facingMode:'environment' },
                                       { facingMode:'user' },
                                       { bar:0 }] } },
    error: null }
];


/**
 * Starts the test run by running through each constraint
 * test by verifying that the right callback and error message is fired.
 */

function testConstraints(tests) {
  var i = 0;
  next();

  function Success() {
    ok(!tests[i].error, tests[i].message);
    i++;
    next();
  }
  function Failure(err) {
    ok(tests[i].error? (err.name == tests[i].error) : false,
       tests[i].message + " (err=" + err.name + ")");
    i++;
    next();
  }
  function next() {
    if (i < tests.length) {
      navigator.mozGetUserMedia(tests[i].constraints, Success, Failure);
    } else {
      SimpleTest.finish();
    }
  }
};
