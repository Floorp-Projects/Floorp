/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Actor } = require("resource://devtools/shared/protocol.js");
const {
  workerTargetSpec,
} = require("resource://devtools/shared/specs/targets/worker.js");

const { ThreadActor } = require("resource://devtools/server/actors/thread.js");
const {
  WebConsoleActor,
} = require("resource://devtools/server/actors/webconsole.js");
const Targets = require("resource://devtools/server/actors/targets/index.js");

const makeDebuggerUtil = require("resource://devtools/server/actors/utils/make-debugger.js");
const {
  SourcesManager,
} = require("resource://devtools/server/actors/utils/sources-manager.js");

const TargetActorMixin = require("resource://devtools/server/actors/targets/target-actor-mixin.js");

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
     * @param {Boolean} workerDebuggerData.workerConsoleApiMessagesDispatchedToMainThread:
     *                  Value of the dom.worker.console.dispatch_events_to_main_thread pref
     * @param {Object} sessionContext: The Session Context to help know what is debugged.
     *                                 See devtools/server/actors/watcher/session-context.js
     */
    initialize(connection, workerGlobal, workerDebuggerData, sessionContext) {
      Actor.prototype.initialize.call(this, connection);

      // workerGlobal is needed by the console actor for evaluations.
      this.workerGlobal = workerGlobal;
      this.sessionContext = sessionContext;

      this._workerDebuggerData = workerDebuggerData;
      this._sourcesManager = null;
      this.workerConsoleApiMessagesDispatchedToMainThread =
        workerDebuggerData.workerConsoleApiMessagesDispatchedToMainThread;

      this.makeDebugger = makeDebuggerUtil.bind(null, {
        findDebuggees: () => {
          return [workerGlobal];
        },
        shouldAddNewGlobalAsDebuggee: () => true,
      });

      // needed by the console actor
      this.threadActor = new ThreadActor(this, this.workerGlobal);

      // needed by the thread actor to communicate with the console when evaluating logpoints.
      this._consoleActor = new WebConsoleActor(this.conn, this);

      this.manage(this.threadActor);
      this.manage(this._consoleActor);
    },

    // Expose the worker URL to the thread actor.
    // so that it can easily know what is the base URL of all worker scripts.
    get workerUrl() {
      return this._workerDebuggerData.url;
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
