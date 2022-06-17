/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

declTest("double register", {
  async test() {
    SimpleTest.doesThrow(
      () =>
        ChromeUtils.registerContentActor(
          "TestProcessActor",
          processActorOptions
        ),
      "Should throw if register has duplicate name."
    );
  },
});
