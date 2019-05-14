/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

declTest("test observer triggering actor creation", {
  async test(browser) {
    await ContentTask.spawn(browser, {}, async function() {
      const TOPIC = "test-js-window-actor-child-observer";
      Services.obs.notifyObservers(content.window, TOPIC, "dataString");

      let child = content.window.getWindowGlobalChild();
      let actorChild = child.getActor("Test");
      ok(actorChild, "JSWindowActorChild should have value.");
      let {subject, topic, data} = actorChild.lastObserved;

      is(subject.getWindowGlobalChild().getActor("Test"), actorChild, "Should have been recieved on the right actor");
      is(topic, TOPIC, "Topic matches");
      is(data, "dataString", "Data matches");
    });
  },
});

declTest("test observers with null data", {
  async test(browser) {
    await ContentTask.spawn(browser, {}, async function() {
      const TOPIC = "test-js-window-actor-child-observer";
      Services.obs.notifyObservers(content.window, TOPIC);

      let child = content.window.getWindowGlobalChild();
      let actorChild = child.getActor("Test");
      ok(actorChild, "JSWindowActorChild should have value.");
      let {subject, topic, data} = actorChild.lastObserved;

      is(subject.getWindowGlobalChild().getActor("Test"), actorChild, "Should have been recieved on the right actor");
      is(topic, TOPIC, "Topic matches");
      is(data, null, "Data matches");
    });
  },
});

declTest("observers don't notify with wrong window", {
  async test(browser) {
    await ContentTask.spawn(browser, {}, async function() {
      const TOPIC = "test-js-window-actor-child-observer";
      Services.obs.notifyObservers(null, TOPIC);
      let child = content.window.getWindowGlobalChild();
      let actorChild = child.getActor("Test");
      ok(actorChild, "JSWindowActorChild should have value.");
      is(actorChild.lastObserved, undefined, "Should not receive wrong window's observer notification!");
    });
  },
});
