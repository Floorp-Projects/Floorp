var protocol = require("devtools/shared/protocol");
var {method, Arg, Option, RetVal} = protocol;

protocol.types.addActorType("child");
protocol.types.addActorType("root");

// The child actor doesn't provide a form description
var ChildActor = protocol.ActorClass({
  typeName: "child",
  initialize(conn) {
    protocol.Actor.prototype.initialize.call(this, conn);
  },

  form(detail) {
    return {
      actor: this.actorID,
      extra: "extra"
    };
  },

  getChild: method(function () {
    return this;
  }, {
    response: RetVal("child")
  }),
});

var ChildFront = protocol.FrontClass(ChildActor, {
  initialize(client) {
    protocol.Front.prototype.initialize.call(this, client);
  },

  form(v, ctx, detail) {
    this.extra = v.extra;
  }
});

// The root actor does provide a form description.
var RootActor = protocol.ActorClass({
  typeName: "root",
  initialize(conn) {
    protocol.Actor.prototype.initialize.call(this, conn);
    this.manage(this);
    this.child = new ChildActor();
  },

  // Basic form type, relies on implicit DictType creation
  formType: {
    childActor: "child"
  },

  sayHello() {
    return {
      from: "root",
      applicationType: "xpcshell-tests",
      traits: []
    };
  },

  // This detail uses explicit DictType creation
  "formType#detail1": protocol.types.addDictType("RootActorFormTypeDetail1", {
    detailItem: "child"
  }),

  // This detail a string type.
  "formType#actorid": "string",

  form(detail) {
    if (detail === "detail1") {
      return {
        actor: this.actorID,
        detailItem: this.child
      };
    } else if (detail === "actorid") {
      return this.actorID;
    }

    return {
      actor: this.actorID,
      childActor: this.child
    };
  },

  getDefault: method(function () {
    return this;
  }, {
    response: RetVal("root")
  }),

  getDetail1: method(function () {
    return this;
  }, {
    response: RetVal("root#detail1")
  }),

  getDetail2: method(function () {
    return this;
  }, {
    response: {
      item: RetVal("root#actorid")
    }
  }),

  getUnknownDetail: method(function () {
    return this;
  }, {
    response: RetVal("root#unknownDetail")
  }),
});

var RootFront = protocol.FrontClass(RootActor, {
  initialize(client) {
    this.actorID = "root";
    protocol.Front.prototype.initialize.call(this, client);

    // Root owns itself.
    this.manage(this);
  },

  form(v, ctx, detail) {
    this.lastForm = v;
  }
});

const run_test = Test(function* () {
  DebuggerServer.createRootActor = (conn => {
    return RootActor(conn);
  });
  DebuggerServer.init();

  const connection = DebuggerServer.connectPipe();
  const conn = new DebuggerClient(connection);
  const client = Async(conn);

  yield client.connect();

  let rootFront = RootFront(conn);

  // Trigger some methods that return forms.
  let retval = yield rootFront.getDefault();
  do_check_true(retval instanceof RootFront);
  do_check_true(rootFront.lastForm.childActor instanceof ChildFront);

  retval = yield rootFront.getDetail1();
  do_check_true(retval instanceof RootFront);
  do_check_true(rootFront.lastForm.detailItem instanceof ChildFront);

  retval = yield rootFront.getDetail2();
  do_check_true(retval instanceof RootFront);
  do_check_true(typeof (rootFront.lastForm) === "string");

  // getUnknownDetail should fail, since no typeName is specified.
  try {
    yield rootFront.getUnknownDetail();
    do_check_true(false);
  } catch (ex) {
  }

  yield client.close();
});
