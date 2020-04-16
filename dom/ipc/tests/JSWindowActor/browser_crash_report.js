/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

declTest("crash actor", {
  allFrames: true,

  async test(browser) {
    if (!("@mozilla.org/toolkit/crash-reporter;1" in Cc)) {
      info("Cannot test crash annotations without a crash reporter");
      return;
    }

    {
      info("Creating a new tab.");
      let newTab = await BrowserTestUtils.openNewForegroundTab(gBrowser, URL);
      let newTabBrowser = newTab.linkedBrowser;

      let parent = newTabBrowser.browsingContext.currentWindowGlobal.getActor(
        "Test"
      );
      ok(parent, "JSWindowActorParent should have value.");

      await SpecialPowers.spawn(newTabBrowser, [], async function() {
        let child = content.windowGlobalChild;
        ok(child, "WindowGlobalChild should have value.");
        is(
          child.isInProcess,
          false,
          "Actor should be loaded in the content process."
        );
        // Make sure that the actor is loaded.
        let actorChild = child.getActor("Test");
        is(actorChild.show(), "TestChild", "actor show should have value.");
        is(
          actorChild.manager,
          child,
          "manager should match WindowGlobalChild."
        );
      });

      info(
        "Crashing from withing an actor. We should have an actor name and a message name."
      );
      let report = await BrowserTestUtils.crashFrame(
        newTabBrowser,
        /* shouldShowTabCrashPage = */ false,
        /* shouldClearMinidumps =  */ true,
        /* browsingContext = */ null,
        { asyncCrash: false }
      );

      is(report.JSActorName, "BrowserTestUtils");
      is(report.JSActorMessage, "BrowserTestUtils:CrashFrame");

      BrowserTestUtils.removeTab(newTab);
    }

    {
      info("Creating a new tab for async crash");
      let newTab = await BrowserTestUtils.openNewForegroundTab(gBrowser, URL);
      let newTabBrowser = newTab.linkedBrowser;

      let parent = newTabBrowser.browsingContext.currentWindowGlobal.getActor(
        "Test"
      );
      ok(parent, "JSWindowActorParent should have value.");

      await SpecialPowers.spawn(newTabBrowser, [], async function() {
        let child = content.windowGlobalChild;
        ok(child, "WindowGlobalChild should have value.");
        is(
          child.isInProcess,
          false,
          "Actor should be loaded in the content process."
        );
        // Make sure that the actor is loaded.
        let actorChild = child.getActor("Test");
        is(actorChild.show(), "TestChild", "actor show should have value.");
        is(
          actorChild.manager,
          child,
          "manager should match WindowGlobalChild."
        );
      });

      info(
        "Crashing from without an actor. We should have neither an actor name nor a message name."
      );
      let report = await BrowserTestUtils.crashFrame(
        newTabBrowser,
        /* shouldShowTabCrashPage = */ false,
        /* shouldClearMinidumps =  */ true,
        /* browsingContext = */ null,
        { asyncCrash: true }
      );

      is(report.JSActorName, null);
      is(report.JSActorMessage, null);

      BrowserTestUtils.removeTab(newTab);
    }
  },
});
