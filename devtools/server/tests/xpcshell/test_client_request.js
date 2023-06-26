/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test the DevToolsClient.request API.

var gClient, gActorId;

const { Actor } = require("resource://devtools/shared/protocol/Actor.js");

class TestActor extends Actor {
  constructor(conn) {
    super(conn, { typeName: "test", methods: [] });

    this.requestTypes = {
      hello: this.hello,
      error: this.error,
    };
  }

  hello() {
    return { hello: "world" };
  }

  error() {
    return { error: "code", message: "human message" };
  }
}

function run_test() {
  ActorRegistry.addGlobalActor(
    {
      constructorName: "TestActor",
      constructorFun: TestActor,
    },
    "test"
  );

  DevToolsServer.init();
  DevToolsServer.registerAllActors();

  add_test(init);
  add_test(test_client_request_promise);
  add_test(test_client_request_promise_error);
  add_test(test_client_request_event_emitter);
  add_test(test_close_client_while_sending_requests);
  add_test(test_client_request_after_close);
  run_next_test();
}

function init() {
  gClient = new DevToolsClient(DevToolsServer.connectPipe());
  gClient
    .connect()
    .then(() => gClient.mainRoot.rootForm)
    .then(response => {
      gActorId = response.test;
      run_next_test();
    });
}

function checkStack(expectedName) {
  let stack = Components.stack;
  while (stack) {
    info(stack.name);
    if (stack.name == expectedName) {
      // Reached back to outer function before request
      ok(true, "Complete stack");
      return;
    }
    stack = stack.asyncCaller || stack.caller;
  }
  ok(false, "Incomplete stack");
}

function test_client_request_promise() {
  // Test that DevToolsClient.request returns a promise that resolves on response
  const request = gClient.request({
    to: gActorId,
    type: "hello",
  });

  request.then(response => {
    Assert.equal(response.from, gActorId);
    Assert.equal(response.hello, "world");
    checkStack("test_client_request_promise/<");
    run_next_test();
  });
}

function test_client_request_promise_error() {
  // Test that DevToolsClient.request returns a promise that reject when server
  // returns an explicit error message
  const request = gClient.request({
    to: gActorId,
    type: "error",
  });

  request.then(
    () => {
      do_throw("Promise shouldn't be resolved on error");
    },
    response => {
      Assert.equal(response.from, gActorId);
      Assert.equal(response.error, "code");
      Assert.equal(response.message, "human message");
      checkStack("test_client_request_promise_error/<");
      run_next_test();
    }
  );
}

function test_client_request_event_emitter() {
  // Test that DevToolsClient.request returns also an EventEmitter object
  const request = gClient.request({
    to: gActorId,
    type: "hello",
  });
  request.on("json-reply", reply => {
    Assert.equal(reply.from, gActorId);
    Assert.equal(reply.hello, "world");
    checkStack("test_client_request_event_emitter");
    run_next_test();
  });
}

function test_close_client_while_sending_requests() {
  // First send a first request that will be "active"
  // while the connection is closed.
  // i.e. will be sent but no response received yet.
  const activeRequest = gClient.request({
    to: gActorId,
    type: "hello",
  });

  // Pile up a second one that will be "pending".
  // i.e. won't event be sent.
  const pendingRequest = gClient.request({
    to: gActorId,
    type: "hello",
  });

  const expectReply = new Promise(resolve => {
    gClient.expectReply("root", function (response) {
      Assert.equal(response.error, "connectionClosed");
      Assert.equal(
        response.message,
        "server side packet can't be received as the connection just closed."
      );
      resolve();
    });
  });

  gClient.close().then(() => {
    activeRequest
      .then(
        () => {
          ok(
            false,
            "First request unexpectedly succeed while closing the connection"
          );
        },
        response => {
          Assert.equal(response.error, "connectionClosed");
          Assert.equal(
            response.message,
            "'hello' active request packet to '" +
              gActorId +
              "' can't be sent as the connection just closed."
          );
        }
      )
      .then(() => pendingRequest)
      .then(
        () => {
          ok(
            false,
            "Second request unexpectedly succeed while closing the connection"
          );
        },
        response => {
          Assert.equal(response.error, "connectionClosed");
          Assert.equal(
            response.message,
            "'hello' pending request packet to '" +
              gActorId +
              "' can't be sent as the connection just closed."
          );
        }
      )
      .then(() => expectReply)
      .then(run_next_test);
  });
}

function test_client_request_after_close() {
  // Test that DevToolsClient.request fails after we called client.close()
  // (with promise API)
  const request = gClient.request({
    to: gActorId,
    type: "hello",
  });

  request.then(
    response => {
      ok(false, "Request succeed even after client.close");
    },
    response => {
      ok(true, "Request failed after client.close");
      Assert.equal(response.error, "connectionClosed");
      ok(
        response.message.match(
          /'hello' request packet to '.*' can't be sent as the connection is closed./
        )
      );
      run_next_test();
    }
  );
}
