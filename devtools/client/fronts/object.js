/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { objectSpec } = require("devtools/shared/specs/object");
const {
  FrontClassWithSpec,
  registerFront,
} = require("devtools/shared/protocol");
const { LongStringFront } = require("devtools/client/fronts/string");

/**
 * A ObjectFront is used as a front end for the ObjectActor that is
 * created on the server, hiding implementation details.
 */
class ObjectFront extends FrontClassWithSpec(objectSpec) {
  constructor(conn = null, targetFront = null, parentFront = null, data) {
    if (!parentFront) {
      throw new Error("ObjectFront require a parent front");
    }

    super(conn, targetFront, parentFront);

    this._grip = data;
    this.actorID = this._grip.actor;
    this.valid = true;

    parentFront.manage(this);
  }

  skipDestroy() {
    // Object fronts are simple fronts, they don't need to be cleaned up on
    // toolbox destroy. `conn` is a DebuggerClient instance, check the
    // `isToolboxDestroy` flag to skip the destroy.
    return this.conn && this.conn.isToolboxDestroy;
  }

  getGrip() {
    return this._grip;
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
  async getPrototypeAndProperties() {
    const result = await super.prototypeAndProperties();

    if (result.prototype) {
      result.prototype = getAdHocFrontOrPrimitiveGrip(result.prototype, this);
    }

    // The result packet can have multiple properties that hold grips which we may need
    // to turn into fronts.
    const gripKeys = ["value", "getterValue", "get", "set"];

    if (result.ownProperties) {
      Object.entries(result.ownProperties).forEach(([key, descriptor]) => {
        if (descriptor) {
          for (const gripKey of gripKeys) {
            if (descriptor.hasOwnProperty(gripKey)) {
              result.ownProperties[key][gripKey] = getAdHocFrontOrPrimitiveGrip(
                descriptor[gripKey],
                this
              );
            }
          }
        }
      });
    }

    if (result.safeGetterValues) {
      Object.entries(result.safeGetterValues).forEach(([key, descriptor]) => {
        if (descriptor) {
          for (const gripKey of gripKeys) {
            if (descriptor.hasOwnProperty(gripKey)) {
              result.safeGetterValues[key][
                gripKey
              ] = getAdHocFrontOrPrimitiveGrip(descriptor[gripKey], this);
            }
          }
        }
      });
    }

    if (result.ownSymbols) {
      result.ownSymbols.forEach((descriptor, i, arr) => {
        if (descriptor) {
          for (const gripKey of gripKeys) {
            if (descriptor.hasOwnProperty(gripKey)) {
              arr[i][gripKey] = getAdHocFrontOrPrimitiveGrip(
                descriptor[gripKey],
                this
              );
            }
          }
        }
      });
    }

    return result;
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
  async getPropertyValue(name, receiverId) {
    const response = await super.propertyValue(name, receiverId);

    if (response.value) {
      const { value } = response;
      if (value.return) {
        response.value.return = getAdHocFrontOrPrimitiveGrip(
          value.return,
          this
        );
      }

      if (value.throw) {
        response.value.throw = getAdHocFrontOrPrimitiveGrip(value.throw, this);
      }
    }
    return response;
  }

  /**
   * Request the prototype of the object.
   */
  async getPrototype() {
    const result = await super.prototype();

    if (!result.prototype) {
      return result;
    }

    result.prototype = getAdHocFrontOrPrimitiveGrip(result.prototype, this);

    return result;
  }

  /**
   * Request the display string of the object.
   */
  getDisplayString() {
    return super.displayString();
  }

  /**
   * Request the state of a promise.
   */
  async getPromiseState() {
    if (this._grip.class !== "Promise") {
      console.error("getPromiseState is only valid for promise grips.");
      return null;
    }

    let response, promiseState;
    try {
      response = await super.promiseState();
      promiseState = response.promiseState;
    } catch (error) {
      // @backward-compat { version 85 } On older server, the promiseState request didn't
      // didn't exist (bug 1552648). The promise state was directly included in the grip.
      if (error.message.includes("unrecognizedPacketType")) {
        promiseState = this._grip.promiseState;
        response = { promiseState };
      } else {
        throw error;
      }
    }

    const { value, reason } = promiseState;

    if (value) {
      promiseState.value = getAdHocFrontOrPrimitiveGrip(value, this);
    }

    if (reason) {
      promiseState.reason = getAdHocFrontOrPrimitiveGrip(reason, this);
    }

    return response;
  }

  /**
   * Request the target and handler internal slots of a proxy.
   */
  async getProxySlots() {
    if (this._grip.class !== "Proxy") {
      console.error("getProxySlots is only valid for proxy grips.");
      return null;
    }

    const response = await super.proxySlots();
    const { proxyHandler, proxyTarget } = response;

    if (proxyHandler) {
      response.proxyHandler = getAdHocFrontOrPrimitiveGrip(proxyHandler, this);
    }

    if (proxyTarget) {
      response.proxyTarget = getAdHocFrontOrPrimitiveGrip(proxyTarget, this);
    }

    return response;
  }

  get isSyntaxError() {
    return this._grip.preview && this._grip.preview.name == "SyntaxError";
  }
}

/**
 * When we are asking the server for the value of a given variable, we might get different
 * type of objects:
 * - a primitive (string, number, null, false, boolean)
 * - a long string
 * - an "object" (i.e. not primitive nor long string)
 *
 * Each of those type need a different front, or none:
 * - a primitive does not allow further interaction with the server, so we don't need
 *   to have a dedicated front.
 * - a long string needs a longStringFront to be able to retrieve the full string.
 * - an object need an objectFront to retrieve properties, symbols and prototype.
 *
 * In the case an ObjectFront is created, we also check if the object has properties
 * that should be turned into fronts as well.
 *
 * @param {String|Number|Object} options: The packet returned by the server.
 * @param {Front} parentFront
 *
 * @returns {Number|String|Object|LongStringFront|ObjectFront}
 */
function getAdHocFrontOrPrimitiveGrip(packet, parentFront) {
  // We only want to try to create a front when it makes sense, i.e when it has an
  // actorID, unless:
  // - it's a Symbol (See Bug 1600299)
  // - it's a mapEntry (the preview.key and preview.value properties can hold actors)
  // - or it is already a front (happens when we are using the legacy listeners in the ResourceWatcher)
  const isPacketAnObject = packet && typeof packet === "object";
  const isFront = !!packet.typeName;
  if (
    !isPacketAnObject ||
    packet.type == "symbol" ||
    (packet.type !== "mapEntry" && !packet.actor) ||
    isFront
  ) {
    return packet;
  }

  const { conn } = parentFront;
  // If the parent front is a target, consider it as the target to use for all objects
  const targetFront = parentFront.isTargetFront
    ? parentFront
    : parentFront.targetFront;

  // We may have already created a front for this object actor since some actor (e.g. the
  // thread actor) cache the object actors they create.
  const existingFront = conn.getFrontByID(packet.actor);
  if (existingFront) {
    return existingFront;
  }

  const { type } = packet;

  if (type === "longString") {
    const longStringFront = new LongStringFront(conn, targetFront, parentFront);
    longStringFront.form(packet);
    parentFront.manage(longStringFront);
    return longStringFront;
  }

  if (type === "mapEntry" && packet.preview) {
    const { key, value } = packet.preview;
    packet.preview.key = getAdHocFrontOrPrimitiveGrip(
      key,
      parentFront,
      targetFront
    );
    packet.preview.value = getAdHocFrontOrPrimitiveGrip(
      value,
      parentFront,
      targetFront
    );
    return packet;
  }

  const objectFront = new ObjectFront(conn, targetFront, parentFront, packet);
  createChildFronts(objectFront, packet);
  return objectFront;
}

/**
 * Create child fronts of the passed object front given a packet. Those child fronts are
 * usually mapping actors of the packet sub-properties (preview items, promise fullfilled
 * values, …).
 *
 * @param {ObjectFront} objectFront
 * @param {String|Number|Object} packet: The packet returned by the server
 */
function createChildFronts(objectFront, packet) {
  if (packet.preview) {
    const { message, entries } = packet.preview;

    // The message could be a longString.
    if (packet.preview.message) {
      packet.preview.message = getAdHocFrontOrPrimitiveGrip(
        message,
        objectFront
      );
    }

    // Handle Map/WeakMap preview entries (the preview might be directly used if has all the
    // items needed, i.e. if the Map has less than 10 items).
    if (entries && Array.isArray(entries)) {
      packet.preview.entries = entries.map(([key, value]) => [
        getAdHocFrontOrPrimitiveGrip(key, objectFront),
        getAdHocFrontOrPrimitiveGrip(value, objectFront),
      ]);
    }
  }

  if (packet && typeof packet.ownProperties === "object") {
    for (const [name, descriptor] of Object.entries(packet.ownProperties)) {
      // The descriptor can have multiple properties that hold grips which we may need
      // to turn into fronts.
      const gripKeys = ["value", "getterValue", "get", "set"];
      for (const key of gripKeys) {
        if (
          descriptor &&
          typeof descriptor === "object" &&
          descriptor.hasOwnProperty(key)
        ) {
          packet.ownProperties[name][key] = getAdHocFrontOrPrimitiveGrip(
            descriptor[key],
            objectFront
          );
        }
      }
    }
  }
}

registerFront(ObjectFront);

exports.ObjectFront = ObjectFront;
exports.getAdHocFrontOrPrimitiveGrip = getAdHocFrontOrPrimitiveGrip;
