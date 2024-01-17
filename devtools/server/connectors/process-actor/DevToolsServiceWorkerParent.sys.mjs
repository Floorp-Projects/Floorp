/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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
ChromeUtils.defineESModuleGetters(lazy, {
  loader: "resource://devtools/shared/loader/Loader.sys.mjs",
});

ChromeUtils.defineLazyGetter(
  lazy,
  "JsWindowActorTransport",
  () =>
    lazy.loader.require("devtools/shared/transport/js-window-actor-transport")
      .JsWindowActorTransport
);

export class DevToolsServiceWorkerParent extends JSProcessActorParent {
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
   * Request the content process to create Service Worker Targets if workers matching the context
   * are already available.
   */
  async instantiateServiceWorkerTargets({
    watcherActorID,
    connectionPrefix,
    sessionContext,
    sessionData,
  }) {
    try {
      await this.sendQuery(
        "DevToolsServiceWorkerParent:instantiate-already-available",
        {
          watcherActorID,
          connectionPrefix,
          sessionContext,
          sessionData,
        }
      );
    } catch (e) {
      console.warn(
        "Failed to create DevTools Service Worker target for process",
        this.manager.osPid,
        "and watcher actor id",
        watcherActorID
      );
      console.warn(e);
    }
  }

  destroyServiceWorkerTargets({ watcherActorID, sessionContext }) {
    return this.sendAsyncMessage("DevToolsServiceWorkerParent:destroy", {
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
      await this.sendQuery(
        "DevToolsServiceWorkerParent:addOrSetSessionDataEntry",
        {
          watcherActorID,
          sessionContext,
          type,
          entries,
          updateType,
        }
      );
    } catch (e) {
      console.warn(
        "Failed to add session data entry for worker targets in process",
        this.manager.osPid,
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
    this.sendAsyncMessage(
      "DevToolsServiceWorkerParent:removeSessionDataEntry",
      {
        watcherActorID,
        sessionContext,
        type,
        entries,
      }
    );
  }

  serviceWorkerTargetAvailable({
    watcherActorID,
    forwardingPrefix,
    serviceWorkerTargetForm,
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
        connection,
        watcher,
        transport,
        actors: new Map(),
      });
    }

    const serviceWorkerTargetActorId = serviceWorkerTargetForm.actor;
    this._connections
      .get(prefix)
      .actors.set(serviceWorkerTargetActorId, serviceWorkerTargetForm);
    watcher.notifyTargetAvailable(serviceWorkerTargetForm);
  }

  serviceWorkerTargetDestroyed({
    watcherActorID,
    forwardingPrefix,
    serviceWorkerTargetForm,
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

    const serviceWorkerTargetActorId = serviceWorkerTargetForm.actor;
    const { actors } = this._connections.get(prefix);
    if (!actors.has(serviceWorkerTargetActorId)) {
      return;
    }

    actors.delete(serviceWorkerTargetActorId);
    watcher.notifyTargetDestroyed(serviceWorkerTargetForm);
  }

  _onConnectionClosed(status, prefix) {
    if (this._connections.has(prefix)) {
      const { connection } = this._connections.get(prefix);
      this._cleanupConnection(connection);
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
   * > The didDestroy method, if present, will be called after the ProcessActor is no
   * > longer able to receive any more messages.
   */
  didDestroy() {
    this._destroy();
  }

  /**
   * Supported Queries
   */

  async sendPacket(packet, prefix) {
    return this.sendAsyncMessage("DevToolsServiceWorkerParent:packet", {
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
      console.error(
        "Failed to sendQuery in DevToolsServiceWorkerParent",
        msg,
        e
      );
      throw e;
    }
  }

  receiveMessage(message) {
    switch (message.name) {
      case "DevToolsServiceWorkerChild:serviceWorkerTargetAvailable":
        return this.serviceWorkerTargetAvailable(message.data);
      case "DevToolsServiceWorkerChild:serviceWorkerTargetDestroyed":
        return this.serviceWorkerTargetDestroyed(message.data);
      case "DevToolsServiceWorkerChild:packet":
        return this.emit("packet-received", message);
      default:
        throw new Error(
          "Unsupported message in DevToolsServiceWorkerParent: " + message.name
        );
    }
  }
}
