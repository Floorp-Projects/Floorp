/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Make sure we get replies in the same order that we sent their
 * requests even when earlier requests take several event ticks to
 * complete.
 */

const { waitForTick } = require("devtools/shared/DevToolsUtils");
const protocol = require("devtools/shared/protocol");
const { Arg, RetVal } = protocol;

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
    promiseReturn: {
      request: { toWait: Arg(0, "number") },
      response: { value: RetVal("number") },
    },
    simpleThrow: {
      response: { value: RetVal("number") },
    },
    promiseThrow: {
      request: { toWait: Arg(0, "number") },
      response: { value: RetVal("number") },
    },
  },
});

const RootActor = protocol.ActorClassWithSpec(rootSpec, {
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

  // Guarantee that this resolves after simpleReturn returns.
  async promiseReturn(toWait) {
    const sequence = this.sequence++;

    // Wait until the number of requests specified by toWait have
    // happened, to test queuing.
    while (this.sequence - sequence < toWait) {
      await waitForTick();
    }

    return sequence;
  },

  simpleThrow() {
    throw new Error(this.sequence++);
  },

  // Guarantee that this resolves after simpleReturn returns.
  promiseThrow(toWait) {
    return this.promiseReturn(toWait).then(Promise.reject);
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

add_task(async function() {
  DevToolsServer.createRootActor = RootActor;
  DevToolsServer.init();

  const trace = connectPipeTracing();
  const client = new DevToolsClient(trace);
  await client.connect();

  const rootFront = client.mainRoot;

  const calls = [];
  let sequence = 0;

  // Execute a call that won't finish processing until 2
  // more calls have happened
  calls.push(
    rootFront.promiseReturn(2).then(ret => {
      // Check right return order
      Assert.equal(sequence, 0);
      // Check request handling order
      Assert.equal(ret, sequence++);
    })
  );

  // Put a few requests into the backlog

  calls.push(
    rootFront.simpleReturn().then(ret => {
      // Check right return order
      Assert.equal(sequence, 1);
      // Check request handling order
      Assert.equal(ret, sequence++);
    })
  );

  calls.push(
    rootFront.simpleReturn().then(ret => {
      // Check right return order
      Assert.equal(sequence, 2);
      // Check request handling order
      Assert.equal(ret, sequence++);
    })
  );

  calls.push(
    rootFront.simpleThrow().then(
      () => {
        Assert.ok(false, "simpleThrow shouldn't succeed!");
      },
      error => {
        // Check right return order
        Assert.equal(sequence++, 3);
      }
    )
  );

  calls.push(
    rootFront.promiseThrow(2).then(
      () => {
        Assert.ok(false, "promiseThrow shouldn't succeed!");
      },
      error => {
        // Check right return order
        Assert.equal(sequence++, 4);
        Assert.ok(true, "simple throw should throw");
      }
    )
  );

  calls.push(
    rootFront.simpleReturn().then(ret => {
      // Check right return order
      Assert.equal(sequence, 5);
      // Check request handling order
      Assert.equal(ret, sequence++);
    })
  );

  // Break up the backlog with a long request that waits
  // for another simpleReturn before completing
  calls.push(
    rootFront.promiseReturn(1).then(ret => {
      // Check right return order
      Assert.equal(sequence, 6);
      // Check request handling order
      Assert.equal(ret, sequence++);
    })
  );

  calls.push(
    rootFront.simpleReturn().then(ret => {
      // Check right return order
      Assert.equal(sequence, 7);
      // Check request handling order
      Assert.equal(ret, sequence++);
    })
  );

  await Promise.all(calls);
  await client.close();
});
