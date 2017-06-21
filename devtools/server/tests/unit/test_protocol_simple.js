/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Test simple requests using the protocol helpers.
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

  events: {
    "oneway": { a: Arg(0) },
    "falsyOptions": {
      zero: Option(0),
      farce: Option(0)
    }
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
      response: RetVal()
    },
    nestedArgs: {
      request: {
        firstArg: Arg(0),
        nest: {
          secondArg: Arg(1),
          nest: {
            thirdArg: Arg(2)
          }
        }
      },
      response: RetVal()
    },
    optionArgs: {
      request: {
        option1: Option(0),
        option2: Option(0)
      },
      response: RetVal()
    },
    optionalArgs: {
      request: {
        a: Arg(0),
        b: Arg(1, "nullable:number")
      },
      response: {
        value: RetVal("number")
      },
    },
    arrayArgs: {
      request: {
        a: Arg(0, "array:number")
      },
      response: {
        arrayReturn: RetVal("array:number")
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
        value: RetVal("string")
      },
    },
    testOneWay: {
      request: { a: Arg(0) },
      oneway: true
    },
    emitFalsyOptions: {
      oneway: true
    }
  }
});

var RootActor = protocol.ActorClassWithSpec(rootSpec, {
  initialize: function (conn) {
    protocol.Actor.prototype.initialize.call(this, conn);
    // Root actor owns itself.
    this.manage(this);
    this.actorID = "root";
  },

  sayHello: simpleHello,

  simpleReturn: function () {
    return 1;
  },

  promiseReturn: function () {
    return promise.resolve(1);
  },

  simpleArgs: function (a, b) {
    return { firstResponse: a + 1, secondResponse: b + 1 };
  },

  nestedArgs: function (a, b, c) {
    return { a: a, b: b, c: c };
  },

  optionArgs: function (options) {
    return { option1: options.option1, option2: options.option2 };
  },

  optionalArgs: function (a, b = 200) {
    return b;
  },

  arrayArgs: function (a) {
    return a;
  },

  nestedArrayArgs: function (a) {
    return a;
  },

  /**
   * Test that the 'type' part of the request packet works
   * correctly when the type isn't the same as the method name
   */
  renamedEcho: function (a) {
    if (this.conn.currentPacket.type != "echo") {
      return "goodbye";
    }
    return a;
  },

  testOneWay: function (a) {
    // Emit to show that we got this message, because there won't be a response.
    events.emit(this, "oneway", a);
  },

  emitFalsyOptions: function () {
    events.emit(this, "falsyOptions", { zero: 0, farce: false });
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
  DebuggerServer.createRootActor = (conn => {
    return RootActor(conn);
  });
  DebuggerServer.init();

  check_except(() => {
    let badActor = ActorClassWithSpec({}, {
      missing: preEvent("missing-event", function () {
      })
    });
    void badActor;
  });

  protocol.types.getType("array:array:array:number");
  protocol.types.getType("array:array:array:number");

  check_except(() => protocol.types.getType("unknown"));
  check_except(() => protocol.types.getType("array:unknown"));
  check_except(() => protocol.types.getType("unknown:number"));
  let trace = connectPipeTracing();
  let client = new DebuggerClient(trace);
  let rootClient;

  client.connect().then(([applicationType, traits]) => {
    trace.expectReceive({"from": "<actorid>",
                         "applicationType": "xpcshell-tests",
                         "traits": []});
    do_check_eq(applicationType, "xpcshell-tests");

    rootClient = RootFront(client);

    rootClient.simpleReturn().then(ret => {
      trace.expectSend({"type": "simpleReturn", "to": "<actorid>"});
      trace.expectReceive({"value": 1, "from": "<actorid>"});
      do_check_eq(ret, 1);
    }).then(() => {
      return rootClient.promiseReturn();
    }).then(ret => {
      trace.expectSend({"type": "promiseReturn", "to": "<actorid>"});
      trace.expectReceive({"value": 1, "from": "<actorid>"});
      do_check_eq(ret, 1);
    }).then(() => {
      // Missing argument should throw an exception
      check_except(() => {
        rootClient.simpleArgs(5);
      });

      return rootClient.simpleArgs(5, 10);
    }).then(ret => {
      trace.expectSend({"type": "simpleArgs",
                        "firstArg": 5,
                        "secondArg": 10,
                        "to": "<actorid>"});
      trace.expectReceive({"firstResponse": 6,
                           "secondResponse": 11,
                           "from": "<actorid>"});
      do_check_eq(ret.firstResponse, 6);
      do_check_eq(ret.secondResponse, 11);
    }).then(() => {
      return rootClient.nestedArgs(1, 2, 3);
    }).then(ret => {
      trace.expectSend({"type": "nestedArgs",
                        "firstArg": 1,
                        "nest": {"secondArg": 2, "nest": {"thirdArg": 3}},
                        "to": "<actorid>"});
      trace.expectReceive({"a": 1, "b": 2, "c": 3, "from": "<actorid>"});
      do_check_eq(ret.a, 1);
      do_check_eq(ret.b, 2);
      do_check_eq(ret.c, 3);
    }).then(() => {
      return rootClient.optionArgs({
        "option1": 5,
        "option2": 10
      });
    }).then(ret => {
      trace.expectSend({"type": "optionArgs",
                        "option1": 5,
                        "option2": 10,
                        "to": "<actorid>"});
      trace.expectReceive({"option1": 5, "option2": 10, "from": "<actorid>"});
      do_check_eq(ret.option1, 5);
      do_check_eq(ret.option2, 10);
    }).then(() => {
      return rootClient.optionArgs({});
    }).then(ret => {
      trace.expectSend({"type": "optionArgs", "to": "<actorid>"});
      trace.expectReceive({"from": "<actorid>"});
      do_check_true(typeof (ret.option1) === "undefined");
      do_check_true(typeof (ret.option2) === "undefined");
    }).then(() => {
      // Explicitly call an optional argument...
      return rootClient.optionalArgs(5, 10);
    }).then(ret => {
      trace.expectSend({"type": "optionalArgs", "a": 5, "b": 10, "to": "<actorid>"});
      trace.expectReceive({"value": 10, "from": "<actorid>"});
      do_check_eq(ret, 10);
    }).then(() => {
      // Now don't pass the optional argument, expect the default.
      return rootClient.optionalArgs(5);
    }).then(ret => {
      trace.expectSend({"type": "optionalArgs", "a": 5, "to": "<actorid>"});
      trace.expectReceive({"value": 200, "from": "<actorid>"});
      do_check_eq(ret, 200);
    }).then(ret => {
      return rootClient.arrayArgs([0, 1, 2, 3, 4, 5]);
    }).then(ret => {
      trace.expectSend({"type": "arrayArgs", "a": [0, 1, 2, 3, 4, 5], "to": "<actorid>"});
      trace.expectReceive({"arrayReturn": [0, 1, 2, 3, 4, 5], "from": "<actorid>"});
      do_check_eq(ret[0], 0);
      do_check_eq(ret[5], 5);
    }).then(() => {
      return rootClient.arrayArgs([[5]]);
    }).then(ret => {
      trace.expectSend({"type": "arrayArgs", "a": [[5]], "to": "<actorid>"});
      trace.expectReceive({"arrayReturn": [[5]], "from": "<actorid>"});
      do_check_eq(ret[0][0], 5);
    }).then(() => {
      return rootClient.renamedEcho("hello");
    }).then(str => {
      trace.expectSend({"type": "echo", "a": "hello", "to": "<actorid>"});
      trace.expectReceive({"value": "hello", "from": "<actorid>"});

      do_check_eq(str, "hello");

      let deferred = promise.defer();
      rootClient.on("oneway", (response) => {
        trace.expectSend({"type": "testOneWay", "a": "hello", "to": "<actorid>"});
        trace.expectReceive({"type": "oneway", "a": "hello", "from": "<actorid>"});

        do_check_eq(response, "hello");
        deferred.resolve();
      });
      do_check_true(typeof (rootClient.testOneWay("hello")) === "undefined");
      return deferred.promise;
    }).then(() => {
      let deferred = promise.defer();
      rootClient.on("falsyOptions", res => {
        trace.expectSend({"type": "emitFalsyOptions", "to": "<actorid>"});
        trace.expectReceive({"type": "falsyOptions",
                             "farce": false,
                             "zero": 0,
                             "from": "<actorid>"});

        do_check_true(res.zero === 0);
        do_check_true(res.farce === false);
        deferred.resolve();
      });
      rootClient.emitFalsyOptions();
      return deferred.promise;
    }).then(() => {
      client.close().then(() => {
        do_test_finished();
      });
    }).catch(err => {
      do_report_unexpected_exception(err, "Failure executing test");
    });
  });
  do_test_pending();
}
