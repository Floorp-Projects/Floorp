/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

/* eslint-disable no-unused-vars */
declTest("test observer triggering actor creation", {
  async test(browser) {
    await SpecialPowers.spawn(browser, [], async function() {
      const TOPIC = "test-js-window-actor-child-observer";
      Services.obs.notifyObservers(content.window, TOPIC, "dataString");

      let child = content.windowGlobalChild;
      let actorChild = child.getActor("TestWindow");
      ok(actorChild, "JSWindowActorChild should have value.");
      let { subject, topic, data } = actorChild.lastObserved;

      is(
        subject.windowGlobalChild.getActor("TestWindow"),
        actorChild,
        "Should have been recieved on the right actor"
      );
      is(topic, TOPIC, "Topic matches");
      is(data, "dataString", "Data matches");
    });
  },
});

declTest("test observers with null data", {
  async test(browser) {
    await SpecialPowers.spawn(browser, [], async function() {
      const TOPIC = "test-js-window-actor-child-observer";
      Services.obs.notifyObservers(content.window, TOPIC);

      let child = content.windowGlobalChild;
      let actorChild = child.getActor("TestWindow");
      ok(actorChild, "JSWindowActorChild should have value.");
      let { subject, topic, data } = actorChild.lastObserved;

      is(
        subject.windowGlobalChild.getActor("TestWindow"),
        actorChild,
        "Should have been recieved on the right actor"
      );
      is(topic, TOPIC, "Topic matches");
      is(data, null, "Data matches");
    });
  },
});

declTest("observers don't notify with wrong window", {
  async test(browser) {
    await SpecialPowers.spawn(browser, [], async function() {
      const TOPIC = "test-js-window-actor-child-observer";
      Services.obs.notifyObservers(null, TOPIC);
      let child = content.windowGlobalChild;
      let actorChild = child.getActor("TestWindow");
      ok(actorChild, "JSWindowActorChild should have value.");
      is(
        actorChild.lastObserved,
        undefined,
        "Should not receive wrong window's observer notification!"
      );
    });
  },
});

declTest("observers notify with audio-playback", {
  url:
    "http://example.com/browser/dom/ipc/tests/JSWindowActor/file_mediaPlayback.html",

  async test(browser) {
    await SpecialPowers.spawn(browser, [], async function() {
      let audio = content.document.querySelector("audio");
      audio.play();

      let child = content.windowGlobalChild;
      let actorChild = child.getActor("TestWindow");
      ok(actorChild, "JSWindowActorChild should have value.");

      let observePromise = new Promise(resolve => {
        actorChild.done = ({ subject, topic, data }) =>
          resolve({ subject, topic, data });
      });

      let { subject, topic, data } = await observePromise;
      is(topic, "audio-playback", "Topic matches");
      is(data, "active", "Data matches");
    });
  },
});
