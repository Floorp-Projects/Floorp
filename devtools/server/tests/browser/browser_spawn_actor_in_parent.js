/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test DevToolsServerConnection.spawnActorInParentProcess.
// This test instanciates a first test actor "InContentActor" that uses
// spawnActorInParentProcess to instanciate the second test actor "InParentActor"

const ACTOR_URL =
  "chrome://mochitests/content/browser/devtools/server/tests/browser/test-spawn-actor-in-parent.js";

const { InContentFront, InParentFront } = require(ACTOR_URL);

add_task(async function() {
  // `spawnActorInParentProcess` is only used by `WebConsoleActor.startListeners(NetworkActivity)` via `spawnInParent`.
  // VsCode adapter isn't using this, but may be some important other tool is?
  //
  // WebConsoleActor.startListeners(NetworkActivity) is still called from:
  // * legacy network event listener (should only be used by the non-multiprocess browser toolbox)
  // * responsive actor (https://searchfox.org/mozilla-central/rev/699174544b058f13f02e7586b3c8fdbf438f084b/devtools/server/actors/emulation/responsive.js#114)
  //   This probably need some refactoring to implement the throttling in the parent process directly.
  await pushPref("devtools.target-switching.server.enabled", false);

  const browser = await addTab("data:text/html;charset=utf-8,foo");

  info("Register target-scoped actor in the content process");
  await registerActorInContentProcess(ACTOR_URL, {
    prefix: "inContent",
    constructor: "InContentActor",
    type: { target: true },
  });

  const tab = gBrowser.getTabForBrowser(browser);
  const target = await createAndAttachTargetForTab(tab);
  const { client } = target;
  const form = target.targetForm;

  // As this Front isn't instantiated by protocol.js, we have to manually
  // set its actor ID and manage it:
  const inContentFront = new InContentFront(client);
  inContentFront.actorID = form.inContentActor;
  inContentFront.manage(inContentFront);

  const isInContent = await inContentFront.isInContent();
  ok(isInContent, "ContentActor really runs in the content process");
  const formSpawn = await inContentFront.spawnInParent(ACTOR_URL);

  // As this Front isn't instantiated by protocol.js, we have to manually
  // set its actor ID and manage it:
  const inParentFront = new InParentFront(client, formSpawn);
  inParentFront.actorID = formSpawn.inParentActor;
  inParentFront.manage(inParentFront);

  const { args, isInParent, conn, mm } = await inParentFront.test();
  is(args[0], 1, "first actor constructor arg is correct");
  is(args[1], 2, "first actor constructor arg is correct");
  is(args[2], 3, "first actor constructor arg is correct");
  ok(isInParent, "The ParentActor really runs in the parent process");
  ok(
    conn,
    "`conn`, first contructor argument is a DevToolsServerConnection instance"
  );
  is(
    mm,
    "ChromeMessageSender",
    "`mm`, last constructor argument is a message manager"
  );

  await target.destroy();
  gBrowser.removeCurrentTab();
});
