/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint-disable max-nested-callbacks */

"use strict";

/**
 * Test simple requests using the protocol helpers.
 */
var protocol = require("devtools/shared/protocol");
var {preEvent, types, Arg, RetVal} = protocol;

var EventEmitter = require("devtools/shared/event-emitter");

function simpleHello() {
  return {
    from: "root",
    applicationType: "xpcshell-tests",
    traits: [],
  };
}

// Predeclaring the actor type so that it can be used in the
// implementation of the child actor.
types.addActorType("childActor");

const childSpec = protocol.generateActorSpec({
  typeName: "childActor",

  events: {
    "event1": {
      a: Arg(0),
      b: Arg(1),
      c: Arg(2)
    },
    "event2": {
      a: Arg(0),
      b: Arg(1),
      c: Arg(2)
    },
    "named-event": {
      type: "namedEvent",
      a: Arg(0),
      b: Arg(1),
      c: Arg(2)
    },
    "object-event": {
      type: "objectEvent",
      detail: Arg(0, "childActor#detail1"),
    },
    "array-object-event": {
      type: "arrayObjectEvent",
      detail: Arg(0, "array:childActor#detail2"),
    }
  },

  methods: {
    echo: {
      request: { str: Arg(0) },
      response: { str: RetVal("string") },
    },
    getDetail1: {
      // This also exercises return-value-as-packet.
      response: RetVal("childActor#detail1"),
    },
    getDetail2: {
      // This also exercises return-value-as-packet.
      response: RetVal("childActor#detail2"),
    },
    getIDDetail: {
      response: {
        idDetail: RetVal("childActor#actorid")
      }
    },
    getIntArray: {
      request: { inputArray: Arg(0, "array:number") },
      response: RetVal("array:number")
    },
    getSibling: {
      request: { id: Arg(0) },
      response: { sibling: RetVal("childActor") }
    },
    emitEvents: {
      response: { value: RetVal("string") },
    },
    release: {
      release: true
    }
  }
});

var ChildActor = protocol.ActorClassWithSpec(childSpec, {
  // Actors returned by this actor should be owned by the root actor.
  marshallPool: function() {
    return this.parent();
  },

  toString: function() {
    return "[ChildActor " + this.childID + "]";
  },

  initialize: function(conn, id) {
    protocol.Actor.prototype.initialize.call(this, conn);
    this.childID = id;
  },

  destroy: function() {
    protocol.Actor.prototype.destroy.call(this);
    this.destroyed = true;
  },

  form: function(detail) {
    if (detail === "actorid") {
      return this.actorID;
    }
    return {
      actor: this.actorID,
      childID: this.childID,
      detail: detail
    };
  },

  echo: function(str) {
    return str;
  },

  getDetail1: function() {
    return this;
  },

  getDetail2: function() {
    return this;
  },

  getIDDetail: function() {
    return this;
  },

  getIntArray: function(inputArray) {
    // Test that protocol.js converts an iterator to an array.
    const f = function* () {
      for (const i of inputArray) {
        yield 2 * i;
      }
    };
    return f();
  },

  getSibling: function(id) {
    return this.parent().getChild(id);
  },

  emitEvents: function() {
    EventEmitter.emit(this, "event1", 1, 2, 3);
    EventEmitter.emit(this, "event2", 4, 5, 6);
    EventEmitter.emit(this, "named-event", 1, 2, 3);
    EventEmitter.emit(this, "object-event", this);
    EventEmitter.emit(this, "array-object-event", [this]);
    return "correct response";
  },

  release: function() { },
});

var ChildFront = protocol.FrontClassWithSpec(childSpec, {
  initialize: function(client, form) {
    protocol.Front.prototype.initialize.call(this, client, form);
  },

  destroy: function() {
    this.destroyed = true;
    protocol.Front.prototype.destroy.call(this);
  },

  marshallPool: function() {
    return this.parent();
  },

  toString: function() {
    return "[child front " + this.childID + "]";
  },

  form: function(form, detail) {
    if (detail === "actorid") {
      return;
    }
    this.childID = form.childID;
    this.detail = form.detail;
  },

  onEvent1: preEvent("event1", function(a, b, c) {
    this.event1arg3 = c;
  }),

  onEvent2a: preEvent("event2", function(a, b, c) {
    return Promise.resolve().then(() => {
      this.event2arg3 = c;
    });
  }),

  onEvent2b: preEvent("event2", function(a, b, c) {
    this.event2arg2 = b;
  }),
});

types.addDictType("manyChildrenDict", {
  child5: "childActor",
  more: "array:childActor",
});

types.addLifetime("temp", "_temporaryHolder");

const rootSpec = protocol.generateActorSpec({
  typeName: "root",

  methods: {
    getChild: {
      request: { str: Arg(0) },
      response: { actor: RetVal("childActor") },
    },
    getChildren: {
      request: { ids: Arg(0, "array:string") },
      response: { children: RetVal("array:childActor") },
    },
    getChildren2: {
      request: { ids: Arg(0, "array:childActor") },
      response: { children: RetVal("array:childActor") },
    },
    getManyChildren: {
      response: RetVal("manyChildrenDict")
    },
    getTemporaryChild: {
      request: { id: Arg(0) },
      response: { child: RetVal("temp:childActor") }
    },
    clearTemporaryChildren: {}
  }
});

var rootActor = null;
var RootActor = protocol.ActorClassWithSpec(rootSpec, {
  toString: function() {
    return "[root actor]";
  },

  initialize: function(conn) {
    rootActor = this;
    this.actorID = "root";
    this._children = {};
    protocol.Actor.prototype.initialize.call(this, conn);
    // Root actor owns itself.
    this.manage(this);
  },

  sayHello: simpleHello,

  getChild: function(id) {
    if (id in this._children) {
      return this._children[id];
    }
    const child = new ChildActor(this.conn, id);
    this._children[id] = child;
    return child;
  },

  getChildren: function(ids) {
    return ids.map(id => this.getChild(id));
  },

  getChildren2: function(ids) {
    const f = function* () {
      for (const c of ids) {
        yield c;
      }
    };
    return f();
  },

  getManyChildren: function() {
    return {
      // note that this isn't in the specialization array.
      foo: "bar",
      child5: this.getChild("child5"),
      more: [ this.getChild("child6"), this.getChild("child7") ]
    };
  },

  // This should remind you of a pause actor.
  getTemporaryChild: function(id) {
    if (!this._temporaryHolder) {
      this._temporaryHolder = this.manage(new protocol.Actor(this.conn));
    }
    return new ChildActor(this.conn, id);
  },

  clearTemporaryChildren: function(id) {
    if (!this._temporaryHolder) {
      return;
    }
    this._temporaryHolder.destroy();
    delete this._temporaryHolder;
  }
});

var RootFront = protocol.FrontClassWithSpec(rootSpec, {
  toString: function() {
    return "[root front]";
  },
  initialize: function(client) {
    this.actorID = "root";
    protocol.Front.prototype.initialize.call(this, client);
    // Root actor owns itself.
    this.manage(this);
  },

  getTemporaryChild: protocol.custom(function(id) {
    if (!this._temporaryHolder) {
      this._temporaryHolder = new protocol.Front(this.conn);
      this._temporaryHolder.actorID = this.actorID + "_temp";
      this._temporaryHolder = this.manage(this._temporaryHolder);
    }
    return this._getTemporaryChild(id);
  }, {
    impl: "_getTemporaryChild"
  }),

  clearTemporaryChildren: protocol.custom(function() {
    if (!this._temporaryHolder) {
      return Promise.resolve(undefined);
    }
    this._temporaryHolder.destroy();
    delete this._temporaryHolder;
    return this._clearTemporaryChildren();
  }, {
    impl: "_clearTemporaryChildren"
  })
});

function run_test() {
  DebuggerServer.createRootActor = (conn => {
    return RootActor(conn);
  });
  DebuggerServer.init();

  const trace = connectPipeTracing();
  const client = new DebuggerClient(trace);
  client.connect().then(([applicationType, traits]) => {
    trace.expectReceive({"from": "<actorid>",
                         "applicationType": "xpcshell-tests",
                         "traits": []});
    Assert.equal(applicationType, "xpcshell-tests");

    const rootFront = RootFront(client);
    let childFront = null;

    const expectRootChildren = size => {
      Assert.equal(rootActor._poolMap.size, size + 1);
      Assert.equal(rootFront._poolMap.size, size + 1);
      if (childFront) {
        Assert.equal(childFront._poolMap.size, 0);
      }
    };

    rootFront.getChild("child1").then(ret => {
      trace.expectSend({"type": "getChild", "str": "child1", "to": "<actorid>"});
      trace.expectReceive({"actor": "<actorid>", "from": "<actorid>"});

      childFront = ret;
      Assert.ok(childFront instanceof ChildFront);
      Assert.equal(childFront.childID, "child1");
      expectRootChildren(1);
    }).then(() => {
      // Request the child again, make sure the same is returned.
      return rootFront.getChild("child1");
    }).then(ret => {
      trace.expectSend({"type": "getChild", "str": "child1", "to": "<actorid>"});
      trace.expectReceive({"actor": "<actorid>", "from": "<actorid>"});

      expectRootChildren(1);
      Assert.ok(ret === childFront);
    }).then(() => {
      return childFront.echo("hello");
    }).then(ret => {
      trace.expectSend({"type": "echo", "str": "hello", "to": "<actorid>"});
      trace.expectReceive({"str": "hello", "from": "<actorid>"});

      Assert.equal(ret, "hello");
    }).then(() => {
      return childFront.getDetail1();
    }).then(ret => {
      trace.expectSend({"type": "getDetail1", "to": "<actorid>"});
      trace.expectReceive({"actor": "<actorid>",
                           "childID": "child1",
                           "detail": "detail1",
                           "from": "<actorid>"});
      Assert.ok(ret === childFront);
      Assert.equal(childFront.detail, "detail1");
    }).then(() => {
      return childFront.getDetail2();
    }).then(ret => {
      trace.expectSend({"type": "getDetail2", "to": "<actorid>"});
      trace.expectReceive({"actor": "<actorid>",
                           "childID": "child1",
                           "detail": "detail2",
                           "from": "<actorid>"});
      Assert.ok(ret === childFront);
      Assert.equal(childFront.detail, "detail2");
    }).then(() => {
      return childFront.getIDDetail();
    }).then(ret => {
      trace.expectSend({"type": "getIDDetail", "to": "<actorid>"});
      trace.expectReceive({"idDetail": childFront.actorID, "from": "<actorid>"});
      Assert.ok(ret === childFront);
    }).then(() => {
      return childFront.getSibling("siblingID");
    }).then(ret => {
      trace.expectSend({"type": "getSibling", "id": "siblingID", "to": "<actorid>"});
      trace.expectReceive({"sibling": {"actor": "<actorid>", "childID": "siblingID"},
                           "from": "<actorid>"});

      expectRootChildren(2);
    }).then(ret => {
      return rootFront.getTemporaryChild("temp1").then(temp1 => {
        trace.expectSend({"type": "getTemporaryChild", "id": "temp1", "to": "<actorid>"});
        trace.expectReceive({"child": {"actor": "<actorid>", "childID": "temp1"},
                             "from": "<actorid>"});

        // At this point we expect two direct children, plus the temporary holder
        // which should hold 1 itself.
        Assert.equal(rootActor._temporaryHolder.__poolMap.size, 1);
        Assert.equal(rootFront._temporaryHolder.__poolMap.size, 1);

        expectRootChildren(3);
        return rootFront.getTemporaryChild("temp2").then(temp2 => {
          trace.expectSend({"type": "getTemporaryChild",
                            "id": "temp2",
                            "to": "<actorid>"});
          trace.expectReceive({"child": {"actor": "<actorid>", "childID": "temp2"},
                               "from": "<actorid>"});

          // Same amount of direct children, and an extra in the temporary holder.
          expectRootChildren(3);
          Assert.equal(rootActor._temporaryHolder.__poolMap.size, 2);
          Assert.equal(rootFront._temporaryHolder.__poolMap.size, 2);

          // Get the children of the temporary holder...
          const checkActors = rootActor._temporaryHolder.__poolMap.values();

          // Now release the temporary holders and expect them to drop again.
          return rootFront.clearTemporaryChildren().then(() => {
            trace.expectSend({"type": "clearTemporaryChildren", "to": "<actorid>"});
            trace.expectReceive({"from": "<actorid>"});

            expectRootChildren(2);
            Assert.ok(!rootActor._temporaryHolder);
            Assert.ok(!rootFront._temporaryHolder);
            for (const checkActor of checkActors) {
              Assert.ok(checkActor.destroyed);
              Assert.ok(checkActor.destroyed);
            }
          });
        });
      });
    }).then(ret => {
      return rootFront.getChildren(["child1", "child2"]);
    }).then(ret => {
      trace.expectSend({"type": "getChildren",
                        "ids": ["child1", "child2"],
                        "to": "<actorid>"});
      trace.expectReceive({"children": [{"actor": "<actorid>", "childID": "child1"},
                                        {"actor": "<actorid>", "childID": "child2"}],
                           "from": "<actorid>"});

      expectRootChildren(3);
      Assert.ok(ret[0] === childFront);
      Assert.ok(ret[1] !== childFront);
      Assert.ok(ret[1] instanceof ChildFront);

      // On both children, listen to events.  We're only
      // going to trigger events on the first child, so an event
      // triggered on the second should cause immediate failures.

      const set = new Set(["event1", "event2", "named-event",
                           "object-event", "array-object-event"]);

      childFront.on("event1", (a, b, c) => {
        Assert.equal(a, 1);
        Assert.equal(b, 2);
        Assert.equal(c, 3);
        // Verify that the pre-event handler was called.
        Assert.equal(childFront.event1arg3, 3);
        set.delete("event1");
      });
      childFront.on("event2", (a, b, c) => {
        Assert.equal(a, 4);
        Assert.equal(b, 5);
        Assert.equal(c, 6);
        // Verify that the async pre-event handler was called,
        // setting the property before this handler was called.
        Assert.equal(childFront.event2arg3, 6);
        // And check that the sync preEvent with the same name is also
        // executed
        Assert.equal(childFront.event2arg2, 5);
        set.delete("event2");
      });
      childFront.on("named-event", (a, b, c) => {
        Assert.equal(a, 1);
        Assert.equal(b, 2);
        Assert.equal(c, 3);
        set.delete("named-event");
      });
      childFront.on("object-event", (obj) => {
        Assert.ok(obj === childFront);
        Assert.equal(childFront.detail, "detail1");
        set.delete("object-event");
      });
      childFront.on("array-object-event", (array) => {
        Assert.ok(array[0] === childFront);
        Assert.equal(childFront.detail, "detail2");
        set.delete("array-object-event");
      });

      const fail = function() {
        do_throw("Unexpected event");
      };
      ret[1].on("event1", fail);
      ret[1].on("event2", fail);
      ret[1].on("named-event", fail);
      ret[1].on("object-event", fail);
      ret[1].on("array-object-event", fail);

      return childFront.emitEvents().then(() => {
        trace.expectSend({"type": "emitEvents", "to": "<actorid>"});
        trace.expectReceive({"type": "event1", "a": 1, "b": 2, "c": 3,
                             "from": "<actorid>"});
        trace.expectReceive({"type": "event2", "a": 4, "b": 5, "c": 6,
                             "from": "<actorid>"});
        trace.expectReceive({"type": "namedEvent", "a": 1, "b": 2, "c": 3,
                             "from": "<actorid>"});
        trace.expectReceive({"type": "objectEvent",
                             "detail": {"actor": "<actorid>",
                                        "childID": "child1",
                                        "detail": "detail1"},
                             "from": "<actorid>"});
        trace.expectReceive({"type": "arrayObjectEvent",
                             "detail": [{"actor": "<actorid>",
                                         "childID": "child1",
                                         "detail": "detail2"}],
                             "from": "<actorid>"});
        trace.expectReceive({"value": "correct response", "from": "<actorid>"});

        Assert.equal(set.size, 0);
      });
    }).then(ret => {
      return rootFront.getManyChildren();
    }).then(ret => {
      trace.expectSend({"type": "getManyChildren", "to": "<actorid>"});
      trace.expectReceive({"foo": "bar",
                           "child5": {"actor": "<actorid>", "childID": "child5"},
                           "more": [{"actor": "<actorid>", "childID": "child6"},
                                    {"actor": "<actorid>", "childID": "child7"}],
                           "from": "<actorid>"});

      // Check all the crazy stuff we did in getManyChildren
      Assert.equal(ret.foo, "bar");
      Assert.equal(ret.child5.childID, "child5");
      Assert.equal(ret.more[0].childID, "child6");
      Assert.equal(ret.more[1].childID, "child7");
    }).then(() => {
      // Test accepting a generator.
      const f = function* () {
        for (const i of [1, 2, 3, 4, 5]) {
          yield i;
        }
      };
      return childFront.getIntArray(f());
    }).then((ret) => {
      Assert.equal(ret.length, 5);
      const expected = [2, 4, 6, 8, 10];
      for (let i = 0; i < 5; ++i) {
        Assert.equal(ret[i], expected[i]);
      }
    }).then(() => {
      return rootFront.getChildren(["child1", "child2"]);
    }).then(ids => {
      const f = function* () {
        for (const id of ids) {
          yield id;
        }
      };
      return rootFront.getChildren2(f());
    }).then(ret => {
      Assert.equal(ret.length, 2);
      Assert.ok(ret[0] === childFront);
      Assert.ok(ret[1] !== childFront);
      Assert.ok(ret[1] instanceof ChildFront);
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
