/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Unit tests for the hawkRequest API
 */

"use strict";

Cu.import("resource:///modules/loop/MozLoopAPI.jsm");

let sandbox;
function assertInSandbox(expr, msg_opt) {
  Assert.ok(Cu.evalInSandbox(expr, sandbox), msg_opt);
}

sandbox = Cu.Sandbox("about:looppanel", { wantXrays: false } );
injectLoopAPI(sandbox, true);

add_task(function* hawk_session_scope_constants() {
  assertInSandbox("typeof mozLoop.LOOP_SESSION_TYPE !== 'undefined'");

  assertInSandbox("mozLoop.LOOP_SESSION_TYPE.GUEST === 1");

  assertInSandbox("mozLoop.LOOP_SESSION_TYPE.FXA === 2");
});

function generateSessionTypeVerificationStub(desiredSessionType) {

  function hawkRequestStub(sessionType, path, method, payloadObj, callback) {
    return new Promise(function (resolve, reject) {
      Assert.equal(desiredSessionType, sessionType);

      resolve();
    });
  }

  return hawkRequestStub;
}

const origHawkRequest = MozLoopService.hawkRequest;
do_register_cleanup(function() {
  MozLoopService.hawkRequest = origHawkRequest;
});

add_task(function* hawk_request_scope_passthrough() {

  // add a stub that verifies the parameter we want
  MozLoopService.hawkRequest =
    generateSessionTypeVerificationStub(sandbox.mozLoop.LOOP_SESSION_TYPE.FXA);

  // call mozLoop.hawkRequest, which calls MozLoopAPI.hawkRequest, which calls
  // MozLoopService.hawkRequest
  Cu.evalInSandbox(
    "mozLoop.hawkRequest(mozLoop.LOOP_SESSION_TYPE.FXA," +
                       " 'call-url/fakeToken', 'POST', {}, function() {})",
    sandbox);

  MozLoopService.hawkRequest =
    generateSessionTypeVerificationStub(sandbox.mozLoop.LOOP_SESSION_TYPE.GUEST);

  Cu.evalInSandbox(
    "mozLoop.hawkRequest(mozLoop.LOOP_SESSION_TYPE.GUEST," +
    " 'call-url/fakeToken', 'POST', {}, function() {})",
    sandbox);

});

function run_test() {
  run_next_test();
}
