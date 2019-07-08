"use strict";

var gClient;

function test() {
  waitForExplicitFinish();
  const actorURL =
    "chrome://mochitests/content/chrome/devtools/server/tests/mochitest/hello-actor.js";

  DebuggerServer.init();
  DebuggerServer.registerAllActors();

  gClient = new DebuggerClient(DebuggerServer.connectPipe());
  gClient
    .connect()
    .then(() => gClient.mainRoot.listTabs())
    .then(async () => {
      const options = {
        prefix: "helloActor",
        constructor: "HelloActor",
        type: { target: true },
      };

      const registry = await gClient.mainRoot.getFront("actorRegistry");
      const actorFront = await registry.registerActor(actorURL, options);
      const tabs = await gClient.mainRoot.listTabs();
      const front = tabs.find(tab => tab.selected);
      ok(!!front.targetForm.helloActor, "Hello actor must exist");

      // Make sure actor's state is maintained across listTabs requests.
      checkActorState(
        front.targetForm.helloActor,
        cleanupActor.bind(this, actorFront)
      );
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
  return gClient.request(
    {
      to: actor,
      type: "count",
    },
    callback
  );
}

var checkActorState = async function(helloActor, callback) {
  let response = await getCount(helloActor);
  ok(!response.error, "No error");
  is(response.count, 1, "The counter must be valid");

  response = await getCount(helloActor);
  ok(!response.error, "No error");
  is(response.count, 2, "The counter must be valid");

  const tabs = await gClient.mainRoot.listTabs();
  const tabTarget = tabs.find(tab => tab.selected);
  is(tabTarget.targetForm.helloActor, helloActor, "Hello actor must be valid");

  response = await getCount(helloActor);
  ok(!response.error, "No error");
  is(response.count, 3, "The counter must be valid");

  callback();
};
