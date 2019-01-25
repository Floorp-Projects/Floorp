/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(function test_registerWindowActor() {
  let windowActorOptions = {
    parent: {
      moduleURI: "resource:///actors/TestParent.jsm",
    },
    child: {
      moduleURI: "resource:///actors/TestChild.jsm",
    },
  };

  Assert.ok(ChromeUtils, "Should be able to get the ChromeUtils interface");
  ChromeUtils.registerWindowActor("Test", windowActorOptions);
  Assert.ok(true);
  Assert.throws(() =>
    ChromeUtils.registerWindowActor("Test", windowActorOptions),
    /NotSupportedError/,
    "Should throw if register duplicate name.");
});
