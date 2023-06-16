/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

declTest("getActor with remoteType match", {
  remoteTypes: ["web"],

  async test(browser) {
    let parent = browser.browsingContext.currentWindowGlobal.domProcess;
    ok(
      parent.getActor("TestProcessActor"),
      "JSProcessActorParent should have value."
    );

    await SpecialPowers.spawn(browser, [], async function () {
      let child = ChromeUtils.domProcessChild;
      ok(child, "DOMProcessChild should have value.");
      ok(
        child.getActor("TestProcessActor"),
        "JSProcessActorChild should have value."
      );
    });
  },
});

declTest("getActor with remoteType mismatch", {
  remoteTypes: ["privilegedabout"],
  url: TEST_URL,

  async test(browser) {
    let parent = browser.browsingContext.currentWindowGlobal.domProcess;
    Assert.throws(
      () => parent.getActor("TestProcessActor"),
      /NotSupportedError/,
      "Should throw if its remoteTypes don't match."
    );

    await SpecialPowers.spawn(browser, [], async function () {
      let child = ChromeUtils.domProcessChild;
      ok(child, "DOMProcessChild should have value.");
      Assert.throws(
        () => child.getActor("TestProcessActor"),
        /NotSupportedError/,
        "Should throw if its remoteTypes don't match."
      );
    });
  },
});

declTest("getActor without includeParent", {
  includeParent: false,

  async test(_browser, win) {
    let parent = win.docShell.browsingContext.currentWindowGlobal.domProcess;
    SimpleTest.doesThrow(
      () => parent.getActor("TestProcessActor"),
      "Should throw if includeParent is false."
    );

    let child = ChromeUtils.domProcessChild;
    SimpleTest.doesThrow(
      () => child.getActor("TestProcessActor"),
      "Should throw if includeParent is false."
    );
  },
});

declTest("getActor with includeParent", {
  includeParent: true,

  async test(_browser, win) {
    let parent = win.docShell.browsingContext.currentWindowGlobal.domProcess;
    let actorParent = parent.getActor("TestProcessActor");
    ok(actorParent, "JSProcessActorParent should have value.");

    let child = ChromeUtils.domProcessChild;
    let actorChild = child.getActor("TestProcessActor");
    ok(actorChild, "JSProcessActorChild should have value.");
  },
});
