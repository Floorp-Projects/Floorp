/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Test Front.watchFronts method.
 */

const protocol = require("devtools/shared/protocol");
const { RetVal } = protocol;

const childSpec = protocol.generateActorSpec({
  typeName: "childActor",

  methods: {
    release: {
      release: true,
    },
  },
});

const ChildActor = protocol.ActorClassWithSpec(childSpec, {
  initialize(conn, id) {
    protocol.Actor.prototype.initialize.call(this, conn);
    this.childID = id;
  },

  release() {},

  form() {
    return {
      actor: this.actorID,
      childID: this.childID,
      foo: "bar",
    };
  },
});

const rootSpec = protocol.generateActorSpec({
  typeName: "root",

  methods: {
    createChild: {
      request: {},
      response: { actor: RetVal("childActor") },
    },
  },
});

const RootActor = protocol.ActorClassWithSpec(rootSpec, {
  typeName: "root",

  initialize(conn) {
    protocol.Actor.prototype.initialize.call(this, conn);

    this.actorID = "root";

    // Root actor owns itself.
    this.manage(this);

    this.sequence = 0;
  },

  sayHello() {
    return {
      from: "root",
      applicationType: "xpcshell-tests",
      traits: [],
    };
  },

  createChild() {
    return new ChildActor(this.conn, this.sequence++);
  },
});

class ChildFront extends protocol.FrontClassWithSpec(childSpec) {
  form(form) {
    this.childID = form.childID;
    this.foo = form.foo;
  }
}
protocol.registerFront(ChildFront);

class RootFront extends protocol.FrontClassWithSpec(rootSpec) {
  constructor(client) {
    super(client);
    this.actorID = "root";
    // Root owns itself.
    this.manage(this);
  }
}
protocol.registerFront(RootFront);

add_task(async function run_test() {
  DevToolsServer.createRootActor = RootActor;
  DevToolsServer.init();

  const trace = connectPipeTracing();
  const client = new DevToolsClient(trace);
  await client.connect();

  const rootFront = client.mainRoot;

  const fronts = [];
  const listener = front => {
    equal(
      front.foo,
      "bar",
      "Front's form is set before watchFronts listeners are called"
    );
    fronts.push(front);
  };
  rootFront.watchFronts("childActor", listener);

  const firstChild = await rootFront.createChild();
  ok(
    firstChild instanceof ChildFront,
    "createChild returns a ChildFront instance"
  );
  equal(firstChild.childID, 0, "First child has ID=0");

  equal(
    fronts.length,
    1,
    "watchFronts fires the callback, even if the front is created in the future"
  );
  equal(
    fronts[0],
    firstChild,
    "watchFronts fires the callback with the right front instance"
  );

  const watchFrontsAfter = await new Promise(resolve => {
    rootFront.watchFronts("childActor", resolve);
  });
  equal(
    watchFrontsAfter,
    firstChild,
    "watchFronts fires the callback, even if the front is already created, " +
      " with the same front instance"
  );

  equal(
    fronts.length,
    1,
    "There is still only one front reported from the first listener"
  );

  const secondChild = await rootFront.createChild();

  equal(
    fronts.length,
    2,
    "After a second call to createChild, two fronts are reported"
  );
  equal(fronts[1], secondChild, "And the new front is the right instance");

  // Test unregistering a front listener
  rootFront.unwatchFronts("childActor", listener);

  const thirdChild = await rootFront.createChild();
  equal(
    fronts.length,
    2,
    "After calling unwatchFronts, the listener is no longer called"
  );

  // Test front destruction
  const destroyed = [];
  rootFront.watchFronts("childActor", null, front => {
    destroyed.push(front);
  });
  await thirdChild.release();
  equal(
    destroyed.length,
    1,
    "After the destruction of the front, one destruction is reported"
  );
  equal(destroyed[0], thirdChild, "And the destroyed front is the right one");

  trace.close();
  await client.close();
});
