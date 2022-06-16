/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

declTest("double register", {
  async test(_browser, _window, fileExt) {
    SimpleTest.doesThrow(
      () =>
        ChromeUtils.registerContentActor(
          "TestProcessActor",
          processActorOptions[fileExt]
        ),
      "Should throw if register has duplicate name."
    );
  },
});
