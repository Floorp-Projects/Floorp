/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * This is an integration test from navigator.mozLoop through to the end
 * effects - rather than just testing MozLoopAPI alone.
 */

"use strict";

Components.utils.import("resource://gre/modules/Promise.jsm", this);

add_task(loadLoopPanel);

add_task(function* test_mozLoop_doNotDisturb() {
  registerCleanupFunction(function () {
    Services.prefs.clearUserPref("loop.do_not_disturb");
  });

  Assert.ok(gMozLoopAPI, "mozLoop should exist");

  // Test doNotDisturb (getter)
  Services.prefs.setBoolPref("loop.do_not_disturb", true);
  Assert.equal(gMozLoopAPI.doNotDisturb, true,
               "Do not disturb should be true");

  // Test doNotDisturb (setter)
  gMozLoopAPI.doNotDisturb = false;
  Assert.equal(Services.prefs.getBoolPref("loop.do_not_disturb"), false,
               "Do not disturb should be false");
});
