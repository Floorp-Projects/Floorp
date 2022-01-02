/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { ActorClassWithSpec } = require("devtools/shared/protocol");

const Resources = require("devtools/server/actors/resources/index");
const {
  SessionDataHelpers,
} = require("devtools/server/actors/watcher/SessionDataHelpers.jsm");
const { STATES: THREAD_STATES } = require("devtools/server/actors/thread");
const {
  RESOURCES,
  BREAKPOINTS,
  TARGET_CONFIGURATION,
  THREAD_CONFIGURATION,
  XHR_BREAKPOINTS,
  EVENT_BREAKPOINTS,
} = SessionDataHelpers.SUPPORTED_DATA;

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
    // eslint-disable-next-line complexity
    async addSessionDataEntry(type, entries, isDocumentCreation = false) {
      if (type == RESOURCES) {
        await this._watchTargetResources(entries);
      } else if (type == BREAKPOINTS) {
        const isTargetCreation =
          this.threadActor.state == THREAD_STATES.DETACHED;
        if (isTargetCreation && !this.targetType.endsWith("worker")) {
          // If addSessionDataEntry is called during target creation, attach the
          // thread actor automatically and pass the initial breakpoints.
          // However, do not attach the thread actor for Workers. They use a codepath
          // which releases the worker on `attach`. For them, the client will call `attach`. (bug 1691986)
          await this.threadActor.attach({ breakpoints: entries });
        } else {
          // If addSessionDataEntry is called for an existing target, set the new
          // breakpoints on the already running thread actor.
          await Promise.all(
            entries.map(({ location, options }) =>
              this.threadActor.setBreakpoint(location, options)
            )
          );
        }
      } else if (type == TARGET_CONFIGURATION) {
        // Only WindowGlobalTargetActor implements updateTargetConfiguration,
        // skip this data entry update for other targets.
        if (typeof this.updateTargetConfiguration == "function") {
          const options = {};
          for (const { key, value } of entries) {
            options[key] = value;
          }
          this.updateTargetConfiguration(options, isDocumentCreation);
        }
      } else if (type == THREAD_CONFIGURATION) {
        const threadOptions = {};

        for (const { key, value } of entries) {
          threadOptions[key] = value;
        }

        if (
          !this.targetType.endsWith("worker") &&
          this.threadActor.state == THREAD_STATES.DETACHED
        ) {
          await this.threadActor.attach(threadOptions);
        } else {
          await this.threadActor.reconfigure(threadOptions);
        }
      } else if (type == XHR_BREAKPOINTS) {
        // The thread actor has to be initialized in order to correctly
        // retrieve the stack trace when hitting an XHR
        if (
          this.threadActor.state == THREAD_STATES.DETACHED &&
          !this.targetType.endsWith("worker")
        ) {
          await this.threadActor.attach();
        }

        await Promise.all(
          entries.map(({ path, method }) =>
            this.threadActor.setXHRBreakpoint(path, method)
          )
        );
      } else if (type == EVENT_BREAKPOINTS) {
        // Same as comments for XHR breakpoints. See lines 117-118
        if (
          this.threadActor.state == THREAD_STATES.DETACHED &&
          !this.targetType.endsWith("worker")
        ) {
          this.threadActor.attach();
        }
        await this.threadActor.setActiveEventBreakpoints(entries);
      }
    },

    removeSessionDataEntry(type, entries) {
      if (type == RESOURCES) {
        return this._unwatchTargetResources(entries);
      } else if (type == BREAKPOINTS) {
        for (const { location } of entries) {
          this.threadActor.removeBreakpoint(location);
        }
      } else if (type == TARGET_CONFIGURATION || type == THREAD_CONFIGURATION) {
        // configuration data entries are always added/updated, never removed.
      } else if (type == XHR_BREAKPOINTS) {
        for (const { path, method } of entries) {
          this.threadActor.removeXHRBreakpoint(path, method);
        }
      }

      return Promise.resolve();
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
