/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const EventEmitter = require("resource://devtools/shared/event-emitter.js");

/**
 * The CustomElementWatcher can be used to be notified if a custom element definition
 * is created for a node.
 *
 * When a custom element is defined for a monitored name, an "element-defined" event is
 * fired with the following Object argument:
 * - {String} name: name of the custom element defined
 * - {Set} Set of impacted node actors
 */
class CustomElementWatcher extends EventEmitter {
  constructor(chromeEventHandler) {
    super();

    this.chromeEventHandler = chromeEventHandler;
    this._onCustomElementDefined = this._onCustomElementDefined.bind(this);
    this.chromeEventHandler.addEventListener(
      "customelementdefined",
      this._onCustomElementDefined
    );

    /**
     * Each window keeps its own custom element registry, all of them are watched
     * separately. The struture of the watchedRegistries is as follows
     *
     * WeakMap(
     *   registry -> Map (
     *     name -> Set(NodeActors)
     *   )
     * )
     */
    this.watchedRegistries = new WeakMap();
  }

  destroy() {
    this.watchedRegistries = null;
    this.chromeEventHandler.removeEventListener(
      "customelementdefined",
      this._onCustomElementDefined
    );
  }

  /**
   * Watch for custom element definitions matching the name of the provided NodeActor.
   */
  manageNode(nodeActor) {
    if (!this._isValidNode(nodeActor)) {
      return;
    }

    if (!this._shouldWatchDefinition(nodeActor)) {
      return;
    }

    const registry = nodeActor.rawNode.ownerGlobal.customElements;
    const registryMap = this._getMapForRegistry(registry);

    const name = nodeActor.rawNode.localName;
    const actorsSet = this._getActorsForName(name, registryMap);
    actorsSet.add(nodeActor);
  }

  /**
   * Stop watching the provided NodeActor.
   */
  unmanageNode(nodeActor) {
    if (!this._isValidNode(nodeActor)) {
      return;
    }

    const win = nodeActor.rawNode.ownerGlobal;
    const registry = win.customElements;
    const registryMap = this._getMapForRegistry(registry);
    const name = nodeActor.rawNode.localName;
    if (registryMap.has(name)) {
      registryMap.get(name).delete(nodeActor);
    }
  }

  /**
   * Retrieve the map of name->nodeActors for a given CustomElementsRegistry.
   * Will create the map if not created yet.
   */
  _getMapForRegistry(registry) {
    if (!this.watchedRegistries.has(registry)) {
      this.watchedRegistries.set(registry, new Map());
    }
    return this.watchedRegistries.get(registry);
  }

  /**
   * Retrieve the set of nodeActors for a given name and registry.
   * Will create the set if not created yet.
   */
  _getActorsForName(name, registryMap) {
    if (!registryMap.has(name)) {
      registryMap.set(name, new Set());
    }
    return registryMap.get(name);
  }

  _shouldWatchDefinition(nodeActor) {
    const doc = nodeActor.rawNode.ownerDocument;
    const namespaceURI = doc.documentElement.namespaceURI;
    const name = nodeActor.rawNode.localName;
    const isValidName = InspectorUtils.isCustomElementName(name, namespaceURI);

    const customElements = doc.defaultView.customElements;
    return isValidName && !customElements.get(name);
  }

  _onCustomElementDefined(event) {
    const doc = event.target;
    const registry = doc.defaultView.customElements;
    const registryMap = this._getMapForRegistry(registry);

    const name = event.detail;
    const actors = this._getActorsForName(name, registryMap);
    this.emit("element-defined", { name, actors });
    registryMap.delete(name);
  }

  /**
   * Some nodes (e.g. inside of <template> tags) don't have a documentElement or an
   * ownerGlobal and can't be watched by this helper.
   */
  _isValidNode(nodeActor) {
    const node = nodeActor.rawNode;
    return (
      !Cu.isDeadWrapper(node) &&
      node.ownerGlobal &&
      node.ownerDocument?.documentElement
    );
  }
}

exports.CustomElementWatcher = CustomElementWatcher;
