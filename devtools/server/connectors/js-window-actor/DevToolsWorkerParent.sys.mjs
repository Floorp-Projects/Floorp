/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { loader } from "resource://devtools/shared/loader/Loader.sys.mjs";
import { EventEmitter } from "resource://gre/modules/EventEmitter.sys.mjs";

const { WatcherRegistry } = ChromeUtils.importESModule(
  "resource://devtools/server/actors/watcher/WatcherRegistry.sys.mjs",
  {
    // WatcherRegistry needs to be a true singleton and loads ActorManagerParent
    // which also has to be a true singleton.
    loadInDevToolsLoader: false,
  }
);

const lazy = {};

loader.lazyRequireGetter(
  lazy,
  "JsWindowActorTransport",
  "resource://devtools/shared/transport/js-window-actor-transport.js",
  true
);

export class DevToolsWorkerParent extends JSWindowActorParent {
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
   * Request the content process to create Worker Targets if workers matching the context
   * are already available.
   */
  async instantiateWorkerTargets({
    watcherActorID,
    connectionPrefix,
    sessionContext,
    sessionData,
  }) {
    try {
      await this.sendQuery(
        "DevToolsWorkerParent:instantiate-already-available",
        {
          watcherActorID,
          connectionPrefix,
          sessionContext,
          sessionData,
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

  destroyWorkerTargets({ watcherActorID, sessionContext }) {
    return this.sendAsyncMessage("DevToolsWorkerParent:destroy", {
      watcherActorID,
      sessionContext,
    });
  }

  /**
   * Communicate to the content process that some data have been added.
   */
  async addOrSetSessionDataEntry({
    watcherActorID,
    sessionContext,
    type,
    entries,
    updateType,
  }) {
    try {
      await this.sendQuery("DevToolsWorkerParent:addOrSetSessionDataEntry", {
        watcherActorID,
        sessionContext,
        type,
        entries,
        updateType,
      });
    } catch (e) {
      console.warn(
        "Failed to add session data entry for worker targets in browsing context",
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
  removeSessionDataEntry({ watcherActorID, sessionContext, type, entries }) {
    this.sendAsyncMessage("DevToolsWorkerParent:removeSessionDataEntry", {
      watcherActorID,
      sessionContext,
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
      const transport = new lazy.JsWindowActorTransport(this, forwardingPrefix);
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
    this._unregisterWatcher(prefix);
  }

  async _unregisterWatcher(connectionPrefix) {
    const connectionInfo = this._connections.get(connectionPrefix);
    if (!connectionInfo) {
      return;
    }

    const { watcher, transport } = connectionInfo;
    const connection = watcher.conn;

    connection.off("closed", this._onConnectionClosed);
    if (transport) {
      // If we have a child transport, the actor has already
      // been created. We need to stop using this transport.
      connection.cancelForwarding(transport._prefix);
      transport.close();
    }

    this._connections.delete(connectionPrefix);

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

      this._unregisterWatcher(watcher.conn.prefix);
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
