/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Test the DebuggerClient.request API.

var gClient, gActorId;

function TestActor(conn) {
  this.conn = conn;
}
TestActor.prototype = {
  actorPrefix: "test",

  hello: function () {
    return {hello: "world"};
  },

  error: function () {
    return {error: "code", message: "human message"};
  }
};
TestActor.prototype.requestTypes = {
  "hello": TestActor.prototype.hello,
  "error": TestActor.prototype.error
};

function run_test()
{
  DebuggerServer.addGlobalActor(TestActor);

  DebuggerServer.init();
  DebuggerServer.addBrowserActors();

  add_test(init);
  add_test(test_client_request_callback);
  add_test(test_client_request_promise);
  add_test(test_client_request_promise_error);
  add_test(test_client_request_event_emitter);
  add_test(test_close_client_while_sending_requests);
  add_test(test_client_request_after_close);
  add_test(test_client_request_after_close_callback);
  run_next_test();
}

function init()
{
  gClient = new DebuggerClient(DebuggerServer.connectPipe());
  gClient.connect()
    .then(() => gClient.listTabs())
    .then(aResponse => {
      gActorId = aResponse.test;
      run_next_test();
    });
}

function checkStack(expectedName) {
  if (!Services.prefs.getBoolPref("javascript.options.asyncstack")) {
    do_print("Async stacks are disabled.");
    return;
  }

  let stack = Components.stack;
  while (stack) {
    do_print(stack.name);
    if (stack.name == expectedName) {
      // Reached back to outer function before request
      ok(true, "Complete stack");
      return;
    }
    stack = stack.asyncCaller || stack.caller;
  }
  ok(false, "Incomplete stack");
}

function test_client_request_callback()
{
  // Test that DebuggerClient.request accepts a `onResponse` callback as 2nd argument
  gClient.request({
    to: gActorId,
    type: "hello"
  }, response => {
    do_check_eq(response.from, gActorId);
    do_check_eq(response.hello, "world");
    checkStack("test_client_request_callback");
    run_next_test();
  });
}

function test_client_request_promise()
{
  // Test that DebuggerClient.request returns a promise that resolves on response
  let request = gClient.request({
    to: gActorId,
    type: "hello"
  });

  request.then(response => {
    do_check_eq(response.from, gActorId);
    do_check_eq(response.hello, "world");
    checkStack("test_client_request_promise");
    run_next_test();
  });
}

function test_client_request_promise_error()
{
  // Test that DebuggerClient.request returns a promise that reject when server
  // returns an explicit error message
  let request = gClient.request({
    to: gActorId,
    type: "error"
  });

  request.then(() => {
    do_throw("Promise shouldn't be resolved on error");
  }, response => {
    do_check_eq(response.from, gActorId);
    do_check_eq(response.error, "code");
    do_check_eq(response.message, "human message");
    checkStack("test_client_request_promise_error");
    run_next_test();
  });
}

function test_client_request_event_emitter()
{
  // Test that DebuggerClient.request returns also an EventEmitter object
  let request = gClient.request({
    to: gActorId,
    type: "hello"
  });
  request.on("json-reply", reply => {
    do_check_eq(reply.from, gActorId);
    do_check_eq(reply.hello, "world");
    checkStack("test_client_request_event_emitter");
    run_next_test();
  });
}

function test_close_client_while_sending_requests() {
  // First send a first request that will be "active"
  // while the connection is closed.
  // i.e. will be sent but no response received yet.
  let activeRequest = gClient.request({
    to: gActorId,
    type: "hello"
  });

  // Pile up a second one that will be "pending".
  // i.e. won't event be sent.
  let pendingRequest = gClient.request({
    to: gActorId,
    type: "hello"
  });

  let expectReply = promise.defer();
  gClient.expectReply("root", function (response) {
    do_check_eq(response.error, "connectionClosed");
    do_check_eq(response.message, "server side packet can't be received as the connection just closed.");
    expectReply.resolve();
  });

  gClient.close().then(() => {
    activeRequest.then(() => {
      ok(false, "First request unexpectedly succeed while closing the connection");
    }, response => {
      do_check_eq(response.error, "connectionClosed");
      do_check_eq(response.message, "'hello' active request packet to '" + gActorId + "' can't be sent as the connection just closed.");
    })
    .then(() => pendingRequest)
    .then(() => {
      ok(false, "Second request unexpectedly succeed while closing the connection");
    }, response => {
      do_check_eq(response.error, "connectionClosed");
      do_check_eq(response.message, "'hello' pending request packet to '" + gActorId + "' can't be sent as the connection just closed.");
    })
    .then(() => expectReply.promise)
    .then(run_next_test);
  });
}

function test_client_request_after_close()
{
  // Test that DebuggerClient.request fails after we called client.close()
  // (with promise API)
  let request = gClient.request({
    to: gActorId,
    type: "hello"
  });

  request.then(response => {
    ok(false, "Request succeed even after client.close");
  }, response => {
    ok(true, "Request failed after client.close");
    do_check_eq(response.error, "connectionClosed");
    ok(response.message.match(/'hello' request packet to '.*' can't be sent as the connection is closed./));
    run_next_test();
  });
}

function test_client_request_after_close_callback()
{
  // Test that DebuggerClient.request fails after we called client.close()
  // (with callback API)
  let request = gClient.request({
    to: gActorId,
    type: "hello"
  }, response => {
    ok(true, "Request failed after client.close");
    do_check_eq(response.error, "connectionClosed");
    ok(response.message.match(/'hello' request packet to '.*' can't be sent as the connection is closed./));
    run_next_test();
  });
}
