/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Test support methods on Target, such as `hasActor`, `getActorDescription`,
// `actorHasMethod` and `getTrait`.

var { WebAudioFront } =
  require("devtools/shared/fronts/webaudio");

async function testTarget(client, target) {
  await target.makeRemote();

  is(target.hasActor("timeline"), true, "target.hasActor() true when actor exists.");
  is(target.hasActor("webaudio"), true, "target.hasActor() true when actor exists.");
  is(target.hasActor("notreal"), false, "target.hasActor() false when actor does not exist.");
  // Create a front to ensure the actor is loaded
  new WebAudioFront(target.client, target.form);

  let desc = await target.getActorDescription("webaudio");
  is(desc.typeName, "webaudio",
    "target.getActorDescription() returns definition data for corresponding actor");
  is(desc.events["start-context"].type, "startContext",
    "target.getActorDescription() returns event data for corresponding actor");

  desc = await target.getActorDescription("nope");
  is(desc, undefined, "target.getActorDescription() returns undefined for non-existing actor");
  desc = await target.getActorDescription();
  is(desc, undefined, "target.getActorDescription() returns undefined for undefined actor");

  let hasMethod = await target.actorHasMethod("audionode", "getType");
  is(hasMethod, true,
    "target.actorHasMethod() returns true for existing actor with method");
  hasMethod = await target.actorHasMethod("audionode", "nope");
  is(hasMethod, false,
    "target.actorHasMethod() returns false for existing actor with no method");
  hasMethod = await target.actorHasMethod("nope", "nope");
  is(hasMethod, false,
    "target.actorHasMethod() returns false for non-existing actor with no method");
  hasMethod = await target.actorHasMethod();
  is(hasMethod, false,
    "target.actorHasMethod() returns false for undefined params");

  is(target.getTrait("customHighlighters"), true,
    "target.getTrait() returns boolean when trait exists");
  is(target.getTrait("giddyup"), undefined,
    "target.getTrait() returns undefined when trait does not exist");

  close(target, client);
}

// Ensure target is closed if client is closed directly
function test() {
  waitForExplicitFinish();

  getChromeActors((client, response) => {
    let options = {
      form: response,
      client: client,
      chrome: true
    };

    TargetFactory.forRemoteTab(options).then(testTarget.bind(null, client));
  });
}

function close(target, client) {
  target.on("close", () => {
    ok(true, "Target was closed");
    finish();
  });
  client.close();
}
