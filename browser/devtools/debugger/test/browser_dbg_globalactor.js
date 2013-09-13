/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Check extension-added global actor API.
 */

const CHROME_URL = "chrome://mochitests/content/browser/browser/devtools/debugger/test/"
const ACTORS_URL = CHROME_URL + "testactors.js";

function test() {
  let gClient;

  if (!DebuggerServer.initialized) {
    DebuggerServer.init(() => true);
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
      ok(globalActor.contains("test_one"),
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

          for (let pool of extraPools) {
            if (Object.keys(pool._actors).some(e => {
              // Tab actors are in the global pool.
              let re = new RegExp(conn._prefix + "tab", "g");
              return e.match(re) !== null;
            })) {
              globalPool = pool;
              break;
            }
          }

          // Then we look if the global pool contains only one test actor.
          let actorPrefix = conn._prefix + "test_one";
          let actors = Object.keys(globalPool._actors).join();
          info("Global actors: " + actors);

          isnot(actors.indexOf(actorPrefix), -1,
            "The test actor exists in the pool.");
          is(actors.indexOf(actorPrefix), actors.lastIndexOf(actorPrefix),
            "Only one actor exists in the pool.");

          gClient.close(finish);
        });
      });
    });
  });
}
