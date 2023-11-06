/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-env mozilla/process-script */

"use strict";

/**
 * Main entry point for DevTools in content processes.
 *
 * This module is loaded early when a content process is started.
 * Note that (at least) JS XPCOM registered at app-startup, will be running before.
 * It is used by the multiprocess browser toolbox in order to debug privileged resources.
 * When debugging a Web page loaded in a Tab, DevToolsFrame JS Window Actor is used instead
 * (DevToolsFrameParent.jsm and DevToolsFrameChild.jsm).
 *
 * This module won't do anything unless DevTools codebase starts adding some data
 * in `Services.cpmm.sharedData` object or send a message manager message via `Services.cpmm`.
 * Also, this module is only loaded, on-demand from process-helper if devtools are watching for process targets.
 */

const SHARED_DATA_KEY_NAME = "DevTools:watchedPerWatcher";

class ContentProcessStartup {
  constructor() {
    // The map is indexed by the Watcher Actor ID.
    // The values are objects containing the following properties:
    // - connection: the DevToolsServerConnection itself
    // - actor: the ContentProcessTargetActor instance
    this._connections = new Map();

    this.observe = this.observe.bind(this);
    this.receiveMessage = this.receiveMessage.bind(this);

    this.addListeners();
    this.maybeCreateExistingTargetActors();
  }

  observe(subject, topic, data) {
    switch (topic) {
      case "xpcom-shutdown": {
        this.destroy();
        break;
      }
    }
  }

  destroy(options) {
    this.removeListeners();

    for (const [, connectionInfo] of this._connections) {
      connectionInfo.connection.close(options);
    }
    this._connections.clear();
  }

  addListeners() {
    Services.obs.addObserver(this.observe, "xpcom-shutdown");

    Services.cpmm.addMessageListener(
      "debug:instantiate-already-available",
      this.receiveMessage
    );
    Services.cpmm.addMessageListener(
      "debug:destroy-target",
      this.receiveMessage
    );
    Services.cpmm.addMessageListener(
      "debug:add-or-set-session-data-entry",
      this.receiveMessage
    );
    Services.cpmm.addMessageListener(
      "debug:remove-session-data-entry",
      this.receiveMessage
    );
    Services.cpmm.addMessageListener(
      "debug:destroy-process-script",
      this.receiveMessage
    );
  }

  removeListeners() {
    Services.obs.removeObserver(this.observe, "xpcom-shutdown");

    Services.cpmm.removeMessageListener(
      "debug:instantiate-already-available",
      this.receiveMessage
    );
    Services.cpmm.removeMessageListener(
      "debug:destroy-target",
      this.receiveMessage
    );
    Services.cpmm.removeMessageListener(
      "debug:add-or-set-session-data-entry",
      this.receiveMessage
    );
    Services.cpmm.removeMessageListener(
      "debug:remove-session-data-entry",
      this.receiveMessage
    );
    Services.cpmm.removeMessageListener(
      "debug:destroy-process-script",
      this.receiveMessage
    );
  }

  receiveMessage(msg) {
    switch (msg.name) {
      case "debug:instantiate-already-available":
        this.createTargetActor(
          msg.data.watcherActorID,
          msg.data.connectionPrefix,
          msg.data.sessionData,
          true
        );
        break;
      case "debug:destroy-target":
        this.destroyTarget(msg.data.watcherActorID);
        break;
      case "debug:add-or-set-session-data-entry":
        this.addOrSetSessionDataEntry(
          msg.data.watcherActorID,
          msg.data.type,
          msg.data.entries,
          msg.data.updateType
        );
        break;
      case "debug:remove-session-data-entry":
        this.removeSessionDataEntry(
          msg.data.watcherActorID,
          msg.data.type,
          msg.data.entries
        );
        break;
      case "debug:destroy-process-script":
        this.destroy(msg.data.options);
        break;
      default:
        throw new Error(`Unsupported message name ${msg.name}`);
    }
  }

  /**
   * Called when the content process just started.
   * This will start creating ContentProcessTarget actors, but only if DevTools code (WatcherActor / WatcherRegistry.jsm)
   * put some data in `sharedData` telling us to do so.
   */
  maybeCreateExistingTargetActors() {
    const { sharedData } = Services.cpmm;

    // Accessing `sharedData` right off the app-startup returns null.
    // Spinning the event loop with dispatchToMainThread seems enough,
    // but it means that we let some more Javascript code run before
    // instantiating the target actor.
    // So we may miss a few resources and will register the breakpoints late.
    if (!sharedData) {
      Services.tm.dispatchToMainThread(
        this.maybeCreateExistingTargetActors.bind(this)
      );
      return;
    }

    const sessionDataByWatcherActor = sharedData.get(SHARED_DATA_KEY_NAME);
    if (!sessionDataByWatcherActor) {
      return;
    }

    // Create one Target actor for each prefix/client which listen to process
    for (const [watcherActorID, sessionData] of sessionDataByWatcherActor) {
      const { connectionPrefix, targets } = sessionData;
      // This is where we only do something significant only if DevTools are opened
      // and requesting to create target actor for content processes
      if (targets?.includes("process")) {
        this.createTargetActor(watcherActorID, connectionPrefix, sessionData);
      }
    }
  }

  /**
   * Instantiate a new ContentProcessTarget for the given connection.
   * This is where we start doing some significant computation that only occurs when DevTools are opened.
   *
   * @param String watcherActorID
   *        The ID of the WatcherActor who requested to observe and create these target actors.
   * @param String parentConnectionPrefix
   *        The prefix of the DevToolsServerConnection of the Watcher Actor.
   *        This is used to compute a unique ID for the target actor.
   * @param Object sessionData
   *        All data managed by the Watcher Actor and WatcherRegistry.jsm, containing
   *        target types, resources types to be listened as well as breakpoints and any
   *        other data meant to be shared across processes and threads.
   * @param Object options Dictionary with optional values:
   * @param Boolean options.ignoreAlreadyCreated
   *        If true, do not throw if the target actor has already been created.
   */
  createTargetActor(
    watcherActorID,
    parentConnectionPrefix,
    sessionData,
    ignoreAlreadyCreated = false
  ) {
    if (this._connections.get(watcherActorID)) {
      if (ignoreAlreadyCreated) {
        return;
      }
      throw new Error(
        "ContentProcessStartup createTargetActor was called more than once" +
          ` for the Watcher Actor (ID: "${watcherActorID}")`
      );
    }
    // Compute a unique prefix, just for this content process,
    // which will be used to create a ChildDebuggerTransport pair between content and parent processes.
    // This is slightly hacky as we typicaly compute Prefix and Actor ID via `DevToolsServerConnection.allocID()`,
    // but here, we can't have access to any DevTools connection as we are really early in the content process startup
    const prefix =
      parentConnectionPrefix + "contentProcess" + Services.appinfo.processID;
    //TODO: probably merge content-process.jsm with this module
    const { initContentProcessTarget } = ChromeUtils.importESModule(
      "resource://devtools/server/startup/content-process.sys.mjs"
    );
    const { actor, connection } = initContentProcessTarget({
      target: Services.cpmm,
      data: {
        watcherActorID,
        parentConnectionPrefix,
        prefix,
        sessionContext: sessionData.sessionContext,
      },
    });
    this._connections.set(watcherActorID, {
      actor,
      connection,
    });

    // Pass initialization data to the target actor
    for (const type in sessionData) {
      actor.addOrSetSessionDataEntry(type, sessionData[type], false, "set");
    }
  }

  destroyTarget(watcherActorID) {
    const connectionInfo = this._connections.get(watcherActorID);
    // This connection has already been cleaned?
    if (!connectionInfo) {
      throw new Error(
        `Trying to destroy a content process target actor that doesn't exists, or has already been destroyed. Watcher Actor ID:${watcherActorID}`
      );
    }
    connectionInfo.connection.close();
    this._connections.delete(watcherActorID);
  }

  async addOrSetSessionDataEntry(watcherActorID, type, entries, updateType) {
    const connectionInfo = this._connections.get(watcherActorID);
    if (!connectionInfo) {
      throw new Error(
        `No content process target actor for this Watcher Actor ID:"${watcherActorID}"`
      );
    }
    const { actor } = connectionInfo;
    await actor.addOrSetSessionDataEntry(type, entries, false, updateType);
    Services.cpmm.sendAsyncMessage("debug:add-or-set-session-data-entry-done", {
      watcherActorID,
    });
  }

  removeSessionDataEntry(watcherActorID, type, entries) {
    const connectionInfo = this._connections.get(watcherActorID);
    if (!connectionInfo) {
      return;
    }
    const { actor } = connectionInfo;
    actor.removeSessionDataEntry(type, entries);
  }
}

// Only start this component for content processes.
// i.e. explicitely avoid running it for the parent process
if (Services.appinfo.processType == Services.appinfo.PROCESS_TYPE_CONTENT) {
  new ContentProcessStartup();
}
