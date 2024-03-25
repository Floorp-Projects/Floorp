/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Actor } = require("resource://devtools/shared/protocol.js");
const {
  TYPES,
  getResourceWatcher,
} = require("resource://devtools/server/actors/resources/index.js");
const Targets = require("devtools/server/actors/targets/index");

loader.lazyRequireGetter(
  this,
  "SessionDataProcessors",
  "resource://devtools/server/actors/targets/session-data-processors/index.js",
  true
);

class BaseTargetActor extends Actor {
  constructor(conn, targetType, spec) {
    super(conn, spec);

    /**
     * Type of target, a string of Targets.TYPES.
     * @return {string}
     */
    this.targetType = targetType;
  }

  /**
   * Process a new data entry, which can be watched resources, breakpoints, ...
   *
   * @param string type
   *        The type of data to be added
   * @param Array<Object> entries
   *        The values to be added to this type of data
   * @param Boolean isDocumentCreation
   *        Set to true if this function is called just after a new document (and its
   *        associated target) is created.
   * @param String updateType
   *        "add" will only add the new entries in the existing data set.
   *        "set" will update the data set with the new entries.
   */
  async addOrSetSessionDataEntry(
    type,
    entries,
    isDocumentCreation = false,
    updateType
  ) {
    const processor = SessionDataProcessors[type];
    if (processor) {
      await processor.addOrSetSessionDataEntry(
        this,
        entries,
        isDocumentCreation,
        updateType
      );
    }
  }

  /**
   * Remove data entries that have been previously added via addOrSetSessionDataEntry
   *
   * See addOrSetSessionDataEntry for argument description.
   */
  removeSessionDataEntry(type, entries) {
    const processor = SessionDataProcessors[type];
    if (processor) {
      processor.removeSessionDataEntry(this, entries);
    }
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
    if (resources.length === 0 || this.isDestroyed()) {
      // Don't try to emit if the resources array is empty or the actor was
      // destroyed.
      return;
    }

    if (this.devtoolsSpawnedBrowsingContextForWebExtension) {
      this.overrideResourceBrowsingContextForWebExtension(resources);
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
  overrideResourceBrowsingContextForWebExtension(resources) {
    const browsingContextID =
      this.devtoolsSpawnedBrowsingContextForWebExtension.id;
    resources.forEach(
      resource => (resource.browsingContextID = browsingContextID)
    );
  }

  // List of actor prefixes (string) which have already been instantiated via getTargetScopedActor method.
  #instantiatedTargetScopedActors = new Set();

  /**
   * Try to return any target scoped actor instance, if it exists.
   * They are lazily instantiated and so will only be available
   * if the client called at least one of their method.
   *
   * @param {String} prefix
   *        Prefix for the actor we would like to retrieve.
   *        Defined in devtools/server/actors/utils/actor-registry.js
   */
  getTargetScopedActor(prefix) {
    if (this.isDestroyed()) {
      return null;
    }
    const form = this.form();
    this.#instantiatedTargetScopedActors.add(prefix);
    return this.conn._getOrCreateActor(form[prefix + "Actor"]);
  }

  /**
   * Returns true, if the related target scoped actor has already been queried
   * and instantiated via `getTargetScopedActor` method.
   *
   * @param {String} prefix
   *        See getTargetScopedActor definition
   * @return Boolean
   *         True, if the actor has already been instantiated.
   */
  hasTargetScopedActor(prefix) {
    return this.#instantiatedTargetScopedActors.has(prefix);
  }

  /**
   * Apply target-specific options.
   *
   * This will be called by the watcher when the DevTools target-configuration
   * is updated, or when a target is created via JSWindowActors.
   *
   * @param {JSON} options
   *        Configuration object provided by the client.
   *        See target-configuration actor.
   * @param {Boolean} calledFromDocumentCreate
   *        True, when this is called with initial configuration when the related target
   *        actor is instantiated.
   */
  updateTargetConfiguration(options = {}, calledFromDocumentCreation = false) {
    // If there is some tracer options, we should start tracing, otherwise we should stop (if we were)
    if (options.tracerOptions) {
      // Ignore the SessionData update if the user requested to start the tracer on next page load and:
      //   - we apply it to an already loaded WindowGlobal,
      //   - the target isn't the top level one.
      if (
        options.tracerOptions.traceOnNextLoad &&
        (!calledFromDocumentCreation || !this.isTopLevelTarget)
      ) {
        if (this.isTopLevelTarget) {
          const consoleMessageWatcher = getResourceWatcher(
            this,
            TYPES.CONSOLE_MESSAGE
          );
          if (consoleMessageWatcher) {
            consoleMessageWatcher.emitMessages([
              {
                arguments: [
                  "Waiting for next navigation or page reload before starting tracing",
                ],
                styles: [],
                level: "jstracer",
                chromeContext: false,
                timeStamp: ChromeUtils.dateNow(),
              },
            ]);
          }
        }
        return;
      }
      // Bug 1874204: For now, in the browser toolbox, only frame and workers are traced.
      // Content process targets are ignored as they would also include each document/frame target.
      // This would require some work to ignore FRAME targets from here, only in case of browser toolbox,
      // and also handle all content process documents for DOM Event logging.
      //
      // Bug 1874219: Also ignore extensions for now as they are all running in the same process,
      // whereas we can only spawn one tracer per thread.
      if (
        this.targetType == Targets.TYPES.PROCESS ||
        this.url?.startsWith("moz-extension://")
      ) {
        return;
      }
      // In the browser toolbox, when debugging the parent process, we should only toggle the tracer in the Parent Process Target Actor.
      // We have to ignore any frame target which may run in the parent process.
      // For example DevTools documents or a tab running in the parent process.
      // (PROCESS_TYPE_DEFAULT refers to the parent process)
      if (
        this.sessionContext.type == "all" &&
        this.targetType === Targets.TYPES.FRAME &&
        this.typeName != "parentProcessTarget" &&
        Services.appinfo.processType == Services.appinfo.PROCESS_TYPE_DEFAULT
      ) {
        return;
      }
      const tracerActor = this.getTargetScopedActor("tracer");
      tracerActor.startTracing(options.tracerOptions);
    } else if (this.hasTargetScopedActor("tracer")) {
      const tracerActor = this.getTargetScopedActor("tracer");
      tracerActor.stopTracing();
    }
  }
}
exports.BaseTargetActor = BaseTargetActor;
