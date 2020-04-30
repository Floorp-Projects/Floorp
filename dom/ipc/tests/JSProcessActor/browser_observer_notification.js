/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

/* eslint-disable no-unused-vars */
declTest("test observer triggering actor creation", {
  async test(browser) {
    await SpecialPowers.spawn(browser, [], async function() {
      const TOPIC = "test-js-content-actor-child-observer";
      Services.obs.notifyObservers(content.window, TOPIC, "dataString");

      let child = ChromeUtils.contentChild;
      let actorChild = child.getActor("TestProcessActor");
      ok(actorChild, "JSProcessActorChild should have value.");
      ok(
        actorChild.lastObserved,
        "JSProcessActorChild lastObserved should have value."
      );
      let { subject, topic, data } = actorChild.lastObserved;
      is(topic, TOPIC, "Topic matches");
      is(data, "dataString", "Data matches");
    });
  },
});

declTest("test observers with null data", {
  async test(browser) {
    await SpecialPowers.spawn(browser, [], async function() {
      const TOPIC = "test-js-content-actor-child-observer";
      Services.obs.notifyObservers(content.window, TOPIC);

      let child = ChromeUtils.contentChild;
      let actorChild = child.getActor("TestProcessActor");
      ok(actorChild, "JSProcessActorChild should have value.");
      let { subject, topic, data } = actorChild.lastObserved;

      is(topic, TOPIC, "Topic matches");
      is(data, null, "Data matches");
    });
  },
});
