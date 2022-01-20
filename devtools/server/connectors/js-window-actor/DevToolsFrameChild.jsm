/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["DevToolsFrameChild"];

const { EventEmitter } = ChromeUtils.import(
  "resource://gre/modules/EventEmitter.jsm"
);
const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
const Loader = ChromeUtils.import(
  "resource://devtools/shared/loader/Loader.jsm"
);
const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);
XPCOMUtils.defineLazyModuleGetters(this, {
  TargetActorRegistry:
    "resource://devtools/server/actors/targets/target-actor-registry.jsm",
});

XPCOMUtils.defineLazyModuleGetters(this, {
  WindowGlobalLogger:
    "resource://devtools/server/connectors/js-window-actor/WindowGlobalLogger.jsm",
});

const isEveryFrameTargetEnabled = Services.prefs.getBoolPref(
  "devtools.every-frame-target.enabled",
  false
);

// Name of the attribute into which we save data in `sharedData` object.
const SHARED_DATA_KEY_NAME = "DevTools:watchedPerWatcher";

/**
 * Helper function to know if a given WindowGlobal should be exposed via watchTargets("frame") API
 */
function shouldNotifyWindowGlobal(
  windowGlobal,
  context,
  { acceptTopLevelTarget = false }
) {
  const browsingContext = windowGlobal.browsingContext;
  // Ignore about:blank loads, which spawn a document that never finishes loading
  // and would require somewhat useless Target and all its related overload.
  const window = Services.wm.getCurrentInnerWindowWithId(
    windowGlobal.innerWindowId
  );

  // By default, before loading the actual document (even an about:blank document),
  // we do load immediately "the initial about:blank document".
  // This is expected by the spec. Typically when creating a new BrowsingContext/DocShell/iframe,
  // we would have such transient initial document.
  // `Document.isInitialDocument` helps identifying this transcient document, which
  // we want to ignore as it would instantiate a very short lived target which
  // confuses many tests and triggers race conditions by spamming many targets.
  //
  // We also ignore some other transient empty documents created while using `window.open()`
  // When using this API with cross process loads, we may create up to three documents/WindowGlobals.
  // We get a first initial about:blank document, and a second document created
  // for moving the document in the right principal.
  // The third document will be the actual document we expect to debug.
  // The second document is an implementation artifact which ideally wouldn't exist
  // and isn't expected by the spec.
  // Note that `window.print` and print preview are using `window.open` and are going throught this.
  if (window.document.isInitialDocument) {
    return false;
  }

  // If we are focusing only on a sub-tree of Browsing Element,
  // Ignore the out of the sub tree elements.
  if (
    context.type == "browser-element" &&
    browsingContext.browserId != context.browserId
  ) {
    return false;
  }

  // For client-side target switching, only mention the "remote frames".
  // i.e. the frames which are in a distinct process compared to their parent document
  // If there is no parent, this is most likely the top level document.
  //
  // Ignore this check for the browser toolbox, as tab's BrowsingContext have no
  // parent and aren't the top level target.
  //
  // `acceptTopLevelTarget` is set both when server side target switching is enabled
  // or when navigating to and from pages in the bfcache
  if (
    !acceptTopLevelTarget &&
    context.type == "browser-element" &&
    !browsingContext.parent
  ) {
    return false;
  }

  // We may process an iframe that runs in the same process as its parent and we don't want
  // to create targets for them if same origin targets are not enabled. Instead the WindowGlobalTargetActor
  // will inspect these children document via docShell tree (typically via `docShells` or `windows` getters).
  // This is quite common when Fission is off as any iframe will run in same process
  // as their parent document. But it can also happen with Fission enabled if iframes have
  // children iframes using the same origin.
  if (!isEveryFrameTargetEnabled && !windowGlobal.isProcessRoot) {
    return false;
  }

  return true;
}

// If true, log info about WindowGlobal's being created.
const DEBUG = false;
function logWindowGlobal(windowGlobal, message) {
  if (!DEBUG) {
    return;
  }
  WindowGlobalLogger.logWindowGlobal(windowGlobal, message);
}

class DevToolsFrameChild extends JSWindowActorChild {
  constructor() {
    super();

    // The map is indexed by the Watcher Actor ID.
    // The values are objects containing the following properties:
    // - connection: the DevToolsServerConnection itself
    // - actor: the WindowGlobalTargetActor instance
    this._connections = new Map();

    this._onConnectionChange = this._onConnectionChange.bind(this);
    EventEmitter.decorate(this);

    // Set the following preference on the constructor, so that we can easily
    // toggle these preferences on and off from tests and have the new value being picked up.

    // bfcache-in-parent changes significantly how navigation behaves.
    // We may start reusing previously existing WindowGlobal and so reuse
    // previous set of JSWindowActor pairs (i.e. DevToolsFrameParent/DevToolsFrameChild).
    // When enabled, regular navigations may also change and spawn new BrowsingContexts.
    // If the page we navigate from supports being stored in bfcache,
    // the navigation will use a new BrowsingContext. And so force spawning
    // a new top-level target.
    XPCOMUtils.defineLazyGetter(
      this,
      "isBfcacheInParentEnabled",
      () =>
        Services.appinfo.sessionHistoryInParent &&
        Services.prefs.getBoolPref("fission.bfcacheInParent", false)
    );
  }

  /**
   * Try to instantiate new target actors for the current WindowGlobal, and that,
   * for all the currently registered Watcher actors.
   *
   * Read the `sharedData` to get metadata about all registered watcher actors.
   * If these watcher actors are interested in the current WindowGlobal,
   * instantiate a new dedicated target actor for each of the watchers.
   *
   * @param Object options
   * @param Boolean options.isBFCache
   *        True, if the request to instantiate a new target comes from a bfcache navigation.
   *        i.e. when we receive a pageshow event with persisted=true.
   *        This will be true regardless of bfcacheInParent being enabled or disabled.
   * @param Boolean options.ignoreIfExisting
   *        By default to false. If true is passed, we avoid instantiating a target actor
   *        if one already exists for this windowGlobal.
   */
  instantiate({ isBFCache = false, ignoreIfExisting = false } = {}) {
    const { sharedData } = Services.cpmm;
    const sessionDataByWatcherActor = sharedData.get(SHARED_DATA_KEY_NAME);
    if (!sessionDataByWatcherActor) {
      throw new Error(
        "Request to instantiate the target(s) for the BrowsingContext, but `sharedData` is empty about watched targets"
      );
    }

    // Create one Target actor for each prefix/client which listen to frames
    for (const [watcherActorID, sessionData] of sessionDataByWatcherActor) {
      const {
        connectionPrefix,
        context,
        isServerTargetSwitchingEnabled,
      } = sessionData;
      // Always create new targets when server targets are enabled as we create targets for all the WindowGlobal's,
      // including all WindowGlobal's of the top target.
      // For bfcache navigations, we only create new targets when bfcacheInParent is enabled,
      // as this would be the only case where new DocShells will be created. This requires us to spawn a
      // new WindowGlobalTargetActor as one such actor is bound to a unique DocShell.
      const acceptTopLevelTarget =
        isServerTargetSwitchingEnabled ||
        (isBFCache && this.isBfcacheInParentEnabled);
      if (
        sessionData.targets.includes("frame") &&
        shouldNotifyWindowGlobal(this.manager, context, {
          acceptTopLevelTarget,
        })
      ) {
        // If this was triggered because of a navigation, we want to retrieve the existing
        // target we were debugging so we can destroy it before creating the new target.
        // This is important because we had cases where the destruction of an old target
        // was unsetting a flag on the **new** target document, breaking the toolbox (See Bug 1721398).

        // We're checking for an existing target given a watcherActorID + browserId + browsingContext
        // Note that a target switch might create a new browsing context, so we wouldn't
        // retrieve the existing target here. We are okay with this as:
        // - this shouldn't happen much
        // - in such case we weren't seeing the issue of Bug 1721398 (the old target can't access the new document)
        const existingTarget = this._findTargetActor({
          watcherActorID,
          context,
          browsingContextId: this.manager.browsingContext.id,
        });

        // See comment in handleEvent(DOMDocElementInserted) to know why we try to
        // create targets if none already exists
        if (existingTarget && ignoreIfExisting) {
          continue;
        }

        // Bail if there is already an existing WindowGlobalTargetActor which wasn't
        // created from a JSWIndowActor.
        // This means we are reloading or navigating (same-process) a Target
        // which has not been created using the Watcher, but from the client (most likely
        // the initial target of a local-tab toolbox).
        // However, we force overriding the first message manager based target in case of
        // BFCache navigations.
        if (
          existingTarget &&
          !existingTarget.createdFromJsWindowActor &&
          !isBFCache
        ) {
          continue;
        }

        // If we decide to instantiate a new target and there was one before,
        // first destroy the previous one.
        // Otherwise its destroy sequence will be executed *after* the new one
        // is being initialized and may easily revert changes made against platform API.
        // (typically toggle platform boolean attributes back to default…)
        if (existingTarget) {
          existingTarget.destroy({ isTargetSwitching: true });
        }

        this._createTargetActor({
          watcherActorID,
          parentConnectionPrefix: connectionPrefix,
          sessionData,
          isDocumentCreation: true,
        });
      }
    }
  }

  /**
   * Instantiate a new WindowGlobalTarget for the given connection.
   *
   * @param Object options
   * @param String options.watcherActorID
   *        The ID of the WatcherActor who requested to observe and create these target actors.
   * @param String options.parentConnectionPrefix
   *        The prefix of the DevToolsServerConnection of the Watcher Actor.
   *        This is used to compute a unique ID for the target actor.
   * @param Object options.sessionData
   *        All data managed by the Watcher Actor and WatcherRegistry.jsm, containing
   *        target types, resources types to be listened as well as breakpoints and any
   *        other data meant to be shared across processes and threads.
   * @param Boolean options.isDocumentCreation
   *        Set to true if the function is called from `instantiate`, i.e. when we're
   *        handling a new document being created.
   * @param Boolean options.fromInstantiateAlreadyAvailable
   *        Set to true if the function is called from handling `DevToolsFrameParent:instantiate-already-available`
   *        query.
   */
  _createTargetActor({
    watcherActorID,
    parentConnectionPrefix,
    sessionData,
    isDocumentCreation,
    fromInstantiateAlreadyAvailable,
  }) {
    if (this._connections.get(watcherActorID)) {
      // If this function is called as a result of a `DevToolsFrameParent:instantiate-already-available`
      // message, we might have a legitimate race condition:
      // In frame-helper, we want to create the initial targets for a given browser element.
      // It might happen that the `DevToolsFrameParent:instantiate-already-available` is
      // aborted if the page navigates (and the document is destroyed) while the query is still pending.
      // In such case, frame-helper will try to send a new message. In the meantime,
      // the DevToolsFrameChild `DOMWindowCreated` handler may already have been called and
      // the new target already created.
      // We don't want to throw in such case, as our end-goal, having a target for the document,
      // is achieved.
      if (fromInstantiateAlreadyAvailable) {
        return;
      }
      throw new Error(
        "DevToolsFrameChild _createTargetActor was called more than once" +
          ` for the same Watcher (Actor ID: "${watcherActorID}")`
      );
    }

    // Compute a unique prefix, just for this WindowGlobal,
    // which will be used to create a JSWindowActorTransport pair between content and parent processes.
    // This is slightly hacky as we typicaly compute Prefix and Actor ID via `DevToolsServerConnection.allocID()`,
    // but here, we can't have access to any DevTools connection as we are really early in the content process startup
    // XXX: WindowGlobal's innerWindowId should be unique across processes, I think. So that should be safe?
    // (this.manager == WindowGlobalChild interface)
    const forwardingPrefix =
      parentConnectionPrefix + "windowGlobal" + this.manager.innerWindowId;

    logWindowGlobal(
      this.manager,
      "Instantiate WindowGlobalTarget with prefix: " + forwardingPrefix
    );

    // In the case of the browser toolbox, tab's BrowsingContext don't have
    // any parent BC and shouldn't be considered as top-level.
    // This is why we check for browserId's.
    const browsingContext = this.manager.browsingContext;
    const isTopLevelTarget =
      !browsingContext.parent &&
      browsingContext.browserId == sessionData.context.browserId;

    const { connection, targetActor } = this._createConnectionAndActor(
      forwardingPrefix,
      isTopLevelTarget
    );
    const form = targetActor.form();
    // Ensure unregistering and destroying the related DevToolsServerConnection+Transport
    // on both content and parent process JSWindowActors.
    targetActor.once("destroyed", () => {
      // This will destroy the content process one
      this._destroyTargetActor(watcherActorID);
      // And this will destroy the parent process one
      this.sendAsyncMessage("DevToolsFrameChild:destroy", {
        actors: [
          {
            watcherActorID,
            form,
          },
        ],
      });
    });
    this._connections.set(watcherActorID, {
      connection,
      actor: targetActor,
    });

    // Immediately queue a message for the parent process,
    // in order to ensure that the JSWindowActorTransport is instantiated
    // before any packet is sent from the content process.
    // As the order of messages is guaranteed to be delivered in the order they
    // were queued, we don't have to wait for anything around this sendAsyncMessage call.
    // In theory, the WindowGlobalTargetActor may emit events in its constructor.
    // If it does, such RDP packets may be lost.
    // The important point here is to send this message before processing the sessionData,
    // which will start the Watcher and start emitting resources on the target actor.
    this.sendAsyncMessage("DevToolsFrameChild:connectFromContent", {
      watcherActorID,
      forwardingPrefix,
      actor: targetActor.form(),
    });

    // Pass initialization data to the target actor
    for (const type in sessionData) {
      // `sessionData` will also contain `browserId` and `watcherTraits`,
      // as well as entries with empty arrays, which shouldn't be processed.
      const entries = sessionData[type];
      if (!Array.isArray(entries) || entries.length == 0) {
        continue;
      }
      targetActor.addSessionDataEntry(type, entries, isDocumentCreation);
    }
  }

  _destroyTargetActor(watcherActorID) {
    const connectionInfo = this._connections.get(watcherActorID);
    // This connection has already been cleaned?
    if (!connectionInfo) {
      throw new Error(
        `Trying to destroy a target actor that doesn't exists, or has already been destroyed. Watcher Actor ID:${watcherActorID}`
      );
    }
    connectionInfo.connection.close();
    this._connections.delete(watcherActorID);
    if (this._connections.size == 0) {
      this.didDestroy();
    }
  }

  _createConnectionAndActor(forwardingPrefix, isTopLevelTarget) {
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

    const { WindowGlobalTargetActor } = this.loader.require(
      "devtools/server/actors/targets/window-global"
    );

    DevToolsServer.init();

    // We want a special server without any root actor and only target-scoped actors.
    // We are going to spawn a WindowGlobalTargetActor instance in the next few lines,
    // it is going to act like a root actor without being one.
    DevToolsServer.registerActors({ target: true });
    DevToolsServer.on("connectionchange", this._onConnectionChange);

    const connection = DevToolsServer.connectToParentWindowActor(
      this,
      forwardingPrefix
    );

    // Create the actual target actor.
    const targetActor = new WindowGlobalTargetActor(connection, {
      docShell: this.docShell,
      // Targets created from the server side, via Watcher actor and DevToolsFrame JSWindow
      // actor pair are following WindowGlobal lifecycle. i.e. will be destroyed on any
      // type of navigation/reload.
      // Note that if devtools.target-switching.server.enabled is false, the top level target
      // won't be created via the codepath. Except if we have a bfcache-in-parent navigation.
      followWindowGlobalLifeCycle: true,
      isTopLevelTarget,
      ignoreSubFrames: isEveryFrameTargetEnabled,
    });
    targetActor.manage(targetActor);
    targetActor.createdFromJsWindowActor = true;

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

  sendPacket(packet, prefix) {
    this.sendAsyncMessage("DevToolsFrameChild:packet", { packet, prefix });
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

  receiveMessage(message) {
    // When debugging only a given tab, all messages but "packet" one pass `browserId`
    // and are expected to match shouldNotifyWindowGlobal result.
    if (
      message.name != "DevToolsFrameParent:packet" &&
      message.data.context.type == "browser-element"
    ) {
      const { browserId } = message.data.context;
      // Re-check here, just to ensure that both parent and content processes agree
      // on what should or should not be watched.
      if (
        this.manager.browsingContext.browserId != browserId &&
        !shouldNotifyWindowGlobal(this.manager, browserId, {
          acceptTopLevelTarget: true,
        })
      ) {
        throw new Error(
          "Mismatch between DevToolsFrameParent and DevToolsFrameChild " +
            (this.manager.browsingContext.browserId == browserId
              ? "window global shouldn't be notified (shouldNotifyWindowGlobal mismatch)"
              : `expected browsing context with browserId ${browserId}, but got ${this.manager.browsingContext.browserId}`)
        );
      }
    }
    switch (message.name) {
      case "DevToolsFrameParent:instantiate-already-available": {
        const { watcherActorID, connectionPrefix, sessionData } = message.data;

        return this._createTargetActor({
          watcherActorID,
          parentConnectionPrefix: connectionPrefix,
          sessionData,
          fromInstantiateAlreadyAvailable: true,
        });
      }
      case "DevToolsFrameParent:destroy": {
        const { watcherActorID } = message.data;
        return this._destroyTargetActor(watcherActorID);
      }
      case "DevToolsFrameParent:addSessionDataEntry": {
        const { watcherActorID, context, type, entries } = message.data;
        return this._addSessionDataEntry(
          watcherActorID,
          context,
          type,
          entries
        );
      }
      case "DevToolsFrameParent:removeSessionDataEntry": {
        const { watcherActorID, context, type, entries } = message.data;
        return this._removeSessionDataEntry(
          watcherActorID,
          context,
          type,
          entries
        );
      }
      case "DevToolsFrameParent:packet":
        return this.emit("packet-received", message);
      default:
        throw new Error(
          "Unsupported message in DevToolsFrameParent: " + message.name
        );
    }
  }

  /**
   * Return an existing target given a WatcherActor id, a browserId and an optional
   * browsing context id.
   * /!\ Note that we may have multiple targets for a given (watcherActorId, browserId) couple,
   * for example if we have 2 remote iframes sharing the same origin, which is why you
   * might want to pass a specific browsing context id to filter the list down.
   *
   * @param {Object} options
   * @param {Object} options.watcherActorID
   * @param {Object} options.context
   * @param {Object} options.browsingContextId: Optional browsing context id to narrow the
   *                 search to a specific browsing context.
   *
   * @returns {WindowGlobalTargetActor|null}
   */
  _findTargetActor({ watcherActorID, context, browsingContextId }) {
    // First let's check if a target was created for this watcher actor in this specific
    // DevToolsFrameChild instance.
    const connectionInfo = this._connections.get(watcherActorID);
    const targetActor = connectionInfo ? connectionInfo.actor : null;
    if (targetActor) {
      return targetActor;
    }

    // If we couldn't find such target, we want to see if a target was created for this
    // (watcherActorId,browserId, {browsingContextId}) in another DevToolsFrameChild instance.
    // This might be the case if we're navigating to a new page with server side target
    // enabled and we want to retrieve the target of the page we're navigating from.
    const isMatchingBrowserElement =
      this.manager.browsingContext.browserId == context.browserId;
    const isMatchingWebExtension =
      this.document.nodePrincipal.addonId == context.addonId;
    if (
      (context.type == "browser-element" && isMatchingBrowserElement) ||
      (context.type == "webextension" && isMatchingWebExtension)
    ) {
      // Ensure retrieving the one target actor related to this connection.
      // This allows to distinguish actors created for various toolboxes.
      // For ex, regular toolbox versus browser console versus browser toolbox
      const connectionPrefix = watcherActorID.replace(/watcher\d+$/, "");
      const targetActors = TargetActorRegistry.getTargetActors(
        context,
        connectionPrefix
      );

      if (!browsingContextId) {
        return targetActors[0] || null;
      }
      return targetActors.find(
        actor => actor.browsingContextID === browsingContextId
      );
    }
    return null;
  }

  _addSessionDataEntry(watcherActorID, context, type, entries) {
    // /!\ We may have an issue here as there could be multiple targets for a given
    // (watcherActorID,browserId) pair.
    // This should be clarified as part of Bug 1725623.
    const targetActor = this._findTargetActor({
      watcherActorID,
      context,
    });

    if (!targetActor) {
      throw new Error(
        `No target actor for this Watcher Actor ID:"${watcherActorID}" / BrowserId:${context.browserId}`
      );
    }
    return targetActor.addSessionDataEntry(type, entries);
  }

  _removeSessionDataEntry(watcherActorID, context, type, entries) {
    // /!\ We may have an issue here as there could be multiple targets for a given
    // (watcherActorID,browserId) pair.
    // This should be clarified as part of Bug 1725623.
    const targetActor = this._findTargetActor({
      watcherActorID,
      context,
    });
    // By the time we are calling this, the target may already have been destroyed.
    if (targetActor) {
      return targetActor.removeSessionDataEntry(type, entries);
    }
    return null;
  }

  handleEvent({ type, persisted, target }) {
    // Ignore any event that may fire for children WindowGlobals/documents
    if (target != this.document) {
      return;
    }

    // DOMWindowCreated is registered from FrameWatcher via `ActorManagerParent.addJSWindowActors`
    // as a DOM event to be listened to and so is fired by JS Window Actor code platform code.
    if (type == "DOMWindowCreated") {
      this.instantiate();
      return;
    }
    // We might have ignored the DOMWindowCreated event because it was the initial about:blank document.
    // But when loading same-process iframes, we reuse the WindowGlobal of the about:bank document
    // to load the actual URL loaded in the iframe. This means we won't have a new DOMWindowCreated
    // for the actual document. There is a DOMDocElementInserted fired just after, that we can catch
    // to create a target for same-process iframes.
    // This means that we still do not create any target for the initial documents.
    // It is complex to instantiate targets for initial documents because:
    // - it would mean spawning two targets for the same WindowGlobal and sharing the same innerWindowId
    // - or have WindowGlobalTargets to handle more than one document (it would mean reusing will-navigate/window-ready events
    // both on client and server)
    if (type == "DOMDocElementInserted") {
      this.instantiate({ ignoreIfExisting: true });
      return;
    }

    // If persisted=true, this is a BFCache navigation.
    //
    // With Fission enabled and bfcacheInParent, BFCache navigations will spawn a new DocShell
    // in the same process:
    // * the previous page won't be destroyed, its JSWindowActor will stay alive (`didDestroy` won't be called)
    //   and a 'pagehide' with persisted=true will be emitted on it.
    // * the new page page won't emit any DOMWindowCreated, but instead a pageshow with persisted=true
    //   will be emitted.

    if (type === "pageshow" && persisted) {
      // Notify all bfcache navigations, even the one for which we don't create a new target
      // as that's being useful for parent process storage resource watchers.
      this.sendAsyncMessage("DevToolsFrameChild:bf-cache-navigation-pageshow");

      // Here we are going to re-instantiate a target that got destroyed before while processing a pagehide event.
      // We force instantiating a new top level target, within `instantiate()` even if server targets are disabled.
      // But we only do that if bfcacheInParent is enabled. Otherwise for same-process, same-docshell bfcache navigation,
      // we don't want to spawn new targets.
      this.instantiate({
        isBFCache: true,
      });
      return;
    }

    if (type === "pagehide" && persisted) {
      // Notify all bfcache navigations, even the one for which we don't create a new target
      // as that's being useful for parent process storage resource watchers.
      this.sendAsyncMessage("DevToolsFrameChild:bf-cache-navigation-pagehide");

      // We might navigate away for the first top level target,
      // which isn't using JSWindowActor (it still uses messages manager and is created by the client, via TabDescriptor.getTarget).
      // We have to unregister it from the TargetActorRegistry, otherwise,
      // if we navigate back to it, the next DOMWindowCreated won't create a new target for it.
      const { sharedData } = Services.cpmm;
      const sessionDataByWatcherActor = sharedData.get(SHARED_DATA_KEY_NAME);
      if (!sessionDataByWatcherActor) {
        throw new Error(
          "Request to instantiate the target(s) for the BrowsingContext, but `sharedData` is empty about watched targets"
        );
      }

      const actors = [];
      // A flag to know if the following for loop ended up destroying all the actors.
      // It may not be the case if one Watcher isn't having server target switching enabled.
      let allActorsAreDestroyed = true;
      for (const [watcherActorID, sessionData] of sessionDataByWatcherActor) {
        const { context, isServerTargetSwitchingEnabled } = sessionData;

        // /!\ We may have an issue here as there could be multiple targets for a given
        // (watcherActorID,browserId) pair.
        // This should be clarified as part of Bug 1725623.
        const existingTarget = this._findTargetActor({
          watcherActorID,
          context,
        });

        if (!existingTarget) {
          continue;
        }

        // Use `originalWindow` as `window` can be set when a document was selected from
        // the iframe picker in the toolbox toolbar.
        if (existingTarget.originalWindow.document != target) {
          throw new Error("Existing target actor is for a distinct document");
        }
        // Do not do anything if both bfcache in parent and server targets are disabled
        // As history navigations will be handled within the same DocShell and by the
        // same WindowGlobalTargetActor. The actor will listen to pageshow/pagehide by itself.
        // We should not destroy any target.
        if (!this.isBfcacheInParentEnabled && !isServerTargetSwitchingEnabled) {
          allActorsAreDestroyed = false;
          continue;
        }

        actors.push({
          watcherActorID,
          form: existingTarget.form(),
        });
        existingTarget.destroy();
      }

      if (actors.length > 0) {
        // The most important is to unregister the actor from TargetActorRegistry,
        // so that it is no longer present in the list when new DOMWindowCreated fires.
        // This will also help notify the client that the target has been destroyed.
        // And if we navigate back to this target, the client will receive the same target actor ID,
        // so that it is really important to destroy it correctly on both server and client.
        this.sendAsyncMessage("DevToolsFrameChild:destroy", { actors });
      }

      if (allActorsAreDestroyed) {
        // Completely clear this JSWindow Actor.
        // Do this after having called _findTargetActor,
        // as it would clear the registered target actors.
        this.didDestroy();
      }
    }
  }

  didDestroy() {
    logWindowGlobal(this.manager, "Destroy WindowGlobalTarget");
    for (const [, connectionInfo] of this._connections) {
      connectionInfo.connection.close();
    }
    this._connections.clear();
    // If we spawned a loader, we bootstrap a server, from which we should
    // unregister the listener in order to prevent leaking the JSWindow Actor.
    if (this.loader) {
      const { DevToolsServer } = this.loader.require(
        "devtools/server/devtools-server"
      );
      DevToolsServer.off("connectionchange", this._onConnectionChange);

      // The connections closed just before may emit "connectionchange"
      // only on following events loops. So that we may avoid destroying the DevToolsServer
      // when all connections are closed.
      // So, force checking for the number of connections right away.
      // browser_debugger_server.js used to track this edgecase.
      this._onConnectionChange();
    }
    if (this.useCustomLoader) {
      this.loader.destroy();
    }
  }
}
