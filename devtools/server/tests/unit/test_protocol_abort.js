/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Outstanding requests should be rejected when the connection aborts
 * unexpectedly.
 */

var protocol = require("devtools/shared/protocol");
var {Arg, Option, RetVal} = protocol;
var events = require("sdk/event/core");

function simpleHello() {
  return {
    from: "root",
    applicationType: "xpcshell-tests",
    traits: [],
  };
}

const rootSpec = protocol.generateActorSpec({
  typeName: "root",

  methods: {
    simpleReturn: {
      response: { value: RetVal() }
    }
  }
});

var RootActor = protocol.ActorClassWithSpec(rootSpec, {
  typeName: "root",
  initialize: function (conn) {
    protocol.Actor.prototype.initialize.call(this, conn);
    // Root actor owns itself.
    this.manage(this);
    this.actorID = "root";
    this.sequence = 0;
  },

  sayHello: simpleHello,

  simpleReturn: function () {
    return this.sequence++;
  }
});

var RootFront = protocol.FrontClassWithSpec(rootSpec, {
  initialize: function (client) {
    this.actorID = "root";
    protocol.Front.prototype.initialize.call(this, client);
    // Root owns itself.
    this.manage(this);
  }
});

function run_test() {
  DebuggerServer.createRootActor = RootActor;
  DebuggerServer.init();

  let trace = connectPipeTracing();
  let client = new DebuggerClient(trace);
  let rootClient;

  client.connect().then(([applicationType, traits]) => {
    rootClient = RootFront(client);

    rootClient.simpleReturn().then(() => {
      ok(false, "Connection was aborted, request shouldn't resolve");
      do_test_finished();
    }, e => {
      let error = e.toString();
      ok(true, "Connection was aborted, request rejected correctly");
      ok(error.includes("Request stack:"), "Error includes request stack");
      ok(error.includes("test_protocol_abort.js"), "Stack includes this test");
      do_test_finished();
    });

    trace.close();
  });

  do_test_pending();
}
