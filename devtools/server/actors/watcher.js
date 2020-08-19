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

const TARGET_HELPERS = {};
loader.lazyRequireGetter(
  TARGET_HELPERS,
  Targets.TYPES.FRAME,
  "devtools/server/actors/watcher/target-helpers/frame-helper"
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

    // Destroy the actor at the end so that its actorID keeps being defined.
    protocol.Actor.prototype.destroy.call(this);
  },

  /*
   * Get the list of the currently watched resources for this watcher.
   *
   * @return Array<String>
   *         Returns the list of currently watched resource types.
   */
  get watchedResources() {
    return WatcherRegistry.getWatchedResources(this);
  },

  form() {
    const hasBrowserElement = !!this.browserElement;

    return {
      actor: this.actorID,
      traits: {
        // FF77+ supports frames in Watcher actor
        frame: true,
        resources: {
          // FF81+ (bug 1642295) added support for:
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
          [Resources.TYPES.ERROR_MESSAGE]: hasBrowserElement,
          [Resources.TYPES.PLATFORM_MESSAGE]: true,
        },
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

    const watchedResources = WatcherRegistry.getWatchedResources(this);
    const targetHelperModule = TARGET_HELPERS[targetType];
    // Await the registration in order to ensure receiving the already existing targets
    await targetHelperModule.createTargets(this, watchedResources);
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
    this.emit("resource-available-form", resources);
  },

  notifyResourceDestroyed(resources) {
    this.emit("resource-destroyed-form", resources);
  },

  notifyResourceUpdated(resources) {
    this.emit("resource-updated-form", resources);
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
    Resources.watchResources(
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
      const targetHelperModule = TARGET_HELPERS[targetType];
      await targetHelperModule.watchResources({
        watcher: this,
        resourceTypes: targetResourceTypes,
      });
    }

    /*
     * The Watcher actor doesn't support watching for targets other than frame targets yet:
     *  - process targets (bug 1620248)
     *  - worker targets (bug 1633712)
     *  - top level tab target (bug 1644397 and possibly some other followup).
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
    const targetActor = this.browserElement
      ? TargetActorRegistry.getTargetActor(this.browserId)
      : TargetActorRegistry.getParentProcessTargetActor();
    if (targetActor) {
      await targetActor.watchTargetResources(frameResourceTypes);
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
        const targetHelperModule = TARGET_HELPERS[targetType];
        targetHelperModule.unwatchResources({
          watcher: this,
          resourceTypes: targetResourceTypes,
        });
      }
    }

    // See comment in watchResources.
    const frameResourceTypes = Resources.getResourceTypesForTargetType(
      resourceTypes,
      Targets.TYPES.FRAME
    );
    const targetActor = this.browserElement
      ? TargetActorRegistry.getTargetActor(this.browserId)
      : TargetActorRegistry.getParentProcessTargetActor();
    if (targetActor) {
      targetActor.unwatchTargetResources(frameResourceTypes);
    }

    // Unregister the JS Window Actor if there is no more DevTools code observing any target/resource
    WatcherRegistry.maybeUnregisteringJSWindowActor();
  },
});
