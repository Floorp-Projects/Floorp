/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Test that clients can catch errors in actors.
 */

const ACTORS_URL =
  "chrome://mochitests/content/browser/devtools/server/tests/browser/error-actor.js";

add_task(async function test_old_actor() {
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
});

const TEST_ERRORS_ACTOR_URL =
  "chrome://mochitests/content/browser/devtools/server/tests/browser/test-errors-actor.js";
add_task(async function test_protocoljs_actor() {
  DevToolsServer.init();
  DevToolsServer.registerAllActors();

  info("Register the new TestErrorsActor");
  require(TEST_ERRORS_ACTOR_URL);
  ActorRegistry.registerModule(TEST_ERRORS_ACTOR_URL, {
    prefix: "testErrors",
    constructor: "TestErrorsActor",
    type: { global: true },
  });

  info("Create a DevTools client/server pair");
  const transport = DevToolsServer.connectPipe();
  const gClient = new DevToolsClient(transport);
  await gClient.connect();

  info("Retrieve a TestErrorsFront instance");
  const testErrorsFront = await gClient.mainRoot.getFront("testErrors");
  ok(testErrorsFront, "has a TestErrorsFront instance");

  await Assert.rejects(testErrorsFront.throwsComponentsException(), e => {
    return (
      e.message ===
      "NS_ERROR_NOT_IMPLEMENTED from: server0.conn1.testErrorsActor8 " +
        "(chrome://mochitests/content/browser/devtools/server/tests/browser/test-errors-actor.js:41:0)"
    );
  });
  await Assert.rejects(testErrorsFront.throwsException(), e => {
    return (
      e.message ===
      'Protocol error (TypeError): can\'t access property "b",' +
        " this.a is undefined from: server0.conn1.testErrorsActor8 " +
        "(chrome://mochitests/content/browser/devtools/server/tests/browser/test-errors-actor.js:46:5)"
    );
  });
  await Assert.rejects(testErrorsFront.throwsJSError(), e => {
    return (
      e.message ===
      "Protocol error (Error): JSError from: server0.conn1.testErrorsActor8 " +
        "(chrome://mochitests/content/browser/devtools/server/tests/browser/test-errors-actor.js:51:11)"
    );
  });

  await gClient.close();
});
