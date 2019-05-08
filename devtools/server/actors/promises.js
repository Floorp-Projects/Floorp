/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const protocol = require("devtools/shared/protocol");
const { promisesSpec } = require("devtools/shared/specs/promises");
const { expectState, ActorPool } = require("devtools/server/actors/common");
const { ObjectActor } = require("devtools/server/actors/object");
const { createValueGrip } = require("devtools/server/actors/object/utils");
const DevToolsUtils = require("devtools/shared/DevToolsUtils");
const EventEmitter = require("devtools/shared/event-emitter");

/**
 * The Promises Actor provides support for getting the list of live promises and
 * observing changes to their settlement state.
 */
var PromisesActor = protocol.ActorClassWithSpec(promisesSpec, {
  /**
   * @param conn DebuggerServerConnection.
   * @param parentActor BrowsingContextTargetActor|RootActor
   */
  initialize: function(conn, parentActor) {
    protocol.Actor.prototype.initialize.call(this, conn);

    this.conn = conn;
    this.parentActor = parentActor;
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

  destroy: function() {
    if (this.state === "attached") {
      this.detach();
    }

    protocol.Actor.prototype.destroy.call(this, this.conn);
  },

  get dbg() {
    if (!this._dbg) {
      this._dbg = this.parentActor.makeDebugger();
    }
    return this._dbg;
  },

  /**
   * Attach to the PromisesActor.
   */
  attach: expectState("detached", function() {
    this.dbg.addDebuggees();

    this._navigationLifetimePool = this._createActorPool();
    this.conn.addActorPool(this._navigationLifetimePool);

    this._newPromises = [];
    this._promisesSettled = [];

    this.dbg.findScripts().forEach(s => {
      this.parentActor.sources.createSourceActor(s.source);
    });

    this.dbg.onNewScript = s => {
      this.parentActor.sources.createSourceActor(s.source);
    };

    EventEmitter.on(this.parentActor, "window-ready", this._onWindowReady);

    this.state = "attached";
  }, "attaching to the PromisesActor"),

  /**
   * Detach from the PromisesActor upon Debugger closing.
   */
  detach: expectState("attached", function() {
    this.dbg.removeAllDebuggees();
    this.dbg.enabled = false;
    this._dbg = null;
    this._newPromises = null;
    this._promisesSettled = null;

    if (this._navigationLifetimePool) {
      this.conn.removeActorPool(this._navigationLifetimePool);
      this._navigationLifetimePool = null;
    }

    EventEmitter.off(this.parentActor, "window-ready", this._onWindowReady);

    this.state = "detached";
  }),

  _createActorPool: function() {
    const pool = new ActorPool(this.conn);
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
  _createObjectActorForPromise: function(promise) {
    if (this._navigationLifetimePool.objectActors.has(promise)) {
      return this._navigationLifetimePool.objectActors.get(promise);
    }

    const actor = new ObjectActor(promise, {
      getGripDepth: () => this._gripDepth,
      incrementGripDepth: () => this._gripDepth++,
      decrementGripDepth: () => this._gripDepth--,
      createValueGrip: v =>
        createValueGrip(v, this._navigationLifetimePool, this.objectGrip),
      sources: () => this.parentActor.sources,
      createEnvironmentActor: () => DevToolsUtils.reportException(
        "PromisesActor", Error("createEnvironmentActor not yet implemented")),
      getGlobalDebugObject: () => null,
    }, this.conn);

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
  objectGrip: function(value) {
    return this._createObjectActorForPromise(value).form();
  },

  /**
   * Get a list of ObjectActors for all live Promise Objects.
   */
  listPromises: function() {
    const promises = this.dbg.findObjects({ class: "Promise" });

    this.dbg.onNewPromise = this._makePromiseEventHandler(this._newPromises,
      "new-promises");
    this.dbg.onPromiseSettled = this._makePromiseEventHandler(
      this._promisesSettled, "promises-settled");

    return promises.map(p => this._createObjectActorForPromise(p));
  },

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
  _makePromiseEventHandler: function(array, eventName) {
    return promise => {
      const actor = this._createObjectActorForPromise(promise);
      const needsScheduling = array.length == 0;

      array.push(actor);

      if (needsScheduling) {
        DevToolsUtils.executeSoon(() => {
          this.emit(eventName, array.splice(0, array.length));
        });
      }
    };
  },

  _onWindowReady: expectState("attached", function({ isTopLevel }) {
    if (!isTopLevel) {
      return;
    }

    this._navigationLifetimePool.destroy();
    this.dbg.removeAllDebuggees();
    this.dbg.addDebuggees();
  }),
});

exports.PromisesActor = PromisesActor;
