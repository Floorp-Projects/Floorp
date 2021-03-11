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
const {
  SourcesManager,
} = require("devtools/server/actors/utils/sources-manager");
const { Actor } = require("devtools/shared/protocol");
const {
  contentProcessTargetSpec,
} = require("devtools/shared/specs/targets/content-process");
const Targets = require("devtools/server/actors/targets/index");
const Resources = require("devtools/server/actors/resources/index");
const TargetActorMixin = require("devtools/server/actors/targets/target-actor-mixin");

loader.lazyRequireGetter(
  this,
  "WorkerDescriptorActorList",
  "devtools/server/actors/worker/worker-descriptor-actor-list",
  true
);
loader.lazyRequireGetter(
  this,
  "MemoryActor",
  "devtools/server/actors/memory",
  true
);

const ContentProcessTargetActor = TargetActorMixin(
  Targets.TYPES.PROCESS,
  contentProcessTargetSpec,
  {
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
      const systemPrincipal = Cc[
        "@mozilla.org/systemprincipal;1"
      ].createInstance(Ci.nsIPrincipal);
      const sandbox = Cu.Sandbox(systemPrincipal, {
        sandboxPrototype,
        wantGlobalProperties: ["ChromeUtils"],
      });
      this._consoleScope = sandbox;

      this._workerList = null;
      this._workerDescriptorActorPool = null;
      this._onWorkerListChanged = this._onWorkerListChanged.bind(this);

      // Try to destroy the Content Process Target when the content process shuts down.
      // The parent process can't communicate during shutdown as the communication channel
      // is already down (message manager or JS Window Actor API).
      // So that we have to observe to some event fired from this process.
      // While such cleanup doesn't sound ultimately necessary (the process will be completely destroyed)
      // mochitests are asserting that there is no leaks during process shutdown.
      // Do not override destroy as Protocol.js may override it when calling destroy,
      // and we won't be able to call removeObserver correctly.
      this.destroyObserver = this.destroy.bind(this);
      Services.obs.addObserver(this.destroyObserver, "xpcom-shutdown");
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

    get sourcesManager() {
      if (!this._sourcesManager) {
        assert(
          this.threadActor,
          "threadActor should exist when creating SourcesManager."
        );
        this._sourcesManager = new SourcesManager(this.threadActor);
      }
      return this._sourcesManager;
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
        processID: Services.appinfo.processID,
        remoteType: Services.appinfo.remoteType,

        traits: {
          networkMonitor: false,
          // See trait description in browsing-context.js
          supportsTopLevelTargetFlag: false,
        },
      };
    },

    ensureWorkerList() {
      if (!this._workerList) {
        this._workerList = new WorkerDescriptorActorList(this.conn, {});
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
          if (this._workerDescriptorActorPool) {
            this._workerDescriptorActorPool.destroy();
          }

          this._workerDescriptorActorPool = pool;
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
      this.ensureWorkerList().workerPauser.setPauseServiceWorkers(
        request.origin
      );
    },

    destroy: function() {
      if (this.isDestroyed()) {
        return;
      }
      Resources.unwatchAllTargetResources(this);

      // Notify the client that this target is being destroyed.
      // So that we can destroy the target front and all its children.
      this.emit("tabDetached");

      Actor.prototype.destroy.call(this);

      if (this.threadActor) {
        this.threadActor = null;
      }

      // Tell the live lists we aren't watching any more.
      if (this._workerList) {
        this._workerList.destroy();
        this._workerList = null;
      }

      if (this._sourcesManager) {
        this._sourcesManager.destroy();
        this._sourcesManager = null;
      }

      if (this._dbg) {
        this._dbg.disable();
        this._dbg = null;
      }

      Services.obs.removeObserver(this.destroyObserver, "xpcom-shutdown");
    },
  }
);

exports.ContentProcessTargetActor = ContentProcessTargetActor;
