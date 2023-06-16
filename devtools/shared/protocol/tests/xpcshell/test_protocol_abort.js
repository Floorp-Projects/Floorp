/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Outstanding requests should be rejected when the connection aborts
 * unexpectedly.
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

add_task(async function () {
  DevToolsServer.createRootActor = conn => new RootActor(conn);
  DevToolsServer.init();

  const trace = connectPipeTracing();
  const client = new DevToolsClient(trace);
  await client.connect();

  const rootFront = client.mainRoot;

  const onSimpleReturn = rootFront.simpleReturn();
  trace.close();

  try {
    await onSimpleReturn;
    ok(false, "Connection was aborted, request shouldn't resolve");
  } catch (e) {
    const error = e.toString();
    ok(true, "Connection was aborted, request rejected correctly");
    ok(error.includes("Request stack:"), "Error includes request stack");
    ok(error.includes("test_protocol_abort.js"), "Stack includes this test");
  }
});
