/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Actor } = require("resource://devtools/shared/protocol.js");

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
    return this.conn._getOrCreateActor(form[prefix + "Actor"]);
  }
}
exports.BaseTargetActor = BaseTargetActor;
