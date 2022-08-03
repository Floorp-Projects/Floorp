/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint-disable max-nested-callbacks */

"use strict";

/**
 * Test simple requests using the protocol helpers.
 */
var protocol = require("devtools/shared/protocol");
var { RetVal, Arg } = protocol;
var EventEmitter = require("devtools/shared/event-emitter");
var { LongStringActor } = require("devtools/server/actors/string");

// The test implicitly relies on this.
require("devtools/client/fronts/string");

function simpleHello() {
  return {
    from: "root",
    applicationType: "xpcshell-tests",
    traits: [],
  };
}

DevToolsServer.LONG_STRING_LENGTH = DevToolsServer.LONG_STRING_INITIAL_LENGTH = DevToolsServer.LONG_STRING_READ_LENGTH = 5;

var SHORT_STR = "abc";
var LONG_STR = "abcdefghijklmnop";

var rootActor = null;

const rootSpec = protocol.generateActorSpec({
  typeName: "root",

  events: {
    "string-event": {
      str: Arg(0, "longstring"),
    },
  },

  methods: {
    shortString: {
      response: { value: RetVal("longstring") },
    },
    longString: {
      response: { value: RetVal("longstring") },
    },
    emitShortString: {
      oneway: true,
    },
    emitLongString: {
      oneway: true,
    },
  },
});

var RootActor = protocol.ActorClassWithSpec(rootSpec, {
  initialize(conn) {
    rootActor = this;
    protocol.Actor.prototype.initialize.call(this, conn);
    // Root actor owns itself.
    this.manage(this);
    this.actorID = "root";
  },

  sayHello: simpleHello,

  shortString() {
    return new LongStringActor(this.conn, SHORT_STR);
  },

  longString() {
    return new LongStringActor(this.conn, LONG_STR);
  },

  emitShortString() {
    EventEmitter.emit(
      this,
      "string-event",
      new LongStringActor(this.conn, SHORT_STR)
    );
  },

  emitLongString() {
    EventEmitter.emit(
      this,
      "string-event",
      new LongStringActor(this.conn, LONG_STR)
    );
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
  DevToolsServer.createRootActor = conn => {
    return RootActor(conn);
  };

  DevToolsServer.init();

  const trace = connectPipeTracing();
  const client = new DevToolsClient(trace);
  let rootFront;

  let strfront = null;

  const expectRootChildren = function(size) {
    Assert.equal(rootActor.__poolMap.size, size + 1);
    Assert.equal(rootFront.__poolMap.size, size + 1);
  };

  client.connect().then(([applicationType, traits]) => {
    rootFront = client.mainRoot;

    // Root actor has no children yet.
    expectRootChildren(0);

    trace.expectReceive({
      from: "<actorid>",
      applicationType: "xpcshell-tests",
      traits: [],
    });
    Assert.equal(applicationType, "xpcshell-tests");
    rootFront
      .shortString()
      .then(ret => {
        trace.expectSend({ type: "shortString", to: "<actorid>" });
        trace.expectReceive({ value: "abc", from: "<actorid>" });

        // Should only own the one reference (itself) at this point.
        expectRootChildren(0);
        strfront = ret;
      })
      .then(() => {
        return strfront.string();
      })
      .then(ret => {
        Assert.equal(ret, SHORT_STR);
      })
      .then(() => {
        return rootFront.longString();
      })
      .then(ret => {
        trace.expectSend({ type: "longString", to: "<actorid>" });
        trace.expectReceive({
          value: {
            type: "longString",
            actor: "<actorid>",
            length: 16,
            initial: "abcde",
          },
          from: "<actorid>",
        });

        strfront = ret;
        // Should own a reference to itself and an extra string now.
        expectRootChildren(1);
      })
      .then(() => {
        return strfront.string();
      })
      .then(ret => {
        trace.expectSend({
          type: "substring",
          start: 5,
          end: 10,
          to: "<actorid>",
        });
        trace.expectReceive({ substring: "fghij", from: "<actorid>" });
        trace.expectSend({
          type: "substring",
          start: 10,
          end: 15,
          to: "<actorid>",
        });
        trace.expectReceive({ substring: "klmno", from: "<actorid>" });
        trace.expectSend({
          type: "substring",
          start: 15,
          end: 20,
          to: "<actorid>",
        });
        trace.expectReceive({ substring: "p", from: "<actorid>" });

        Assert.equal(ret, LONG_STR);
      })
      .then(() => {
        return strfront.release();
      })
      .then(() => {
        trace.expectSend({ type: "release", to: "<actorid>" });
        trace.expectReceive({ from: "<actorid>" });

        // That reference should be removed now.
        expectRootChildren(0);
      })
      .then(() => {
        return new Promise(resolve => {
          rootFront.once("string-event", str => {
            trace.expectSend({ type: "emitShortString", to: "<actorid>" });
            trace.expectReceive({
              type: "string-event",
              str: "abc",
              from: "<actorid>",
            });

            Assert.ok(!!str);
            strfront = str;
            // Shouldn't generate any new references
            expectRootChildren(0);
            // will generate no packets.
            strfront.string().then(value => {
              resolve(value);
            });
          });
          rootFront.emitShortString();
        });
      })
      .then(value => {
        Assert.equal(value, SHORT_STR);
      })
      .then(() => {
        // Will generate no packets
        return strfront.release();
      })
      .then(() => {
        return new Promise(resolve => {
          rootFront.once("string-event", str => {
            trace.expectSend({ type: "emitLongString", to: "<actorid>" });
            trace.expectReceive({
              type: "string-event",
              str: {
                type: "longString",
                actor: "<actorid>",
                length: 16,
                initial: "abcde",
              },
              from: "<actorid>",
            });

            Assert.ok(!!str);
            // Should generate one new reference
            expectRootChildren(1);
            strfront = str;
            strfront.string().then(value => {
              trace.expectSend({
                type: "substring",
                start: 5,
                end: 10,
                to: "<actorid>",
              });
              trace.expectReceive({ substring: "fghij", from: "<actorid>" });
              trace.expectSend({
                type: "substring",
                start: 10,
                end: 15,
                to: "<actorid>",
              });
              trace.expectReceive({ substring: "klmno", from: "<actorid>" });
              trace.expectSend({
                type: "substring",
                start: 15,
                end: 20,
                to: "<actorid>",
              });
              trace.expectReceive({ substring: "p", from: "<actorid>" });

              resolve(value);
            });
          });
          rootFront.emitLongString();
        });
      })
      .then(value => {
        Assert.equal(value, LONG_STR);
      })
      .then(() => {
        return strfront.release();
      })
      .then(() => {
        trace.expectSend({ type: "release", to: "<actorid>" });
        trace.expectReceive({ from: "<actorid>" });
        expectRootChildren(0);
      })
      .then(() => {
        client.close().then(() => {
          do_test_finished();
        });
      })
      .catch(err => {
        do_report_unexpected_exception(err, "Failure executing test");
      });
  });
  do_test_pending();
}
