/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Check extension-added global actor API.
 */

const CHROME_URL = "chrome://mochitests/content/browser/devtools/client/debugger/test/mochitest/"
const ACTORS_URL = CHROME_URL + "testactors.js";

function test() {
  let gClient;

  if (!DebuggerServer.initialized) {
    DebuggerServer.init();
    DebuggerServer.addBrowserActors();
  }

  DebuggerServer.addActors(ACTORS_URL);

  let transport = DebuggerServer.connectPipe();
  gClient = new DebuggerClient(transport);
  gClient.connect((aType, aTraits) => {
    is(aType, "browser",
      "Root actor should identify itself as a browser.");

    gClient.listTabs(aResponse => {
      let globalActor = aResponse.testGlobalActor1;
      ok(globalActor, "Found the test tab actor.")
      ok(globalActor.includes("test_one"),
        "testGlobalActor1's actorPrefix should be used.");

      gClient.request({ to: globalActor, type: "ping" }, aResponse => {
        is(aResponse.pong, "pong", "Actor should respond to requests.");

        // Send another ping to see if the same actor is used.
        gClient.request({ to: globalActor, type: "ping" }, aResponse => {
          is(aResponse.pong, "pong", "Actor should respond to requests.");

          // Make sure that lazily-created actors are created only once.
          let conn = transport._serverConnection;

          // First we look for the pool of global actors.
          let extraPools = conn._extraPools;
          let globalPool;

          let actorPrefix = conn._prefix + "test_one";
          let count = 0;
          for (let pool of extraPools) {
            count += Object.keys(pool._actors).filter(e => {
              return e.startsWith(actorPrefix);
            }).length;
          }
          is(count, 2,
            "Only two actor exists in all pools. One tab actor and one global.");

          gClient.close(finish);
        });
      });
    });
  });
}
