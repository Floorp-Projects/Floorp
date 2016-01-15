/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

const { LoopAPI } = Cu.import("chrome://loop/content/modules/MozLoopAPI.jsm", {});
var [, gHandlers] = LoopAPI.inspect();

add_task(function* test_mozLoop_doNotDisturb() {
  do_register_cleanup(function() {
    Services.prefs.clearUserPref("loop.do_not_disturb");
  });

  // Test doNotDisturb (getter)
  Services.prefs.setBoolPref("loop.do_not_disturb", true);
  gHandlers.GetDoNotDisturb({}, result => {
    Assert.equal(result, true, "Do not disturb should be true");
  });

  // Test doNotDisturb (setter)
  gHandlers.SetDoNotDisturb({ data: [false] }, () => {});
  Assert.equal(Services.prefs.getBoolPref("loop.do_not_disturb"), false,
               "Do not disturb should be false");
});
