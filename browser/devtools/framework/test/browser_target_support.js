/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Test support methods on Target, such as `hasActor`, `getActorDescription`,
// `actorHasMethod` and `getTrait`.

let { DebuggerServer } =
  Cu.import("resource://gre/modules/devtools/dbg-server.jsm", {});
let { DebuggerClient } =
  Cu.import("resource://gre/modules/devtools/dbg-client.jsm", {});
let { devtools } =
  Cu.import("resource://gre/modules/devtools/Loader.jsm", {});
let { Task } =
  Cu.import("resource://gre/modules/Task.jsm", {});
let { WebAudioFront } =
  devtools.require("devtools/server/actors/webaudio");

function* testTarget (client, target) {
  yield target.makeRemote();

  ise(target.hasActor("timeline"), true, "target.hasActor() true when actor exists.");
  ise(target.hasActor("webaudio"), true, "target.hasActor() true when actor exists.");
  ise(target.hasActor("notreal"), false, "target.hasActor() false when actor does not exist.");
  // Create a front to ensure the actor is loaded
  let front = new WebAudioFront(target.client, target.form);

  let desc = yield target.getActorDescription("webaudio");
  ise(desc.typeName, "webaudio",
    "target.getActorDescription() returns definition data for corresponding actor");
  ise(desc.events["start-context"]["type"], "startContext",
    "target.getActorDescription() returns event data for corresponding actor");

  desc = yield target.getActorDescription("nope");
  ise(desc, undefined, "target.getActorDescription() returns undefined for non-existing actor");
  desc = yield target.getActorDescription();
  ise(desc, undefined, "target.getActorDescription() returns undefined for undefined actor");

  let hasMethod = yield target.actorHasMethod("audionode", "getType");
  ise(hasMethod, true,
    "target.actorHasMethod() returns true for existing actor with method");
  hasMethod = yield target.actorHasMethod("audionode", "nope");
  ise(hasMethod, false,
    "target.actorHasMethod() returns false for existing actor with no method");
  hasMethod = yield target.actorHasMethod("nope", "nope");
  ise(hasMethod, false,
    "target.actorHasMethod() returns false for non-existing actor with no method");
  hasMethod = yield target.actorHasMethod();
  ise(hasMethod, false,
    "target.actorHasMethod() returns false for undefined params");

  ise(target.getTrait("customHighlighters")[0], "BoxModelHighlighter",
    "target.getTrait() returns objects when trait exists");
  ise(target.getTrait("giddyup"), undefined,
    "target.getTrait() returns undefined when trait does not exist");

  close(target, client);
}

// Ensure target is closed if client is closed directly
function test() {
  waitForExplicitFinish();

  if (!DebuggerServer.initialized) {
    DebuggerServer.init(function () { return true; });
    DebuggerServer.addBrowserActors();
  }

  var client = new DebuggerClient(DebuggerServer.connectPipe());
  client.connect(() => {
    client.listTabs(response => {
      let options = {
        form: response,
        client: client,
        chrome: true
      };

      devtools.TargetFactory.forRemoteTab(options).then(Task.async(testTarget).bind(null, client));
    });
  });
}

function close (target, client) {
  target.on("close", () => {
    ok(true, "Target was closed");
    DebuggerServer.destroy();
    finish();
  });
  client.close();
}
