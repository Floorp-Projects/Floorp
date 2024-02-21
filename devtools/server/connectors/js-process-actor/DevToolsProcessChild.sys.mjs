/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { EventEmitter } from "resource://gre/modules/EventEmitter.sys.mjs";

const lazy = {};
ChromeUtils.defineESModuleGetters(
  lazy,
  {
    releaseDistinctSystemPrincipalLoader:
      "resource://devtools/shared/loader/DistinctSystemPrincipalLoader.sys.mjs",
    useDistinctSystemPrincipalLoader:
      "resource://devtools/shared/loader/DistinctSystemPrincipalLoader.sys.mjs",
  },
  { global: "contextual" }
);

// Name of the attribute into which we save data in `sharedData` object.
const SHARED_DATA_KEY_NAME = "DevTools:watchedPerWatcher";

// If true, log info about DOMProcess's being created.
const DEBUG = false;

/**
 * Print information about operation being done against each content process.
 *
 * @param {nsIDOMProcessChild} domProcessChild
 *        The process for which we should log a message.
 * @param {String} message
 *        Message to log.
 */
function logDOMProcess(domProcessChild, message) {
  if (!DEBUG) {
    return;
  }
  dump(" [pid:" + domProcessChild + "] " + message + "\n");
}

export class DevToolsProcessChild extends JSProcessActorChild {
  constructor() {
    super();

    // The map is indexed by the Watcher Actor ID.
    // The values are objects containing the following properties:
    // - connection: the DevToolsServerConnection itself
    // - actor: the ContentProcessTargetActor instance
    this._connections = new Map();

    this._onConnectionChange = this._onConnectionChange.bind(this);
    EventEmitter.decorate(this);
  }

  instantiate() {
    const { sharedData } = Services.cpmm;
    const watchedDataByWatcherActor = sharedData.get(SHARED_DATA_KEY_NAME);
    if (!watchedDataByWatcherActor) {
      throw new Error(
        "Request to instantiate the target(s) for the process, but `sharedData` is empty about watched targets"
      );
    }

    // Create one Target actor for each prefix/client which listen to processes
    for (const [watcherActorID, sessionData] of watchedDataByWatcherActor) {
      const { connectionPrefix } = sessionData;

      if (sessionData.targets?.includes("process")) {
        this._createTargetActor(watcherActorID, connectionPrefix, sessionData);
      }
    }
  }

  /**
   * Instantiate a new ProcessTarget for the given connection.
   *
   * @param String watcherActorID
   *        The ID of the WatcherActor who requested to observe and create these target actors.
   * @param String parentConnectionPrefix
   *        The prefix of the DevToolsServerConnection of the Watcher Actor.
   *        This is used to compute a unique ID for the target actor.
   * @param Object sessionData
   *        All data managed by the Watcher Actor and WatcherRegistry.sys.mjs, containing
   *        target types, resources types to be listened as well as breakpoints and any
   *        other data meant to be shared across processes and threads.
   */
  _createTargetActor(watcherActorID, parentConnectionPrefix, sessionData) {
    // This method will be concurrently called from `observe()` and `DevToolsProcessParent:instantiate-already-available`
    // When the JSprocessActor initializes itself and when the watcher want to force instantiating existing targets.
    // Simply ignore the second call as there is nothing to return, neither to wait for as this method is synchronous.
    if (this._connections.has(watcherActorID)) {
      return;
    }

    // Compute a unique prefix, just for this DOM Process,
    // which will be used to create a JSWindowActorTransport pair between content and parent processes.
    // This is slightly hacky as we typicaly compute Prefix and Actor ID via `DevToolsServerConnection.allocID()`,
    // but here, we can't have access to any DevTools connection as we are really early in the content process startup
    // XXX: nsIDOMProcessChild's childID should be unique across processes, I think. So that should be safe?
    // (this.manager == nsIDOMProcessChild interface)
    // Ensure appending a final slash, otherwise the prefix may be the same between childID 1 and 10...
    const forwardingPrefix =
      parentConnectionPrefix + "contentProcess" + this.manager.childID + "/";

    logDOMProcess(
      this.manager,
      "Instantiate ContentProcessTarget with prefix: " + forwardingPrefix
    );

    const { connection, targetActor } = this._createConnectionAndActor(
      watcherActorID,
      forwardingPrefix,
      sessionData
    );
    this._connections.set(watcherActorID, {
      connection,
      actor: targetActor,
    });

    // Immediately queue a message for the parent process,
    // in order to ensure that the JSWindowActorTransport is instantiated
    // before any packet is sent from the content process.
    // As the order of messages is guaranteed to be delivered in the order they
    // were queued, we don't have to wait for anything around this sendAsyncMessage call.
    // In theory, the ContentProcessTargetActor may emit events in its constructor.
    // If it does, such RDP packets may be lost. But in practice, no events
    // are emitted during its construction. Instead the frontend will start
    // the communication first.
    this.sendAsyncMessage("DevToolsProcessChild:connectFromContent", {
      watcherActorID,
      forwardingPrefix,
      actor: targetActor.form(),
    });

    // Pass initialization data to the target actor
    for (const type in sessionData) {
      // `sessionData` will also contain `browserId` as well as entries with empty arrays,
      // which shouldn't be processed.
      const entries = sessionData[type];
      if (!Array.isArray(entries) || !entries.length) {
        continue;
      }
      targetActor.addOrSetSessionDataEntry(
        type,
        sessionData[type],
        false,
        "set"
      );
    }
  }

  _destroyTargetActor(watcherActorID, isModeSwitching) {
    const connectionInfo = this._connections.get(watcherActorID);
    // This connection has already been cleaned?
    if (!connectionInfo) {
      throw new Error(
        `Trying to destroy a target actor that doesn't exists, or has already been destroyed. Watcher Actor ID:${watcherActorID}`
      );
    }
    connectionInfo.connection.close({ isModeSwitching });
    this._connections.delete(watcherActorID);
    if (this._connections.size == 0) {
      this.didDestroy({ isModeSwitching });
    }
  }

  _createConnectionAndActor(watcherActorID, forwardingPrefix, sessionData) {
    if (!this.loader) {
      this.loader = lazy.useDistinctSystemPrincipalLoader(this);
    }
    const { DevToolsServer } = this.loader.require(
      "devtools/server/devtools-server"
    );

    const { ContentProcessTargetActor } = this.loader.require(
      "devtools/server/actors/targets/content-process"
    );

    DevToolsServer.init();

    // For browser content toolbox, we do need a regular root actor and all tab
    // actors, but don't need all the "browser actors" that are only useful when
    // debugging the parent process via the browser toolbox.
    DevToolsServer.registerActors({ target: true });
    DevToolsServer.on("connectionchange", this._onConnectionChange);

    const connection = DevToolsServer.connectToParentWindowActor(
      this,
      forwardingPrefix,
      "DevToolsProcessChild:packet"
    );

    // Create the actual target actor.
    const targetActor = new ContentProcessTargetActor(connection, {
      sessionContext: sessionData.sessionContext,
    });
    // There is no root actor in content processes and so
    // the target actor can't be managed by it, but we do have to manage
    // the actor to have it working and be registered in the DevToolsServerConnection.
    // We make it manage itself and become a top level actor.
    targetActor.manage(targetActor);

    const form = targetActor.form();
    targetActor.once("destroyed", options => {
      // This will destroy the content process one
      this._destroyTargetActor(watcherActorID, options.isModeSwitching);
      // And this will destroy the parent process one
      try {
        this.sendAsyncMessage("DevToolsProcessChild:destroy", {
          actors: [
            {
              watcherActorID,
              form,
            },
          ],
          options,
        });
      } catch (e) {
        // Ignore exception when the JSProcessActorChild has already been destroyed.
        // We often try to emit this message while the process is being destroyed,
        // but sendAsyncMessage doesn't have time to complete and throws.
        if (
          !e.message.includes("JSProcessActorChild cannot send at the moment")
        ) {
          throw e;
        }
      }
    });

    return { connection, targetActor };
  }

  /**
   * Destroy the server once its last connection closes. Note that multiple
   * frame scripts may be running in parallel and reuse the same server.
   */
  _onConnectionChange() {
    if (this._destroyed) {
      return;
    }
    this._destroyed = true;

    const { DevToolsServer } = this.loader.require(
      "devtools/server/devtools-server"
    );

    // Only destroy the server if there is no more connections to it. It may be
    // used to debug another tab running in the same process.
    if (DevToolsServer.hasConnection() || DevToolsServer.keepAlive) {
      return;
    }

    DevToolsServer.off("connectionchange", this._onConnectionChange);
    DevToolsServer.destroy();
  }

  /**
   * Supported Queries
   */

  sendPacket(packet, prefix) {
    this.sendAsyncMessage("DevToolsProcessChild:packet", { packet, prefix });
  }

  /**
   * JsWindowActor API
   */

  async sendQuery(msg, args) {
    try {
      const res = await super.sendQuery(msg, args);
      return res;
    } catch (e) {
      console.error("Failed to sendQuery in DevToolsProcessChild", msg);
      console.error(e.toString());
      throw e;
    }
  }

  receiveMessage(message) {
    switch (message.name) {
      case "DevToolsProcessParent:instantiate-already-available": {
        const { watcherActorID, connectionPrefix, sessionData } = message.data;
        return this._createTargetActor(
          watcherActorID,
          connectionPrefix,
          sessionData
        );
      }
      case "DevToolsProcessParent:destroy": {
        const { watcherActorID, isModeSwitching } = message.data;
        return this._destroyTargetActor(watcherActorID, isModeSwitching);
      }
      case "DevToolsProcessParent:addOrSetSessionDataEntry": {
        const { watcherActorID, type, entries, updateType } = message.data;
        return this._addOrSetSessionDataEntry(
          watcherActorID,
          type,
          entries,
          updateType
        );
      }
      case "DevToolsProcessParent:removeSessionDataEntry": {
        const { watcherActorID, type, entries } = message.data;
        return this._removeSessionDataEntry(watcherActorID, type, entries);
      }
      case "DevToolsProcessParent:packet":
        return this.emit("packet-received", message);
      default:
        throw new Error(
          "Unsupported message in DevToolsProcessParent: " + message.name
        );
    }
  }

  _getTargetActorForWatcherActorID(watcherActorID) {
    const connectionInfo = this._connections.get(watcherActorID);
    return connectionInfo?.actor;
  }

  _addOrSetSessionDataEntry(watcherActorID, type, entries, updateType) {
    const targetActor = this._getTargetActorForWatcherActorID(watcherActorID);
    if (!targetActor) {
      throw new Error(
        `No target actor for this Watcher Actor ID:"${watcherActorID}"`
      );
    }
    return targetActor.addOrSetSessionDataEntry(
      type,
      entries,
      false,
      updateType
    );
  }

  _removeSessionDataEntry(watcherActorID, type, entries) {
    const targetActor = this._getTargetActorForWatcherActorID(watcherActorID);
    // By the time we are calling this, the target may already have been destroyed.
    if (!targetActor) {
      return null;
    }
    return targetActor.removeSessionDataEntry(type, entries);
  }

  observe(subject, topic) {
    if (topic === "init-devtools-content-process-actor") {
      // This is triggered by the process actor registration and some code in process-helper.js
      // which defines a unique topic to be observed
      this.instantiate();
    }
  }

  didDestroy(options) {
    for (const { connection } of this._connections.values()) {
      connection.close(options);
    }
    this._connections.clear();
    if (this.loader) {
      lazy.releaseDistinctSystemPrincipalLoader(this);
      this.loader = null;
    }
  }
}
