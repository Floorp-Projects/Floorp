/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

requestLongerTimeout(2);

declTest("getActor with mismatch", {
  matches: ["*://*/*"],

  async test(browser) {
    let parent = browser.browsingContext.currentWindowGlobal;
    ok(parent, "WindowGlobalParent should have value.");
    Assert.throws(
      () => parent.getActor("Test"),
      /NS_ERROR_NOT_AVAILABLE/,
      "Should throw if it doesn't match."
    );

    await SpecialPowers.spawn(browser, [], async function() {
      let child = content.windowGlobalChild;
      ok(child, "WindowGlobalChild should have value.");

      Assert.throws(
        () => child.getActor("Test"),
        /NS_ERROR_NOT_AVAILABLE/,
        "Should throw if it doesn't match."
      );
    });
  },
});

declTest("getActor with matches", {
  matches: ["*://*/*"],
  url: TEST_URL,

  async test(browser) {
    let parent = browser.browsingContext.currentWindowGlobal;
    ok(parent.getActor("Test"), "JSWindowActorParent should have value.");

    await SpecialPowers.spawn(browser, [], async function() {
      let child = content.windowGlobalChild;
      ok(child, "WindowGlobalChild should have value.");
      ok(child.getActor("Test"), "JSWindowActorChild should have value.");
    });
  },
});

declTest("getActor with iframe matches", {
  allFrames: true,
  matches: ["*://*/*"],

  async test(browser) {
    await SpecialPowers.spawn(browser, [TEST_URL], async function(url) {
      // Create and append an iframe into the window's document.
      let frame = content.document.createElement("iframe");
      frame.src = url;
      content.document.body.appendChild(frame);
      await ContentTaskUtils.waitForEvent(frame, "load");

      is(content.frames.length, 1, "There should be an iframe.");
      await content.SpecialPowers.spawn(frame, [], () => {
        let child = content.windowGlobalChild;
        Assert.ok(
          child.getActor("Test"),
          "JSWindowActorChild should have value."
        );
      });
    });
  },
});

declTest("getActor with iframe mismatch", {
  allFrames: true,
  matches: ["about:home"],

  async test(browser) {
    await SpecialPowers.spawn(browser, [TEST_URL], async function(url) {
      // Create and append an iframe into the window's document.
      let frame = content.document.createElement("iframe");
      frame.src = url;
      content.document.body.appendChild(frame);
      await ContentTaskUtils.waitForEvent(frame, "load");

      is(content.frames.length, 1, "There should be an iframe.");
      await content.SpecialPowers.spawn(frame, [], () => {
        let child = content.windowGlobalChild;
        Assert.throws(
          () => child.getActor("Test"),
          /NS_ERROR_NOT_AVAILABLE/,
          "Should throw if it doesn't match."
        );
      });
    });
  },
});

declTest("getActor with remoteType match", {
  remoteTypes: ["web"],

  async test(browser) {
    let parent = browser.browsingContext.currentWindowGlobal;
    ok(parent.getActor("Test"), "JSWindowActorParent should have value.");

    await SpecialPowers.spawn(browser, [], async function() {
      let child = content.windowGlobalChild;
      ok(child, "WindowGlobalChild should have value.");
      ok(child.getActor("Test"), "JSWindowActorChild should have value.");
    });
  },
});

declTest("getActor with iframe remoteType match", {
  allFrames: true,
  remoteTypes: ["web"],

  async test(browser) {
    await SpecialPowers.spawn(browser, [TEST_URL], async function(url) {
      let child = content.windowGlobalChild;
      ok(child, "WindowGlobalChild should have value.");
      ok(child.getActor("Test"), "JSWindowActorChild should have value.");

      // Create and append an iframe into the window's document.
      let frame = content.document.createElement("iframe");
      frame.src = url;
      content.document.body.appendChild(frame);
      await ContentTaskUtils.waitForEvent(frame, "load");

      is(content.frames.length, 1, "There should be an iframe.");
      await content.SpecialPowers.spawn(frame, [], () => {
        child = content.windowGlobalChild;
        Assert.ok(
          child.getActor("Test"),
          "JSWindowActorChild should have value."
        );
      });
    });
  },
});

declTest("getActor with remoteType mismatch", {
  remoteTypes: ["privilegedabout"],
  url: TEST_URL,

  async test(browser) {
    let parent = browser.browsingContext.currentWindowGlobal;
    Assert.throws(
      () => parent.getActor("Test"),
      /NS_ERROR_NOT_AVAILABLE/,
      "Should throw if its remoteTypes don't match."
    );

    await SpecialPowers.spawn(browser, [], async function() {
      let child = content.windowGlobalChild;
      ok(child, "WindowGlobalChild should have value.");
      Assert.throws(
        () => child.getActor("Test"),
        /NS_ERROR_NOT_AVAILABLE/,
        "Should throw if its remoteTypes don't match."
      );
    });
  },
});

declTest("getActor with iframe messageManagerGroups match", {
  allFrames: true,
  messageManagerGroups: ["browsers"],

  async test(browser) {
    let parent = browser.browsingContext.currentWindowGlobal;
    ok(parent.getActor("Test"), "JSWindowActorParent should have value.");

    await SpecialPowers.spawn(browser, [TEST_URL], async function(url) {
      let child = content.windowGlobalChild;
      ok(child, "WindowGlobalChild should have value.");
      ok(child.getActor("Test"), "JSWindowActorChild should have value.");
    });
  },
});

declTest("getActor with iframe messageManagerGroups mismatch", {
  allFrames: true,
  messageManagerGroups: ["sidebars"],

  async test(browser) {
    let parent = browser.browsingContext.currentWindowGlobal;
    Assert.throws(
      () => parent.getActor("Test"),
      /NS_ERROR_NOT_AVAILABLE/,
      "Should throw if its messageManagerGroups doesn't match."
    );

    await SpecialPowers.spawn(browser, [TEST_URL], async function(url) {
      let child = content.windowGlobalChild;
      ok(child, "WindowGlobalChild should have value.");
      Assert.throws(
        () => child.getActor("Test"),
        /NS_ERROR_NOT_AVAILABLE/,
        "Should throw if its messageManagerGroups doesn't match."
      );
    });
  },
});

declTest("getActor without allFrames", {
  allFrames: false,

  async test(browser) {
    await SpecialPowers.spawn(browser, [], async function() {
      // Create and append an iframe into the window's document.
      let frame = content.document.createElement("iframe");
      content.document.body.appendChild(frame);
      is(content.frames.length, 1, "There should be an iframe.");
      let child = frame.contentWindow.windowGlobalChild;
      Assert.throws(
        () => child.getActor("Test"),
        /NS_ERROR_NOT_AVAILABLE/,
        "Should throw if allFrames is false."
      );
    });
  },
});

declTest("getActor with allFrames", {
  allFrames: true,

  async test(browser) {
    await SpecialPowers.spawn(browser, [], async function() {
      // Create and append an iframe into the window's document.
      let frame = content.document.createElement("iframe");
      content.document.body.appendChild(frame);
      is(content.frames.length, 1, "There should be an iframe.");
      let child = frame.contentWindow.windowGlobalChild;
      let actorChild = child.getActor("Test");
      ok(actorChild, "JSWindowActorChild should have value.");
    });
  },
});

declTest("getActor without includeChrome", {
  includeChrome: false,

  async test(_browser, win) {
    let parent = win.docShell.browsingContext.currentWindowGlobal;
    SimpleTest.doesThrow(
      () => parent.getActor("Test"),
      "Should throw if includeChrome is false."
    );
  },
});

declTest("getActor with includeChrome", {
  includeChrome: true,

  async test(_browser, win) {
    let parent = win.docShell.browsingContext.currentWindowGlobal;
    let actorParent = parent.getActor("Test");
    ok(actorParent, "JSWindowActorParent should have value.");
  },
});
