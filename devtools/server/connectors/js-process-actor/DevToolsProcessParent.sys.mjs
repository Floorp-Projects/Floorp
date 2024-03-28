/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { loader } from "resource://devtools/shared/loader/Loader.sys.mjs";
import { EventEmitter } from "resource://gre/modules/EventEmitter.sys.mjs";

const { ParentProcessWatcherRegistry } = ChromeUtils.importESModule(
  "resource://devtools/server/actors/watcher/ParentProcessWatcherRegistry.sys.mjs",
  // ParentProcessWatcherRegistry needs to be a true singleton and loads ActorManagerParent
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

    EventEmitter.decorate(this);
  }

  #destroyed = false;
  #connections = new Map();

  /**
   * Request the content process to create all the targets currently watched
   * and start observing for new ones to be created later.
   */
  watchTargets({ watcherActorID, targetType }) {
    return this.sendQuery("DevToolsProcessParent:watchTargets", {
      watcherActorID,
      targetType,
    });
  }

  /**
   * Request the content process to stop observing for currently watched targets
   * and destroy all the currently active ones.
   */
  unwatchTargets({ watcherActorID, targetType, options }) {
    this.sendAsyncMessage("DevToolsProcessParent:unwatchTargets", {
      watcherActorID,
      targetType,
      options,
    });
  }

  /**
   * Communicate to the content process that some data have been added or set.
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

  destroyWatcher({ watcherActorID }) {
    return this.sendAsyncMessage("DevToolsProcessParent:destroyWatcher", {
      watcherActorID,
    });
  }

  /**
   * Called when the content process notified us about a new target actor
   */
  #onTargetAvailable({ watcherActorID, forwardingPrefix, targetActorForm }) {
    const watcher = ParentProcessWatcherRegistry.getWatcher(watcherActorID);

    if (!watcher) {
      throw new Error(
        `Watcher Actor with ID '${watcherActorID}' can't be found.`
      );
    }
    const connection = watcher.conn;

    // If this is the first target actor for this watcher,
    // hook up the DevToolsServerConnection which will bridge
    // communication between the parent process DevToolsServer
    // and the content process.
    if (!this.#connections.get(watcher.conn.prefix)) {
      connection.on("closed", this.#onConnectionClosed);

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

      this.#connections.set(watcher.conn.prefix, {
        watcher,
        connection,
        // This prefix is the prefix of the DevToolsServerConnection, running
        // in the content process, for which we should forward packets to, based on its prefix.
        // While `watcher.connection` is also a DevToolsServerConnection, but from this process,
        // the parent process. It is the one receiving Client packets and the one, from which
        // we should forward packets from.
        forwardingPrefix,
        transport,
        targetActorForms: [],
      });
    }

    this.#connections
      .get(watcher.conn.prefix)
      .targetActorForms.push(targetActorForm);

    watcher.notifyTargetAvailable(targetActorForm);
  }

  /**
   * Called when the content process notified us about a target actor that has been destroyed.
   */
  #onTargetDestroyed({ actors, options }) {
    for (const { watcherActorID, targetActorForm } of actors) {
      const watcher = ParentProcessWatcherRegistry.getWatcher(watcherActorID);
      // As we instruct to destroy all targets when the watcher is destroyed,
      // we may easily receive the target destruction notification *after*
      // the watcher has been removed from the registry.
      if (!watcher || watcher.isDestroyed()) {
        continue;
      }
      watcher.notifyTargetDestroyed(targetActorForm, options);
      const connectionInfo = this.#connections.get(watcher.conn.prefix);
      if (connectionInfo) {
        const idx = connectionInfo.targetActorForms.findIndex(
          form => form.actor == targetActorForm.actor
        );
        if (idx != -1) {
          connectionInfo.targetActorForms.splice(idx, 1);
        }
        // Once the last active target is removed, disconnect the DevTools transport
        // and cleanup everything bound to this DOM Process. We will re-instantiate
        // a new connection/transport on the next reported target actor.
        if (!connectionInfo.targetActorForms.length) {
          this.#cleanupConnection(connectionInfo.connection);
        }
      }
    }
  }

  #onConnectionClosed = (status, prefix) => {
    if (this.#connections.has(prefix)) {
      const { connection } = this.#connections.get(prefix);
      this.#cleanupConnection(connection);
    }
  };

  /**
   * Close and unregister a given DevToolsServerConnection.
   *
   * @param {DevToolsServerConnection} connection
   * @param {object} options
   * @param {boolean} options.isModeSwitching
   *        true when this is called as the result of a change to the devtools.browsertoolbox.scope pref
   */
  async #cleanupConnection(connection, options = {}) {
    const watcherConnectionInfo = this.#connections.get(connection.prefix);
    if (watcherConnectionInfo) {
      const { forwardingPrefix, transport } = watcherConnectionInfo;
      if (transport) {
        // If we have a child transport, the actor has already
        // been created. We need to stop using this transport.
        transport.close(options);
      }
      // When cancelling the forwarding, one RDP event is sent to the client to purge all requests
      // and actors related to a given prefix.
      // Be careful that any late RDP event would be ignored by the client passed this call.
      connection.cancelForwarding(forwardingPrefix);
    }

    connection.off("closed", this.#onConnectionClosed);

    this.#connections.delete(connection.prefix);
    if (!this.#connections.size) {
      this.#destroy(options);
    }
  }

  /**
   * Destroy and cleanup everything for this DOM Process.
   *
   * @param {object} options
   * @param {boolean} options.isModeSwitching
   *        true when this is called as the result of a change to the devtools.browsertoolbox.scope pref
   */
  #destroy(options) {
    if (this.#destroyed) {
      return;
    }
    this.#destroyed = true;

    for (const {
      targetActorForms,
      connection,
      watcher,
    } of this.#connections.values()) {
      for (const actor of targetActorForms) {
        watcher.notifyTargetDestroyed(actor, options);
      }
      this.#cleanupConnection(connection, options);
    }
  }

  /**
   * Used by DevTools Transport to send packets to the content process.
   */

  sendPacket(packet, prefix) {
    this.sendAsyncMessage("DevToolsProcessParent:packet", { packet, prefix });
  }

  /**
   * JsProcessActor API
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

  /**
   * Called by the JSProcessActor API when the content process sent us a message
   */
  receiveMessage(message) {
    switch (message.name) {
      case "DevToolsProcessChild:targetAvailable":
        return this.#onTargetAvailable(message.data);
      case "DevToolsProcessChild:packet":
        return this.emit("packet-received", message);
      case "DevToolsProcessChild:targetDestroyed":
        return this.#onTargetDestroyed(message.data);
      case "DevToolsProcessChild:bf-cache-navigation-pageshow": {
        const browsingContext = BrowsingContext.get(
          message.data.browsingContextId
        );
        for (const watcherActor of ParentProcessWatcherRegistry.getWatchersForBrowserId(
          browsingContext.browserId
        )) {
          watcherActor.emit("bf-cache-navigation-pageshow", {
            windowGlobal: browsingContext.currentWindowGlobal,
          });
        }
        return null;
      }
      case "DevToolsProcessChild:bf-cache-navigation-pagehide": {
        const browsingContext = BrowsingContext.get(
          message.data.browsingContextId
        );
        for (const watcherActor of ParentProcessWatcherRegistry.getWatchersForBrowserId(
          browsingContext.browserId
        )) {
          watcherActor.emit("bf-cache-navigation-pagehide", {
            windowGlobal: browsingContext.currentWindowGlobal,
          });
        }
        return null;
      }
      default:
        throw new Error(
          "Unsupported message in DevToolsProcessParent: " + message.name
        );
    }
  }

  /**
   * Called by the JSProcessActor API when this content process is destroyed.
   */
  didDestroy() {
    this.#destroy();
  }
}

export class BrowserToolboxDevToolsProcessParent extends DevToolsProcessParent {}
