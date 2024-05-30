/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";
const { Actor } = require("resource://devtools/shared/protocol.js");
const { watcherSpec } = require("resource://devtools/shared/specs/watcher.js");

const Resources = require("resource://devtools/server/actors/resources/index.js");
const { TargetActorRegistry } = ChromeUtils.importESModule(
  "resource://devtools/server/actors/targets/target-actor-registry.sys.mjs",
  { global: "shared" }
);
const { ParentProcessWatcherRegistry } = ChromeUtils.importESModule(
  "resource://devtools/server/actors/watcher/ParentProcessWatcherRegistry.sys.mjs",
  // ParentProcessWatcherRegistry needs to be a true singleton and loads ActorManagerParent
  // which also has to be a true singleton.
  { global: "shared" }
);
const { getAllBrowsingContextsForContext } = ChromeUtils.importESModule(
  "resource://devtools/server/actors/watcher/browsing-context-helpers.sys.mjs",
  { global: "contextual" }
);
const {
  SESSION_TYPES,
} = require("resource://devtools/server/actors/watcher/session-context.js");

loader.lazyRequireGetter(
  this,
  "NetworkParentActor",
  "resource://devtools/server/actors/network-monitor/network-parent.js",
  true
);
loader.lazyRequireGetter(
  this,
  "BlackboxingActor",
  "resource://devtools/server/actors/blackboxing.js",
  true
);
loader.lazyRequireGetter(
  this,
  "BreakpointListActor",
  "resource://devtools/server/actors/breakpoint-list.js",
  true
);
loader.lazyRequireGetter(
  this,
  "TargetConfigurationActor",
  "resource://devtools/server/actors/target-configuration.js",
  true
);
loader.lazyRequireGetter(
  this,
  "ThreadConfigurationActor",
  "resource://devtools/server/actors/thread-configuration.js",
  true
);

exports.WatcherActor = class WatcherActor extends Actor {
  /**
   * Initialize a new WatcherActor which is the main entry point to debug
   * something. The main features of this actor are to:
   * - observe targets related to the context we are debugging.
   *   This is done via watchTargets/unwatchTargets methods, and
   *   target-available-form/target-destroyed-form events.
   * - observe resources related to the observed targets.
   *   This is done via watchResources/unwatchResources methods, and
   *   resource-available-form/resource-updated-form/resource-destroyed-form events.
   *   Note that these events are also emited on both the watcher actor,
   *   for resources observed from the parent process, as well as on the
   *   target actors, when the resources are observed from the target's process or thread.
   *
   * @param {DevToolsServerConnection} conn
   *        The connection to use in order to communicate back to the client.
   * @param {object} sessionContext
   *        The Session Context to help know what is debugged.
   *        See devtools/server/actors/watcher/session-context.js
   * @param {Number} sessionContext.browserId: If this is a "browser-element" context type,
   *        the "browserId" of the <browser> element we would like to debug.
   * @param {Boolean} sessionContext.isServerTargetSwitchingEnabled: Flag to to know if we should
   *        spawn new top level targets for the debugged context.
   */
  constructor(conn, sessionContext) {
    super(conn, watcherSpec);
    this._sessionContext = sessionContext;
    if (sessionContext.type == SESSION_TYPES.BROWSER_ELEMENT) {
      // Retrieve the <browser> element for the given browser ID
      const browsingContext = BrowsingContext.getCurrentTopByBrowserId(
        sessionContext.browserId
      );
      if (!browsingContext) {
        throw new Error(
          "Unable to retrieve the <browser> element for browserId=" +
            sessionContext.browserId
        );
      }
      this._browserElement = browsingContext.embedderElement;
    }

    // Sometimes we get iframe targets before the top-level targets
    // mostly when doing bfcache navigations, lets cache the early iframes targets and
    // flush them after the top-level target is available. See Bug 1726568 for details.
    this._earlyIframeTargets = {};

    // All currently available WindowGlobal target's form, keyed by `innerWindowId`.
    //
    // This helps to:
    // - determine if the iframe targets are early or not.
    //   i.e. if it is notified before its parent target is available.
    // - notify the destruction of all children targets when a parent is destroyed.
    //   i.e. have a reliable order of destruction between parent and children.
    //
    // Note that there should be just one top-level window target at a time,
    // but there are certain cases when a new target is available before the
    // old target is destroyed.
    this._currentWindowGlobalTargets = new Map();

    // The Browser Toolbox requires to load modules in a distinct compartment in order
    // to be able to debug system compartments modules (most of Firefox internal codebase).
    // This is a requirement coming from SpiderMonkey Debugger API and relates to the thread actor.
    this._jsActorName =
      sessionContext.type == SESSION_TYPES.ALL
        ? "BrowserToolboxDevToolsProcess"
        : "DevToolsProcess";
  }

  get sessionContext() {
    return this._sessionContext;
  }

  /**
   * If we are debugging only one Tab or Document, returns its BrowserElement.
   * For Tabs, it will be the <browser> element used to load the web page.
   *
   * This is typicaly used to fetch:
   * - its `browserId` attribute, which uniquely defines it,
   * - its `browsingContextID` or `browsingContext`, which helps inspecting its content.
   */
  get browserElement() {
    return this._browserElement;
  }

  getAllBrowsingContexts(options) {
    return getAllBrowsingContextsForContext(this.sessionContext, options);
  }

  /**
   * Helper to know if the context we are debugging has been already destroyed
   */
  isContextDestroyed() {
    if (this.sessionContext.type == "browser-element") {
      return !this.browserElement.browsingContext;
    } else if (this.sessionContext.type == "webextension") {
      return !BrowsingContext.get(this.sessionContext.addonBrowsingContextID);
    } else if (this.sessionContext.type == "all") {
      return false;
    }
    throw new Error(
      "Unsupported session context type: " + this.sessionContext.type
    );
  }

  destroy() {
    // Only try to notify content processes if the watcher was in the registry.
    // Otherwise it means that it wasn't connected to any process and the JS Process Actor
    // wouldn't be registered.
    if (ParentProcessWatcherRegistry.getWatcher(this.actorID)) {
      // Emit one IPC message on destroy to all the processes
      const domProcesses = ChromeUtils.getAllDOMProcesses();
      for (const domProcess of domProcesses) {
        domProcess.getActor(this._jsActorName).destroyWatcher({
          watcherActorID: this.actorID,
        });
      }
    }

    // Ensure destroying all Resource Watcher instantiated in the parent process
    Resources.unwatchResources(
      this,
      Resources.getParentProcessResourceTypes(Object.values(Resources.TYPES))
    );

    ParentProcessWatcherRegistry.unregisterWatcher(this.actorID);

    // In case the watcher actor is leaked, prevent leaking the browser window
    this._browserElement = null;

    // Destroy the actor in order to ensure destroying all its children actors.
    // As this actor is a pool with children actors, when the transport/connection closes
    // we expect all actors and its children to be destroyed.
    super.destroy();
  }

  /*
   * Get the list of the currently watched resources for this watcher.
   *
   * @return Array<String>
   *         Returns the list of currently watched resource types.
   */
  get sessionData() {
    return ParentProcessWatcherRegistry.getSessionData(this);
  }

  form() {
    return {
      actor: this.actorID,
      // The resources and target traits should be removed all at the same time since the
      // client has generic ways to deal with all of them (See Bug 1680280).
      traits: {
        ...this.sessionContext.supportedTargets,
        resources: this.sessionContext.supportedResources,
      },
    };
  }

  /**
   * Start watching for a new target type.
   *
   * This will instantiate Target Actors for existing debugging context of this type,
   * but will also create actors as context of this type get created.
   * The actors are notified to the client via "target-available-form" RDP events.
   * We also notify about target actors destruction via "target-destroyed-form".
   * Note that we are guaranteed to receive all existing target actor by the time this method
   * resolves.
   *
   * @param {string} targetType
   *        Type of context to observe. See Targets.TYPES object.
   */
  async watchTargets(targetType) {
    ParentProcessWatcherRegistry.watchTargets(this, targetType);

    // When debugging a tab, ensure processing the top level target first
    // (for now, other session context types are instantiating the top level target
    // from the descriptor's getTarget method instead of the Watcher)
    let topLevelTargetProcess;
    if (this.sessionContext.type == SESSION_TYPES.BROWSER_ELEMENT) {
      topLevelTargetProcess =
        this.browserElement.browsingContext.currentWindowGlobal?.domProcess;
      if (topLevelTargetProcess) {
        await topLevelTargetProcess.getActor(this._jsActorName).watchTargets({
          watcherActorID: this.actorID,
          targetType,
        });
        // Stop execution if we were destroyed in the meantime
        if (this.isDestroyed()) {
          return;
        }
      }
    }

    // We have to reach out all the content processes as the page may navigate
    // to any other content process when navigating to another origin.
    // It may even run in the parent process when loading about:robots.
    const domProcesses = ChromeUtils.getAllDOMProcesses();
    const promises = [];
    for (const domProcess of domProcesses) {
      if (domProcess == topLevelTargetProcess) {
        continue;
      }
      promises.push(
        domProcess
          .getActor(this._jsActorName)
          .watchTargets({
            watcherActorID: this.actorID,
            targetType,
          })
          .catch(e => {
            // Ignore any process that got destroyed while trying to send the request
            if (!domProcess.canSend) {
              console.warn(
                "Content process closed while requesting targets",
                domProcess.name,
                domProcess.remoteType
              );
              return;
            }
            throw e;
          })
      );
    }
    await Promise.all(promises);
  }

  /**
   * Stop watching for a given target type.
   *
   * @param {string} targetType
   *        Type of context to observe. See Targets.TYPES object.
   * @param {object} options
   * @param {boolean} options.isModeSwitching
   *        true when this is called as the result of a change to the devtools.browsertoolbox.scope pref
   */
  unwatchTargets(targetType, options = {}) {
    const isWatchingTargets = ParentProcessWatcherRegistry.unwatchTargets(
      this,
      targetType
    );
    if (!isWatchingTargets) {
      return;
    }

    const domProcesses = ChromeUtils.getAllDOMProcesses();
    for (const domProcess of domProcesses) {
      domProcess.getActor(this._jsActorName).unwatchTargets({
        watcherActorID: this.actorID,
        targetType,
        options,
      });
    }
  }

  /**
   * Flush any early iframe targets relating to this top level
   * window target.
   * @param {number} topInnerWindowID
   */
  _flushIframeTargets(topInnerWindowID) {
    while (this._earlyIframeTargets[topInnerWindowID]?.length > 0) {
      const actor = this._earlyIframeTargets[topInnerWindowID].shift();
      this.emit("target-available-form", actor);
    }
  }

  /**
   * Called by a Watcher module, whenever a new target is available
   */
  notifyTargetAvailable(actor) {
    // Emit immediately for worker, process & extension targets
    // as they don't have a parent browsing context.
    if (!actor.traits?.isBrowsingContext) {
      this.emit("target-available-form", actor);
      return;
    }

    // If isBrowsingContext trait is true, we are processing a WindowGlobalTarget.
    // (this trait should be renamed)
    this._currentWindowGlobalTargets.set(actor.innerWindowId, actor);

    // The top-level is always the same for the browser-toolbox
    if (this.sessionContext.type == "all") {
      this.emit("target-available-form", actor);
      return;
    }

    if (actor.isTopLevelTarget) {
      this.emit("target-available-form", actor);
      // Flush any existing early iframe targets
      this._flushIframeTargets(actor.innerWindowId);

      if (this.sessionContext.type == SESSION_TYPES.BROWSER_ELEMENT) {
        // Ignore any pending exception as this request may be pending
        // while the toolbox closes. And we don't want to delay target emission
        // on this as this is a implementation detail.
        this.updateDomainSessionDataForServiceWorkers(actor.url).catch(
          () => {}
        );
      }
    } else if (this._currentWindowGlobalTargets.has(actor.topInnerWindowId)) {
      // Emit the event immediately if the top-level target is already available
      this.emit("target-available-form", actor);
    } else if (this._earlyIframeTargets[actor.topInnerWindowId]) {
      // Add the early iframe target to the list of other early targets.
      this._earlyIframeTargets[actor.topInnerWindowId].push(actor);
    } else {
      // Set the first early iframe target
      this._earlyIframeTargets[actor.topInnerWindowId] = [actor];
    }
  }

  /**
   * Called by a Watcher module, whenever a target has been destroyed
   *
   * @param {object} actor
   *        the actor form of the target being destroyed
   * @param {object} options
   * @param {boolean} options.isModeSwitching
   *        true when this is called as the result of a change to the devtools.browsertoolbox.scope pref
   */
  async notifyTargetDestroyed(actor, options = {}) {
    // Emit immediately for worker, process & extension targets
    // as they don't have a parent browsing context.
    if (!actor.innerWindowId) {
      this.emit("target-destroyed-form", actor, options);
      return;
    }
    // Flush all iframe targets if we are destroying a top level target.
    if (actor.isTopLevelTarget) {
      // First compute the list of children actors, as notifyTargetDestroy will mutate _currentWindowGlobalTargets
      const childrenActors = [
        ...this._currentWindowGlobalTargets.values(),
      ].filter(
        form =>
          form.topInnerWindowId == actor.innerWindowId &&
          // Ignore the top level target itself, because its topInnerWindowId will be its innerWindowId
          form.innerWindowId != actor.innerWindowId
      );
      childrenActors.map(form => this.notifyTargetDestroyed(form, options));
    }
    if (this._earlyIframeTargets[actor.innerWindowId]) {
      delete this._earlyIframeTargets[actor.innerWindowId];
    }
    this._currentWindowGlobalTargets.delete(actor.innerWindowId);
    const documentEventWatcher = Resources.getResourceWatcher(
      this,
      Resources.TYPES.DOCUMENT_EVENT
    );
    // If we have a Watcher class instantiated, ensure that target-destroyed is sent
    // *after* DOCUMENT_EVENT's will-navigate. Otherwise this resource will have an undefined
    // `targetFront` attribute, as it is associated with the target from which we navigate
    // and not the one we navigate to.
    //
    // About documentEventWatcher check: We won't have any watcher class if we aren't
    // using server side Watcher classes.
    // i.e. when we are using the legacy listener for DOCUMENT_EVENT.
    // This is still the case for all toolboxes but the one for local and remote tabs.
    //
    // About isServerTargetSwitchingEnabled check: if we are using the watcher class
    // we may still use client side target, which will still use legacy listeners for
    // will-navigate and so will-navigate will be emitted by the target actor itself.
    //
    // About isTopLevelTarget check: only top level targets emit will-navigate,
    // so there is no reason to delay target-destroy for remote iframes.
    if (
      documentEventWatcher &&
      this.sessionContext.isServerTargetSwitchingEnabled &&
      actor.isTopLevelTarget
    ) {
      await documentEventWatcher.onceWillNavigateIsEmitted(actor.innerWindowId);
    }
    this.emit("target-destroyed-form", actor, options);
  }

  /**
   * Given a browsingContextID, returns its parent browsingContextID. Returns null if a
   * parent browsing context couldn't be found. Throws if the browsing context
   * corresponding to the passed browsingContextID couldn't be found.
   *
   * @param {Integer} browsingContextID
   * @returns {Integer|null}
   */
  getParentBrowsingContextID(browsingContextID) {
    const browsingContext = BrowsingContext.get(browsingContextID);
    if (!browsingContext) {
      throw new Error(
        `BrowsingContext with ID=${browsingContextID} doesn't exist.`
      );
    }
    // Top-level documents of tabs, loaded in a <browser> element expose a null `parent`.
    // i.e. Their BrowsingContext has no parent and is considered top level.
    // But... in the context of the Browser Toolbox, we still consider them as child of the browser window.
    // So, for them, fallback on `embedderWindowGlobal`, which will typically be the WindowGlobal for browser.xhtml.
    if (browsingContext.parent) {
      return browsingContext.parent.id;
    }
    if (browsingContext.embedderWindowGlobal) {
      return browsingContext.embedderWindowGlobal.browsingContext.id;
    }
    return null;
  }

  /**
   * Called by Resource Watchers, when new resources are available, updated or destroyed.
   *
   * @param String updateType
   *        Can be "available", "updated" or "destroyed"
   * @param Array<json> resources
   *        List of all resource's form. A resource is a JSON object piped over to the client.
   *        It can contain actor IDs, actor forms, to be manually marshalled by the client.
   */
  notifyResources(updateType, resources) {
    if (resources.length === 0) {
      // Don't try to emit if the resources array is empty.
      return;
    }

    if (this.sessionContext.type == "webextension") {
      this._overrideResourceBrowsingContextForWebExtension(resources);
    }

    this.emit(`resource-${updateType}-form`, resources);
  }

  /**
   * For WebExtension, we have to hack all resource's browsingContextID
   * in order to ensure emitting them with the fixed, original browsingContextID
   * related to the fallback document created by devtools which always exists.
   * The target's form will always be relating to that BrowsingContext IDs (browsing context ID and inner window id).
   * Even if the target switches internally to another document via WindowGlobalTargetActor._setWindow.
   *
   * @param {Array<Objects>} List of resources
   */
  _overrideResourceBrowsingContextForWebExtension(resources) {
    resources.forEach(resource => {
      resource.browsingContextID = this.sessionContext.addonBrowsingContextID;
    });
  }

  /**
   * Try to retrieve Target Actors instantiated in the parent process which aren't
   * instantiated via the Watcher actor (and its dependencies):
   * - top level target for the browser toolboxes
   * - xpcshell targets for xpcshell debugging
   *
   * See comment in `watchResources`.
   *
   * @return {Set<TargetActor>} Matching target actors.
   */
  getTargetActorsInParentProcess() {
    if (TargetActorRegistry.xpcShellTargetActors.size) {
      return TargetActorRegistry.xpcShellTargetActors;
    }

    // Note: For browser-element debugging, the WindowGlobalTargetActor returned here is created
    // for a parent process page and lives in the parent process.
    const actors = TargetActorRegistry.getTargetActors(
      this.sessionContext,
      this.conn.prefix
    );

    switch (this.sessionContext.type) {
      case "all":
        const parentProcessTargetActor = actors.find(
          actor => actor.typeName === "parentProcessTarget"
        );
        if (parentProcessTargetActor) {
          return new Set([parentProcessTargetActor]);
        }
        return new Set();
      case "browser-element":
      case "webextension":
        // All target actors for browser-element and webextension sessions
        // should be created using the JS Window actors.
        return new Set();
      default:
        throw new Error(
          "Unsupported session context type: " + this.sessionContext.type
        );
    }
  }

  /**
   * Start watching for a list of resource types.
   * This should only resolve once all "already existing" resources of these types
   * are notified to the client via resource-available-form event on related target actors.
   *
   * @param {Array<string>} resourceTypes
   *        List of all types to listen to.
   */
  async watchResources(resourceTypes) {
    // First process resources which have to be listened from the parent process
    // (the watcher actor always runs in the parent process)
    await Resources.watchResources(
      this,
      Resources.getParentProcessResourceTypes(resourceTypes)
    );

    // Bail out early if all resources were watched from parent process.
    // In this scenario, we do not need to update these resource types in the ParentProcessWatcherRegistry
    // as targets do not care about them.
    if (!Resources.hasResourceTypesForTargets(resourceTypes)) {
      return;
    }

    ParentProcessWatcherRegistry.watchResources(this, resourceTypes);

    const promises = [];
    const domProcesses = ChromeUtils.getAllDOMProcesses();
    for (const domProcess of domProcesses) {
      promises.push(
        domProcess
          .getActor(this._jsActorName)
          .addOrSetSessionDataEntry({
            watcherActorID: this.actorID,
            sessionContext: this.sessionContext,
            type: "resources",
            entries: resourceTypes,
            updateType: "add",
          })
          .catch(e => {
            // Ignore any process that got destroyed while trying to send the request
            if (!domProcess.canSend) {
              console.warn(
                "Content process closed while requesting resources",
                domProcess.name,
                domProcess.remoteType
              );
              return;
            }
            throw e;
          })
      );
    }
    await Promise.all(promises);

    // Stop execution if we were destroyed in the meantime
    if (this.isDestroyed()) {
      return;
    }

    /*
     * The Watcher actor doesn't support watching the top level target
     * (bug 1644397 and possibly some other followup).
     *
     * Because of that, we miss reaching these targets in the previous lines of this function.
     * Since all BrowsingContext target actors register themselves to the TargetActorRegistry,
     * we use it here in order to reach those missing targets, which are running in the
     * parent process (where this WatcherActor lives as well):
     *  - the parent process target (which inherits from WindowGlobalTargetActor)
     *  - top level tab target for documents loaded in the parent process (e.g. about:robots).
     *    When the tab loads document in the content process, the FrameTargetHelper will
     *    reach it via the JSWindowActor API. Even if it uses MessageManager for anything
     *    else (RDP packet forwarding, creation and destruction).
     *
     * We will eventually get rid of this code once all targets are properly supported by
     * the Watcher Actor and we have target helpers for all of them.
     */
    const targetActors = this.getTargetActorsInParentProcess();
    for (const targetActor of targetActors) {
      const targetActorResourceTypes = Resources.getResourceTypesForTargetType(
        resourceTypes,
        targetActor.targetType
      );
      await targetActor.addOrSetSessionDataEntry(
        "resources",
        targetActorResourceTypes,
        false,
        "add"
      );
    }
  }

  /**
   * Stop watching for a list of resource types.
   *
   * @param {Array<string>} resourceTypes
   *        List of all types to listen to.
   */
  unwatchResources(resourceTypes) {
    // First process resources which are listened from the parent process
    // (the watcher actor always runs in the parent process)
    Resources.unwatchResources(
      this,
      Resources.getParentProcessResourceTypes(resourceTypes)
    );

    // Bail out early if all resources were all watched from parent process.
    // In this scenario, we do not need to update these resource types in the ParentProcessWatcherRegistry
    // as targets do not care about them.
    if (!Resources.hasResourceTypesForTargets(resourceTypes)) {
      return;
    }

    const isWatchingResources = ParentProcessWatcherRegistry.unwatchResources(
      this,
      resourceTypes
    );
    if (!isWatchingResources) {
      return;
    }

    // Prevent trying to unwatch when the related BrowsingContext has already
    // been destroyed
    if (!this.isContextDestroyed()) {
      const domProcesses = ChromeUtils.getAllDOMProcesses();
      for (const domProcess of domProcesses) {
        domProcess.getActor(this._jsActorName).removeSessionDataEntry({
          watcherActorID: this.actorID,
          sessionContext: this.sessionContext,
          type: "resources",
          entries: resourceTypes,
        });
      }
    }

    // See comment in watchResources.
    const targetActors = this.getTargetActorsInParentProcess();
    for (const targetActor of targetActors) {
      const targetActorResourceTypes = Resources.getResourceTypesForTargetType(
        resourceTypes,
        targetActor.targetType
      );
      targetActor.removeSessionDataEntry("resources", targetActorResourceTypes);
    }
  }

  clearResources(resourceTypes) {
    // First process resources which have to be listened from the parent process
    // (the watcher actor always runs in the parent process)
    // TODO: content process / worker thread resources are not cleared. See Bug 1774573
    Resources.clearResources(
      this,
      Resources.getParentProcessResourceTypes(resourceTypes)
    );
  }

  /**
   * Returns the network actor.
   *
   * @return {Object} actor
   *        The network actor.
   */
  getNetworkParentActor() {
    if (!this._networkParentActor) {
      this._networkParentActor = new NetworkParentActor(this);
    }

    return this._networkParentActor;
  }

  /**
   * Returns the blackboxing actor.
   *
   * @return {Object} actor
   *        The blackboxing actor.
   */
  getBlackboxingActor() {
    if (!this._blackboxingActor) {
      this._blackboxingActor = new BlackboxingActor(this);
    }

    return this._blackboxingActor;
  }

  /**
   * Returns the breakpoint list actor.
   *
   * @return {Object} actor
   *        The breakpoint list actor.
   */
  getBreakpointListActor() {
    if (!this._breakpointListActor) {
      this._breakpointListActor = new BreakpointListActor(this);
    }

    return this._breakpointListActor;
  }

  /**
   * Returns the target configuration actor.
   *
   * @return {Object} actor
   *        The configuration actor.
   */
  getTargetConfigurationActor() {
    if (!this._targetConfigurationListActor) {
      this._targetConfigurationListActor = new TargetConfigurationActor(this);
    }
    return this._targetConfigurationListActor;
  }

  /**
   * Returns the thread configuration actor.
   *
   * @return {Object} actor
   *        The configuration actor.
   */
  getThreadConfigurationActor() {
    if (!this._threadConfigurationListActor) {
      this._threadConfigurationListActor = new ThreadConfigurationActor(this);
    }
    return this._threadConfigurationListActor;
  }

  /**
   * Server internal API, called by other actors, but not by the client.
   * Used to agrement some new entries for a given data type (watchers target, resources,
   * breakpoints,...)
   *
   * @param {String} type
   *        Data type to contribute to.
   * @param {Array<*>} entries
   *        List of values to add or set for this data type.
   * @param {String} updateType
   *        "add" will only add the new entries in the existing data set.
   *        "set" will update the data set with the new entries.
   */
  async addOrSetDataEntry(type, entries, updateType) {
    ParentProcessWatcherRegistry.addOrSetSessionDataEntry(
      this,
      type,
      entries,
      updateType
    );

    const promises = [];
    const domProcesses = ChromeUtils.getAllDOMProcesses();
    for (const domProcess of domProcesses) {
      promises.push(
        domProcess
          .getActor(this._jsActorName)
          .addOrSetSessionDataEntry({
            watcherActorID: this.actorID,
            sessionContext: this.sessionContext,
            type,
            entries,
            updateType,
          })
          .catch(e => {
            // Ignore any process that got destroyed while trying to send the request
            if (!domProcess.canSend) {
              console.warn(
                "Content process closed while sending session data",
                domProcess.name,
                domProcess.remoteType
              );
              return;
            }
            throw e;
          })
      );
    }
    await Promise.all(promises);

    // Stop execution if we were destroyed in the meantime
    if (this.isDestroyed()) {
      return;
    }

    // See comment in watchResources
    const targetActors = this.getTargetActorsInParentProcess();
    for (const targetActor of targetActors) {
      await targetActor.addOrSetSessionDataEntry(
        type,
        entries,
        false,
        updateType
      );
    }
  }

  /**
   * Server internal API, called by other actors, but not by the client.
   * Used to remve some existing entries for a given data type (watchers target, resources,
   * breakpoints,...)
   *
   * @param {String} type
   *        Data type to modify.
   * @param {Array<*>} entries
   *        List of values to remove from this data type.
   */
  removeDataEntry(type, entries) {
    ParentProcessWatcherRegistry.removeSessionDataEntry(this, type, entries);

    const domProcesses = ChromeUtils.getAllDOMProcesses();
    for (const domProcess of domProcesses) {
      domProcess.getActor(this._jsActorName).removeSessionDataEntry({
        watcherActorID: this.actorID,
        sessionContext: this.sessionContext,
        type,
        entries,
      });
    }

    // See comment in addOrSetDataEntry
    const targetActors = this.getTargetActorsInParentProcess();
    for (const targetActor of targetActors) {
      targetActor.removeSessionDataEntry(type, entries);
    }
  }

  /**
   * Retrieve the current watched data for the provided type.
   *
   * @param {String} type
   *        Data type to retrieve.
   */
  getSessionDataForType(type) {
    return this.sessionData?.[type];
  }

  /**
   * Special code dedicated to Service Worker debugging.
   * This will notify the Service Worker JS Process Actors about the new top level page domain.
   * So that we start tracking that domain's workers.
   *
   * @param {String} newTargetUrl
   */
  async updateDomainSessionDataForServiceWorkers(newTargetUrl) {
    let host = "";
    // Accessing `host` can throw on some URLs with no valid host like about:home.
    // In such scenario, reset the host to an empty string.
    try {
      host = new URL(newTargetUrl).host;
    } catch (e) {}

    ParentProcessWatcherRegistry.addOrSetSessionDataEntry(
      this,
      "browser-element-host",
      [host],
      "set"
    );

    return this.addOrSetDataEntry("browser-element-host", [host], "set");
  }
};
