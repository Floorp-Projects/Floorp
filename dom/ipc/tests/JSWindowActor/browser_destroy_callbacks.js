/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

declTest("destroy actor by iframe remove", {
  allFrames: true,

  async test(browser) {
    await SpecialPowers.spawn(browser, [], async function() {
      // Create and append an iframe into the window's document.
      let frame = content.document.createElement("iframe");
      frame.id = "frame";
      content.document.body.appendChild(frame);
      await ContentTaskUtils.waitForEvent(frame, "load");
      is(content.window.frames.length, 1, "There should be an iframe.");
      let child = frame.contentWindow.windowGlobalChild;
      let actorChild = child.getActor("TestWindow");
      ok(actorChild, "JSWindowActorChild should have value.");

      {
        let error = actorChild.uninitializedGetterError;
        const prop = "contentWindow";
        Assert.ok(
          error,
          `Should get error accessing '${prop}' before actor initialization`
        );
        if (error) {
          Assert.equal(
            error.name,
            "InvalidStateError",
            "Error should be an InvalidStateError"
          );
          Assert.equal(
            error.message,
            `JSWindowActorChild.${prop} getter: Cannot access property '${prop}' before actor is initialized`,
            "Error should have informative message"
          );
        }
      }

      let didDestroyPromise = new Promise(resolve => {
        const TOPIC = "test-js-window-actor-diddestroy";
        Services.obs.addObserver(function obs(subject, topic, data) {
          ok(data, "didDestroyCallback data should be true.");
          is(subject, actorChild, "Should have this value");

          Services.obs.removeObserver(obs, TOPIC);
          // Make a trip through the event loop to ensure that the
          // actor's manager has been cleared before running remaining
          // checks.
          Services.tm.dispatchToMainThread(resolve);
        }, TOPIC);
      });

      info("Remove frame");
      content.document.getElementById("frame").remove();
      await didDestroyPromise;

      Assert.throws(
        () => child.getActor("TestWindow"),
        /InvalidStateError/,
        "Should throw if frame destroy."
      );

      for (let prop of [
        "document",
        "browsingContext",
        "docShell",
        "contentWindow",
      ]) {
        let error;
        try {
          void actorChild[prop];
        } catch (e) {
          error = e;
        }
        Assert.ok(
          error,
          `Should get error accessing '${prop}' after actor destruction`
        );
        if (error) {
          Assert.equal(
            error.name,
            "InvalidStateError",
            "Error should be an InvalidStateError"
          );
          Assert.equal(
            error.message,
            `JSWindowActorChild.${prop} getter: Cannot access property '${prop}' after actor 'TestWindow' has been destroyed`,
            "Error should have informative message"
          );
        }
      }
    });
  },
});

declTest("destroy actor by page navigates", {
  allFrames: true,

  async test(browser) {
    info("creating an in-process frame");
    await SpecialPowers.spawn(browser, [URL], async function(url) {
      let frame = content.document.createElement("iframe");
      frame.src = url;
      content.document.body.appendChild(frame);
    });

    info("navigating page");
    await SpecialPowers.spawn(browser, [TEST_URL], async function(url) {
      let frame = content.document.querySelector("iframe");
      frame.contentWindow.location = url;
      let child = frame.contentWindow.windowGlobalChild;
      let actorChild = child.getActor("TestWindow");
      ok(actorChild, "JSWindowActorChild should have value.");

      let didDestroyPromise = new Promise(resolve => {
        const TOPIC = "test-js-window-actor-diddestroy";
        Services.obs.addObserver(function obs(subject, topic, data) {
          ok(data, "didDestroyCallback data should be true.");
          is(subject, actorChild, "Should have this value");

          Services.obs.removeObserver(obs, TOPIC);
          resolve();
        }, TOPIC);
      });

      await Promise.all([
        didDestroyPromise,
        ContentTaskUtils.waitForEvent(frame, "load"),
      ]);

      Assert.throws(
        () => child.getActor("TestWindow"),
        /InvalidStateError/,
        "Should throw if frame destroy."
      );
    });
  },
});

declTest("destroy actor by tab being closed", {
  allFrames: true,

  async test(browser) {
    info("creating a new tab");
    let newTab = await BrowserTestUtils.openNewForegroundTab(gBrowser, URL);
    let newTabBrowser = newTab.linkedBrowser;

    let parent = newTabBrowser.browsingContext.currentWindowGlobal.getActor(
      "TestWindow"
    );
    ok(parent, "JSWindowActorParent should have value.");

    // We can't depend on `SpecialPowers.spawn` to resolve our promise, as the
    // frame message manager will be being shut down at the same time. Instead
    // send messages over the per-process message manager which should still be
    // active.
    let didDestroyPromise = new Promise(resolve => {
      Services.ppmm.addMessageListener(
        "test-jswindowactor-diddestroy",
        function onmessage(msg) {
          Services.ppmm.removeMessageListener(
            "test-jswindowactor-diddestroy",
            onmessage
          );
          resolve();
        }
      );
    });

    info("setting up destroy listeners");
    await SpecialPowers.spawn(newTabBrowser, [], () => {
      let child = content.windowGlobalChild;
      let actorChild = child.getActor("TestWindow");
      ok(actorChild, "JSWindowActorChild should have value.");

      Services.obs.addObserver(function obs(subject, topic, data) {
        if (subject != actorChild) {
          return;
        }
        dump("DidDestroy called\n");
        Services.obs.removeObserver(obs, "test-js-window-actor-diddestroy");
        Services.cpmm.sendAsyncMessage("test-jswindowactor-diddestroy", data);
      }, "test-js-window-actor-diddestroy");
    });

    info("removing new tab");
    await BrowserTestUtils.removeTab(newTab);
    info("waiting for destroy callbacks to fire");
    await didDestroyPromise;
    info("got didDestroy callback");
  },
});
