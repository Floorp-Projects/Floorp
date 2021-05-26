/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Cu } = require("chrome");
const { DevToolsServer } = require("devtools/server/devtools-server");
const DevToolsUtils = require("devtools/shared/DevToolsUtils");
const { assert } = DevToolsUtils;

loader.lazyRequireGetter(
  this,
  "LongStringActor",
  "devtools/server/actors/string",
  true
);

loader.lazyRequireGetter(
  this,
  "symbolGrip",
  "devtools/server/actors/object/symbol",
  true
);

loader.lazyRequireGetter(
  this,
  "ObjectActor",
  "devtools/server/actors/object",
  true
);

loader.lazyRequireGetter(
  this,
  "EnvironmentActor",
  "devtools/server/actors/environment",
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
 * Returns true if value is an object or function
 *
 * @param value
 * @returns {boolean}
 */

function isObjectOrFunction(value) {
  return value && (typeof value == "object" || typeof value == "function");
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
  if (isObjectOrFunction(value)) {
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
 * Create a grip for the given debuggee value. If the value is an object or a long string,
 * it will create an actor and add it to the pool
 * @param {any} value: The debuggee value.
 * @param {Pool} pool: The pool where the created actor will be added.
 * @param {Function} makeObjectGrip: Function that will be called to create the grip for
 *                                   non-primitive values.
 */
function createValueGrip(value, pool, makeObjectGrip) {
  switch (typeof value) {
    case "boolean":
      return value;

    case "string":
      if (stringIsLong(value)) {
        for (const child of pool.poolChildren()) {
          if (child instanceof LongStringActor && child.str == value) {
            return child.form();
          }
        }

        const actor = new LongStringActor(pool.conn, value);
        pool.manage(actor);
        return actor.form();
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
  return str.length >= DevToolsServer.LONG_STRING_LENGTH;
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
  "BigInt64Array",
  "BigUint64Array",
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

  // For typed arrays, `DevToolsUtils.getProperty` is not reliable because the `length`
  // getter could be shadowed by an own property, and `getOwnPropertyNames` is
  // unnecessarily slow. Obtain the `length` getter safely and call it manually.
  const typedProto = Object.getPrototypeOf(Uint8Array.prototype);
  const getter = Object.getOwnPropertyDescriptor(typedProto, "length").get;
  return getter.call(object.unsafeDereference());
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

/**
 * Returns an array of properties based on event class name.
 *
 * @param className
 * @returns {Array}
 */
function getPropsForEvent(className) {
  const positionProps = ["buttons", "clientX", "clientY", "layerX", "layerY"];
  const eventToPropsMap = {
    MouseEvent: positionProps,
    DragEvent: positionProps,
    PointerEvent: positionProps,
    SimpleGestureEvent: positionProps,
    WheelEvent: positionProps,
    KeyboardEvent: ["key", "charCode", "keyCode"],
    TransitionEvent: ["propertyName", "pseudoElement"],
    AnimationEvent: ["animationName", "pseudoElement"],
    ClipboardEvent: ["clipboardData"],
  };

  if (className in eventToPropsMap) {
    return eventToPropsMap[className];
  }

  return [];
}

/**
 * Returns an array of of all properties of an object
 *
 * @param obj
 * @param rawObj
 * @returns {Array}
 */
function getPropNamesFromObject(obj, rawObj) {
  let names = [];

  try {
    if (isStorage(obj)) {
      // local and session storage cannot be iterated over using
      // Object.getOwnPropertyNames() because it skips keys that are duplicated
      // on the prototype e.g. "key", "getKeys" so we need to gather the real
      // keys using the storage.key() function.
      for (let j = 0; j < rawObj.length; j++) {
        names.push(rawObj.key(j));
      }
    } else {
      names = obj.getOwnPropertyNames();
    }
  } catch (ex) {
    // Calling getOwnPropertyNames() on some wrapped native prototypes is not
    // allowed: "cannot modify properties of a WrappedNative". See bug 952093.
  }

  return names;
}

/**
 * Returns an array of private properties of an object
 *
 * @param obj
 * @returns {Array}
 */
function getSafePrivatePropertiesSymbols(obj) {
  try {
    return obj.getOwnPrivateProperties();
  } catch (ex) {
    return [];
  }
}

/**
 * Returns an array of all symbol properties of an object
 *
 * @param obj
 * @returns {Array}
 */
function getSafeOwnPropertySymbols(obj) {
  try {
    return obj.getOwnPropertySymbols();
  } catch (ex) {
    return [];
  }
}

/**
 * Returns an array modifiers based on keys
 *
 * @param rawObj
 * @returns {Array}
 */
function getModifiersForEvent(rawObj) {
  const modifiers = [];
  const keysToModifiersMap = {
    altKey: "Alt",
    ctrlKey: "Control",
    metaKey: "Meta",
    shiftKey: "Shift",
  };

  for (const key in keysToModifiersMap) {
    if (keysToModifiersMap.hasOwnProperty(key) && rawObj[key]) {
      modifiers.push(keysToModifiersMap[key]);
    }
  }

  return modifiers;
}

/**
 * Make a debuggee value for the given value.
 *
 * @param TargetActor targetActor
 *        The Target Actor from which this object originates.
 * @param mixed value
 *        The value you want to get a debuggee value for.
 * @return object
 *         Debuggee value for |value|.
 */
function makeDebuggeeValue(targetActor, value) {
  if (isObject(value)) {
    try {
      const global = Cu.getGlobalForObject(value);
      const dbgGlobal = targetActor.dbg.makeGlobalObjectReference(global);
      return dbgGlobal.makeDebuggeeValue(value);
    } catch (ex) {
      // The above can throw an exception if value is not an actual object
      // or 'Object in compartment marked as invisible to Debugger'
    }
  }
  const dbgGlobal = targetActor.dbg.makeGlobalObjectReference(
    targetActor.window || targetActor.workerGlobal
  );
  return dbgGlobal.makeDebuggeeValue(value);
}

function isObject(value) {
  return Object(value) === value;
}

/**
 * Create a grip for the given string.
 *
 * @param TargetActor targetActor
 *        The Target Actor from which this object originates.
 */
function createStringGrip(targetActor, string) {
  if (string && stringIsLong(string)) {
    const actor = new LongStringActor(targetActor.conn, string);
    targetActor.manage(actor);
    return actor.form();
  }
  return string;
}

/**
 * Create a grip for the given value.
 *
 * @param TargetActor targetActor
 *        The Target Actor from which this object originates.
 * @param mixed value
 *        The value you want to get a debuggee value for.
 * @param Number depth
 *        Depth of the object compared to the top level object,
 *        when we are inspecting nested attributes.
 * @return object
 */
function createValueGripForTarget(targetActor, value, depth = 0) {
  return createValueGrip(
    value,
    targetActor,
    createObjectGrip.bind(null, targetActor, depth)
  );
}

/**
 * Create and return an environment actor that corresponds to the provided
 * Debugger.Environment. This is a straightforward clone of the ThreadActor's
 * method except that it stores the environment actor in the web console
 * actor's pool.
 *
 * @param Debugger.Environment environment
 *        The lexical environment we want to extract.
 * @param TargetActor targetActor
 *        The Target Actor to use as parent actor.
 * @return The EnvironmentActor for |environment| or |undefined| for host
 *         functions or functions scoped to a non-debuggee global.
 */
function createEnvironmentActor(environment, targetActor) {
  if (!environment) {
    return undefined;
  }

  if (environment.actor) {
    return environment.actor;
  }

  const actor = new EnvironmentActor(environment, targetActor);
  targetActor.manage(actor);
  environment.actor = actor;

  return actor;
}

/**
 * Create a grip for the given object.
 *
 * @param TargetActor targetActor
 *        The Target Actor from which this object originates.
 * @param Number depth
 *        Depth of the object compared to the top level object,
 *        when we are inspecting nested attributes.
 * @param object object
 *        The object you want.
 * @param object pool
 *        A Pool where the new actor instance is added.
 * @param object
 *        The object grip.
 */
function createObjectGrip(targetActor, depth, object, pool) {
  let gripDepth = depth;
  const actor = new ObjectActor(
    object,
    {
      thread: targetActor.threadActor,
      getGripDepth: () => gripDepth,
      incrementGripDepth: () => gripDepth++,
      decrementGripDepth: () => gripDepth--,
      createValueGrip: v => createValueGripForTarget(targetActor, v, gripDepth),
      createEnvironmentActor: env => createEnvironmentActor(env, targetActor),
    },
    targetActor.conn
  );
  pool.manage(actor);
  return actor.form();
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
  isArrayIndex,
  getPropsForEvent,
  getPropNamesFromObject,
  getSafeOwnPropertySymbols,
  getSafePrivatePropertiesSymbols,
  getModifiersForEvent,
  isObjectOrFunction,
  createStringGrip,
  makeDebuggeeValue,
  createValueGripForTarget,
};
