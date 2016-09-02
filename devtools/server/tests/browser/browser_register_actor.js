var gClient;

function test() {
  waitForExplicitFinish();
  var {ActorRegistryFront} = require("devtools/shared/fronts/actor-registry");
  var actorURL = "chrome://mochitests/content/chrome/devtools/server/tests/mochitest/hello-actor.js";

  if (!DebuggerServer.initialized) {
    DebuggerServer.init();
    DebuggerServer.addBrowserActors();
  }

  gClient = new DebuggerClient(DebuggerServer.connectPipe());
  gClient.connect()
    .then(() => gClient.listTabs())
    .then(aResponse => {

      var options = {
        prefix: "helloActor",
        constructor: "HelloActor",
        type: { tab: true }
      };

      var registry = ActorRegistryFront(gClient, aResponse);
      registry.registerActor(actorURL, options).then(actorFront => {
        gClient.listTabs(response => {
          var tab = response.tabs[response.selected];
          ok(!!tab.helloActor, "Hello actor must exist");

          // Make sure actor's state is maintained across listTabs requests.
          checkActorState(tab.helloActor, () => {

            // Clean up
            actorFront.unregister().then(() => {
              gClient.close().then(() => {
                DebuggerServer.destroy();
                gClient = null;
                finish();
              });
            });
          });
        });
      });
    });
}

function checkActorState(helloActor, callback) {
  getCount(helloActor, response => {
    ok(!response.error, "No error");
    is(response.count, 1, "The counter must be valid");

    getCount(helloActor, response => {
      ok(!response.error, "No error");
      is(response.count, 2, "The counter must be valid");

      gClient.listTabs(response => {
        var tab = response.tabs[response.selected];
        is(tab.helloActor, helloActor, "Hello actor must be valid");

        getCount(helloActor, response => {
          ok(!response.error, "No error");
          is(response.count, 3, "The counter must be valid");

          callback();
        });
      });
    });
  });
}

function getCount(actor, callback) {
  gClient.request({
    to: actor,
    type: "count"
  }, callback);
}
