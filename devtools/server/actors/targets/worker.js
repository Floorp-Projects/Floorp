/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Actor } = require("devtools/shared/protocol");
const { workerTargetSpec } = require("devtools/shared/specs/targets/worker");

const { ThreadActor } = require("devtools/server/actors/thread");
const { WebConsoleActor } = require("devtools/server/actors/webconsole");
const Targets = require("devtools/server/actors/targets/index");

const makeDebuggerUtil = require("devtools/server/actors/utils/make-debugger");
const {
  SourcesManager,
} = require("devtools/server/actors/utils/sources-manager");

const TargetActorMixin = require("devtools/server/actors/targets/target-actor-mixin");

exports.WorkerTargetActor = TargetActorMixin(
  Targets.TYPES.WORKER,
  workerTargetSpec,
  {
    /**
     * Target actor for a worker in the content process.
     *
     * @param {DevToolsServerConnection} connection: The connection to the client.
     * @param {WorkerGlobalScope} workerGlobal: The worker global.
     * @param {Object} workerDebuggerData: The worker debugger information
     * @param {String} workerDebuggerData.id: The worker debugger id
     * @param {String} workerDebuggerData.url: The worker debugger url
     * @param {String} workerDebuggerData.type: The worker debugger type
     */
    initialize: function(connection, workerGlobal, workerDebuggerData) {
      Actor.prototype.initialize.call(this, connection);

      // workerGlobal is needed by the console actor for evaluations.
      this.workerGlobal = workerGlobal;

      this._workerDebuggerData = workerDebuggerData;
      this._sourcesManager = null;

      this.makeDebugger = makeDebuggerUtil.bind(null, {
        findDebuggees: () => {
          return [workerGlobal];
        },
        shouldAddNewGlobalAsDebuggee: () => true,
      });
    },

    form() {
      return {
        actor: this.actorID,
        threadActor: this.threadActor?.actorID,
        consoleActor: this._consoleActor?.actorID,
        id: this._workerDebuggerData.id,
        type: this._workerDebuggerData.type,
        url: this._workerDebuggerData.url,
        traits: {
          // See trait description in browsing-context.js
          supportsTopLevelTargetFlag: false,
        },
      };
    },

    attach() {
      if (this.threadActor) {
        return;
      }

      // needed by the console actor
      this.threadActor = new ThreadActor(this, this.workerGlobal);

      // needed by the thread actor to communicate with the console when evaluating logpoints.
      this._consoleActor = new WebConsoleActor(this.conn, this);

      this.manage(this.threadActor);
      this.manage(this._consoleActor);
    },

    get dbg() {
      if (!this._dbg) {
        this._dbg = this.makeDebugger();
      }
      return this._dbg;
    },

    get sourcesManager() {
      if (this._sourcesManager === null) {
        this._sourcesManager = new SourcesManager(this.threadActor);
      }

      return this._sourcesManager;
    },

    // This is called from the ThreadActor#onAttach method
    onThreadAttached() {
      // This isn't an RDP event and is only listened to from startup/worker.js.
      this.emit("worker-thread-attached");
    },

    destroy() {
      Actor.prototype.destroy.call(this);

      if (this._sourcesManager) {
        this._sourcesManager.destroy();
        this._sourcesManager = null;
      }

      this.workerGlobal = null;
      this._dbg = null;
      this._consoleActor = null;
      this.threadActor = null;
    },
  }
);
