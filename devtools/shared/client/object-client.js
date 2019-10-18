/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { objectSpec } = require("devtools/shared/specs/object");
const {
  FrontClassWithSpec,
  registerFront,
} = require("devtools/shared/protocol");

/**
 * Grip clients are used to retrieve information about the relevant object.
 *
 * @param client DebuggerClient
 *        The debugger client parent.
 * @param grip object
 *        A pause-lifetime object grip returned by the protocol.
 */
class ObjectClient extends FrontClassWithSpec(objectSpec) {
  constructor(client, grip) {
    super(client);
    this._grip = grip;
    this._client = client;
    this.valid = true;
    this.actorID = this._grip.actor;

    this.manage(this);
  }

  get actor() {
    return this.actorID;
  }

  get _transport() {
    return this._client._transport;
  }

  get isFrozen() {
    return this._grip.frozen;
  }

  get isSealed() {
    return this._grip.sealed;
  }

  get isExtensible() {
    return this._grip.extensible;
  }

  getDefinitionSite() {
    if (this._grip.class != "Function") {
      console.error("getDefinitionSite is only valid for function grips.");
      return null;
    }
    return super.definitionSite();
  }

  /**
   * Request the names of a function's formal parameters.
   */
  getParameterNames() {
    if (this._grip.class !== "Function") {
      console.error("getParameterNames is only valid for function grips.");
      return null;
    }
    return super.parameterNames();
  }

  /**
   * Request the names of the properties defined on the object and not its
   * prototype.
   */
  getOwnPropertyNames() {
    return super.ownPropertyNames();
  }

  /**
   * Request the prototype and own properties of the object.
   */
  getPrototypeAndProperties() {
    return super.prototypeAndProperties();
  }

  /**
   * Request a PropertyIteratorFront instance to ease listing
   * properties for this object.
   *
   * @param options Object
   *        A dictionary object with various boolean attributes:
   *        - ignoreIndexedProperties Boolean
   *          If true, filters out Array items.
   *          e.g. properties names between `0` and `object.length`.
   *        - ignoreNonIndexedProperties Boolean
   *          If true, filters out items that aren't array items
   *          e.g. properties names that are not a number between `0`
   *          and `object.length`.
   *        - sort Boolean
   *          If true, the iterator will sort the properties by name
   *          before dispatching them.
   */
  enumProperties(options) {
    return super.enumProperties(options);
  }

  /**
   * Request a PropertyIteratorFront instance to enumerate entries in a
   * Map/Set-like object.
   */
  enumEntries() {
    if (
      !["Map", "WeakMap", "Set", "WeakSet", "Storage"].includes(
        this._grip.class
      )
    ) {
      console.error(
        "enumEntries is only valid for Map/Set/Storage-like grips."
      );
      return null;
    }
    return super.enumEntries();
  }

  /**
   * Request a SymbolIteratorFront instance to enumerate symbols in an object.
   */
  enumSymbols() {
    if (this._grip.type !== "object") {
      console.error("enumSymbols is only valid for objects grips.");
      return null;
    }
    return super.enumSymbols();
  }

  /**
   * Request the property descriptor of the object's specified property.
   *
   * @param name string The name of the requested property.
   */
  getProperty(name) {
    return super.property(name);
  }

  /**
   * Request the value of the object's specified property.
   *
   * @param name string The name of the requested property.
   * @param receiverId string|null The actorId of the receiver to be used for getters.
   */
  getPropertyValue(name, receiverId) {
    return super.propertyValue(name, receiverId);
  }

  /**
   * Request the prototype of the object.
   */
  getPrototype() {
    return super.prototype();
  }

  /**
   * Request the display string of the object.
   */
  getDisplayString() {
    return super.displayString();
  }

  /**
   * Request the scope of the object.
   */
  getScope() {
    if (this._grip.class !== "Function") {
      console.error("scope is only valid for function grips.");
      return null;
    }
    return super.scope();
  }

  /**
   * Request the target and handler internal slots of a proxy.
   */
  getProxySlots() {
    if (this._grip.class !== "Proxy") {
      console.error("getProxySlots is only valid for proxy grips.");
      return null;
    }

    const response = super.proxySlots();
    // Before Firefox 68 (bug 1392760), the proxySlots request didn't exist.
    // The proxy target and handler were directly included in the grip.
    if (response.error === "unrecognizedPacketType") {
      const { proxyTarget, proxyHandler } = this._grip;
      return { proxyTarget, proxyHandler };
    }

    return response;
  }
}

module.exports = ObjectClient;
registerFront(ObjectClient);
