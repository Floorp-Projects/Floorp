/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { ActorClassWithSpec, Actor } = require("devtools/shared/protocol");
const { workerTargetSpec } = require("devtools/shared/specs/targets/worker");

const { ThreadActor } = require("devtools/server/actors/thread");
const { WebConsoleActor } = require("devtools/server/actors/webconsole");
const Targets = require("devtools/server/actors/targets/index");

const makeDebuggerUtil = require("devtools/server/actors/utils/make-debugger");
const { TabSources } = require("devtools/server/actors/utils/TabSources");

const Resources = require("devtools/server/actors/resources/index");

exports.WorkerTargetActor = ActorClassWithSpec(workerTargetSpec, {
  targetType: Targets.TYPES.WORKER,

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
    this._sources = null;

    this.makeDebugger = makeDebuggerUtil.bind(null, {
      findDebuggees: () => {
        return [workerGlobal];
      },
      shouldAddNewGlobalAsDebuggee: () => true,
    });

    this.notifyResourceAvailable = this.notifyResourceAvailable.bind(this);
  },

  form() {
    return {
      actor: this.actorID,
      threadActor: this.threadActor?.actorID,
      consoleActor: this._consoleActor?.actorID,
      id: this._workerDebuggerData.id,
      type: this._workerDebuggerData.type,
      url: this._workerDebuggerData.url,
      traits: {},
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

  get sources() {
    if (this._sources === null) {
      this._sources = new TabSources(this.threadActor);
    }

    return this._sources;
  },

  // This is called from the ThreadActor#onAttach method
  onThreadAttached() {
    // This isn't an RDP event and is only listened to from startup/worker.js.
    this.emit("worker-thread-attached");
  },

  addWatcherDataEntry(type, entries) {
    if (type == "resources") {
      return this._watchTargetResources(entries);
    }

    return Promise.resolve();
  },

  removeWatcherDataEntry(type, entries) {
    if (type == "resources") {
      return this._unwatchTargetResources(entries);
    }

    return Promise.resolve();
  },

  /**
   * These two methods will create and destroy resource watchers
   * for each resource type. This will end up calling `notifyResourceAvailable`
   * whenever new resources are observed.
   */
  _watchTargetResources(resourceTypes) {
    return Resources.watchResources(this, resourceTypes);
  },

  _unwatchTargetResources(resourceTypes) {
    return Resources.unwatchResources(this, resourceTypes);
  },

  /**
   * Called by Watchers, when new resources are available.
   *
   * @param Array<json> resources
   *        List of all available resources. A resource is a JSON object piped over to the client.
   *        It may contain actor IDs, actor forms, to be manually marshalled by the client.
   */
  notifyResourceAvailable(resources) {
    this.emit("resource-available-form", resources);
  },

  destroy() {
    Actor.prototype.destroy.call(this);

    if (this._sources) {
      this._sources.destroy();
      this._sources = null;
    }

    this.workerGlobal = null;
    this._dbg = null;
    this.threadActor = null;
    this._consoleActor = null;
  },
});
