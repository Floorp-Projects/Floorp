/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// Test that `process.childID` is defined.

declTest("test childid", {
  async test(browser) {
    let parent = browser.browsingContext.currentWindowGlobal;
    ok(
      parent.domProcess.childID,
      "parent domProcess.childID should have a value."
    );
    await SpecialPowers.spawn(
      browser,
      [parent.domProcess.childID],
      async function (parentChildID) {
        ok(
          ChromeUtils.domProcessChild.childID,
          "child process.childID should have a value."
        );
        let childID = ChromeUtils.domProcessChild.childID;
        is(parentChildID, childID);
      }
    );
  },
});
