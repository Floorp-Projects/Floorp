/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const protocol = require("devtools/shared/protocol");
const { method, RetVal, Arg, types } = protocol;
const { expectState, ActorPool } = require("devtools/server/actors/common");
const { ObjectActor,
        createValueGrip } = require("devtools/server/actors/object");
const DevToolsUtils = require("devtools/shared/DevToolsUtils");
loader.lazyRequireGetter(this, "events", "sdk/event/core");

// Teach protocol.js how to deal with legacy actor types
types.addType("ObjectActor", {
  write: actor => actor.grip(),
  read: grip => grip
});

/**
 * The Promises Actor provides support for getting the list of live promises and
 * observing changes to their settlement state.
 */
var PromisesActor = protocol.ActorClass({
  typeName: "promises",

  events: {
    // Event emitted for new promises allocated in debuggee and bufferred by
    // sending the list of promise objects in a batch.
    "new-promises": {
      type: "new-promises",
      data: Arg(0, "array:ObjectActor"),
    },
    // Event emitted for promise settlements.
    "promises-settled": {
      type: "promises-settled",
      data: Arg(0, "array:ObjectActor")
    }
  },

  /**
   * @param conn DebuggerServerConnection.
   * @param parent TabActor|RootActor
   */
  initialize: function (conn, parent) {
    protocol.Actor.prototype.initialize.call(this, conn);

    this.conn = conn;
    this.parent = parent;
    this.state = "detached";
    this._dbg = null;
    this._gripDepth = 0;
    this._navigationLifetimePool = null;
    this._newPromises = null;
    this._promisesSettled = null;

    this.objectGrip = this.objectGrip.bind(this);
    this._makePromiseEventHandler = this._makePromiseEventHandler.bind(this);
    this._onWindowReady = this._onWindowReady.bind(this);
  },

  destroy: function () {
    protocol.Actor.prototype.destroy.call(this, this.conn);

    if (this.state === "attached") {
      this.detach();
    }
  },

  get dbg() {
    if (!this._dbg) {
      this._dbg = this.parent.makeDebugger();
    }
    return this._dbg;
  },

  /**
   * Attach to the PromisesActor.
   */
  attach: method(expectState("detached", function () {
    this.dbg.addDebuggees();

    this._navigationLifetimePool = this._createActorPool();
    this.conn.addActorPool(this._navigationLifetimePool);

    this._newPromises = [];
    this._promisesSettled = [];

    this.dbg.findScripts().forEach(s => {
      this.parent.sources.createSourceActors(s.source);
    });

    this.dbg.onNewScript = s => {
      this.parent.sources.createSourceActors(s.source);
    };

    events.on(this.parent, "window-ready", this._onWindowReady);

    this.state = "attached";
  }, "attaching to the PromisesActor"), {
    request: {},
    response: {}
  }),

  /**
   * Detach from the PromisesActor upon Debugger closing.
   */
  detach: method(expectState("attached", function () {
    this.dbg.removeAllDebuggees();
    this.dbg.enabled = false;
    this._dbg = null;
    this._newPromises = null;
    this._promisesSettled = null;

    if (this._navigationLifetimePool) {
      this.conn.removeActorPool(this._navigationLifetimePool);
      this._navigationLifetimePool = null;
    }

    events.off(this.parent, "window-ready", this._onWindowReady);

    this.state = "detached";
  }, "detaching from the PromisesActor"), {
    request: {},
    response: {}
  }),

  _createActorPool: function () {
    let pool = new ActorPool(this.conn);
    pool.objectActors = new WeakMap();
    return pool;
  },

  /**
   * Create an ObjectActor for the given Promise object.
   *
   * @param object promise
   *        The promise object
   * @return object
   *        An ObjectActor object that wraps the given Promise object
   */
  _createObjectActorForPromise: function (promise) {
    if (this._navigationLifetimePool.objectActors.has(promise)) {
      return this._navigationLifetimePool.objectActors.get(promise);
    }

    let actor = new ObjectActor(promise, {
      getGripDepth: () => this._gripDepth,
      incrementGripDepth: () => this._gripDepth++,
      decrementGripDepth: () => this._gripDepth--,
      createValueGrip: v =>
        createValueGrip(v, this._navigationLifetimePool, this.objectGrip),
      sources: () => this.parent.sources,
      createEnvironmentActor: () => DevToolsUtils.reportException(
        "PromisesActor", Error("createEnvironmentActor not yet implemented")),
      getGlobalDebugObject: () => DevToolsUtils.reportException(
        "PromisesActor", Error("getGlobalDebugObject not yet implemented")),
    });

    this._navigationLifetimePool.addActor(actor);
    this._navigationLifetimePool.objectActors.set(promise, actor);

    return actor;
  },

  /**
   * Get a grip for the given Promise object.
   *
   * @param object value
   *        The Promise object
   * @return object
   *        The grip for the given Promise object
   */
  objectGrip: function (value) {
    return this._createObjectActorForPromise(value).grip();
  },

  /**
   * Get a list of ObjectActors for all live Promise Objects.
   */
  listPromises: method(function () {
    let promises = this.dbg.findObjects({ class: "Promise" });

    this.dbg.onNewPromise = this._makePromiseEventHandler(this._newPromises,
      "new-promises");
    this.dbg.onPromiseSettled = this._makePromiseEventHandler(
      this._promisesSettled, "promises-settled");

    return promises.map(p => this._createObjectActorForPromise(p));
  }, {
    request: {
    },
    response: {
      promises: RetVal("array:ObjectActor")
    }
  }),

  /**
   * Creates an event handler for onNewPromise that will add the new
   * Promise ObjectActor to the array and schedule it to be emitted as a
   * batch for the provided event.
   *
   * @param array array
   *        The list of Promise ObjectActors to emit
   * @param string eventName
   *        The event name
   */
  _makePromiseEventHandler: function (array, eventName) {
    return promise => {
      let actor = this._createObjectActorForPromise(promise);
      let needsScheduling = array.length == 0;

      array.push(actor);

      if (needsScheduling) {
        DevToolsUtils.executeSoon(() => {
          events.emit(this, eventName, array.splice(0, array.length));
        });
      }
    };
  },

  _onWindowReady: expectState("attached", function ({ isTopLevel }) {
    if (!isTopLevel) {
      return;
    }

    this._navigationLifetimePool.cleanup();
    this.dbg.removeAllDebuggees();
    this.dbg.addDebuggees();
  })
});

exports.PromisesActor = PromisesActor;

exports.PromisesFront = protocol.FrontClass(PromisesActor, {
  initialize: function (client, form) {
    protocol.Front.prototype.initialize.call(this, client, form);
    this.actorID = form.promisesActor;
    this.manage(this);
  },

  destroy: function () {
    protocol.Front.prototype.destroy.call(this);
  }
});
