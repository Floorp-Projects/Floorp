/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";
const protocol = require("devtools/shared/protocol");
const { watcherSpec } = require("devtools/shared/specs/watcher");
const Services = require("Services");

const Resources = require("devtools/server/actors/resources/index");
const {
  TargetActorRegistry,
} = require("devtools/server/actors/targets/target-actor-registry.jsm");
const {
  WatcherRegistry,
} = require("devtools/server/actors/watcher/WatcherRegistry.jsm");
const Targets = require("devtools/server/actors/targets/index");

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

exports.WatcherActor = protocol.ActorClassWithSpec(watcherSpec, {
  /**
   * Optionally pass a `browser` in the second argument
   * in order to focus only on targets related to a given <browser> element.
   */
  initialize: function(conn, options) {
    protocol.Actor.prototype.initialize.call(this, conn);
    this._browser = options && options.browser;

    this.notifyResourceAvailable = this.notifyResourceAvailable.bind(this);
    this.notifyResourceDestroyed = this.notifyResourceDestroyed.bind(this);
    this.notifyResourceUpdated = this.notifyResourceUpdated.bind(this);
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
    return this._browser;
  },

  /**
   * Unique identifier, which helps designates one precise browser element, the one
   * we may debug. This is only set if we actually debug a browser element.
   * So, that will be typically set when we debug a tab, but not when we debug
   * a process, or a worker.
   */
  get browserId() {
    return this._browser?.browserId;
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
  get watchedData() {
    return WatcherRegistry.getWatchedData(this);
  },

  form() {
    const enableServerWatcher = Services.prefs.getBoolPref(
      "devtools.testing.enableServerWatcherSupport",
      false
    );

    const hasBrowserElement = !!this.browserElement;

    return {
      actor: this.actorID,
      // The resources and target traits should be removed all at the same time since the
      // client has generic ways to deal with all of them (See Bug 1680280).
      traits: {
        [Targets.TYPES.FRAME]: true,
        [Targets.TYPES.PROCESS]: true,
        [Targets.TYPES.WORKER]: hasBrowserElement,
        resources: {
          // In Firefox 81 we added support for:
          // - CONSOLE_MESSAGE
          // - CSS_CHANGE
          // - CSS_MESSAGE
          // - DOCUMENT_EVENT
          // - ERROR_MESSAGE
          // - PLATFORM_MESSAGE
          //
          // We enabled them for content toolboxes only because we don't support
          // content process targets yet. Bug 1620248 should help supporting
          // them and enable this more broadly.
          //
          // New server-side resources can be gated behind
          // `devtools.testing.enableServerWatcherSupport` if needed.
          [Resources.TYPES.CONSOLE_MESSAGE]: hasBrowserElement,
          [Resources.TYPES.CSS_CHANGE]: hasBrowserElement,
          [Resources.TYPES.CSS_MESSAGE]: hasBrowserElement,
          [Resources.TYPES.DOCUMENT_EVENT]: hasBrowserElement,
          [Resources.TYPES.CACHE_STORAGE]: hasBrowserElement,
          [Resources.TYPES.ERROR_MESSAGE]: hasBrowserElement,
          [Resources.TYPES.LOCAL_STORAGE]: hasBrowserElement,
          [Resources.TYPES.SESSION_STORAGE]: hasBrowserElement,
          [Resources.TYPES.PLATFORM_MESSAGE]: true,
          [Resources.TYPES.NETWORK_EVENT]: hasBrowserElement,
          [Resources.TYPES.NETWORK_EVENT_STACKTRACE]: hasBrowserElement,
          [Resources.TYPES.STYLESHEET]:
            enableServerWatcher && hasBrowserElement,
          [Resources.TYPES.SOURCE]: hasBrowserElement,
          [Resources.TYPES.THREAD_STATE]: hasBrowserElement,
          [Resources.TYPES.SERVER_SENT_EVENT]: hasBrowserElement,
          [Resources.TYPES.WEBSOCKET]: hasBrowserElement,
        },
        // @backward-compat { version 85 } When removing this trait, consumers using
        // the TargetList to retrieve the Breakpoints front should still be careful to check
        // that the Watcher is available
        "set-breakpoints": true,
        // @backward-compat { version 87 } Starting with FF87, if the watcher is
        // supported, the TargetConfiguration actor can be used to set configuration
        // flags which impact BrowsingContext targets.
        // When removing this trait, consumers should still check that the Watcher is
        // available.
        "target-configuration": true,
        // @backward-compat { version 88 } Watcher now supports setting the XHR via
        // the BreakpointListActor.
        // When removing this trait, consumers should still check that the Watcher is
        // available.
        "set-xhr-breakpoints": true,
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
   * Called by a Watcher module, whenever a new target is available
   */
  notifyTargetAvailable(actor) {
    this.emit("target-available-form", actor);
  },

  /**
   * Called by a Watcher module, whenever a target has been destroyed
   */
  notifyTargetDestroyed(actor) {
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
    this._emitResourcesForm("resource-available-form", resources);
  },

  notifyResourceDestroyed(resources) {
    this._emitResourcesForm("resource-destroyed-form", resources);
  },

  notifyResourceUpdated(resources) {
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
   * Try to retrieve a parent process TargetActor:
   * - either when debugging a parent process page (when browserElement is set to the page's tab),
   * - or when debugging the main process (when browserElement is null).
   *
   * See comment in `watchResources`, this will handle targets which are ignored by Frame and Process
   * target helpers. (and only those which are ignored)
   */
  _getTargetActorInParentProcess() {
    return this.browserElement
      ? // Note: if any, the BrowsingContextTargetActor returned here is created for a parent process
        // page and lives in the parent process.
        TargetActorRegistry.getTargetActor(this.browserId)
      : TargetActorRegistry.getParentProcessTargetActor();
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
      await targetHelperModule.addWatcherDataEntry({
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
     *  - the parent process target (which inherits from BrowsingContextTargetActor)
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
        await targetActor.addWatcherDataEntry("resources", frameResourceTypes);
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
    if (!this.browserElement || this.browserElement.browsingContext) {
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
        targetHelperModule.removeWatcherDataEntry({
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
        targetActor.removeWatcherDataEntry("resources", frameResourceTypes);
      }
    }

    // Unregister the JS Window Actor if there is no more DevTools code observing any target/resource
    WatcherRegistry.maybeUnregisteringJSWindowActor();
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
   * Returns the configuration actor.
   *
   * @return {Object} actor
   *        The configuration actor.
   */
  getTargetConfigurationActor() {
    if (!this._configurationListActor) {
      this._configurationListActor = new TargetConfigurationActor(this);
    }
    return this._configurationListActor;
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
    WatcherRegistry.addWatcherDataEntry(this, type, entries);

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
          await targetHelperModule.addWatcherDataEntry({
            watcher: this,
            type,
            entries,
          });
        })
    );

    // See comment in watchResources
    const targetActor = this._getTargetActorInParentProcess();
    if (targetActor) {
      await targetActor.addWatcherDataEntry(type, entries);
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
    WatcherRegistry.removeWatcherDataEntry(this, type, entries);

    Object.values(Targets.TYPES)
      .filter(
        targetType =>
          // See comment in addDataEntry
          WatcherRegistry.isWatchingTargets(this, targetType) ||
          targetType === Targets.TYPES.FRAME
      )
      .forEach(targetType => {
        const targetHelperModule = TARGET_HELPERS[targetType];
        targetHelperModule.removeWatcherDataEntry({
          watcher: this,
          type,
          entries,
        });
      });

    // See comment in addDataEntry
    const targetActor = this._getTargetActorInParentProcess();
    if (targetActor) {
      targetActor.removeWatcherDataEntry(type, entries);
    }
  },

  /**
   * Retrieve the current watched data for the provided type.
   *
   * @param {String} type
   *        Data type to retrieve.
   */
  getWatchedData(type) {
    return this.watchedData?.[type];
  },
});
