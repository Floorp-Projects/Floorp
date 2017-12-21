/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Make sure we get replies in the same order that we sent their
 * requests even when earlier requests take several event ticks to
 * complete.
 */

var protocol = require("devtools/shared/protocol");
var {Arg, RetVal} = protocol;

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
      response: { value: RetVal("number") }
    },
    promiseThrow: {
      response: { value: RetVal("number") },
    }
  }
});

var RootActor = protocol.ActorClassWithSpec(rootSpec, {
  initialize: function (conn) {
    protocol.Actor.prototype.initialize.call(this, conn);
    // Root actor owns itself.
    this.manage(this);
    this.actorID = "root";
    this.sequence = 0;
  },

  sayHello: simpleHello,

  simpleReturn: function () {
    return this.sequence++;
  },

  promiseReturn: function (toWait) {
    // Guarantee that this resolves after simpleReturn returns.
    let deferred = defer();
    let sequence = this.sequence++;

    // Wait until the number of requests specified by toWait have
    // happened, to test queuing.
    let check = () => {
      if ((this.sequence - sequence) < toWait) {
        do_execute_soon(check);
        return;
      }
      deferred.resolve(sequence);
    };
    do_execute_soon(check);

    return deferred.promise;
  },

  simpleThrow: function () {
    throw new Error(this.sequence++);
  },

  promiseThrow: function () {
    // Guarantee that this resolves after simpleReturn returns.
    let deferred = defer();
    let sequence = this.sequence++;
    // This should be enough to force a failure if the code is broken.
    do_timeout(150, () => {
      deferred.reject(sequence++);
    });
    return deferred.promise;
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
  DebuggerServer.createRootActor = RootActor;
  DebuggerServer.init();

  let trace = connectPipeTracing();
  let client = new DebuggerClient(trace);
  let rootClient;

  client.connect().then(([applicationType, traits]) => {
    rootClient = RootFront(client);

    let calls = [];
    let sequence = 0;

    // Execute a call that won't finish processing until 2
    // more calls have happened
    calls.push(rootClient.promiseReturn(2).then(ret => {
      // Check right return order
      Assert.equal(sequence, 0);
      // Check request handling order
      Assert.equal(ret, sequence++);
    }));

    // Put a few requests into the backlog

    calls.push(rootClient.simpleReturn().then(ret => {
      // Check right return order
      Assert.equal(sequence, 1);
      // Check request handling order
      Assert.equal(ret, sequence++);
    }));

    calls.push(rootClient.simpleReturn().then(ret => {
      // Check right return order
      Assert.equal(sequence, 2);
      // Check request handling order
      Assert.equal(ret, sequence++);
    }));

    calls.push(rootClient.simpleThrow().then(() => {
      Assert.ok(false, "simpleThrow shouldn't succeed!");
    }, error => {
      // Check right return order
      Assert.equal(sequence++, 3);
    }));

    // While packets are sent in the correct order, rejection handlers
    // registered in "Promise.jsm" may be invoked later than fulfillment
    // handlers, meaning that we can't check the actual order with certainty.
    let deferAfterRejection = defer();

    calls.push(rootClient.promiseThrow().then(() => {
      Assert.ok(false, "promiseThrow shouldn't succeed!");
    }, error => {
      // Check right return order
      Assert.equal(sequence++, 4);
      Assert.ok(true, "simple throw should throw");
      deferAfterRejection.resolve();
    }));

    calls.push(rootClient.simpleReturn().then(ret => {
      return deferAfterRejection.promise.then(function () {
        // Check right return order
        Assert.equal(sequence, 5);
        // Check request handling order
        Assert.equal(ret, sequence++);
      });
    }));

    // Break up the backlog with a long request that waits
    // for another simpleReturn before completing
    calls.push(rootClient.promiseReturn(1).then(ret => {
      return deferAfterRejection.promise.then(function () {
        // Check right return order
        Assert.equal(sequence, 6);
        // Check request handling order
        Assert.equal(ret, sequence++);
      });
    }));

    calls.push(rootClient.simpleReturn().then(ret => {
      return deferAfterRejection.promise.then(function () {
        // Check right return order
        Assert.equal(sequence, 7);
        // Check request handling order
        Assert.equal(ret, sequence++);
      });
    }));

    promise.all(calls).then(() => {
      client.close().then(() => {
        do_test_finished();
      });
    });
  });
  do_test_pending();
}
