/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/*
 * Target actor for all resources in a content process of Firefox (chrome sandboxes, frame
 * scripts, documents, etc.)
 *
 * See devtools/docs/backend/actor-hierarchy.md for more details.
 */

const { Cc, Ci, Cu } = require("chrome");
const Services = require("Services");

const { ChromeDebuggerActor } = require("devtools/server/actors/thread");
const { WebConsoleActor } = require("devtools/server/actors/webconsole");
const makeDebugger = require("devtools/server/actors/utils/make-debugger");
const { ActorPool } = require("devtools/server/actors/common");
const { Pool } = require("devtools/shared/protocol");
const { assert } = require("devtools/shared/DevToolsUtils");
const { TabSources } = require("devtools/server/actors/utils/TabSources");

loader.lazyRequireGetter(
  this,
  "WorkerTargetActorList",
  "devtools/server/actors/worker/worker-target-actor-list",
  true
);
loader.lazyRequireGetter(
  this,
  "MemoryActor",
  "devtools/server/actors/memory",
  true
);
loader.lazyRequireGetter(
  this,
  "PromisesActor",
  "devtools/server/actors/promises",
  true
);

function ContentProcessTargetActor(connection) {
  this.conn = connection;
  this._contextPool = new ActorPool(this.conn);
  this.conn.addActorPool(this._contextPool);
  this.threadActor = null;

  // Use a see-everything debugger
  this.makeDebugger = makeDebugger.bind(null, {
    findDebuggees: dbg => dbg.findAllGlobals(),
    shouldAddNewGlobalAsDebuggee: global => true,
  });

  const sandboxPrototype = {
    get tabs() {
      return Array.from(
        Services.ww.getWindowEnumerator(),
        win => win.docShell.messageManager
      );
    },
  };

  // Scope into which the webconsole executes:
  // A sandbox with chrome privileges with a `tabs` getter.
  const systemPrincipal = Cc["@mozilla.org/systemprincipal;1"].createInstance(
    Ci.nsIPrincipal
  );
  const sandbox = Cu.Sandbox(systemPrincipal, {
    sandboxPrototype,
    wantGlobalProperties: ["ChromeUtils"],
  });
  this._consoleScope = sandbox;

  this._workerList = null;
  this._workerTargetActorPool = null;
  this._onWorkerListChanged = this._onWorkerListChanged.bind(this);
}
exports.ContentProcessTargetActor = ContentProcessTargetActor;

ContentProcessTargetActor.prototype = {
  actorPrefix: "contentProcessTarget",

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
      assert(
        this.threadActor,
        "threadActor should exist when creating sources."
      );
      this._sources = new TabSources(this.threadActor);
    }
    return this._sources;
  },

  form: function() {
    if (!this._consoleActor) {
      this._consoleActor = new WebConsoleActor(this.conn, this);
      this._contextPool.addActor(this._consoleActor);
    }

    if (!this.threadActor) {
      this.threadActor = new ChromeDebuggerActor(this.conn, this);
      this._contextPool.addActor(this.threadActor);
    }
    if (!this.memoryActor) {
      this.memoryActor = new MemoryActor(this.conn, this);
      this._contextPool.addActor(this.memoryActor);
    }
    // Promises actor is being tested by xpcshell test, which uses the content process
    // target actor. But this actor isn't being used outside of tests yet.
    if (!this._promisesActor) {
      this._promisesActor = new PromisesActor(this.conn, this);
      this._contextPool.addActor(this._promisesActor);
    }

    return {
      actor: this.actorID,
      name: "Content process",

      consoleActor: this._consoleActor.actorID,
      chromeDebugger: this.threadActor.actorID,
      memoryActor: this.memoryActor.actorID,
      promisesActor: this._promisesActor.actorID,

      traits: {
        networkMonitor: false,
      },
    };
  },

  onListWorkers: function() {
    if (!this._workerList) {
      this._workerList = new WorkerTargetActorList(this.conn, {});
    }
    return this._workerList.getList().then(actors => {
      const pool = new Pool(this.conn);
      for (const actor of actors) {
        pool.manage(actor);
      }

      // Do not destroy the pool before transfering ownership to the newly created
      // pool, so that we do not accidently destroy actors that are still in use.
      if (this._workerTargetActorPool) {
        this._workerTargetActorPool.destroy();
      }

      this._workerTargetActorPool = pool;
      this._workerList.onListChanged = this._onWorkerListChanged;

      return {
        from: this.actorID,
        workers: actors.map(actor => actor.form()),
      };
    });
  },

  _onWorkerListChanged: function() {
    this.conn.send({ from: this.actorID, type: "workerListChanged" });
    this._workerList.onListChanged = null;
  },

  destroy: function() {
    this.conn.removeActorPool(this._contextPool);
    this._contextPool = null;

    // Tell the live lists we aren't watching any more.
    if (this._workerList) {
      this._workerList.onListChanged = null;
    }
  },

  preNest: function() {
    // TODO: freeze windows
    // window mediator doesn't work in child.
    // it doesn't throw, but doesn't return any window
  },

  postNest: function() {},
};

ContentProcessTargetActor.prototype.requestTypes = {
  listWorkers: ContentProcessTargetActor.prototype.onListWorkers,
};
