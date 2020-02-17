/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Test that clients can catch errors in old style actors.
 */

const ACTORS_URL =
  "chrome://mochitests/content/browser/devtools/server/tests/browser/error-actor.js";

async function test() {
  DevToolsServer.init();
  DevToolsServer.registerAllActors();

  ActorRegistry.registerModule(ACTORS_URL, {
    prefix: "error",
    constructor: "ErrorActor",
    type: { global: true },
  });

  const transport = DevToolsServer.connectPipe();
  const gClient = new DevToolsClient(transport);
  await gClient.connect();

  const { errorActor } = await gClient.mainRoot.rootForm;
  ok(errorActor, "Found the error actor.");

  await Assert.rejects(
    gClient.request({ to: errorActor, type: "error" }),
    err =>
      err.error == "unknownError" &&
      /error occurred while processing 'error/.test(err.message),
    "The request should be rejected"
  );

  await gClient.close();

  finish();
}
