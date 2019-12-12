/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["DevToolsFrameParent"];
const { loader } = ChromeUtils.import("resource://devtools/shared/Loader.jsm");

loader.lazyRequireGetter(
  this,
  "JsWindowActorTransport",
  "devtools/shared/transport/js-window-actor-transport",
  true
);

const { EventEmitter } = ChromeUtils.import(
  "resource://gre/modules/EventEmitter.jsm"
);

class DevToolsFrameParent extends JSWindowActorParent {
  constructor() {
    super();

    this._destroyed = false;

    // Map of DebuggerServerConnection's used to forward the messages from/to
    // the client. The connections run in the parent process, as this code. We
    // may have more than one when there is more than one client debugging the
    // same frame. For example, a content toolbox and the browser toolbox.
    //
    // The map is indexed by the connection prefix.
    // The values are objects containing the following properties:
    // - actor: the frame target actor(as a form)
    // - connection: the DebuggerServerConnection used to communicate with the
    //   frame target actor
    // - forwardingPrefix: the forwarding prefix used by the connection to know
    //   how to forward packets to the frame target
    // - transport: the JsWindowActorTransport
    //
    // Reminder about prefixes: all DebuggerServerConnections have a `prefix`
    // which can be considered as a kind of id. On top of this, parent process
    // DebuggerServerConnections also have forwarding prefixes because they are
    // responsible for forwarding messages to content process connections.
    this._connections = new Map();

    this._onConnectionClosed = this._onConnectionClosed.bind(this);
    EventEmitter.decorate(this);
  }

  async connectToFrame(connection) {
    // Compute the same prefix that's used by DebuggerServerConnection when
    // forwarding packets to the target frame.
    const forwardingPrefix = connection.allocID("child");

    try {
      const { actor } = await this.connect({ prefix: forwardingPrefix });
      connection.on("closed", this._onConnectionClosed);

      // Create a js-window-actor based transport.
      const transport = new JsWindowActorTransport(this, forwardingPrefix);
      transport.hooks = {
        onPacket: connection.send.bind(connection),
        onClosed() {},
      };
      transport.ready();

      connection.setForwarding(forwardingPrefix, transport);

      this._connections.set(connection.prefix, {
        actor,
        connection,
        forwardingPrefix,
        transport,
      });

      return actor;
    } catch (e) {
      // Might fail if we have an actor destruction.
      console.error("Failed to connect to DevToolsFrameChild actor");
      console.error(e.toString());
      return null;
    }
  }

  _onConnectionClosed(prefix) {
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

    // Notify the child process to clean the target-scoped actors.
    try {
      // Bug 1169643: Ignore any exception as the child process
      // may already be destroyed by now.
      await this.disconnect({ prefix: forwardingPrefix });
    } catch (e) {
      // Nothing to do
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

    for (const { actor, connection } of this._connections.values()) {
      if (actor) {
        // The FrameTargetActor within the child process doesn't necessary
        // have time to uninitialize itself when the frame is closed/killed.
        // So ensure telling the client that the related actor is detached.
        connection.send({ from: actor.actor, type: "tabDetached" });
      }

      this._cleanupConnection(connection);
    }

    this.emit("devtools-frame-parent-actor-destroyed");
  }

  /**
   * Supported Queries
   */

  async connect(args) {
    return this.sendQuery("DevToolsFrameParent:connect", args);
  }

  async disconnect(args) {
    return this.sendQuery("DevToolsFrameParent:disconnect", args);
  }

  async sendPacket(packet, prefix) {
    return this.sendQuery("DevToolsFrameParent:packet", { packet, prefix });
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

  receiveMessage(data) {
    switch (data.name) {
      case "DevToolsFrameChild:packet":
        return this.emit("packet-received", data);
      default:
        throw new Error(
          "Unsupported message in DevToolsFrameParent: " + data.name
        );
    }
  }

  didDestroy() {
    this._destroy();
  }
}
