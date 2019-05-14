/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

declTest("destroy actor by iframe remove", {
  allFrames: true,

  async test(browser) {
    await ContentTask.spawn(browser, {}, async function() {
      // Create and append an iframe into the window's document.
      let frame = content.document.createElement("iframe");
      frame.id = "frame";
      content.document.body.appendChild(frame);
      await ContentTaskUtils.waitForEvent(frame, "load");
      is(content.window.frames.length, 1, "There should be an iframe.");
      let child = frame.contentWindow.window.getWindowGlobalChild();
      let actorChild = child.getActor("Test");
      ok(actorChild, "JSWindowActorChild should have value.");

      let willDestroyPromise = new Promise(resolve => {
        const TOPIC = "test-js-window-actor-willdestroy";
        Services.obs.addObserver(function obs(subject, topic, data) {
          ok(data, "willDestroyCallback data should be true.");

          Services.obs.removeObserver(obs, TOPIC);
          resolve();
        }, TOPIC);
      });

      let didDestroyPromise = new Promise(resolve => {
        const TOPIC = "test-js-window-actor-diddestroy";
        Services.obs.addObserver(function obs(subject, topic, data) {
          ok(data, "didDestroyCallback data should be true.");

          Services.obs.removeObserver(obs, TOPIC);
          resolve();
        }, TOPIC);
      });

      info("Remove frame");
      content.document.getElementById("frame").remove();
      await Promise.all([willDestroyPromise, didDestroyPromise]);

      Assert.throws(() => child.getActor("Test"),
        /InvalidStateError/, "Should throw if frame destroy.");
    });
  },
});

declTest("destroy actor by page navigates", {
  allFrames: true,

  async test(browser) {
    info("creating an in-process frame");
    await ContentTask.spawn(browser, URL, async function(url) {
      let frame = content.document.createElement("iframe");
      frame.src = url;
      content.document.body.appendChild(frame);
    });

    info("navigating page");
    await ContentTask.spawn(browser, TEST_URL, async function(url) {
      let frame = content.document.querySelector("iframe");
      frame.contentWindow.location = url;
      let child = frame.contentWindow.window.getWindowGlobalChild();
      let actorChild = child.getActor("Test");
      ok(actorChild, "JSWindowActorChild should have value.");
      await ContentTaskUtils.waitForEvent(frame, "load");

      Assert.throws(() => child.getActor("Test"),
              /InvalidStateError/, "Should throw if frame destroy.");
    });
  },
});
