/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { DebuggerServer } = require("devtools/server/debugger-server");
const DevToolsUtils = require("devtools/shared/DevToolsUtils");
const { assert } = DevToolsUtils;

loader.lazyRequireGetter(
  this,
  "longStringGrip",
  "devtools/server/actors/object/long-string",
  true
);
loader.lazyRequireGetter(
  this,
  "symbolGrip",
  "devtools/server/actors/object/symbol",
  true
);

/**
 * Get thisDebugger.Object referent's `promiseState`.
 *
 * @returns Object
 *          An object of one of the following forms:
 *          - { state: "pending" }
 *          - { state: "fulfilled", value }
 *          - { state: "rejected", reason }
 */
function getPromiseState(obj) {
  if (obj.class != "Promise") {
    throw new Error(
      "Can't call `getPromiseState` on `Debugger.Object`s that don't " +
        "refer to Promise objects."
    );
  }

  const state = { state: obj.promiseState };
  if (state.state === "fulfilled") {
    state.value = obj.promiseValue;
  } else if (state.state === "rejected") {
    state.reason = obj.promiseReason;
  }
  return state;
}

/**
 * Make a debuggee value for the given object, if needed. Primitive values
 * are left the same.
 *
 * Use case: you have a raw JS object (after unsafe dereference) and you want to
 * send it to the client. In that case you need to use an ObjectActor which
 * requires a debuggee value. The Debugger.Object.prototype.makeDebuggeeValue()
 * method works only for JS objects and functions.
 *
 * @param Debugger.Object obj
 * @param any value
 * @return object
 */
function makeDebuggeeValueIfNeeded(obj, value) {
  if (value && (typeof value == "object" || typeof value == "function")) {
    return obj.makeDebuggeeValue(value);
  }
  return value;
}

/**
 * Convert a debuggee value into the underlying raw object, if needed.
 */
function unwrapDebuggeeValue(value) {
  if (value && typeof value == "object") {
    return value.unsafeDereference();
  }
  return value;
}

/**
 * Create a grip for the given debuggee value.  If the value is an
 * object, will create an actor with the given lifetime.
 */
function createValueGrip(value, pool, makeObjectGrip) {
  switch (typeof value) {
    case "boolean":
      return value;

    case "string":
      if (stringIsLong(value)) {
        return longStringGrip(value, pool);
      }
      return value;

    case "number":
      if (value === Infinity) {
        return { type: "Infinity" };
      } else if (value === -Infinity) {
        return { type: "-Infinity" };
      } else if (Number.isNaN(value)) {
        return { type: "NaN" };
      } else if (!value && 1 / value === -Infinity) {
        return { type: "-0" };
      }
      return value;

    case "bigint":
      return {
        type: "BigInt",
        text: value.toString(),
      };

    case "undefined":
      return { type: "undefined" };

    case "object":
      if (value === null) {
        return { type: "null" };
      } else if (
        value.optimizedOut ||
        value.uninitialized ||
        value.missingArguments
      ) {
        // The slot is optimized out, an uninitialized binding, or
        // arguments on a dead scope
        return {
          type: "null",
          optimizedOut: value.optimizedOut,
          uninitialized: value.uninitialized,
          missingArguments: value.missingArguments,
        };
      }
      return makeObjectGrip(value, pool);

    case "symbol":
      return symbolGrip(value, pool);

    default:
      assert(false, "Failed to provide a grip for: " + value);
      return null;
  }
}

/**
 * of passing the value directly over the protocol.
 *
 * @param str String
 *        The string we are checking the length of.
 */
function stringIsLong(str) {
  return str.length >= DebuggerServer.LONG_STRING_LENGTH;
}

const TYPED_ARRAY_CLASSES = [
  "Uint8Array",
  "Uint8ClampedArray",
  "Uint16Array",
  "Uint32Array",
  "Int8Array",
  "Int16Array",
  "Int32Array",
  "Float32Array",
  "Float64Array",
];

/**
 * Returns true if a debuggee object is a typed array.
 *
 * @param obj Debugger.Object
 *        The debuggee object to test.
 * @return Boolean
 */
function isTypedArray(object) {
  return TYPED_ARRAY_CLASSES.includes(object.class);
}

/**
 * Returns true if a debuggee object is an array, including a typed array.
 *
 * @param obj Debugger.Object
 *        The debuggee object to test.
 * @return Boolean
 */
function isArray(object) {
  return isTypedArray(object) || object.class === "Array";
}

/**
 * Returns the length of an array (or typed array).
 *
 * @param object Debugger.Object
 *        The debuggee object of the array.
 * @return Number
 * @throws if the object is not an array.
 */
function getArrayLength(object) {
  if (!isArray(object)) {
    throw new Error("Expected an array, got a " + object.class);
  }

  // Real arrays have a reliable `length` own property.
  if (object.class === "Array") {
    return DevToolsUtils.getProperty(object, "length");
  }

  // When replaying, we use a special API to get typed array lengths. We can't
  // invoke getters on the proxy returned by unsafeDereference().
  if (isReplaying) {
    return object.getTypedArrayLength();
  }

  // For typed arrays, `DevToolsUtils.getProperty` is not reliable because the `length`
  // getter could be shadowed by an own property, and `getOwnPropertyNames` is
  // unnecessarily slow. Obtain the `length` getter safely and call it manually.
  const typedProto = Object.getPrototypeOf(Uint8Array.prototype);
  const getter = Object.getOwnPropertyDescriptor(typedProto, "length").get;
  return getter.call(object.unsafeDereference());
}

/**
 * Returns the number of elements in a Set or Map.
 *
 * @param object Debugger.Object
 *        The debuggee object of the Set or Map.
 * @return Number
 */
function getContainerSize(object) {
  if (object.class != "Set" && object.class != "Map") {
    throw new Error(`Expected a set/map, got a ${object.class}`);
  }

  if (isReplaying) {
    return object.getContainerSize();
  }

  return DevToolsUtils.getProperty(object, "size");
}

/**
 * Returns true if the parameter is suitable to be an array index.
 *
 * @param str String
 * @return Boolean
 */
function isArrayIndex(str) {
  // Transform the parameter to a 32-bit unsigned integer.
  const num = str >>> 0;
  // Check that the parameter is a canonical Uint32 index.
  return (
    num + "" === str &&
    // Array indices cannot attain the maximum Uint32 value.
    num != -1 >>> 0
  );
}

/**
 * Returns true if a debuggee object is a local or sessionStorage object.
 *
 * @param object Debugger.Object
 *        The debuggee object to test.
 * @return Boolean
 */
function isStorage(object) {
  return object.class === "Storage";
}

/**
 * Returns the length of a local or sessionStorage object.
 *
 * @param object Debugger.Object
 *        The debuggee object of the array.
 * @return Number
 * @throws if the object is not a local or sessionStorage object.
 */
function getStorageLength(object) {
  if (!isStorage(object)) {
    throw new Error("Expected a storage object, got a " + object.class);
  }
  return DevToolsUtils.getProperty(object, "length");
}

// Get the string representation of a Debugger.Object for a RegExp.
function getRegExpString(object) {
  if (isReplaying) {
    return object.getRegExpString();
  }

  return DevToolsUtils.callPropertyOnObject(object, "toString");
}

// Get the time associated with a Debugger.Object for a Date.
function getDateTime(object) {
  if (isReplaying) {
    return object.getDateTime();
  }

  return DevToolsUtils.callPropertyOnObject(object, "getTime");
}

// Get the properties of a Debugger.Object for an Error which are needed to
// preview the object.
function getErrorProperties(object) {
  if (isReplaying) {
    return object.getErrorProperties();
  }

  return {
    name: DevToolsUtils.getProperty(object, "name"),
    message: DevToolsUtils.getProperty(object, "message"),
    stack: DevToolsUtils.getProperty(object, "stack"),
    fileName: DevToolsUtils.getProperty(object, "fileName"),
    lineNumber: DevToolsUtils.getProperty(object, "lineNumber"),
    columnNumber: DevToolsUtils.getProperty(object, "columnNumber"),
  };
}

module.exports = {
  getPromiseState,
  makeDebuggeeValueIfNeeded,
  unwrapDebuggeeValue,
  createValueGrip,
  stringIsLong,
  isTypedArray,
  isArray,
  isStorage,
  getArrayLength,
  getStorageLength,
  getContainerSize,
  isArrayIndex,
  getRegExpString,
  getDateTime,
  getErrorProperties,
};
