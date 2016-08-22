/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const {Cc, Ci, Cu, Cr} = require("chrome");
const events = require("sdk/event/core");
const protocol = require("devtools/shared/protocol");
const {serializeStack, parseStack} = require("toolkit/loader");

const {on, once, off, emit} = events;
const {method, Arg, Option, RetVal} = protocol;

const { functionCallSpec, callWatcherSpec } = require("devtools/shared/specs/call-watcher");
const { CallWatcherFront } = require("devtools/shared/fronts/call-watcher");

/**
 * This actor contains information about a function call, like the function
 * type, name, stack, arguments, returned value etc.
 */
var FunctionCallActor = protocol.ActorClassWithSpec(functionCallSpec, {
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
  initialize: function (conn, [window, global, caller, type, name, stack, timestamp, args, result], holdWeak) {
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
      let weakRefs = {
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
    }
    // Otherwise, hold strong references to the objects.
    else {
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
  form: function () {
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
  getDetails: function () {
    let { type, name, stack, timestamp } = this.details;

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
  _generateArgsPreview: function (args) {
    let { global, name, caller } = this.details;

    // Get method signature to determine if there are any enums
    // used in this method.
    let methodSignatureEnums;

    let knownGlobal = CallWatcherFront.KNOWN_METHODS[global];
    if (knownGlobal) {
      let knownMethod = knownGlobal[name];
      if (knownMethod) {
        let isOverloaded = typeof knownMethod.enums === "function";
        if (isOverloaded) {
          methodSignatureEnums = methodSignatureEnums(args);
        } else {
          methodSignatureEnums = knownMethod.enums;
        }
      }
    }

    let serializeArgs = () => args.map((arg, i) => {
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
  _generateStringPreview: function (data) {
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
 * This actor observes function calls on certain objects or globals.
 */
var CallWatcherActor = exports.CallWatcherActor = protocol.ActorClassWithSpec(callWatcherSpec, {
  initialize: function (conn, tabActor) {
    protocol.Actor.prototype.initialize.call(this, conn);
    this.tabActor = tabActor;
    this._onGlobalCreated = this._onGlobalCreated.bind(this);
    this._onGlobalDestroyed = this._onGlobalDestroyed.bind(this);
    this._onContentFunctionCall = this._onContentFunctionCall.bind(this);
    on(this.tabActor, "window-ready", this._onGlobalCreated);
    on(this.tabActor, "window-destroyed", this._onGlobalDestroyed);
  },
  destroy: function (conn) {
    protocol.Actor.prototype.destroy.call(this, conn);
    off(this.tabActor, "window-ready", this._onGlobalCreated);
    off(this.tabActor, "window-destroyed", this._onGlobalDestroyed);
    this.finalize();
  },

  /**
   * Lightweight listener invoked whenever an instrumented function is called
   * while recording. We're doing this to avoid the event emitter overhead,
   * since this is expected to be a very hot function.
   */
  onCall: null,

  /**
   * Starts waiting for the current tab actor's document global to be
   * created, in order to instrument the specified objects and become
   * aware of everything the content does with them.
   */
  setup: function ({ tracedGlobals, tracedFunctions, startRecording, performReload, holdWeak, storeCalls }) {
    if (this._initialized) {
      return;
    }
    this._initialized = true;
    this._timestampEpoch = 0;

    this._functionCalls = [];
    this._tracedGlobals = tracedGlobals || [];
    this._tracedFunctions = tracedFunctions || [];
    this._holdWeak = !!holdWeak;
    this._storeCalls = !!storeCalls;

    if (startRecording) {
      this.resumeRecording();
    }
    if (performReload) {
      this.tabActor.window.location.reload();
    }
  },

  /**
   * Stops listening for document global changes and puts this actor
   * to hibernation. This method is called automatically just before the
   * actor is destroyed.
   */
  finalize: function () {
    if (!this._initialized) {
      return;
    }
    this._initialized = false;
    this._finalized = true;

    this._tracedGlobals = null;
    this._tracedFunctions = null;
  },

  /**
   * Returns whether the instrumented function calls are currently recorded.
   */
  isRecording: function () {
    return this._recording;
  },

  /**
   * Initialize the timestamp epoch used to offset function call timestamps.
   */
  initTimestampEpoch: function () {
    this._timestampEpoch = this.tabActor.window.performance.now();
  },

  /**
   * Starts recording function calls.
   */
  resumeRecording: function () {
    this._recording = true;
  },

  /**
   * Stops recording function calls.
   */
  pauseRecording: function () {
    this._recording = false;
    return this._functionCalls;
  },

  /**
   * Erases all the recorded function calls.
   * Calling `resumeRecording` or `pauseRecording` does not erase history.
   */
  eraseRecording: function () {
    this._functionCalls = [];
  },

  /**
   * Invoked whenever the current tab actor's document global is created.
   */
  _onGlobalCreated: function ({window, id, isTopLevel}) {
    if (!this._initialized) {
      return;
    }

    // TODO: bug 981748, support more than just the top-level documents.
    if (!isTopLevel) {
      return;
    }

    let self = this;
    this._tracedWindowId = id;

    let unwrappedWindow = XPCNativeWrapper.unwrap(window);
    let callback = this._onContentFunctionCall;

    for (let global of this._tracedGlobals) {
      let prototype = unwrappedWindow[global].prototype;
      let properties = Object.keys(prototype);
      properties.forEach(name => overrideSymbol(global, prototype, name, callback));
    }

    for (let name of this._tracedFunctions) {
      overrideSymbol("window", unwrappedWindow, name, callback);
    }

    /**
     * Instruments a method, getter or setter on the specified target object to
     * invoke a callback whenever it is called.
     */
    function overrideSymbol(global, target, name, callback) {
      let propertyDescriptor = Object.getOwnPropertyDescriptor(target, name);

      if (propertyDescriptor.get || propertyDescriptor.set) {
        overrideAccessor(global, target, name, propertyDescriptor, callback);
        return;
      }
      if (propertyDescriptor.writable && typeof propertyDescriptor.value == "function") {
        overrideFunction(global, target, name, propertyDescriptor, callback);
        return;
      }
    }

    /**
     * Instruments a function on the specified target object.
     */
    function overrideFunction(global, target, name, descriptor, callback) {
      // Invoking .apply on an unxrayed content function doesn't work, because
      // the arguments array is inaccessible to it. Get Xrays back.
      let originalFunc = Cu.unwaiveXrays(target[name]);

      Cu.exportFunction(function (...args) {
        let result;
        try {
          result = Cu.waiveXrays(originalFunc.apply(this, args));
        } catch (e) {
          throw createContentError(e, unwrappedWindow);
        }

        if (self._recording) {
          let type = CallWatcherFront.METHOD_FUNCTION;
          let stack = getStack(name);
          let timestamp = self.tabActor.window.performance.now() - self._timestampEpoch;
          callback(unwrappedWindow, global, this, type, name, stack, timestamp, args, result);
        }
        return result;
      }, target, { defineAs: name });

      Object.defineProperty(target, name, {
        configurable: descriptor.configurable,
        enumerable: descriptor.enumerable,
        writable: true
      });
    }

    /**
     * Instruments a getter or setter on the specified target object.
     */
    function overrideAccessor(global, target, name, descriptor, callback) {
      // Invoking .apply on an unxrayed content function doesn't work, because
      // the arguments array is inaccessible to it. Get Xrays back.
      let originalGetter = Cu.unwaiveXrays(target.__lookupGetter__(name));
      let originalSetter = Cu.unwaiveXrays(target.__lookupSetter__(name));

      Object.defineProperty(target, name, {
        get: function (...args) {
          if (!originalGetter) return undefined;
          let result = Cu.waiveXrays(originalGetter.apply(this, args));

          if (self._recording) {
            let type = CallWatcherFront.GETTER_FUNCTION;
            let stack = getStack(name);
            let timestamp = self.tabActor.window.performance.now() - self._timestampEpoch;
            callback(unwrappedWindow, global, this, type, name, stack, timestamp, args, result);
          }
          return result;
        },
        set: function (...args) {
          if (!originalSetter) return;
          originalSetter.apply(this, args);

          if (self._recording) {
            let type = CallWatcherFront.SETTER_FUNCTION;
            let stack = getStack(name);
            let timestamp = self.tabActor.window.performance.now() - self._timestampEpoch;
            callback(unwrappedWindow, global, this, type, name, stack, timestamp, args, undefined);
          }
        },
        configurable: descriptor.configurable,
        enumerable: descriptor.enumerable
      });
    }

    /**
     * Stores the relevant information about calls on the stack when
     * a function is called.
     */
    function getStack(caller) {
      try {
        // Using Components.stack wouldn't be a better idea, since it's
        // much slower because it attempts to retrieve the C++ stack as well.
        throw new Error();
      } catch (e) {
        var stack = e.stack;
      }

      // Of course, using a simple regex like /(.*?)@(.*):(\d*):\d*/ would be
      // much prettier, but this is a very hot function, so let's sqeeze
      // every drop of performance out of it.
      let calls = [];
      let callIndex = 0;
      let currNewLinePivot = stack.indexOf("\n") + 1;
      let nextNewLinePivot = stack.indexOf("\n", currNewLinePivot);

      while (nextNewLinePivot > 0) {
        let nameDelimiterIndex = stack.indexOf("@", currNewLinePivot);
        let columnDelimiterIndex = stack.lastIndexOf(":", nextNewLinePivot - 1);
        let lineDelimiterIndex = stack.lastIndexOf(":", columnDelimiterIndex - 1);

        if (!calls[callIndex]) {
          calls[callIndex] = { name: "", file: "", line: 0 };
        }
        if (!calls[callIndex + 1]) {
          calls[callIndex + 1] = { name: "", file: "", line: 0 };
        }

        if (callIndex > 0) {
          let file = stack.substring(nameDelimiterIndex + 1, lineDelimiterIndex);
          let line = stack.substring(lineDelimiterIndex + 1, columnDelimiterIndex);
          let name = stack.substring(currNewLinePivot, nameDelimiterIndex);
          calls[callIndex].name = name;
          calls[callIndex - 1].file = file;
          calls[callIndex - 1].line = line;
        } else {
          // Since the topmost stack frame is actually our overwritten function,
          // it will not have the expected name.
          calls[0].name = caller;
        }

        currNewLinePivot = nextNewLinePivot + 1;
        nextNewLinePivot = stack.indexOf("\n", currNewLinePivot);
        callIndex++;
      }

      return calls;
    }
  },

  /**
   * Invoked whenever the current tab actor's inner window is destroyed.
   */
  _onGlobalDestroyed: function ({window, id, isTopLevel}) {
    if (this._tracedWindowId == id) {
      this.pauseRecording();
      this.eraseRecording();
      this._timestampEpoch = 0;
    }
  },

  /**
   * Invoked whenever an instrumented function is called.
   */
  _onContentFunctionCall: function (...details) {
    // If the consuming tool has finalized call-watcher, ignore the
    // still-instrumented calls.
    if (this._finalized) {
      return;
    }

    let functionCall = new FunctionCallActor(this.conn, details, this._holdWeak);

    if (this._storeCalls) {
      this._functionCalls.push(functionCall);
    }

    if (this.onCall) {
      this.onCall(functionCall);
    } else {
      emit(this, "call", functionCall);
    }
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

    for (let key in object) {
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
  let flags = [];
  for (let flag in table) {
    if (INVALID_ENUMS.indexOf(table[flag]) !== -1) {
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
  return table[arg] = flags.join(" | ") || arg;
}

/**
 * Creates a new error from an error that originated from content but was called
 * from a wrapped overridden method. This is so we can make our own error
 * that does not look like it originated from the call watcher.
 *
 * We use toolkit/loader's parseStack and serializeStack rather than the
 * parsing done in the local `getStack` function, because it does not expose
 * column number, would have to change the protocol models `call-stack-items` and `call-details`
 * which hurts backwards compatibility, and the local `getStack` is an optimized, hot function.
 */
function createContentError(e, win) {
  let { message, name, stack } = e;
  let parsedStack = parseStack(stack);
  let { fileName, lineNumber, columnNumber } = parsedStack[parsedStack.length - 1];
  let error;

  let isDOMException = e instanceof Ci.nsIDOMDOMException;
  let constructor = isDOMException ? win.DOMException : (win[e.name] || win.Error);

  if (isDOMException) {
    error = new constructor(message, name);
    Object.defineProperties(error, {
      code: { value: e.code },
      columnNumber: { value: 0 }, // columnNumber is always 0 for DOMExceptions?
      filename: { value: fileName }, // note the lowercase `filename`
      lineNumber: { value: lineNumber },
      result: { value: e.result },
      stack: { value: serializeStack(parsedStack) }
    });
  }
  else {
    // Constructing an error here retains all the stack information,
    // and we can add message, fileName and lineNumber via constructor, though
    // need to manually add columnNumber.
    error = new constructor(message, fileName, lineNumber);
    Object.defineProperty(error, "columnNumber", {
      value: columnNumber
    });
  }
  return error;
}
