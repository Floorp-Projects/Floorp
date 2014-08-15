/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * This is an integration test from navigator.mozLoop through to the end
 * effects - rather than just testing MozLoopAPI alone.
 */

add_task(loadLoopPanel);

add_task(function* test_mozLoop_charPref() {
  registerCleanupFunction(function () {
    Services.prefs.clearUserPref("loop.test");
  });

  Assert.ok(gMozLoopAPI, "mozLoop should exist");

  // Test setLoopCharPref
  gMozLoopAPI.setLoopCharPref("test", "foo");
  Assert.equal(Services.prefs.getCharPref("loop.test"), "foo",
               "should set loop pref value correctly");

  // Test getLoopCharPref
  Assert.equal(gMozLoopAPI.getLoopCharPref("test"), "foo",
               "should get loop pref value correctly");
});

add_task(function* test_mozLoop_boolPref() {
  registerCleanupFunction(function () {
    Services.prefs.clearUserPref("loop.testBool");
  });

  Assert.ok(gMozLoopAPI, "mozLoop should exist");

  Services.prefs.setBoolPref("loop.testBool", true);

  // Test getLoopCharPref
  Assert.equal(gMozLoopAPI.getLoopBoolPref("testBool"), true,
               "should get loop pref value correctly");
});
