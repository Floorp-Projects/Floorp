/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

const { LoopAPI } = Cu.import("chrome://loop/content/modules/MozLoopAPI.jsm", {});
var [, gHandlers] = LoopAPI.inspect();

add_task(function* test_mozLoop_charPref() {
  do_register_cleanup(function() {
    Services.prefs.clearUserPref("loop.test");
  });

  // Test setLoopPref
  gHandlers.SetLoopPref({ data: ["test", "foo", Ci.nsIPrefBranch.PREF_STRING] },
    () => {});
  Assert.equal(Services.prefs.getCharPref("loop.test"), "foo",
               "should set loop pref value correctly");

  // Test getLoopPref
  gHandlers.GetLoopPref({ data: ["test"] }, result => {
    Assert.equal(result, "foo", "should get loop pref value correctly");
  });
});

add_task(function* test_mozLoop_boolPref() {
  do_register_cleanup(function() {
    Services.prefs.clearUserPref("loop.testBool");
  });

  Services.prefs.setBoolPref("loop.testBool", true);

  // Test getLoopPref
  gHandlers.GetLoopPref({ data: ["testBool"] }, result => {
    Assert.equal(result, true, "should get loop pref value correctly");
  });
});
