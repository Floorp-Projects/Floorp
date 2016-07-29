/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

const { LoopAPI } = Cu.import("chrome://loop/content/modules/MozLoopAPI.jsm", {});
var [, gHandlers] = LoopAPI.inspect();

add_task(function* test_mozLoop_gettingStartedURL() {
  let expectedURL = MozLoopService.getTourURL().href;
  // Test gettingStartedURL
  gHandlers.GettingStartedURL({ data: [] }, result => {
    Assert.equal(result, expectedURL, "should get mozLoopService GettingStartedURL value correctly");
  });
});
