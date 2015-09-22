/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Test the DebuggerClient.registerClient API

var EventEmitter = require("devtools/shared/event-emitter");

var gClient;
var gActors;
var gTestClient;

function TestActor(conn) {
  this.conn = conn;
}
TestActor.prototype = {
  actorPrefix: "test",

  start: function () {
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
  start: function () {
    this.client.request({
      to: this.actor,
      type: "start"
    });
  },

  detach: function (onDone) {
    this.detached = true;
    onDone();
  }
};

function run_test()
{
  DebuggerServer.addGlobalActor(TestActor);

  DebuggerServer.init();
  DebuggerServer.addBrowserActors();

  add_test(init);
  add_test(test_client_events);
  add_test(close_client);
  run_next_test();
}

function init()
{
  gClient = new DebuggerClient(DebuggerServer.connectPipe());
  gClient.connect(function onConnect() {
    gClient.listTabs(function onListTabs(aResponse) {
      gActors = aResponse;
      gTestClient = new TestClient(gClient, aResponse);
      run_next_test();
    });
  });
}

function test_client_events()
{
  // Test DebuggerClient.registerClient and DebuggerServerConnection.sendActorEvent
  gTestClient.on("foo", function (type, data) {
    do_check_eq(type, "foo");
    do_check_eq(data.hello, "world");
    run_next_test();
  });
  gTestClient.start();
}

function close_client() {
  gClient.close(() => {
    // Check that client.detach method is call on client destruction
    do_check_true(gTestClient.detached);
    run_next_test()
  });
}

