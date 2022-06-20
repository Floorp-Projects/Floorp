/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";
const protocol = require("devtools/shared/protocol");
const { watcherSpec } = require("devtools/shared/specs/watcher");

const Resources = require("devtools/server/actors/resources/index");
const {
  TargetActorRegistry,
} = require("devtools/server/actors/targets/target-actor-registry.jsm");
const {
  WatcherRegistry,
} = require("devtools/server/actors/watcher/WatcherRegistry.jsm");
const Targets = require("devtools/server/actors/targets/index");
const {
  getAllBrowsingContextsForContext,
} = require("devtools/server/actors/watcher/browsing-context-helpers.jsm");

const TARGET_HELPERS = {};
loader.lazyRequireGetter(
  TARGET_HELPERS,
  Targets.TYPES.FRAME,
  "devtools/server/actors/watcher/target-helpers/frame-helper"
);
loader.lazyRequireGetter(
  TARGET_HELPERS,
  Targets.TYPES.PROCESS,
  "devtools/server/actors/watcher/target-helpers/process-helper"
);
loader.lazyRequireGetter(
  TARGET_HELPERS,
  Targets.TYPES.WORKER,
  "devtools/server/actors/watcher/target-helpers/worker-helper"
);

loader.lazyRequireGetter(
  this,
  "NetworkParentActor",
  "devtools/server/actors/network-monitor/network-parent",
  true
);
loader.lazyRequireGetter(
  this,
  "BlackboxingActor",
  "devtools/server/actors/blackboxing",
  true
);
loader.lazyRequireGetter(
  this,
  "BreakpointListActor",
  "devtools/server/actors/breakpoint-list",
  true
);
loader.lazyRequireGetter(
  this,
  "TargetConfigurationActor",
  "devtools/server/actors/target-configuration",
  true
);
loader.lazyRequireGetter(
  this,
  "ThreadConfigurationActor",
  "devtools/server/actors/thread-configuration",
  true
);

exports.WatcherActor = protocol.ActorClassWithSpec(watcherSpec, {
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
  initialize: function(conn, sessionContext) {
    protocol.Actor.prototype.initialize.call(this, conn);
    this._sessionContext = sessionContext;
    if (sessionContext.type == "browser-element") {
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

    this.notifyResourceAvailable = this.notifyResourceAvailable.bind(this);
    this.notifyResourceDestroyed = this.notifyResourceDestroyed.bind(this);
    this.notifyResourceUpdated = this.notifyResourceUpdated.bind(this);
  },

  get sessionContext() {
    return this._sessionContext;
  },

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
  },

  getAllBrowsingContexts(options) {
    return getAllBrowsingContextsForContext(this.sessionContext, options);
  },

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
  },

  destroy: function() {
    // Force unwatching for all types, even if we weren't watching.
    // This is fine as unwatchTarget is NOOP if we weren't already watching for this target type.
    for (const targetType of Object.values(Targets.TYPES)) {
      this.unwatchTargets(targetType);
    }
    this.unwatchResources(Object.values(Resources.TYPES));

    WatcherRegistry.unregisterWatcher(this);

    // Destroy the actor at the end so that its actorID keeps being defined.
    protocol.Actor.prototype.destroy.call(this);
  },

  /*
   * Get the list of the currently watched resources for this watcher.
   *
   * @return Array<String>
   *         Returns the list of currently watched resource types.
   */
  get sessionData() {
    return WatcherRegistry.getSessionData(this);
  },

  form() {
    return {
      actor: this.actorID,
      // The resources and target traits should be removed all at the same time since the
      // client has generic ways to deal with all of them (See Bug 1680280).
      traits: {
        ...this.sessionContext.supportedTargets,
        resources: this.sessionContext.supportedResources,
        // @backward-compat { version 103 } Clear resources not supported by old servers
        supportsClearResources: true,
      },
    };
  },

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
    WatcherRegistry.watchTargets(this, targetType);

    const targetHelperModule = TARGET_HELPERS[targetType];
    // Await the registration in order to ensure receiving the already existing targets
    await targetHelperModule.createTargets(this);
  },

  /**
   * Stop watching for a given target type.
   *
   * @param {string} targetType
   *        Type of context to observe. See Targets.TYPES object.
   */
  unwatchTargets(targetType) {
    const isWatchingTargets = WatcherRegistry.unwatchTargets(this, targetType);
    if (!isWatchingTargets) {
      return;
    }

    const targetHelperModule = TARGET_HELPERS[targetType];
    targetHelperModule.destroyTargets(this);

    // Unregister the JS Window Actor if there is no more DevTools code observing any target/resource
    WatcherRegistry.maybeUnregisteringJSWindowActor();
  },

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
  },

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
  },

  /**
   * Called by a Watcher module, whenever a target has been destroyed
   */
  async notifyTargetDestroyed(actor) {
    // Emit immediately for worker, process & extension targets
    // as they don't have a parent browsing context.
    if (!actor.innerWindowId) {
      this.emit("target-destroyed-form", actor);
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
      childrenActors.map(form => this.notifyTargetDestroyed(form));
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
    this.emit("target-destroyed-form", actor);
  },

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
  },

  /**
   * Called by Resource Watchers, when new resources are available.
   *
   * @param Array<json> resources
   *        List of all available resources. A resource is a JSON object piped over to the client.
   *        It may contain actor IDs, actor forms, to be manually marshalled by the client.
   */
  notifyResourceAvailable(resources) {
    if (this.sessionContext.type == "webextension") {
      this._overrideResourceBrowsingContextForWebExtension(resources);
    }
    this._emitResourcesForm("resource-available-form", resources);
  },

  notifyResourceDestroyed(resources) {
    if (this.sessionContext.type == "webextension") {
      this._overrideResourceBrowsingContextForWebExtension(resources);
    }
    this._emitResourcesForm("resource-destroyed-form", resources);
  },

  notifyResourceUpdated(resources) {
    if (this.sessionContext.type == "webextension") {
      this._overrideResourceBrowsingContextForWebExtension(resources);
    }
    this._emitResourcesForm("resource-updated-form", resources);
  },

  /**
   * Wrapper around emit for resource forms.
   */
  _emitResourcesForm(name, resources) {
    if (resources.length === 0) {
      // Don't try to emit if the resources array is empty.
      return;
    }
    this.emit(name, resources);
  },

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
  },

  /**
   * Try to retrieve a parent process TargetActor:
   * - either when debugging a parent process page (when browserElement is set to the page's tab),
   * - or when debugging the main process (when browserElement is null), including xpcshell tests
   *
   * See comment in `watchResources`, this will handle targets which are ignored by Frame and Process
   * target helpers. (and only those which are ignored)
   */
  _getTargetActorInParentProcess() {
    // Note: For browser-element debugging, the WindowGlobalTargetActor returned here is created
    // for a parent process page and lives in the parent process.
    return TargetActorRegistry.getTopLevelTargetActorForContext(
      this.sessionContext,
      this.conn.prefix
    );
  },

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
    // In this scenario, we do not need to update these resource types in the WatcherRegistry
    // as targets do not care about them.
    if (!Resources.hasResourceTypesForTargets(resourceTypes)) {
      return;
    }

    WatcherRegistry.watchResources(this, resourceTypes);

    // Fetch resources from all existing targets
    for (const targetType in TARGET_HELPERS) {
      // We process frame targets even if we aren't watching them,
      // because frame target helper codepath handles the top level target, if it runs in the *content* process.
      // It will do another check to `isWatchingTargets(FRAME)` internally.
      // Note that the workaround at the end of this method, using TargetActorRegistry
      // is specific to top level target running in the *parent* process.
      if (
        !WatcherRegistry.isWatchingTargets(this, targetType) &&
        targetType != Targets.TYPES.FRAME
      ) {
        continue;
      }
      const targetResourceTypes = Resources.getResourceTypesForTargetType(
        resourceTypes,
        targetType
      );
      if (targetResourceTypes.length == 0) {
        continue;
      }
      const targetHelperModule = TARGET_HELPERS[targetType];
      await targetHelperModule.addSessionDataEntry({
        watcher: this,
        type: "resources",
        entries: targetResourceTypes,
      });
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
    const frameResourceTypes = Resources.getResourceTypesForTargetType(
      resourceTypes,
      Targets.TYPES.FRAME
    );
    if (frameResourceTypes.length > 0) {
      const targetActor = this._getTargetActorInParentProcess();
      if (targetActor) {
        await targetActor.addSessionDataEntry("resources", frameResourceTypes);
      }
    }
  },

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
    // In this scenario, we do not need to update these resource types in the WatcherRegistry
    // as targets do not care about them.
    if (!Resources.hasResourceTypesForTargets(resourceTypes)) {
      return;
    }

    const isWatchingResources = WatcherRegistry.unwatchResources(
      this,
      resourceTypes
    );
    if (!isWatchingResources) {
      return;
    }

    // Prevent trying to unwatch when the related BrowsingContext has already
    // been destroyed
    if (!this.isContextDestroyed()) {
      for (const targetType in TARGET_HELPERS) {
        // Frame target helper handles the top level target, if it runs in the content process
        // so we should always process it. It does a second check to isWatchingTargets.
        if (
          !WatcherRegistry.isWatchingTargets(this, targetType) &&
          targetType != Targets.TYPES.FRAME
        ) {
          continue;
        }
        const targetResourceTypes = Resources.getResourceTypesForTargetType(
          resourceTypes,
          targetType
        );
        if (targetResourceTypes.length == 0) {
          continue;
        }
        const targetHelperModule = TARGET_HELPERS[targetType];
        targetHelperModule.removeSessionDataEntry({
          watcher: this,
          type: "resources",
          entries: targetResourceTypes,
        });
      }
    }

    // See comment in watchResources.
    const frameResourceTypes = Resources.getResourceTypesForTargetType(
      resourceTypes,
      Targets.TYPES.FRAME
    );
    if (frameResourceTypes.length > 0) {
      const targetActor = this._getTargetActorInParentProcess();
      if (targetActor) {
        targetActor.removeSessionDataEntry("resources", frameResourceTypes);
      }
    }

    // Unregister the JS Window Actor if there is no more DevTools code observing any target/resource
    WatcherRegistry.maybeUnregisteringJSWindowActor();
  },

  clearResources(resourceTypes) {
    // First process resources which have to be listened from the parent process
    // (the watcher actor always runs in the parent process)
    // TODO: content process / worker thread resources are not cleared. See Bug 1774573
    Resources.clearResources(
      this,
      Resources.getParentProcessResourceTypes(resourceTypes)
    );
  },

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
  },

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
  },

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
  },

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
  },

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
  },

  /**
   * Server internal API, called by other actors, but not by the client.
   * Used to agrement some new entries for a given data type (watchers target, resources,
   * breakpoints,...)
   *
   * @param {String} type
   *        Data type to contribute to.
   * @param {Array<*>} entries
   *        List of values to add for this data type.
   */
  async addDataEntry(type, entries) {
    WatcherRegistry.addSessionDataEntry(this, type, entries);

    await Promise.all(
      Object.values(Targets.TYPES)
        .filter(
          targetType =>
            // We process frame targets even if we aren't watching them,
            // because frame target helper codepath handles the top level target, if it runs in the *content* process.
            // It will do another check to `isWatchingTargets(FRAME)` internally.
            // Note that the workaround at the end of this method, using TargetActorRegistry
            // is specific to top level target running in the *parent* process.
            WatcherRegistry.isWatchingTargets(this, targetType) ||
            targetType === Targets.TYPES.FRAME
        )
        .map(async targetType => {
          const targetHelperModule = TARGET_HELPERS[targetType];
          await targetHelperModule.addSessionDataEntry({
            watcher: this,
            type,
            entries,
          });
        })
    );

    // See comment in watchResources
    const targetActor = this._getTargetActorInParentProcess();
    if (targetActor) {
      await targetActor.addSessionDataEntry(type, entries);
    }
  },

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
    WatcherRegistry.removeSessionDataEntry(this, type, entries);

    Object.values(Targets.TYPES)
      .filter(
        targetType =>
          // See comment in addDataEntry
          WatcherRegistry.isWatchingTargets(this, targetType) ||
          targetType === Targets.TYPES.FRAME
      )
      .forEach(targetType => {
        const targetHelperModule = TARGET_HELPERS[targetType];
        targetHelperModule.removeSessionDataEntry({
          watcher: this,
          type,
          entries,
        });
      });

    // See comment in addDataEntry
    const targetActor = this._getTargetActorInParentProcess();
    if (targetActor) {
      targetActor.removeSessionDataEntry(type, entries);
    }
  },

  /**
   * Retrieve the current watched data for the provided type.
   *
   * @param {String} type
   *        Data type to retrieve.
   */
  getSessionDataForType(type) {
    return this.sessionData?.[type];
  },
});
