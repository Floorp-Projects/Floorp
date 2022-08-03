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
var { RetVal } = protocol;

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
    },
  },
});

var RootActor = protocol.ActorClassWithSpec(rootSpec, {
  initialize(conn) {
    protocol.Actor.prototype.initialize.call(this, conn);
    // Root actor owns itself.
    this.manage(this);
    this.actorID = "root";
    this.sequence = 0;
  },

  sayHello: simpleHello,

  simpleReturn() {
    return this.sequence++;
  },
});

class RootFront extends protocol.FrontClassWithSpec(rootSpec) {
  constructor(client) {
    super(client);
    this.actorID = "root";
    // Root owns itself.
    this.manage(this);
  }
}
protocol.registerFront(RootFront);

function run_test() {
  DevToolsServer.createRootActor = RootActor;
  DevToolsServer.init();

  const trace = connectPipeTracing();
  const client = new DevToolsClient(trace);
  let rootFront;

  client.connect().then(function onConnect() {
    rootFront = client.mainRoot;

    rootFront
      .simpleReturn()
      .then(
        () => {
          let stack = Components.stack;
          while (stack) {
            info(stack.name);
            if (stack.name.includes("onConnect")) {
              // Reached back to outer function before request
              ok(true, "Complete stack");
              return;
            }
            stack = stack.asyncCaller || stack.caller;
          }
          ok(false, "Incomplete stack");
        },
        () => {
          ok(false, "Request failed unexpectedly");
        }
      )
      .then(() => {
        client.close().then(() => {
          do_test_finished();
        });
      });
  });

  do_test_pending();
}
