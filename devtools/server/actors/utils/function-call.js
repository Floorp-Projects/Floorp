/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const {Cu} = require("chrome");
const protocol = require("devtools/shared/protocol");

const { functionCallSpec } = require("devtools/shared/specs/function-call");
const { KNOWN_METHODS } = require("devtools/shared/fronts/function-call");

/**
 * This actor contains information about a function call, like the function
 * type, name, stack, arguments, returned value etc.
 */
exports.FunctionCallActor = protocol.ActorClassWithSpec(functionCallSpec, {
  /**
   * Creates the function call actor.
   *
   * @param DebuggerServerConnection conn
   *        The server connection.
   * @param DOMWindow window
   *        The content window.
   * @param string global
   *        The name of the global object owning this function, like
   *        "CanvasRenderingContext2D" or "WebGLRenderingContext".
   * @param object caller
   *        The object owning the function when it was called.
   *        For example, in `foo.bar()`, the caller is `foo`.
   * @param number type
   *        Either METHOD_FUNCTION, METHOD_GETTER or METHOD_SETTER.
   * @param string name
   *        The called function's name.
   * @param array stack
   *        The called function's stack, as a list of { name, file, line } objects.
   * @param number timestamp
   *        The performance.now() timestamp when the function was called.
   * @param array args
   *        The called function's arguments.
   * @param any result
   *        The value returned by the function call.
   * @param boolean holdWeak
   *        Determines whether or not FunctionCallActor stores a weak reference
   *        to the underlying objects.
   */
  initialize: function(
    conn,
    [window, global, caller, type, name, stack, timestamp, args, result],
    holdWeak
  ) {
    protocol.Actor.prototype.initialize.call(this, conn);

    this.details = {
      global: global,
      type: type,
      name: name,
      stack: stack,
      timestamp: timestamp
    };

    // Store a weak reference to all objects so we don't
    // prevent natural GC if `holdWeak` was passed into
    // setup as truthy.
    if (holdWeak) {
      const weakRefs = {
        window: Cu.getWeakReference(window),
        caller: Cu.getWeakReference(caller),
        args: Cu.getWeakReference(args),
        result: Cu.getWeakReference(result),
      };

      Object.defineProperties(this.details, {
        window: { get: () => weakRefs.window.get() },
        caller: { get: () => weakRefs.caller.get() },
        args: { get: () => weakRefs.args.get() },
        result: { get: () => weakRefs.result.get() },
      });
    } else {
      // Otherwise, hold strong references to the objects.
      this.details.window = window;
      this.details.caller = caller;
      this.details.args = args;
      this.details.result = result;
    }

    // The caller, args and results are string names for now. It would
    // certainly be nicer if they were Object actors. Make this smarter, so
    // that the frontend can inspect each argument, be it object or primitive.
    // Bug 978960.
    this.details.previews = {
      caller: this._generateStringPreview(caller),
      args: this._generateArgsPreview(args),
      result: this._generateStringPreview(result)
    };
  },

  /**
   * Customize the marshalling of this actor to provide some generic information
   * directly on the Front instance.
   */
  form: function() {
    return {
      actor: this.actorID,
      type: this.details.type,
      name: this.details.name,
      file: this.details.stack[0].file,
      line: this.details.stack[0].line,
      timestamp: this.details.timestamp,
      callerPreview: this.details.previews.caller,
      argsPreview: this.details.previews.args,
      resultPreview: this.details.previews.result
    };
  },

  /**
   * Gets more information about this function call, which is not necessarily
   * available on the Front instance.
   */
  getDetails: function() {
    const { type, name, stack, timestamp } = this.details;

    // Since not all calls on the stack have corresponding owner files (e.g.
    // callbacks of a requestAnimationFrame etc.), there's no benefit in
    // returning them, as the user can't jump to the Debugger from them.
    for (let i = stack.length - 1; ;) {
      if (stack[i].file) {
        break;
      }
      stack.pop();
      i--;
    }

    // XXX: Use grips for objects and serialize them properly, in order
    // to add the function's caller, arguments and return value. Bug 978957.
    return {
      type: type,
      name: name,
      stack: stack,
      timestamp: timestamp
    };
  },

  /**
   * Serializes the arguments so that they can be easily be transferred
   * as a string, but still be useful when displayed in a potential UI.
   *
   * @param array args
   *        The source arguments.
   * @return string
   *         The arguments as a string.
   */
  _generateArgsPreview: function(args) {
    const { global, name, caller } = this.details;

    // Get method signature to determine if there are any enums
    // used in this method.
    let methodSignatureEnums;

    const knownGlobal = KNOWN_METHODS[global];
    if (knownGlobal) {
      const knownMethod = knownGlobal[name];
      if (knownMethod) {
        const isOverloaded = typeof knownMethod.enums === "function";
        if (isOverloaded) {
          methodSignatureEnums = knownMethod.enums(args);
        } else {
          methodSignatureEnums = knownMethod.enums;
        }
      }
    }

    const serializeArgs = () => args.map((arg, i) => {
      // XXX: Bug 978960.
      if (arg === undefined) {
        return "undefined";
      }
      if (arg === null) {
        return "null";
      }
      if (typeof arg == "function") {
        return "Function";
      }
      if (typeof arg == "object") {
        return "Object";
      }
      // If this argument matches the method's signature
      // and is an enum, change it to its constant name.
      if (methodSignatureEnums && methodSignatureEnums.has(i)) {
        return getBitToEnumValue(global, caller, arg);
      }
      return arg + "";
    });

    return serializeArgs().join(", ");
  },

  /**
   * Serializes the data so that it can be easily be transferred
   * as a string, but still be useful when displayed in a potential UI.
   *
   * @param object data
   *        The source data.
   * @return string
   *         The arguments as a string.
   */
  _generateStringPreview: function(data) {
    // XXX: Bug 978960.
    if (data === undefined) {
      return "undefined";
    }
    if (data === null) {
      return "null";
    }
    if (typeof data == "function") {
      return "Function";
    }
    if (typeof data == "object") {
      return "Object";
    }
    return data + "";
  }
});

/**
 * A lookup table for cross-referencing flags or properties with their name
 * assuming they look LIKE_THIS most of the time.
 *
 * For example, when gl.clear(gl.COLOR_BUFFER_BIT) is called, the actual passed
 * argument's value is 16384, which we want identified as "COLOR_BUFFER_BIT".
 */
var gEnumRegex = /^[A-Z][A-Z0-9_]+$/;
var gEnumsLookupTable = {};

// These values are returned from errors, or empty values,
// and need to be ignored when checking arguments due to the bitwise math.
var INVALID_ENUMS = [
  "INVALID_ENUM", "NO_ERROR", "INVALID_VALUE", "OUT_OF_MEMORY", "NONE"
];

function getBitToEnumValue(type, object, arg) {
  let table = gEnumsLookupTable[type];

  // If mapping not yet created, do it on the first run.
  if (!table) {
    table = gEnumsLookupTable[type] = {};

    for (const key in object) {
      if (key.match(gEnumRegex)) {
        // Maps `16384` to `"COLOR_BUFFER_BIT"`, etc.
        table[object[key]] = key;
      }
    }
  }

  // If a single bit value, just return it.
  if (table[arg]) {
    return table[arg];
  }

  // Otherwise, attempt to reduce it to the original bit flags:
  // `16640` -> "COLOR_BUFFER_BIT | DEPTH_BUFFER_BIT"
  const flags = [];
  for (let flag in table) {
    if (INVALID_ENUMS.includes(table[flag])) {
      continue;
    }

    // Cast to integer as all values are stored as strings
    // in `table`
    flag = flag | 0;
    if (flag && (arg & flag) === flag) {
      flags.push(table[flag]);
    }
  }

  // Cache the combined bitmask value
  table[arg] = flags.join(" | ") || arg;
  return table[arg];
}
