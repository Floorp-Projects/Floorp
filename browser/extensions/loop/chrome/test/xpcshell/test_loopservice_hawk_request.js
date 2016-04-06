/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Unit tests for handling hawkRequest
 */

"use strict";

/* exported run_test */

Cu.import("resource://services-common/utils.js");

add_task(function* request_with_unicode() {
  // Note unicodeName must be unicode, not utf-8
  const unicodeName = "y\xf8\xfc"; // "yøü"

  loopServer.registerPathHandler("/fake", (request, response) => {
    let body = CommonUtils.readBytesFromInputStream(request.bodyInputStream);
    let jsonBody = JSON.parse(CommonUtils.decodeUTF8(body));
    Assert.equal(jsonBody.name, unicodeName);

    response.setStatusLine(null, 200, "OK");
    response.processAsync();
    response.finish();
  });

  yield MozLoopServiceInternal.hawkRequestInternal(LOOP_SESSION_TYPE.GUEST, "/fake", "POST", { name: unicodeName }).then(
    () => Assert.ok(true, "Should have accepted"),
    () => Assert.ok(false, "Should have accepted"));
});

add_task(function* request_with_unicode() {
  loopServer.registerPathHandler("/fake", (request, response) => {
    Assert.ok(request.hasHeader("x-loop-addon-ver"), "Should have an add-on version header");
    Assert.equal(request.getHeader("x-loop-addon-ver"), "3.1", "Should have the correct add-on version");

    response.setStatusLine(null, 200, "OK");
    response.processAsync();
    response.finish();
  });

  // Pretend we're not enabled so full initialisation doesn't take place.
  Services.prefs.setBoolPref("loop.enabled", false);

  try {
    yield MozLoopService.initialize("3.1");
  } catch (ex) {
    // Do nothing - this will throw due to being disabled, that's ok.
  }

  Services.prefs.clearUserPref("loop.enabled");

  yield MozLoopServiceInternal.hawkRequestInternal(LOOP_SESSION_TYPE.GUEST, "/fake", "POST", {}).then(
    () => Assert.ok(true, "Should have accepted"),
    () => Assert.ok(false, "Should have accepted"));
});

function run_test() {
  setupFakeLoopServer();

  do_register_cleanup(() => {
    Services.prefs.clearUserPref("loop.hawk-session-token");
    Services.prefs.clearUserPref("loop.hawk-session-token.fxa");
    MozLoopService.errors.clear();
  });

  run_next_test();
}
