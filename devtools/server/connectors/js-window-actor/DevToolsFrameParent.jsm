/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["DevToolsFrameParent"];
const { loader } = ChromeUtils.import("resource://devtools/shared/Loader.jsm");
const { EventEmitter } = ChromeUtils.import(
  "resource://gre/modules/EventEmitter.jsm"
);
const { WatcherRegistry } = ChromeUtils.import(
  "resource://devtools/server/actors/descriptors/watcher/WatcherRegistry.jsm"
);

loader.lazyRequireGetter(
  this,
  "JsWindowActorTransport",
  "devtools/shared/transport/js-window-actor-transport",
  true
);

class DevToolsFrameParent extends JSWindowActorParent {
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
   * Request the content process to create the Frame Target if there is one
   * already available that matches the Browsing Context ID
   */
  instantiateTarget({
    watcherActorID,
    connectionPrefix,
    browserId,
    watchedResources,
  }) {
    return this.sendQuery("DevToolsFrameParent:instantiate-already-available", {
      watcherActorID,
      connectionPrefix,
      browserId,
      watchedResources,
    });
  }

  destroyTarget({ watcherActorID, browserId }) {
    this.sendAsyncMessage("DevToolsFrameParent:destroy", {
      watcherActorID,
      browserId,
    });
  }

  /**
   * Request the content process to fetch all already existing resources,
   * and also start listening for the future ones to be created.
   */
  watchFrameResources({ watcherActorID, browserId, resourceTypes }) {
    return this.sendQuery("DevToolsFrameParent:watchResources", {
      watcherActorID,
      browserId,
      resourceTypes,
    });
  }

  unwatchFrameResources({ watcherActorID, browserId, resourceTypes }) {
    this.sendAsyncMessage("DevToolsFrameParent:unwatchResources", {
      watcherActorID,
      browserId,
      resourceTypes,
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
    const transport = new JsWindowActorTransport(this, forwardingPrefix);
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

  async _cleanupConnection(connection) {
    const { forwardingPrefix, transport } = this._connections.get(
      connection.prefix
    );

    connection.off("closed", this._onConnectionClosed);
    if (transport) {
      // If we have a child transport, the actor has already
      // been created. We need to stop using this transport.
      transport.close();
    }

    connection.cancelForwarding(forwardingPrefix);
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

    for (const { actor, connection, watcher } of this._connections.values()) {
      watcher.notifyTargetDestroyed(actor);

      // XXX: we should probably get rid of this
      if (actor && connection.transport) {
        // The FrameTargetActor within the child process doesn't necessary
        // have time to uninitialize itself when the frame is closed/killed.
        // So ensure telling the client that the related actor is detached.
        connection.send({ from: actor.actor, type: "tabDetached" });
      }

      this._cleanupConnection(connection);
    }
  }

  /**
   * Supported Queries
   */

  sendPacket(packet, prefix) {
    this.sendAsyncMessage("DevToolsFrameParent:packet", { packet, prefix });
  }

  /**
   * JsWindowActor API
   */

  async sendQuery(msg, args) {
    try {
      const res = await super.sendQuery(msg, args);
      return res;
    } catch (e) {
      console.error("Failed to sendQuery in DevToolsFrameParent", msg);
      console.error(e.toString());
      throw e;
    }
  }

  receiveMessage(message) {
    switch (message.name) {
      case "DevToolsFrameChild:connectFromContent":
        return this.connectFromContent(message.data);
      case "DevToolsFrameChild:packet":
        return this.emit("packet-received", message);
      default:
        throw new Error(
          "Unsupported message in DevToolsFrameParent: " + message.name
        );
    }
  }

  didDestroy() {
    this._destroy();
  }
}
