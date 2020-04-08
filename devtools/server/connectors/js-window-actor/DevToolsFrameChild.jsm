/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["DevToolsFrameChild"];

const { EventEmitter } = ChromeUtils.import(
  "resource://gre/modules/EventEmitter.jsm"
);
const Loader = ChromeUtils.import("resource://devtools/shared/Loader.jsm");

class DevToolsFrameChild extends JSWindowActorChild {
  constructor() {
    super();

    // The map is indexed by the connection prefix.
    // The values are objects containing the following properties:
    // - connection: the DevToolsServerConnection itself
    // - actor: the FrameTargetActor instance
    this._connections = new Map();

    this._onConnectionChange = this._onConnectionChange.bind(this);
    EventEmitter.decorate(this);
  }

  connect(msg) {
    this.useCustomLoader = this.document.nodePrincipal.isSystemPrincipal;

    // When debugging chrome pages, use a new dedicated loader.
    this.loader = this.useCustomLoader
      ? new Loader.DevToolsLoader({
          invisibleToDebugger: true,
        })
      : Loader;

    const { prefix } = msg.data;
    if (this._connections.get(prefix)) {
      throw new Error(
        "DevToolsFrameChild connect was called more than once" +
          ` for the same connection (prefix: "${prefix}")`
      );
    }

    const { connection, targetActor } = this._createConnectionAndActor(prefix);
    this._connections.set(prefix, { connection, actor: targetActor });

    const { actor } = this._connections.get(prefix);
    return { actor: actor.form() };
  }

  _createConnectionAndActor(prefix) {
    const { DevToolsServer } = this.loader.require(
      "devtools/server/devtools-server"
    );
    const { ActorPool } = this.loader.require("devtools/server/actors/common");
    const { FrameTargetActor } = this.loader.require(
      "devtools/server/actors/targets/frame"
    );

    DevToolsServer.init();

    // We want a special server without any root actor and only target-scoped actors.
    // We are going to spawn a FrameTargetActor instance in the next few lines,
    // it is going to act like a root actor without being one.
    DevToolsServer.registerActors({ target: true });
    DevToolsServer.on("connectionchange", this._onConnectionChange);

    const connection = DevToolsServer.connectToParentWindowActor(prefix, this);

    // Create the actual target actor.
    const targetActor = new FrameTargetActor(connection, this.docShell);

    // Add the newly created actor to the connection pool.
    const actorPool = new ActorPool(connection, "frame-child");
    actorPool.addActor(targetActor);
    connection.addActorPool(actorPool);

    return { connection, targetActor };
  }

  /**
   * Destroy the server once its last connection closes. Note that multiple
   * frame scripts may be running in parallel and reuse the same server.
   */
  _onConnectionChange() {
    const { DevToolsServer } = this.loader.require(
      "devtools/server/devtools-server"
    );

    // Only destroy the server if there is no more connections to it. It may be
    // used to debug another tab running in the same process.
    if (DevToolsServer.hasConnection() || DevToolsServer.keepAlive) {
      return;
    }

    if (this._destroyed) {
      return;
    }
    this._destroyed = true;

    DevToolsServer.off("connectionchange", this._onConnectionChange);
    DevToolsServer.destroy();
  }

  disconnect(msg) {
    const { prefix } = msg.data;
    const connectionInfo = this._connections.get(prefix);
    if (!connectionInfo) {
      console.error(
        "No connection available in DevToolsFrameChild::disconnect"
      );
      return;
    }

    // Call DevToolsServerConnection.close to destroy all child actors. It
    // should end up calling DevToolsServerConnection.onClosed that would
    // actually cleanup all actor pools.
    connectionInfo.connection.close();
    this._connections.delete(prefix);
  }

  /**
   * Supported Queries
   */

  async sendPacket(packet, prefix) {
    return this.sendQuery("DevToolsFrameChild:packet", { packet, prefix });
  }

  /**
   * JsWindowActor API
   */

  async sendQuery(msg, args) {
    try {
      const res = await super.sendQuery(msg, args);
      return res;
    } catch (e) {
      console.error("Failed to sendQuery in DevToolsFrameChild", msg);
      console.error(e.toString());
      throw e;
    }
  }

  receiveMessage(data) {
    switch (data.name) {
      case "DevToolsFrameParent:connect":
        return this.connect(data);
      case "DevToolsFrameParent:disconnect":
        return this.disconnect(data);
      case "DevToolsFrameParent:packet":
        return this.emit("packet-received", data);
      default:
        throw new Error(
          "Unsupported message in DevToolsFrameParent: " + data.name
        );
    }
  }

  didDestroy() {
    for (const [, connectionInfo] of this._connections) {
      connectionInfo.connection.close();
    }
    if (this.useCustomLoader) {
      this.loader.destroy();
    }
  }
}
