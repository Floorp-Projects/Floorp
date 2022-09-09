/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const { loader } = ChromeUtils.import(
  "resource://devtools/shared/loader/Loader.jsm"
);
const { EventEmitter } = ChromeUtils.import(
  "resource://gre/modules/EventEmitter.jsm"
);
import { WatcherRegistry } from "resource://devtools/server/actors/watcher/WatcherRegistry.sys.mjs";

const lazy = {};

loader.lazyRequireGetter(
  lazy,
  "JsWindowActorTransport",
  "devtools/shared/transport/js-window-actor-transport",
  true
);

export class DevToolsFrameParent extends JSWindowActorParent {
  constructor() {
    super();

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
  async instantiateTarget({
    watcherActorID,
    connectionPrefix,
    sessionContext,
    sessionData,
  }) {
    await this.sendQuery("DevToolsFrameParent:instantiate-already-available", {
      watcherActorID,
      connectionPrefix,
      sessionContext,
      sessionData,
    });
  }

  /**
   * @param {object} arg
   * @param {object} arg.sessionContext
   * @param {object} arg.options
   * @param {boolean} arg.options.isModeSwitching
   *        true when this is called as the result of a change to the devtools.browsertoolbox.scope pref
   */
  destroyTarget({ watcherActorID, sessionContext, options }) {
    this.sendAsyncMessage("DevToolsFrameParent:destroy", {
      watcherActorID,
      sessionContext,
      options,
    });
  }

  /**
   * Communicate to the content process that some data have been added.
   */
  async addSessionDataEntry({ watcherActorID, sessionContext, type, entries }) {
    try {
      await this.sendQuery("DevToolsFrameParent:addSessionDataEntry", {
        watcherActorID,
        sessionContext,
        type,
        entries,
      });
    } catch (e) {
      console.warn(
        "Failed to add session data entry for frame targets in browsing context",
        this.browsingContext.id
      );
      console.warn(e);
    }
  }

  /**
   * Communicate to the content process that some data have been removed.
   */
  removeSessionDataEntry({ watcherActorID, sessionContext, type, entries }) {
    this.sendAsyncMessage("DevToolsFrameParent:removeSessionDataEntry", {
      watcherActorID,
      sessionContext,
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
    const transport = new lazy.JsWindowActorTransport(this, forwardingPrefix);
    transport.hooks = {
      onPacket: connection.send.bind(connection),
      onTransportClosed() {},
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

  _onConnectionClosed(status, connectionPrefix) {
    this._unregisterWatcher(connectionPrefix);
  }

  /**
   * Given a watcher connection prefix, unregister everything related to the Watcher
   * in this JSWindowActor.
   *
   * @param {String} connectionPrefix
   *        The connection prefix of the watcher to unregister
   */
  async _unregisterWatcher(connectionPrefix) {
    const connectionInfo = this._connections.get(connectionPrefix);
    if (!connectionInfo) {
      return;
    }
    const { forwardingPrefix, transport, connection } = connectionInfo;
    this._connections.delete(connectionPrefix);

    connection.off("closed", this._onConnectionClosed);
    if (transport) {
      // If we have a child transport, the actor has already
      // been created. We need to stop using this transport.
      transport.close();
    }

    connection.cancelForwarding(forwardingPrefix);
  }

  /**
   * Destroy everything that we did related to the current WindowGlobal that
   * this JSWindow Actor represents:
   *  - close all transports that were used as bridge to communicate with the
   *    DevToolsFrameChild, running in the content process
   *  - unregister these transports from DevToolsServer (cancelForwarding)
   *  - notify the client, via the WatcherActor that all related targets,
   *    one per client/connection are all destroyed
   *
   * Note that with bfcacheInParent, we may reuse a JSWindowActor pair after closing all connections.
   * This is can happen outside of the destruction of the actor.
   * We may reuse a DevToolsFrameParent and DevToolsFrameChild pair.
   * When navigating away, we will destroy them and call this method.
   * Then when navigating back, we will reuse the same instances.
   * So that we should be careful to keep the class fully function and only clear all its state.
   *
   * @param {object} options
   * @param {boolean} options.isModeSwitching
   *        true when this is called as the result of a change to the devtools.browsertoolbox.scope pref
   */
  _closeAllConnections(options) {
    for (const { actor, watcher } of this._connections.values()) {
      watcher.notifyTargetDestroyed(actor, options);
      this._unregisterWatcher(watcher.conn.prefix);
    }
    this._connections.clear();
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

  receiveMessage(message) {
    switch (message.name) {
      case "DevToolsFrameChild:connectFromContent":
        return this.connectFromContent(message.data);
      case "DevToolsFrameChild:packet":
        return this.emit("packet-received", message);
      case "DevToolsFrameChild:destroy":
        for (const { form, watcherActorID } of message.data.actors) {
          const watcher = WatcherRegistry.getWatcher(watcherActorID);
          // As we instruct to destroy all targets when the watcher is destroyed,
          // we may easily receive the target destruction notification *after*
          // the watcher has been removed from the registry.
          if (watcher) {
            watcher.notifyTargetDestroyed(form, message.data.options);
            this._unregisterWatcher(watcher.conn.prefix);
          }
        }
        return null;
      case "DevToolsFrameChild:bf-cache-navigation-pageshow":
        for (const watcherActor of WatcherRegistry.getWatchersForBrowserId(
          this.browsingContext.browserId
        )) {
          watcherActor.emit("bf-cache-navigation-pageshow", {
            windowGlobal: this.browsingContext.currentWindowGlobal,
          });
        }
        return null;
      case "DevToolsFrameChild:bf-cache-navigation-pagehide":
        for (const watcherActor of WatcherRegistry.getWatchersForBrowserId(
          this.browsingContext.browserId
        )) {
          watcherActor.emit("bf-cache-navigation-pagehide", {
            windowGlobal: this.browsingContext.currentWindowGlobal,
          });
        }
        return null;
      default:
        throw new Error(
          "Unsupported message in DevToolsFrameParent: " + message.name
        );
    }
  }

  didDestroy() {
    this._closeAllConnections();
  }
}
