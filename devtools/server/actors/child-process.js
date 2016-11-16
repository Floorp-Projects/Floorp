/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Cc, Ci, Cu } = require("chrome");

const { ChromeDebuggerActor } = require("devtools/server/actors/script");
const { WebConsoleActor } = require("devtools/server/actors/webconsole");
const makeDebugger = require("devtools/server/actors/utils/make-debugger");
const { ActorPool } = require("devtools/server/main");
const Services = require("Services");
const { assert } = require("devtools/shared/DevToolsUtils");
const { TabSources } = require("./utils/TabSources");

loader.lazyRequireGetter(this, "WorkerActorList", "devtools/server/actors/worker", true);

function ChildProcessActor(aConnection) {
  this.conn = aConnection;
  this._contextPool = new ActorPool(this.conn);
  this.conn.addActorPool(this._contextPool);
  this.threadActor = null;

  // Use a see-everything debugger
  this.makeDebugger = makeDebugger.bind(null, {
    findDebuggees: dbg => dbg.findAllGlobals(),
    shouldAddNewGlobalAsDebuggee: global => true
  });

  // Scope into which the webconsole executes:
  // An empty sandbox with chrome privileges
  let systemPrincipal = Cc["@mozilla.org/systemprincipal;1"]
    .createInstance(Ci.nsIPrincipal);
  let sandbox = Cu.Sandbox(systemPrincipal);
  this._consoleScope = sandbox;

  this._workerList = null;
  this._workerActorPool = null;
  this._onWorkerListChanged = this._onWorkerListChanged.bind(this);
}
exports.ChildProcessActor = ChildProcessActor;

ChildProcessActor.prototype = {
  actorPrefix: "process",

  get isRootActor() {
    return true;
  },

  get exited() {
    return !this._contextPool;
  },

  get url() {
    return undefined;
  },

  get window() {
    return this._consoleScope;
  },

  get sources() {
    if (!this._sources) {
      assert(this.threadActor, "threadActor should exist when creating sources.");
      this._sources = new TabSources(this.threadActor);
    }
    return this._sources;
  },

  form: function () {
    if (!this._consoleActor) {
      this._consoleActor = new WebConsoleActor(this.conn, this);
      this._contextPool.addActor(this._consoleActor);
    }

    if (!this.threadActor) {
      this.threadActor = new ChromeDebuggerActor(this.conn, this);
      this._contextPool.addActor(this.threadActor);
    }

    return {
      actor: this.actorID,
      name: "Content process",

      consoleActor: this._consoleActor.actorID,
      chromeDebugger: this.threadActor.actorID,

      traits: {
        highlightable: false,
        networkMonitor: false,
      },
    };
  },

  onListWorkers: function () {
    if (!this._workerList) {
      this._workerList = new WorkerActorList(this.conn, {});
    }
    return this._workerList.getList().then(actors => {
      let pool = new ActorPool(this.conn);
      for (let actor of actors) {
        pool.addActor(actor);
      }

      this.conn.removeActorPool(this._workerActorPool);
      this._workerActorPool = pool;
      this.conn.addActorPool(this._workerActorPool);

      this._workerList.onListChanged = this._onWorkerListChanged;

      return {
        "from": this.actorID,
        "workers": actors.map(actor => actor.form())
      };
    });
  },

  _onWorkerListChanged: function () {
    this.conn.send({ from: this.actorID, type: "workerListChanged" });
    this._workerList.onListChanged = null;
  },

  destroy: function () {
    this.conn.removeActorPool(this._contextPool);
    this._contextPool = null;

    // Tell the live lists we aren't watching any more.
    if (this._workerList) {
      this._workerList.onListChanged = null;
    }
  },

  preNest: function () {
    // TODO: freeze windows
    // window mediator doesn't work in child.
    // it doesn't throw, but doesn't return any window
  },

  postNest: function () {
  },
};

ChildProcessActor.prototype.requestTypes = {
  "listWorkers": ChildProcessActor.prototype.onListWorkers,
};
