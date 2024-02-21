/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { loader } from "resource://devtools/shared/loader/Loader.sys.mjs";
import { EventEmitter } from "resource://gre/modules/EventEmitter.sys.mjs";

const { WatcherRegistry } = ChromeUtils.importESModule(
  "resource://devtools/server/actors/watcher/WatcherRegistry.sys.mjs",
  // WatcherRegistry needs to be a true singleton and loads ActorManagerParent
  // which also has to be a true singleton.
  { global: "shared" }
);

const lazy = {};
loader.lazyRequireGetter(
  lazy,
  "JsWindowActorTransport",
  "devtools/shared/transport/js-window-actor-transport",
  true
);

export class DevToolsProcessParent extends JSProcessActorParent {
  constructor() {
    super();

    this._destroyed = false;

    // Map of DevToolsServerConnection's used to forward the messages from/to
    // the client. The connections run in the parent process, as this code. We
    // may have more than one when there is more than one client debugging the
    // same frame. For example, a content toolbox and the browser toolbox.
    //
    // The map is indexed by the connection prefix.
    // The values are objects containing the following properties:
    // - actor: the frame target actor(as a form)
    // - connection: the DevToolsServerConnection used to communicate with the
    //   frame target actor
    // - prefix: the forwarding prefix used by the connection to know
    //   how to forward packets to the frame target
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
   * Request the content process to create the ContentProcessTarget
   */
  instantiateTarget({
    watcherActorID,
    connectionPrefix,
    sessionContext,
    sessionData,
  }) {
    return this.sendQuery(
      "DevToolsProcessParent:instantiate-already-available",
      {
        watcherActorID,
        connectionPrefix,
        sessionContext,
        sessionData,
      }
    );
  }

  destroyTarget({ watcherActorID, isModeSwitching }) {
    this.sendAsyncMessage("DevToolsProcessParent:destroy", {
      watcherActorID,
      isModeSwitching,
    });
  }

  /**
   * Communicate to the content process that some data have been added.
   */
  addOrSetSessionDataEntry({ watcherActorID, type, entries, updateType }) {
    return this.sendQuery("DevToolsProcessParent:addOrSetSessionDataEntry", {
      watcherActorID,
      type,
      entries,
      updateType,
    });
  }

  /**
   * Communicate to the content process that some data have been removed.
   */
  removeSessionDataEntry({ watcherActorID, type, entries }) {
    this.sendAsyncMessage("DevToolsProcessParent:removeSessionDataEntry", {
      watcherActorID,
      type,
      entries,
    });
  }

  connectFromContent({ watcherActorID, forwardingPrefix, actor }) {
    const watcher = WatcherRegistry.getWatcher(watcherActorID);

    if (!watcher) {
      throw new Error(
        `Watcher Actor with ID '${watcherActorID}' can't be found.`
      );
    }
    const connection = watcher.conn;

    connection.on("closed", this._onConnectionClosed);

    // Create a js-window-actor based transport.
    const transport = new lazy.JsWindowActorTransport(
      this,
      forwardingPrefix,
      "DevToolsProcessParent:packet"
    );
    transport.hooks = {
      onPacket: connection.send.bind(connection),
      onClosed() {},
    };
    transport.ready();

    connection.setForwarding(forwardingPrefix, transport);

    this._connections.set(watcher.conn.prefix, {
      watcher,
      connection,
      // This prefix is the prefix of the DevToolsServerConnection, running
      // in the content process, for which we should forward packets to, based on its prefix.
      // While `watcher.connection` is also a DevToolsServerConnection, but from this process,
      // the parent process. It is the one receiving Client packets and the one, from which
      // we should forward packets from.
      forwardingPrefix,
      transport,
      actor,
    });

    watcher.notifyTargetAvailable(actor);
  }

  _onConnectionClosed(status, prefix) {
    if (this._connections.has(prefix)) {
      const { connection } = this._connections.get(prefix);
      this._cleanupConnection(connection);
    }
  }

  /**
   * Close and unregister a given DevToolsServerConnection.
   *
   * @param {DevToolsServerConnection} connection
   * @param {object} options
   * @param {boolean} options.isModeSwitching
   *        true when this is called as the result of a change to the devtools.browsertoolbox.scope pref
   */
  async _cleanupConnection(connection, options = {}) {
    const connectionInfo = this._connections.get(connection.prefix);
    if (!connectionInfo) {
      return;
    }
    const { forwardingPrefix, transport } = connectionInfo;

    connection.off("closed", this._onConnectionClosed);
    if (transport) {
      // If we have a child transport, the actor has already
      // been created. We need to stop using this transport.
      transport.close(options);
    }

    this._connections.delete(connection.prefix);
    if (!this._connections.size) {
      this._destroy(options);
    }

    // When cancelling the forwarding, one RDP event is sent to the client to purge all requests
    // and actors related to a given prefix. Do this *after* calling _destroy which will emit
    // the target-destroyed RDP event. This helps the Watcher Front retrieve the related target front,
    // otherwise it would be too eagerly destroyed by the purge event.
    connection.cancelForwarding(forwardingPrefix);
  }

  /**
   * Destroy and cleanup everything for this DOM Process.
   *
   * @param {object} options
   * @param {boolean} options.isModeSwitching
   *        true when this is called as the result of a change to the devtools.browsertoolbox.scope pref
   */
  _destroy(options) {
    if (this._destroyed) {
      return;
    }
    this._destroyed = true;

    for (const { actor, connection, watcher } of this._connections.values()) {
      watcher.notifyTargetDestroyed(actor, options);
      this._cleanupConnection(connection, options);
    }
  }

  /**
   * Supported Queries
   */

  sendPacket(packet, prefix) {
    this.sendAsyncMessage("DevToolsProcessParent:packet", { packet, prefix });
  }

  /**
   * JsWindowActor API
   */

  async sendQuery(msg, args) {
    try {
      const res = await super.sendQuery(msg, args);
      return res;
    } catch (e) {
      console.error("Failed to sendQuery in DevToolsProcessParent", msg);
      console.error(e.toString());
      throw e;
    }
  }

  receiveMessage(message) {
    switch (message.name) {
      case "DevToolsProcessChild:connectFromContent":
        return this.connectFromContent(message.data);
      case "DevToolsProcessChild:packet":
        return this.emit("packet-received", message);
      case "DevToolsProcessChild:destroy":
        for (const { form, watcherActorID } of message.data.actors) {
          const watcher = WatcherRegistry.getWatcher(watcherActorID);
          // As we instruct to destroy all targets when the watcher is destroyed,
          // we may easily receive the target destruction notification *after*
          // the watcher has been removed from the registry.
          if (watcher) {
            watcher.notifyTargetDestroyed(form, message.data.options);
            this._cleanupConnection(watcher.conn, message.data.options);
          }
        }
        return null;
      default:
        throw new Error(
          "Unsupported message in DevToolsProcessParent: " + message.name
        );
    }
  }

  didDestroy() {
    this._destroy();
  }
}
