/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test the DebuggerClient.registerClient API

var EventEmitter = require("devtools/shared/event-emitter");

var gClient;
var gTestClient;

function TestActor(conn) {
  this.conn = conn;
}
TestActor.prototype = {
  actorPrefix: "test",

  start: function() {
    this.conn.sendActorEvent(this.actorID, "foo", {
      hello: "world"
    });
    return {};
  }
};
TestActor.prototype.requestTypes = {
  "start": TestActor.prototype.start
};

function TestClient(client, form) {
  this.client = client;
  this.actor = form.test;
  this.events = ["foo"];
  EventEmitter.decorate(this);
  client.registerClient(this);

  this.detached = false;
}
TestClient.prototype = {
  start: function() {
    this.client.request({
      to: this.actor,
      type: "start"
    });
  },

  detach: function(onDone) {
    this.detached = true;
    onDone();
  }
};

function run_test() {
  DebuggerServer.addGlobalActor({
    constructorName: "TestActor",
    constructorFun: TestActor,
  }, "test");

  DebuggerServer.init();
  DebuggerServer.registerAllActors();

  add_test(init);
  add_test(test_client_events);
  add_test(close_client);
  run_next_test();
}

function init() {
  gClient = new DebuggerClient(DebuggerServer.connectPipe());
  gClient.connect()
    .then(() => gClient.listTabs())
    .then(response => {
      gTestClient = new TestClient(gClient, response);
      run_next_test();
    });
}

function test_client_events() {
  // Test DebuggerClient.registerClient and DebuggerServerConnection.sendActorEvent
  gTestClient.on("foo", function(data) {
    Assert.equal(data.hello, "world");
    run_next_test();
  });
  gTestClient.start();
}

function close_client() {
  gClient.close().then(() => {
    // Check that client.detach method is call on client destruction
    Assert.ok(gTestClient.detached);
    run_next_test();
  });
}

