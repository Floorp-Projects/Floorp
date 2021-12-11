/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const DevToolsUtils = require("devtools/shared/DevToolsUtils");

loader.lazyRequireGetter(
  this,
  "ObjectUtils",
  "devtools/server/actors/object/utils"
);

/**
 * Stringify a Debugger.Object based on its class.
 *
 * @param Debugger.Object obj
 *        The object to stringify.
 * @return String
 *         The stringification for the object.
 */
function stringify(obj) {
  if (!DevToolsUtils.isSafeDebuggerObject(obj)) {
    const unwrapped = DevToolsUtils.unwrap(obj);
    if (unwrapped === undefined) {
      return "<invisibleToDebugger>";
    } else if (unwrapped.isProxy) {
      return "<proxy>";
    }
    // The following line should not be reached. It's there just in case somebody
    // modifies isSafeDebuggerObject to return false for additional kinds of objects.
    return "[object " + obj.class + "]";
  } else if (obj.class == "DeadObject") {
    return "<dead object>";
  }

  const stringifier = stringifiers[obj.class] || stringifiers.Object;

  try {
    return stringifier(obj);
  } catch (e) {
    DevToolsUtils.reportException("stringify", e);
    return "<failed to stringify object>";
  }
}

/**
 * Create a function that can safely stringify Debugger.Objects of a given
 * builtin type.
 *
 * @param Function ctor
 *        The builtin class constructor.
 * @return Function
 *         The stringifier for the class.
 */
function createBuiltinStringifier(ctor) {
  return obj => {
    try {
      return ctor.prototype.toString.call(obj.unsafeDereference());
    } catch (err) {
      // The debuggee will see a "Function" class if the object is callable and
      // its compartment is not subsumed. The above will throw if it's not really
      // a function, e.g. if it's a callable proxy.
      return "[object " + obj.class + "]";
    }
  };
}

/**
 * Stringify a Debugger.Object-wrapped Error instance.
 *
 * @param Debugger.Object obj
 *        The object to stringify.
 * @return String
 *         The stringification of the object.
 */
function errorStringify(obj) {
  let name = DevToolsUtils.getProperty(obj, "name");
  if (name === "" || name === undefined) {
    name = obj.class;
  } else if (isObject(name)) {
    name = stringify(name);
  }

  let message = DevToolsUtils.getProperty(obj, "message");
  if (isObject(message)) {
    message = stringify(message);
  }

  if (message === "" || message === undefined) {
    return name;
  }
  return name + ": " + message;
}

// Used to prevent infinite recursion when an array is found inside itself.
var seen = null;

var stringifiers = {
  Error: errorStringify,
  EvalError: errorStringify,
  RangeError: errorStringify,
  ReferenceError: errorStringify,
  SyntaxError: errorStringify,
  TypeError: errorStringify,
  URIError: errorStringify,
  Boolean: createBuiltinStringifier(Boolean),
  Function: createBuiltinStringifier(Function),
  Number: createBuiltinStringifier(Number),
  RegExp: createBuiltinStringifier(RegExp),
  String: createBuiltinStringifier(String),
  Object: obj => "[object " + obj.class + "]",
  Array: obj => {
    // If we're at the top level then we need to create the Set for tracking
    // previously stringified arrays.
    const topLevel = !seen;
    if (topLevel) {
      seen = new Set();
    } else if (seen.has(obj)) {
      return "";
    }

    seen.add(obj);

    const len = ObjectUtils.getArrayLength(obj);
    let string = "";

    // Array.length is always a non-negative safe integer.
    for (let i = 0; i < len; i++) {
      const desc = obj.getOwnPropertyDescriptor(i);
      if (desc) {
        const { value } = desc;
        if (value != null) {
          string += isObject(value) ? stringify(value) : value;
        }
      }

      if (i < len - 1) {
        string += ",";
      }
    }

    if (topLevel) {
      seen = null;
    }

    return string;
  },
  DOMException: obj => {
    const message = DevToolsUtils.getProperty(obj, "message") || "<no message>";
    const result = (+DevToolsUtils.getProperty(obj, "result")).toString(16);
    const code = DevToolsUtils.getProperty(obj, "code");
    const name = DevToolsUtils.getProperty(obj, "name") || "<unknown>";

    return (
      '[Exception... "' +
      message +
      '" ' +
      'code: "' +
      code +
      '" ' +
      'nsresult: "0x' +
      result +
      " (" +
      name +
      ')"]'
    );
  },
  Promise: obj => {
    const { state, value, reason } = ObjectUtils.getPromiseState(obj);
    let statePreview = state;
    if (state != "pending") {
      const settledValue = state === "fulfilled" ? value : reason;
      statePreview +=
        ": " +
        (typeof settledValue === "object" && settledValue !== null
          ? stringify(settledValue)
          : settledValue);
    }
    return "Promise (" + statePreview + ")";
  },
};

/**
 * Determine if a given value is non-primitive.
 *
 * @param Any value
 *        The value to test.
 * @return Boolean
 *         Whether the value is non-primitive.
 */
function isObject(value) {
  const type = typeof value;
  return type == "object" ? value !== null : type == "function";
}

module.exports = stringify;
