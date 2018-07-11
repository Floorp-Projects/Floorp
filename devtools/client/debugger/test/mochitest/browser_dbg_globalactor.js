/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Check extension-added global actor API.
 */

const ACTORS_URL = CHROME_URL + "testactors.js";

function test() {
  let gClient;

  DebuggerServer.init();
  DebuggerServer.registerAllActors();

  DebuggerServer.registerModule(ACTORS_URL, {
    prefix: "testOne",
    constructor: "TestActor1",
    type: { global: true },
  });

  let transport = DebuggerServer.connectPipe();
  gClient = new DebuggerClient(transport);
  gClient.connect().then(([aType, aTraits]) => {
    is(aType, "browser",
      "Root actor should identify itself as a browser.");

    gClient.listTabs().then(aResponse => {
      let globalActor = aResponse.testOneActor;
      ok(globalActor, "Found the test global actor.");
      ok(globalActor.includes("testOne"),
        "testGlobalActor1's actorPrefix should be used.");

      gClient.request({ to: globalActor, type: "ping" }, aResponse => {
        is(aResponse.pong, "pong", "Actor should respond to requests.");

        // Send another ping to see if the same actor is used.
        gClient.request({ to: globalActor, type: "ping" }, aResponse => {
          is(aResponse.pong, "pong", "Actor should respond to requests.");

          // Make sure that lazily-created actors are created only once.
          let count = 0;
          for (let connID of Object.getOwnPropertyNames(DebuggerServer._connections)) {
            let conn = DebuggerServer._connections[connID];
            let actorPrefix = conn._prefix + "testOne";
            for (let pool of conn._extraPools) {
              count += Object.keys(pool._actors).filter(e => {
                return e.startsWith(actorPrefix);
              }).length;
            }
          }

          is(count, 1,
            "Only one actor exists in all pools. One global actor.");

          gClient.close().then(finish);
        });
      });
    });
  });
}
