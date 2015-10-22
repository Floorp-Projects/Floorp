/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Unit tests for handling hawkRequest
 */

"use strict";

Cu.import("resource://services-common/utils.js");

add_task(function* request_with_unicode() {
  const unicodeName = "yøü";

  loopServer.registerPathHandler("/fake", (request, response) => {
    let body = CommonUtils.readBytesFromInputStream(request.bodyInputStream);
    let jsonBody = JSON.parse(body);
    Assert.equal(jsonBody.name, CommonUtils.encodeUTF8(unicodeName));

    response.setStatusLine(null, 200, "OK");
    response.processAsync();
    response.finish();
  });

  yield MozLoopServiceInternal.hawkRequestInternal(LOOP_SESSION_TYPE.GUEST, "/fake", "POST", { name: unicodeName }).then(
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
