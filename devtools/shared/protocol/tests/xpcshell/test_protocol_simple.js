/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Test simple requests using the protocol helpers.
 */

var protocol = require("devtools/shared/protocol");
var { Arg, Option, RetVal } = protocol;
var EventEmitter = require("devtools/shared/event-emitter");

function simpleHello() {
  return {
    from: "root",
    applicationType: "xpcshell-tests",
    traits: [],
  };
}

const rootSpec = protocol.generateActorSpec({
  typeName: "root",

  events: {
    oneway: { a: Arg(0) },
    falsyOptions: {
      zero: Option(0),
      farce: Option(0),
    },
  },

  methods: {
    simpleReturn: {
      response: { value: RetVal() },
    },
    promiseReturn: {
      response: { value: RetVal("number") },
    },
    simpleArgs: {
      request: {
        firstArg: Arg(0),
        secondArg: Arg(1),
      },
      response: RetVal(),
    },
    optionArgs: {
      request: {
        option1: Option(0),
        option2: Option(0),
      },
      response: RetVal(),
    },
    optionalArgs: {
      request: {
        a: Arg(0),
        b: Arg(1, "nullable:number"),
      },
      response: {
        value: RetVal("number"),
      },
    },
    arrayArgs: {
      request: {
        a: Arg(0, "array:number"),
      },
      response: {
        arrayReturn: RetVal("array:number"),
      },
    },
    nestedArrayArgs: {
      request: { a: Arg(0, "array:array:number") },
      response: { value: RetVal("array:array:number") },
    },
    renamedEcho: {
      request: {
        type: "echo",
        a: Arg(0),
      },
      response: {
        value: RetVal("string"),
      },
    },
    testOneWay: {
      request: { a: Arg(0) },
      oneway: true,
    },
    emitFalsyOptions: {
      oneway: true,
    },
  },
});

var RootActor = protocol.ActorClassWithSpec(rootSpec, {
  initialize(conn) {
    protocol.Actor.prototype.initialize.call(this, conn);
    // Root actor owns itself.
    this.manage(this);
    this.actorID = "root";
  },

  sayHello: simpleHello,

  simpleReturn() {
    return 1;
  },

  promiseReturn() {
    return Promise.resolve(1);
  },

  simpleArgs(a, b) {
    return { firstResponse: a + 1, secondResponse: b + 1 };
  },

  optionArgs(options) {
    return { option1: options.option1, option2: options.option2 };
  },

  optionalArgs(a, b = 200) {
    return b;
  },

  arrayArgs(a) {
    return a;
  },

  nestedArrayArgs(a) {
    return a;
  },

  /**
   * Test that the 'type' part of the request packet works
   * correctly when the type isn't the same as the method name
   */
  renamedEcho(a) {
    if (this.conn.currentPacket.type != "echo") {
      return "goodbye";
    }
    return a;
  },

  testOneWay(a) {
    // Emit to show that we got this message, because there won't be a response.
    EventEmitter.emit(this, "oneway", a);
  },

  emitFalsyOptions() {
    EventEmitter.emit(this, "falsyOptions", { zero: 0, farce: false });
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
  DevToolsServer.createRootActor = conn => {
    return RootActor(conn);
  };
  DevToolsServer.init();

  Assert.throws(() => {
    const badActor = protocol.ActorClassWithSpec({}, {});
    void badActor;
  }, /Actor specification must have a typeName member/);

  protocol.types.getType("array:array:array:number");
  protocol.types.getType("array:array:array:number");

  Assert.throws(
    () => protocol.types.getType("unknown"),
    /Unknown type:/,
    "Should throw for unknown type"
  );
  Assert.throws(
    () => protocol.types.getType("array:unknown"),
    /Unknown type:/,
    "Should throw for unknown type"
  );
  Assert.throws(
    () => protocol.types.getType("unknown:number"),
    /Unknown collection type:/,
    "Should throw for unknown collection type"
  );
  const trace = connectPipeTracing();
  const client = new DevToolsClient(trace);

  const [applicationType] = await client.connect();
  trace.expectReceive({
    from: "<actorid>",
    applicationType: "xpcshell-tests",
    traits: [],
  });
  Assert.equal(applicationType, "xpcshell-tests");

  const rootFront = client.mainRoot;

  let ret = await rootFront.simpleReturn();
  trace.expectSend({ type: "simpleReturn", to: "<actorid>" });
  trace.expectReceive({ value: 1, from: "<actorid>" });
  Assert.equal(ret, 1);

  ret = await rootFront.promiseReturn();
  trace.expectSend({ type: "promiseReturn", to: "<actorid>" });
  trace.expectReceive({ value: 1, from: "<actorid>" });
  Assert.equal(ret, 1);

  Assert.throws(
    () => rootFront.simpleArgs(5),
    /undefined passed where a value is required/,
    "Should throw if simpleArgs is missing an argument."
  );

  ret = await rootFront.simpleArgs(5, 10);
  trace.expectSend({
    type: "simpleArgs",
    firstArg: 5,
    secondArg: 10,
    to: "<actorid>",
  });
  trace.expectReceive({
    firstResponse: 6,
    secondResponse: 11,
    from: "<actorid>",
  });
  Assert.equal(ret.firstResponse, 6);
  Assert.equal(ret.secondResponse, 11);

  ret = await rootFront.optionArgs({
    option1: 5,
    option2: 10,
  });
  trace.expectSend({
    type: "optionArgs",
    option1: 5,
    option2: 10,
    to: "<actorid>",
  });
  trace.expectReceive({ option1: 5, option2: 10, from: "<actorid>" });
  Assert.equal(ret.option1, 5);
  Assert.equal(ret.option2, 10);

  ret = await rootFront.optionArgs({});
  trace.expectSend({ type: "optionArgs", to: "<actorid>" });
  trace.expectReceive({ from: "<actorid>" });
  Assert.ok(typeof ret.option1 === "undefined");
  Assert.ok(typeof ret.option2 === "undefined");

  // Explicitly call an optional argument...
  ret = await rootFront.optionalArgs(5, 10);
  trace.expectSend({
    type: "optionalArgs",
    a: 5,
    b: 10,
    to: "<actorid>",
  });
  trace.expectReceive({ value: 10, from: "<actorid>" });
  Assert.equal(ret, 10);

  // Now don't pass the optional argument, expect the default.
  ret = await rootFront.optionalArgs(5);
  trace.expectSend({ type: "optionalArgs", a: 5, to: "<actorid>" });
  trace.expectReceive({ value: 200, from: "<actorid>" });
  Assert.equal(ret, 200);

  ret = await rootFront.arrayArgs([0, 1, 2, 3, 4, 5]);
  trace.expectSend({
    type: "arrayArgs",
    a: [0, 1, 2, 3, 4, 5],
    to: "<actorid>",
  });
  trace.expectReceive({
    arrayReturn: [0, 1, 2, 3, 4, 5],
    from: "<actorid>",
  });
  Assert.equal(ret[0], 0);
  Assert.equal(ret[5], 5);

  ret = await rootFront.arrayArgs([[5]]);
  trace.expectSend({ type: "arrayArgs", a: [[5]], to: "<actorid>" });
  trace.expectReceive({ arrayReturn: [[5]], from: "<actorid>" });
  Assert.equal(ret[0][0], 5);

  const str = await rootFront.renamedEcho("hello");
  trace.expectSend({ type: "echo", a: "hello", to: "<actorid>" });
  trace.expectReceive({ value: "hello", from: "<actorid>" });
  Assert.equal(str, "hello");

  const onOneWay = rootFront.once("oneway");
  Assert.ok(typeof rootFront.testOneWay("hello") === "undefined");
  const response = await onOneWay;
  trace.expectSend({ type: "testOneWay", a: "hello", to: "<actorid>" });
  trace.expectReceive({
    type: "oneway",
    a: "hello",
    from: "<actorid>",
  });
  Assert.equal(response, "hello");

  const onFalsyOptions = rootFront.once("falsyOptions");
  rootFront.emitFalsyOptions();
  const res = await onFalsyOptions;
  trace.expectSend({ type: "emitFalsyOptions", to: "<actorid>" });
  trace.expectReceive({
    type: "falsyOptions",
    farce: false,
    zero: 0,
    from: "<actorid>",
  });

  Assert.ok(res.zero === 0);
  Assert.ok(res.farce === false);

  await client.close();
});
