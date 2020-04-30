/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["DevToolsFrameChild"];

const { EventEmitter } = ChromeUtils.import(
  "resource://gre/modules/EventEmitter.jsm"
);
const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
const Loader = ChromeUtils.import("resource://devtools/shared/Loader.jsm");

// If true, log info about WindowGlobal's being created.
const DEBUG = false;

/**
 * Helper function to know if a given WindowGlobal should be exposed via watchTargets(window-global) API
 */
function shouldNotifyWindowGlobal(windowGlobal, watchedBrowsingContextID) {
  const browsingContext = windowGlobal.browsingContext;

  // Ignore about:blank loads, which spawn a document that never finishes loading
  // and would require somewhat useless Target and all its related overload.
  const window = Services.wm.getCurrentInnerWindowWithId(
    windowGlobal.innerWindowId
  );
  if (!window.docShell.hasLoadedNonBlankURI) {
    return false;
  }

  // If we are focusing only on a sub-tree of BrowsingContext,
  // Ignore the out of the sub tree elements.
  if (
    watchedBrowsingContextID &&
    browsingContext.top.id != watchedBrowsingContextID
  ) {
    return false;
  }

  // For now, we only mention the "remote frames".
  // i.e. the frames which are in a distinct process compared to their parent document
  // If there is no parent, this is most likely the top level document.
  // Ignore it only if this is the top level target we are watching.
  // For now we don't expect a target to be created, but we will as TabDescriptors arise.
  if (
    !browsingContext.parent &&
    browsingContext.id == watchedBrowsingContextID
  ) {
    return false;
  }

  // `isInProcess` is always false, even if the window runs in the same process.
  // `osPid` attribute is not set on WindowGlobalChild
  // so it is hard to guess if the given WindowGlobal runs in this process or not,
  // which is what we want to know here. Here is a workaround way to know it :/
  // ---
  // Also. It might be a bit surprising to have a DevToolsFrameChild/JSWindowActorChild
  // to be instantiated for WindowGlobals that aren't from this process... Is that expected?
  if (Cu.isRemoteProxy(windowGlobal.window)) {
    return false;
  }

  // When Fission is turned off, we still process here the iframes that are running in the
  // same process.
  // As we can't use isInProcess, nor osPid (see previous block), we have
  // to fallback to other checks. Here we check if we are able to access the parent document's window.
  // If we can, it means that it runs in the same process as the current iframe we are processing.
  if (
    browsingContext.parent &&
    browsingContext.parent.window &&
    !Cu.isRemoteProxy(browsingContext.parent.window)
  ) {
    return false;
  }

  return true;
}

function logWindowGlobal(windowGlobal, message) {
  if (!DEBUG) {
    return;
  }
  const browsingContext = windowGlobal.browsingContext;
  dump(
    message +
      " | BrowsingContext.id: " +
      browsingContext.id +
      " Inner Window ID: " +
      windowGlobal.innerWindowId +
      " pid:" +
      windowGlobal.osPid +
      " isClosed:" +
      windowGlobal.isClosed +
      " isInProcess:" +
      windowGlobal.isInProcess +
      " isCurrentGlobal:" +
      windowGlobal.isCurrentGlobal +
      " currentRemoteType:" +
      browsingContext.currentRemoteType +
      " hasParent:" +
      (browsingContext.parent ? browsingContext.parent.id : "no") +
      " => " +
      (windowGlobal.documentURI ? windowGlobal.documentURI.spec : "no-uri") +
      "\n"
  );
}

class DevToolsFrameChild extends JSWindowActorChild {
  constructor() {
    super();

    // The map is indexed by the connection prefix.
    // The values are objects containing the following properties:
    // - connection: the DevToolsServerConnection itself
    // - actor: the FrameTargetActor instance
    this._connections = new Map();

    this._onConnectionChange = this._onConnectionChange.bind(this);
    this._onSharedDataChanged = this._onSharedDataChanged.bind(this);
    EventEmitter.decorate(this);
  }

  instantiate() {
    const { sharedData } = Services.cpmm;
    const perPrefixMap = sharedData.get("DevTools:watchedPerPrefix");
    if (!perPrefixMap) {
      throw new Error(
        "Request to instantiate the target(s) for the BrowsingContext, but `sharedData` is empty about watched targets"
      );
    }

    // Create one Target actor for each prefix/client which listen to frames
    for (const [prefix, { targets, browsingContextID }] of perPrefixMap) {
      if (
        targets.has("frame") &&
        shouldNotifyWindowGlobal(this.manager, browsingContextID)
      ) {
        this._createTargetActor(prefix);
      }
    }
  }

  // Instantiate a new WindowGlobalTarget for the given connection
  _createTargetActor(parentConnectionPrefix) {
    if (this._connections.get(parentConnectionPrefix)) {
      throw new Error(
        "DevToolsFrameChild _createTargetActor was called more than once" +
          ` for the same connection (prefix: "${parentConnectionPrefix}")`
      );
    }

    // Compute a unique prefix, just for this WindowGlobal,
    // which will be used to create a JSWindowActorTransport pair between content and parent processes.
    // This is slightly hacky as we typicaly compute Prefix and Actor ID via `DevToolsServerConnection.allocID()`,
    // but here, we can't have access to any DevTools connection as we are really early in the content process startup
    // XXX: WindowGlobal's innerWindowId should be unique across processes, I think. So that should be safe?
    // (this.manager == WindowGlobalChild interface)
    const prefix =
      parentConnectionPrefix + "windowGlobal" + this.manager.innerWindowId;

    logWindowGlobal(
      this.manager,
      "Instantiate WindowGlobalTarget with prefix: " + prefix
    );

    const { connection, targetActor } = this._createConnectionAndActor(prefix);
    this._connections.set(prefix, { connection, actor: targetActor });

    if (!this._isListeningForChange) {
      // Watch for disabling in order to destroy this DevToolsClientChild and its WindowGlobalTargets
      Services.cpmm.sharedData.addEventListener(
        "change",
        this._onSharedDataChanged
      );
      this._isListeningForChange = true;
    }

    // Immediately queue a message for the parent process,
    // in order to ensure that the JSWindowActorTransport is instantiated
    // before any packet is sent from the content process.
    // As the order of messages is quaranteed to be delivered in the order they
    // were queued, we don't have to wait for anything around this sendAsyncMessage call.
    // In theory, the FrameTargetActor may emit events in its constructor.
    // If it does, such RDP packets may be lost. But in practice, no events
    // are emitted during its construction. Instead the frontend will start
    // the communication first.
    this.sendAsyncMessage("DevToolsFrameChild:connectFromContent", {
      parentConnectionPrefix,
      prefix,
      actor: targetActor.form(),
    });
  }

  _createConnectionAndActor(prefix) {
    this.useCustomLoader = this.document.nodePrincipal.isSystemPrincipal;

    // When debugging chrome pages, use a new dedicated loader, using a distinct chrome compartment.
    if (!this.loader) {
      this.loader = this.useCustomLoader
        ? new Loader.DevToolsLoader({
            invisibleToDebugger: true,
          })
        : Loader;
    }
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
    const targetActor = new FrameTargetActor(connection, this.docShell, {
      followWindowGlobalLifeCycle: true,
      doNotFireFrameUpdates: true,
    });

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
      case "DevToolsFrameParent:instantiate-already-available":
        const { prefix, browsingContextID } = data.data;
        // Re-check here, just to ensure that both parent and content processes agree
        // on what should or should not be watched.
        if (!shouldNotifyWindowGlobal(this.manager, browsingContextID)) {
          throw new Error(
            "Mismatch between DevToolsFrameParent and DevToolsFrameChild shouldNotifyWindowGlobal"
          );
        }
        return this._createTargetActor(prefix);
      case "DevToolsFrameParent:packet":
        return this.emit("packet-received", data);
      default:
        throw new Error(
          "Unsupported message in DevToolsFrameParent: " + data.name
        );
    }
  }

  handleEvent({ type }) {
    // DOMWindowCreated is registered from FrameWatcher via `ActorManagerParent.addActors`
    // as a DOM event to be listened to and so is fired by JS Window Actor code platform code.
    if (type == "DOMWindowCreated") {
      this.instantiate();
    }
  }

  _onSharedDataChanged({ type, changedKeys }) {
    if (type == "change") {
      if (!changedKeys.includes("DevTools:watchedPerPrefix")) {
        return;
      }
      const { sharedData } = Services.cpmm;
      const perPrefixMap = sharedData.get("DevTools:watchedPerPrefix");
      if (!perPrefixMap) {
        this.didDestroy();
        return;
      }
      let isStillWatching = false;
      // Destroy the JSWindow Actor if we stopped watching frames from all the clients.
      for (const [prefix, { targets }] of perPrefixMap) {
        // This one prefix/connection still watches for frame
        if (targets.has("frame")) {
          isStillWatching = true;
          continue;
        }
        const connectionInfo = this._connections.get(prefix);
        // This connection wasn't watching, or at least did not instantiate a target actor
        if (!connectionInfo) {
          continue;
        }
        connectionInfo.connection.close();
        this._connections.delete(prefix);
      }
      // If all the connections stopped watching, destroy everything
      if (!isStillWatching) {
        this.didDestroy();
      }
    } else {
      throw new Error("Unsupported event:" + type + "\n");
    }
  }

  didDestroy() {
    for (const [, connectionInfo] of this._connections) {
      connectionInfo.connection.close();
    }
    this._connections.clear();
    if (this.useCustomLoader) {
      this.loader.destroy();
    }
    if (this._isListeningForChange) {
      Services.cpmm.sharedData.removeEventListener(
        "change",
        this._onSharedDataChanged
      );
    }
  }
}
