/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { ActorClassWithSpec } = require("devtools/shared/protocol");

const Resources = require("devtools/server/actors/resources/index");

loader.lazyRequireGetter(
  this,
  "SessionData",
  "devtools/server/actors/targets/session-data/index",
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
      const processor = SessionData[type];
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
      const processor = SessionData[type];
      if (processor) {
        processor.removeSessionDataEntry(this, entries);
      }
    },

    /**
     * These two methods will create and destroy resource watchers
     * for each resource type. This will end up calling `notifyResourceAvailable`
     * whenever new resources are observed.
     *
     * We have these shortcut methods in this module, because this is called from DevToolsFrameChild
     * which is a JSM and doesn't have a reference to a DevTools Loader.
     */
    _watchTargetResources(resourceTypes) {
      return Resources.watchResources(this, resourceTypes);
    },

    _unwatchTargetResources(resourceTypes) {
      return Resources.unwatchResources(this, resourceTypes);
    },

    /**
     * Called by Watchers, when new resources are available.
     *
     * @param Array<json> resources
     *        List of all available resources. A resource is a JSON object piped over to the client.
     *        It may contain actor IDs, actor forms, to be manually marshalled by the client.
     */
    notifyResourceAvailable(resources) {
      if (this.devtoolsSpawnedBrowsingContextForWebExtension) {
        this.overrideResourceBrowsingContextForWebExtension(resources);
      }
      this._emitResourcesForm("resource-available-form", resources);
    },

    notifyResourceDestroyed(resources) {
      if (this.devtoolsSpawnedBrowsingContextForWebExtension) {
        this.overrideResourceBrowsingContextForWebExtension(resources);
      }
      this._emitResourcesForm("resource-destroyed-form", resources);
    },

    notifyResourceUpdated(resources) {
      if (this.devtoolsSpawnedBrowsingContextForWebExtension) {
        this.overrideResourceBrowsingContextForWebExtension(resources);
      }
      this._emitResourcesForm("resource-updated-form", resources);
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

    /**
     * Wrapper around emit for resource forms to bail early after destroy.
     */
    _emitResourcesForm(name, resources) {
      if (resources.length === 0 || this.isDestroyed()) {
        // Don't try to emit if the resources array is empty or the actor was
        // destroyed.
        return;
      }
      this.emit(name, resources);
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
    this.notifyResourceAvailable = this.notifyResourceAvailable.bind(this);
    this.notifyResourceDestroyed = this.notifyResourceDestroyed.bind(this);
    this.notifyResourceUpdated = this.notifyResourceUpdated.bind(this);

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
