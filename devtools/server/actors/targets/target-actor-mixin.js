/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { ActorClassWithSpec } = require("devtools/shared/protocol");

loader.lazyRequireGetter(
  this,
  "SessionDataProcessors",
  "devtools/server/actors/targets/session-data-processors/index",
  true
);
loader.lazyRequireGetter(
  this,
  "StyleSheetsManager",
  "devtools/server/actors/utils/stylesheets-manager",
  true
);

module.exports = function(targetType, targetActorSpec, implementation) {
  const proto = {
    /**
     * Type of target, a string of Targets.TYPES.
     * @return {string}
     */
    targetType,

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
     */
    async addSessionDataEntry(type, entries, isDocumentCreation = false) {
      const processor = SessionDataProcessors[type];
      if (processor) {
        await processor.addSessionDataEntry(this, entries, isDocumentCreation);
      }
    },

    /**
     * Remove data entries that have been previously added via addSessionDataEntry
     *
     * See addSessionDataEntry for argument description.
     */
    removeSessionDataEntry(type, entries) {
      const processor = SessionDataProcessors[type];
      if (processor) {
        processor.removeSessionDataEntry(this, entries);
      }
    },

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
    overrideResourceBrowsingContextForWebExtension(resources) {
      const browsingContextID = this
        .devtoolsSpawnedBrowsingContextForWebExtension.id;
      resources.forEach(
        resource => (resource.browsingContextID = browsingContextID)
      );
    },

    getStyleSheetManager() {
      if (!this._styleSheetManager) {
        this._styleSheetManager = new StyleSheetsManager(this);
      }
      return this._styleSheetManager;
    },
  };
  // Use getOwnPropertyDescriptors in order to prevent calling getter from implementation
  Object.defineProperties(
    proto,
    Object.getOwnPropertyDescriptors(implementation)
  );
  proto.initialize = function() {
    if (typeof implementation.initialize == "function") {
      implementation.initialize.apply(this, arguments);
    }
  };
  proto.destroy = function() {
    if (this._styleSheetManager) {
      this._styleSheetManager.destroy();
      this._styleSheetManager = null;
    }

    if (typeof implementation.destroy == "function") {
      implementation.destroy.apply(this, arguments);
    }
  };
  return ActorClassWithSpec(targetActorSpec, proto);
};
