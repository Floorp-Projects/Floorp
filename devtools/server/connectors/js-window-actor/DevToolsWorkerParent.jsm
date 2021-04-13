/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["DevToolsWorkerParent"];
const { loader } = ChromeUtils.import("resource://devtools/shared/Loader.jsm");
const { EventEmitter } = ChromeUtils.import(
  "resource://gre/modules/EventEmitter.jsm"
);
const { WatcherRegistry } = ChromeUtils.import(
  "resource://devtools/server/actors/watcher/WatcherRegistry.jsm"
);

loader.lazyRequireGetter(
  this,
  "JsWindowActorTransport",
  "devtools/shared/transport/js-window-actor-transport",
  true
);

class DevToolsWorkerParent extends JSWindowActorParent {
  constructor() {
    super();

    this._destroyed = false;

    // Map of DevToolsServerConnection's used to forward the messages from/to
    // the client. The connections run in the parent process, as this code. We
    // may have more than one when there is more than one client debugging the
    // same worker. For example, a content toolbox and the browser toolbox.
    //
    // The map is indexed by the connection prefix, and the values are object with the
    // following properties:
    // - watcher: The WatcherActor
    // - actors: A Map of the worker target actors form, indexed by WorkerTarget actorID
    // - transport: the JsWindowActorTransport
    //
    // Reminder about prefixes: all DevToolsServerConnections have a `prefix`
    // which can be considered as a kind of id. On top of this, parent process
    // DevToolsServerConnections also have forwarding prefixes because they are
    // responsible for forwarding messages to content process connections.
    this._connections = new Map();

    this._onConnectionClosed = this._onConnectionClosed.bind(this);
    EventEmitter.decorate(this);
  }

  /**
   * Request the content process to create Worker Targets if workers matching the browserId
   * are already available.
   */
  async instantiateWorkerTargets({
    watcherActorID,
    connectionPrefix,
    browserId,
    watchedData,
  }) {
    try {
      await this.sendQuery(
        "DevToolsWorkerParent:instantiate-already-available",
        {
          watcherActorID,
          connectionPrefix,
          browserId,
          watchedData,
        }
      );
    } catch (e) {
      console.warn(
        "Failed to create DevTools Worker target for browsingContext",
        this.browsingContext.id,
        "and watcher actor id",
        watcherActorID
      );
      console.warn(e);
    }
  }

  destroyWorkerTargets({ watcher, browserId }) {
    return this.sendAsyncMessage("DevToolsWorkerParent:destroy", {
      watcherActorID: watcher.actorID,
      browserId,
    });
  }

  /**
   * Communicate to the content process that some data have been added.
   */
  async addWatcherDataEntry({ watcherActorID, type, entries }) {
    try {
      await this.sendQuery("DevToolsWorkerParent:addWatcherDataEntry", {
        watcherActorID,
        type,
        entries,
      });
    } catch (e) {
      console.warn(
        "Failed to add watcher data entry for worker targets in browsing context",
        this.browsingContext.id,
        "and watcher actor id",
        watcherActorID
      );
      console.warn(e);
    }
  }

  /**
   * Communicate to the content process that some data have been removed.
   */
  removeWatcherDataEntry({ watcherActorID, type, entries }) {
    this.sendAsyncMessage("DevToolsWorkerParent:removeWatcherDataEntry", {
      watcherActorID,
      type,
      entries,
    });
  }

  workerTargetAvailable({
    watcherActorID,
    forwardingPrefix,
    workerTargetForm,
  }) {
    if (this._destroyed) {
      return;
    }

    const watcher = WatcherRegistry.getWatcher(watcherActorID);

    if (!watcher) {
      throw new Error(
        `Watcher Actor with ID '${watcherActorID}' can't be found.`
      );
    }

    const connection = watcher.conn;
    const { prefix } = connection;
    if (!this._connections.has(prefix)) {
      connection.on("closed", this._onConnectionClosed);

      // Create a js-window-actor based transport.
      const transport = new JsWindowActorTransport(this, forwardingPrefix);
      transport.hooks = {
        onPacket: connection.send.bind(connection),
        onTransportClosed() {},
      };
      transport.ready();

      connection.setForwarding(forwardingPrefix, transport);

      this._connections.set(prefix, {
        watcher,
        transport,
        actors: new Map(),
      });
    }

    const workerTargetActorId = workerTargetForm.actor;
    this._connections
      .get(prefix)
      .actors.set(workerTargetActorId, workerTargetForm);
    watcher.notifyTargetAvailable(workerTargetForm);
  }

  workerTargetDestroyed({
    watcherActorID,
    forwardingPrefix,
    workerTargetForm,
  }) {
    const watcher = WatcherRegistry.getWatcher(watcherActorID);

    if (!watcher) {
      throw new Error(
        `Watcher Actor with ID '${watcherActorID}' can't be found.`
      );
    }

    const connection = watcher.conn;
    const { prefix } = connection;
    if (!this._connections.has(prefix)) {
      return;
    }

    const workerTargetActorId = workerTargetForm.actor;
    const { actors } = this._connections.get(prefix);
    if (!actors.has(workerTargetActorId)) {
      return;
    }

    actors.delete(workerTargetActorId);
    watcher.notifyTargetDestroyed(workerTargetForm);
  }

  _onConnectionClosed(status, prefix) {
    if (this._connections.has(prefix)) {
      const { watcher } = this._connections.get(prefix);
      this._cleanupConnection(watcher.conn);
    }
  }

  async _cleanupConnection(connection) {
    if (!this._connections || !this._connections.has(connection.prefix)) {
      return;
    }

    const { transport } = this._connections.get(connection.prefix);

    connection.off("closed", this._onConnectionClosed);
    if (transport) {
      // If we have a child transport, the actor has already
      // been created. We need to stop using this transport.
      connection.cancelForwarding(transport._prefix);
      transport.close();
    }

    this._connections.delete(connection.prefix);
    if (!this._connections.size) {
      this._destroy();
    }
  }

  _destroy() {
    if (this._destroyed) {
      return;
    }
    this._destroyed = true;

    for (const { actors, watcher } of this._connections.values()) {
      for (const actor of actors.values()) {
        watcher.notifyTargetDestroyed(actor);
      }

      this._cleanupConnection(watcher.conn);
    }
  }

  /**
   * Part of JSActor API
   * https://searchfox.org/mozilla-central/rev/d9f92154813fbd4a528453c33886dc3a74f27abb/dom/chrome-webidl/JSActor.webidl#41-42,52
   *
   * > The didDestroy method, if present, will be called after the (JSWindow)actor is no
   * > longer able to receive any more messages.
   */
  didDestroy() {
    this._destroy();
  }

  /**
   * Supported Queries
   */

  async sendPacket(packet, prefix) {
    return this.sendAsyncMessage("DevToolsWorkerParent:packet", {
      packet,
      prefix,
    });
  }

  /**
   * JsWindowActor API
   */

  async sendQuery(msg, args) {
    try {
      const res = await super.sendQuery(msg, args);
      return res;
    } catch (e) {
      console.error("Failed to sendQuery in DevToolsWorkerParent", msg, e);
      throw e;
    }
  }

  receiveMessage(message) {
    switch (message.name) {
      case "DevToolsWorkerChild:workerTargetAvailable":
        return this.workerTargetAvailable(message.data);
      case "DevToolsWorkerChild:workerTargetDestroyed":
        return this.workerTargetDestroyed(message.data);
      case "DevToolsWorkerChild:packet":
        return this.emit("packet-received", message);
      default:
        throw new Error(
          "Unsupported message in DevToolsWorkerParent: " + message.name
        );
    }
  }
}
