/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Client request stacks should span the entire process from before making the
 * request to handling the reply from the server.  The server frames are not
 * included, nor can they be in most cases, since the server can be a remote
 * device.
 */

var protocol = require("devtools/shared/protocol");
var {RetVal} = protocol;

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
      response: { value: RetVal() },
    }
  }
});

var RootActor = protocol.ActorClassWithSpec(rootSpec, {
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
  if (!Services.prefs.getBoolPref("javascript.options.asyncstack")) {
    info("Async stacks are disabled.");
    return;
  }

  DebuggerServer.createRootActor = RootActor;
  DebuggerServer.init();

  let trace = connectPipeTracing();
  let client = new DebuggerClient(trace);
  let rootClient;

  client.connect().then(function onConnect() {
    rootClient = RootFront(client);

    rootClient.simpleReturn().then(() => {
      let stack = Components.stack;
      while (stack) {
        info(stack.name);
        if (stack.name == "onConnect") {
          // Reached back to outer function before request
          ok(true, "Complete stack");
          return;
        }
        stack = stack.asyncCaller || stack.caller;
      }
      ok(false, "Incomplete stack");
    }, () => {
      ok(false, "Request failed unexpectedly");
    }).then(() => {
      client.close().then(() => {
        do_test_finished();
      });
    });
  });

  do_test_pending();
}
