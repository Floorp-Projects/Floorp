"use strict";

var gClient;

function test() {
  waitForExplicitFinish();
  const {ActorRegistryFront} = require("devtools/shared/fronts/actor-registry");
  const actorURL = "chrome://mochitests/content/chrome/devtools/server/tests/mochitest/hello-actor.js";

  DebuggerServer.init();
  DebuggerServer.registerAllActors();

  gClient = new DebuggerClient(DebuggerServer.connectPipe());
  gClient.connect()
    .then(() => gClient.listTabs())
    .then(response => {
      const options = {
        prefix: "helloActor",
        constructor: "HelloActor",
        type: { tab: true }
      };

      const registry = ActorRegistryFront(gClient, response);
      registry.registerActor(actorURL, options).then(actorFront => {
        gClient.listTabs().then(res => {
          const tab = res.tabs[res.selected];
          ok(!!tab.helloActor, "Hello actor must exist");

          // Make sure actor's state is maintained across listTabs requests.
          checkActorState(tab.helloActor, cleanupActor.bind(this, actorFront));
        });
      });
    });
}

function cleanupActor(actorFront) {
  // Clean up
  actorFront.unregister().then(() => {
    gClient.close().then(() => {
      DebuggerServer.destroy();
      gClient = null;
      finish();
    });
  });
}

function getCount(actor, callback) {
  return gClient.request({
    to: actor,
    type: "count"
  }, callback);
}

var checkActorState = async function(helloActor, callback) {
  let response = await getCount(helloActor);
  ok(!response.error, "No error");
  is(response.count, 1, "The counter must be valid");

  response = await getCount(helloActor);
  ok(!response.error, "No error");
  is(response.count, 2, "The counter must be valid");

  const {tabs, selected} = await gClient.listTabs();
  const tab = tabs[selected];
  is(tab.helloActor, helloActor, "Hello actor must be valid");

  response = await getCount(helloActor);
  ok(!response.error, "No error");
  is(response.count, 3, "The counter must be valid");

  callback();
};
