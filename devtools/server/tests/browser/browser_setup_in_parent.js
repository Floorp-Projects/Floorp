/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const ACTOR_URL =
  "chrome://mochitests/content/browser/devtools/server/tests/browser/test-setup-in-parent.js";

const { TestSetupInParentFront } = require(ACTOR_URL);

add_task(async function() {
  const browser = await addTab("data:text/html;charset=utf-8,foo");

  info("Register target-scoped actor in the content process");
  await registerActorInContentProcess(ACTOR_URL, {
    prefix: "testSetupInParent",
    constructor: "TestSetupInParentActor",
    type: { target: true },
  });

  const tab = gBrowser.getTabForBrowser(browser);
  const target = await createAndAttachTargetForTab(tab);
  const { client } = target;
  const form = target.targetForm;

  // As this Front isn't instantiated by protocol.js, we have to manually
  // set its actor ID and manage it:
  const front = new TestSetupInParentFront(client);
  front.actorID = form.testSetupInParentActor;
  front.manage(front);

  // Wait also for a reponse from setupInParent called from setup-in-child.js
  const onDone = new Promise(resolve => {
    const onParent = (_, topic, args) => {
      Services.obs.removeObserver(onParent, "test:setupParent");
      args = JSON.parse(args);

      is(args[0], true, "Got `mm` argument, a message manager");
      ok(args[1].match(/server\d+.conn\d+.child\d+/), "Got `prefix` argument");

      resolve();
    };
    Services.obs.addObserver(onParent, "test:setupParent");
  });

  await front.callSetupInParent();
  await onDone;

  await target.destroy();
  gBrowser.removeCurrentTab();
});
