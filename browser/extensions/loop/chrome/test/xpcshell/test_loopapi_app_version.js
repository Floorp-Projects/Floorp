/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

const { LoopAPI } = Cu.import("chrome://loop/content/modules/MozLoopAPI.jsm", {});
var [, gHandlers] = LoopAPI.inspect();

add_task(function test_mozLoop_npsSurveyURL() {
  MozLoopService.initialize("1.3.2alpha");
  // Test "alpha" has been stripped from string and MozLoopApi returns MozLoopService value
  gHandlers.GetAddonVersion({ data: [] }, result => {
    Assert.equal(result, "1.3.2", "expected output should have stripped alpha from string");
  });
});
