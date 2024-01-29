/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  TYPES: { CSS_REGISTERED_PROPERTIES },
} = require("resource://devtools/server/actors/resources/index.js");

/**
 * @typedef InspectorCSSPropertyDefinition (see InspectorUtils.webidl)
 * @type {object}
 * @property {string} name
 * @property {string} syntax
 * @property {boolean} inherits
 * @property {string} initialValue
 * @property {boolean} fromJS - true if property was registered via CSS.registerProperty
 */

class CSSRegisteredPropertiesWatcher {
  #abortController;
  #onAvailable;
  #onUpdated;
  #onDestroyed;
  #registeredPropertiesCache = new Map();
  #styleSheetsManager;
  #targetActor;

  /**
   * Start watching for all registered CSS properties (@property/CSS.registerProperty)
   * related to a given Target Actor.
   *
   * @param TargetActor targetActor
   *        The target actor from which we should observe css changes.
   * @param Object options
   *        Dictionary object with following attributes:
   *        - onAvailable: mandatory function
   *        - onUpdated: mandatory function
   *        - onDestroyed: mandatory function
   *          This will be called for each resource.
   */
  async watch(targetActor, { onAvailable, onUpdated, onDestroyed }) {
    this.#targetActor = targetActor;
    this.#onAvailable = onAvailable;
    this.#onUpdated = onUpdated;
    this.#onDestroyed = onDestroyed;

    // Notify about existing properties
    const registeredProperties = this.#getRegisteredProperties();
    for (const registeredProperty of registeredProperties) {
      this.#registeredPropertiesCache.set(
        registeredProperty.name,
        registeredProperty
      );
    }

    this.#notifyResourcesAvailable(registeredProperties);

    // Listen for new properties being registered via CSS.registerProperty
    this.#abortController = new AbortController();
    const { signal } = this.#abortController;
    this.#targetActor.chromeEventHandler.addEventListener(
      "csscustompropertyregistered",
      this.#onCssCustomPropertyRegistered,
      { capture: true, signal }
    );

    // Watch for stylesheets being added/modified or destroyed, but don't handle existing
    // stylesheets, as we already have the existing properties from this.#getRegisteredProperties.
    this.#styleSheetsManager = targetActor.getStyleSheetsManager();
    await this.#styleSheetsManager.watch({
      onAvailable: this.#refreshCacheAndNotify,
      onUpdated: this.#refreshCacheAndNotify,
      onDestroyed: this.#refreshCacheAndNotify,
      ignoreExisting: true,
    });
  }

  /**
   * Get all the registered properties for the target actor document.
   *
   * @returns Array<InspectorCSSPropertyDefinition>
   */
  #getRegisteredProperties() {
    return InspectorUtils.getCSSRegisteredProperties(
      this.#targetActor.window.document
    );
  }

  /**
   * Compute a resourceId from a given property definition
   *
   * @param {InspectorCSSPropertyDefinition} propertyDefinition
   * @returns string
   */
  #getRegisteredPropertyResourceId(propertyDefinition) {
    return `${this.#targetActor.actorID}:css-registered-property:${
      propertyDefinition.name
    }`;
  }

  /**
   * Called when a stylesheet is added, removed or modified.
   * This will retrieve the registered properties at this very moment, and notify
   * about new, updated and removed registered properties.
   */
  #refreshCacheAndNotify = async () => {
    const registeredProperties = this.#getRegisteredProperties();
    const existingPropertiesNames = new Set(
      this.#registeredPropertiesCache.keys()
    );

    const added = [];
    const updated = [];
    const removed = [];

    for (const registeredProperty of registeredProperties) {
      // If the property isn't in the cache already, this is a new one.
      if (!this.#registeredPropertiesCache.has(registeredProperty.name)) {
        added.push(registeredProperty);
        this.#registeredPropertiesCache.set(
          registeredProperty.name,
          registeredProperty
        );
        continue;
      }

      // Removing existing property from the Set so we can then later get the properties
      // that don't exist anymore.
      existingPropertiesNames.delete(registeredProperty.name);

      // The property already existed, so we need to check if its definition was modified
      const cachedRegisteredProperty = this.#registeredPropertiesCache.get(
        registeredProperty.name
      );

      const resourceUpdates = {};
      let wasUpdated = false;
      if (registeredProperty.syntax !== cachedRegisteredProperty.syntax) {
        resourceUpdates.syntax = registeredProperty.syntax;
        wasUpdated = true;
      }
      if (registeredProperty.inherits !== cachedRegisteredProperty.inherits) {
        resourceUpdates.inherits = registeredProperty.inherits;
        wasUpdated = true;
      }
      if (
        registeredProperty.initialValue !==
        cachedRegisteredProperty.initialValue
      ) {
        resourceUpdates.initialValue = registeredProperty.initialValue;
        wasUpdated = true;
      }

      if (wasUpdated === true) {
        updated.push({
          registeredProperty,
          resourceUpdates,
        });
        this.#registeredPropertiesCache.set(
          registeredProperty.name,
          registeredProperty
        );
      }
    }

    // If there are items left in the Set, it means they weren't processed in the for loop
    // before, meaning they don't exist anymore.
    for (const registeredPropertyName of existingPropertiesNames) {
      removed.push(this.#registeredPropertiesCache.get(registeredPropertyName));
      this.#registeredPropertiesCache.delete(registeredPropertyName);
    }

    this.#notifyResourcesAvailable(added);
    this.#notifyResourcesUpdated(updated);
    this.#notifyResourcesDestroyed(removed);
  };

  /**
   * csscustompropertyregistered event listener callback (fired when a property
   * is registered via CSS.registerProperty).
   *
   * @param {CSSCustomPropertyRegisteredEvent} event
   */
  #onCssCustomPropertyRegistered = event => {
    // Ignore event if property was registered from a global different from the target global.
    if (
      this.#targetActor.ignoreSubFrames &&
      event.target.ownerGlobal !== this.#targetActor.window
    ) {
      return;
    }

    const registeredProperty = event.propertyDefinition;
    this.#registeredPropertiesCache.set(
      registeredProperty.name,
      registeredProperty
    );
    this.#notifyResourcesAvailable([registeredProperty]);
  };

  /**
   * @param {Array<InspectorCSSPropertyDefinition>} registeredProperties
   */
  #notifyResourcesAvailable = registeredProperties => {
    if (!registeredProperties.length) {
      return;
    }

    for (const registeredProperty of registeredProperties) {
      registeredProperty.resourceId =
        this.#getRegisteredPropertyResourceId(registeredProperty);
      registeredProperty.resourceType = CSS_REGISTERED_PROPERTIES;
    }
    this.#onAvailable(registeredProperties);
  };

  /**
   * @param {Array<Object>} updates: Array of update object, which have the following properties:
   *        - {InspectorCSSPropertyDefinition} registeredProperty: The property definition
   *                                            of the updated property
   *        - {Object} resourceUpdates: An object containing all the fields that are
   *                                    modified for the registered property.
   */
  #notifyResourcesUpdated = updates => {
    if (!updates.length) {
      return;
    }

    for (const update of updates) {
      update.resourceId = this.#getRegisteredPropertyResourceId(
        update.registeredProperty
      );
      update.resourceType = CSS_REGISTERED_PROPERTIES;
      // We don't need to send the property definition
      delete update.registeredProperty;
    }

    this.#onUpdated(updates);
  };

  /**
   * @param {Array<InspectorCSSPropertyDefinition>} registeredProperties
   */
  #notifyResourcesDestroyed = registeredProperties => {
    if (!registeredProperties.length) {
      return;
    }

    this.#onDestroyed(
      registeredProperties.map(registeredProperty => ({
        resourceType: CSS_REGISTERED_PROPERTIES,
        resourceId: this.#getRegisteredPropertyResourceId(registeredProperty),
      }))
    );
  };

  destroy() {
    this.#styleSheetsManager.unwatch({
      onAvailable: this.#refreshCacheAndNotify,
      onUpdated: this.#refreshCacheAndNotify,
      onDestroyed: this.#refreshCacheAndNotify,
    });

    this.#abortController.abort();
  }
}

module.exports = CSSRegisteredPropertiesWatcher;
