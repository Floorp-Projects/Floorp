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

const { ThreadActor } = require("devtools/server/actors/thread");
const { WebConsoleActor } = require("devtools/server/actors/webconsole");
const makeDebugger = require("devtools/server/actors/utils/make-debugger");
const { Pool } = require("devtools/shared/protocol");
const { assert } = require("devtools/shared/DevToolsUtils");
const { TabSources } = require("devtools/server/actors/utils/TabSources");
const { ActorClassWithSpec, Actor } = require("devtools/shared/protocol");
const {
  contentProcessTargetSpec,
} = require("devtools/shared/specs/targets/content-process");

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

const ContentProcessTargetActor = ActorClassWithSpec(contentProcessTargetSpec, {
  initialize: function(connection) {
    Actor.prototype.initialize.call(this, connection);
    this.conn = connection;
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
  },

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

  /*
   * Return a Debugger instance or create one if there is none yet
   */
  get dbg() {
    if (!this._dbg) {
      this._dbg = this.makeDebugger();
    }
    return this._dbg;
  },

  form: function() {
    if (!this._consoleActor) {
      this._consoleActor = new WebConsoleActor(this.conn, this);
      this.manage(this._consoleActor);
    }

    if (!this.threadActor) {
      this.threadActor = new ThreadActor(this, null);
      this.manage(this.threadActor);
    }
    if (!this.memoryActor) {
      this.memoryActor = new MemoryActor(this.conn, this);
      this.manage(this.memoryActor);
    }

    return {
      actor: this.actorID,
      consoleActor: this._consoleActor.actorID,
      threadActor: this.threadActor.actorID,
      memoryActor: this.memoryActor.actorID,

      traits: {
        networkMonitor: false,
      },
    };
  },

  ensureWorkerList() {
    if (!this._workerList) {
      this._workerList = new WorkerTargetActorList(this.conn, {});
    }
    return this._workerList;
  },

  listWorkers: function() {
    return this.ensureWorkerList()
      .getList()
      .then(actors => {
        const pool = new Pool(this.conn, "workers");
        for (const actor of actors) {
          pool.manage(actor);
        }

        // Do not destroy the pool before transfering ownership to the newly created
        // pool, so that we do not accidentally destroy actors that are still in use.
        if (this._workerTargetActorPool) {
          this._workerTargetActorPool.destroy();
        }

        this._workerTargetActorPool = pool;
        this._workerList.onListChanged = this._onWorkerListChanged;

        return {
          from: this.actorID,
          workers: actors,
        };
      });
  },

  _onWorkerListChanged: function() {
    this.conn.send({ from: this.actorID, type: "workerListChanged" });
    this._workerList.onListChanged = null;
  },

  pauseMatchingServiceWorkers(request) {
    this.ensureWorkerList().workerPauser.setPauseServiceWorkers(request.origin);
  },

  destroy: function() {
    Actor.prototype.destroy.call(this);

    // Tell the live lists we aren't watching any more.
    if (this._workerList) {
      this._workerList.destroy();
      this._workerList = null;
    }

    if (this._sources) {
      this._sources.destroy();
      this._sources = null;
    }

    if (this._dbg) {
      this._dbg.disable();
      this._dbg = null;
    }
  },
});

exports.ContentProcessTargetActor = ContentProcessTargetActor;
