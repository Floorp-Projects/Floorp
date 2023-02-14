/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Client request stacks should span the entire process from before making the
 * request to handling the reply from the server.  The server frames are not
 * included, nor can they be in most cases, since the server can be a remote
 * device.
 */

var protocol = require("resource://devtools/shared/protocol.js");
var { RetVal } = protocol;

const rootSpec = protocol.generateActorSpec({
  typeName: "root",

  methods: {
    simpleReturn: {
      response: { value: RetVal() },
    },
  },
});

class RootActor extends protocol.Actor {
  constructor(conn) {
    super(conn, rootSpec);

    // Root actor owns itself.
    this.manage(this);
    this.actorID = "root";
    this.sequence = 0;
  }

  sayHello() {
    return {
      from: "root",
      applicationType: "xpcshell-tests",
      traits: [],
    };
  }

  simpleReturn() {
    return this.sequence++;
  }
}

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
  DevToolsServer.createRootActor = conn => new RootActor(conn);
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
